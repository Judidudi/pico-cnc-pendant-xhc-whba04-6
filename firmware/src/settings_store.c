#include "settings_store.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

// ---------------------------------------------------------------------
// Persistent settings, laid out so that a save NEVER erases flash while the
// machine can move.
//
// Why that matters: an erase runs from RAM with interrupts off, which
// freezes the main loop and with it the safety heartbeat. Measured on this
// board it took 112 to 160 ms. Sizing the monostable above that would leave
// a real firmware hang undetected for just as long.
//
// The layout: TWO sectors, each holding 16 slots of one flash page. A save
// programs the next free slot - a page program, a few milliseconds, no
// erase. When a sector is full the other one takes over; it is already
// blank, because the stale sector is erased at BOOT (see
// settings_store_boot_maintenance()), before the heartbeat starts and before
// the machine can be enabled.
//
// Newest wins: every record carries a sequence number, and loading picks the
// valid record with the highest one.
//
// A side effect worth having: a power loss in the middle of a write damages
// only the slot being written. The previous record is untouched, so there is
// no window in which nothing valid exists - unlike erase-then-write, where
// the whole sector is blank for the duration.
// ---------------------------------------------------------------------

#define SETTINGS_MAGIC 0x434E4331u  // "CNC1"

// Version 6: two-sector layout with a sequence number per record.
//
// 5 added the user settings, 4 the key assignment, 3 dropped the operating
// modes, 2 the 180 degree panel rotation. The rule stays: raise the version,
// let old records fall away and fall back to defaults rather than loading
// something that no longer fits.
// 7: the default colour theme became Light and an assignment may now be
// "None" or a macro. Only the DEFAULT changed, not the layout - but a stored
// record from version 6 would keep handing back the old default, so it is
// discarded and written afresh.
#define SETTINGS_VERSION 7u

#define SETTINGS_SECTOR_COUNT     2
#define SETTINGS_SLOTS_PER_SECTOR (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t sequence;  // higher is newer
    xpt2046_calibration_t calibration;
    button_mapping_t mapping;
    pendant_settings_t settings;
    uint32_t checksum;
} settings_record_t;

_Static_assert(sizeof(settings_record_t) <= FLASH_PAGE_SIZE,
               "record does not fit into one flash page");

// The last two sectors of the flash, far beyond the program code
static const uint32_t sector_offset[SETTINGS_SECTOR_COUNT] = {
    PICO_FLASH_SIZE_BYTES - 2u * FLASH_SECTOR_SIZE,
    PICO_FLASH_SIZE_BYTES - 1u * FLASH_SECTOR_SIZE,
};

// Where the newest record currently lives, and the mirror of its contents.
// The mirror is what lets each save function change its own part and leave
// the rest of the record alone.
static settings_record_t current;
static bool current_valid = false;

// Whether flash actually holds a valid record. Separate from current_valid,
// and the distinction matters: with nothing stored, prime_current() seeds
// the mirror from the LIVE state, so a plain "has anything changed?" compare
// would answer no and the very first save would be skipped. That hit every
// device once after a version bump - the first setting a user changed was
// silently not persisted.
static bool stored_exists = false;
static uint8_t active_sector = 0;
static int8_t active_slot = -1;  // -1: nothing stored yet
static uint32_t next_sequence = 1;

static const settings_record_t *slot_record(uint8_t sector, uint8_t slot)
{
    return (const settings_record_t *) (XIP_BASE + sector_offset[sector] +
                                        (uint32_t) slot * FLASH_PAGE_SIZE);
}

// FNV-1a over everything ahead of the checksum field. No protection against
// tampering, only against half-written or never-written slots.
static uint32_t checksum_of(const settings_record_t *record)
{
    const uint8_t *bytes = (const uint8_t *) record;
    uint32_t hash = 0x811C9DC5u;

    for (size_t i = 0; i < offsetof(settings_record_t, checksum); i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }

    return hash;
}

// Rejects a record whose assignment is not usable: a byte that is not in
// the action table, or the same action on two keys. Both would come from a
// corrupted slot, and a duplicate makes one of the two keys look dead
// because the report would carry the code twice.
//
// "None" is exempt from the duplicate check - several keys doing nothing is
// an ordinary state, not corruption.
static bool mapping_is_sane(const button_mapping_t *mapping)
{
    for (uint8_t i = 0; i < PENDANT_BUTTON_COUNT; i++) {
        uint8_t value = mapping->assigned_function[i];

        if (pendant_action_index(value) == PENDANT_ACTION_NOT_FOUND) {
            return false;
        }
        if (value == PENDANT_ACTION_NONE) {
            continue;
        }

        for (uint8_t j = i + 1; j < PENDANT_BUTTON_COUNT; j++) {
            if (mapping->assigned_function[j] == value) {
                return false;
            }
        }
    }

    return true;
}

static bool record_is_valid(const settings_record_t *record)
{
    return record->magic == SETTINGS_MAGIC && record->version == SETTINGS_VERSION &&
           record->checksum == checksum_of(record) && mapping_is_sane(&record->mapping) &&
           pendant_settings_are_sane(&record->settings);
}

static bool sector_is_blank(uint8_t sector)
{
    const uint32_t *words = (const uint32_t *) (XIP_BASE + sector_offset[sector]);

    for (size_t i = 0; i < FLASH_SECTOR_SIZE / sizeof(uint32_t); i++) {
        if (words[i] != 0xFFFFFFFFu) {
            return false;
        }
    }

    return true;
}

static void erase_sector(uint8_t sector)
{
    uint64_t start_us = time_us_64();

    uint32_t irq_state = save_and_disable_interrupts();
    flash_range_erase(sector_offset[sector], FLASH_SECTOR_SIZE);
    restore_interrupts(irq_state);

    printf("settings: erased sector %u, blocked for %llu us\n", (unsigned) sector,
           (unsigned long long) (time_us_64() - start_us));
}

// Programs one slot. This is the only flash access a normal save performs,
// and it is the reason a save no longer stalls the loop for 100+ ms.
static bool program_slot(uint8_t sector, uint8_t slot, const settings_record_t *record)
{
    // Static rather than on the stack: a flash page is 256 bytes, and the
    // 4 KB stack does not have that much to spare next to LVGL
    static uint8_t page[FLASH_PAGE_SIZE];

    memset(page, 0xFF, sizeof(page));
    memcpy(page, record, sizeof(*record));

    uint64_t start_us = time_us_64();

    uint32_t irq_state = save_and_disable_interrupts();
    flash_range_program(sector_offset[sector] + (uint32_t) slot * FLASH_PAGE_SIZE, page,
                        sizeof(page));
    restore_interrupts(irq_state);

    printf("settings: wrote slot %u/%u, blocked for %llu us\n", (unsigned) sector,
           (unsigned) slot, (unsigned long long) (time_us_64() - start_us));

    return memcmp(slot_record(sector, slot), record, sizeof(*record)) == 0;
}

bool settings_store_load(void)
{
    const settings_record_t *best = NULL;
    uint8_t best_sector = 0;
    uint8_t best_slot = 0;

    for (uint8_t sector = 0; sector < SETTINGS_SECTOR_COUNT; sector++) {
        for (uint8_t slot = 0; slot < SETTINGS_SLOTS_PER_SECTOR; slot++) {
            const settings_record_t *record = slot_record(sector, slot);

            if (!record_is_valid(record)) {
                continue;
            }
            if (best == NULL || record->sequence > best->sequence) {
                best = record;
                best_sector = sector;
                best_slot = slot;
            }
        }
    }

    if (best == NULL) {
        return false;
    }

    current = *best;
    current_valid = true;
    stored_exists = true;
    active_sector = best_sector;
    active_slot = (int8_t) best_slot;
    next_sequence = best->sequence + 1u;

    xpt2046_set_calibration(&current.calibration);
    *pendant_mapping() = current.mapping;
    *pendant_settings() = current.settings;
    return true;
}

void settings_store_boot_maintenance(void)
{
    if (!current_valid) {
        // Nothing stored at all. Make sure both sectors are blank so the
        // first save is a plain page program like every other one.
        for (uint8_t sector = 0; sector < SETTINGS_SECTOR_COUNT; sector++) {
            if (!sector_is_blank(sector)) {
                erase_sector(sector);
            }
        }
        return;
    }

    // Everything outside the active sector is history. Erasing it here keeps
    // the spare ready, so the next sector switch during operation costs a
    // page program instead of an erase.
    uint8_t stale = (uint8_t) (active_sector ^ 1u);
    if (!sector_is_blank(stale)) {
        erase_sector(stale);
    }
}

// Fills the mirror when nothing was loaded, so the first write still
// produces a complete, consistent record.
static void prime_current(void)
{
    if (current_valid) {
        return;
    }

    current.magic = SETTINGS_MAGIC;
    current.version = SETTINGS_VERSION;
    current.calibration = xpt2046_get_calibration();
    current.mapping = *pendant_mapping();
    current.settings = *pendant_settings();
    current_valid = true;
}

static bool commit(settings_record_t *candidate)
{
    uint8_t sector = active_sector;
    int16_t slot = (int16_t) (active_slot + 1);

    if (slot >= (int16_t) SETTINGS_SLOTS_PER_SECTOR) {
        sector = (uint8_t) (sector ^ 1u);
        slot = 0;

        // Should already be blank - boot maintenance erases it. The check is
        // the fallback for a device that has not been restarted since the
        // last switch, and it is the only path on which a save can still
        // cost an erase.
        if (!sector_is_blank(sector)) {
            erase_sector(sector);
        }
    }

    candidate->magic = SETTINGS_MAGIC;
    candidate->version = SETTINGS_VERSION;
    candidate->sequence = next_sequence;
    candidate->checksum = checksum_of(candidate);

    if (!program_slot(sector, (uint8_t) slot, candidate)) {
        return false;
    }

    current = *candidate;
    active_sector = sector;
    active_slot = (int8_t) slot;
    stored_exists = true;
    next_sequence++;
    return true;
}

bool settings_store_save_calibration(const xpt2046_calibration_t *calibration)
{
    prime_current();

    if (stored_exists &&
        memcmp(&current.calibration, calibration, sizeof(*calibration)) == 0) {
        return true;  // nothing to do - do not spend a slot on it
    }

    settings_record_t candidate = current;
    candidate.calibration = *calibration;
    return commit(&candidate);
}

bool settings_store_save_user_data(void)
{
    prime_current();

    const button_mapping_t *mapping = pendant_mapping();
    const pendant_settings_t *settings = pendant_settings();

    if (stored_exists && memcmp(&current.mapping, mapping, sizeof(*mapping)) == 0 &&
        memcmp(&current.settings, settings, sizeof(*settings)) == 0) {
        return true;
    }

    settings_record_t candidate = current;
    candidate.mapping = *mapping;
    candidate.settings = *settings;
    return commit(&candidate);
}

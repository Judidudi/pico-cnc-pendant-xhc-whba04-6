#include "pendant_settings.h"

#include <string.h>

#include "ili9341_driver.h"
#include "ui_theme.h"
#include "xpt2046_touch.h"

const uint16_t pendant_auto_return_seconds[PENDANT_AUTO_RETURN_OPTIONS] = { 0, 15, 30,
                                                                            60 };

#define DEFAULT_AUTO_RETURN_INDEX 2  // 30 s

static pendant_settings_t g_settings;

pendant_settings_t *pendant_settings(void)
{
    return &g_settings;
}

void pendant_settings_set_defaults(void)
{
    memset(&g_settings, 0, sizeof(g_settings));
    g_settings.theme = UI_THEME_LIGHT;
    g_settings.auto_return_index = DEFAULT_AUTO_RETURN_INDEX;
    g_settings.handwheel_reversed = 0;
    g_settings.unit_inch = 0;
    g_settings.rotate_180 = 0;
}

bool pendant_settings_are_sane(const pendant_settings_t *settings)
{
    return settings->theme < UI_THEME_COUNT &&
           settings->auto_return_index < PENDANT_AUTO_RETURN_OPTIONS &&
           settings->handwheel_reversed <= 1 && settings->unit_inch <= 1 &&
           settings->rotate_180 <= 1;
}

void pendant_settings_apply(void)
{
    ui_theme_set((ui_theme_t) g_settings.theme);

    // Rotation is done by the panel itself through MADCTL, not by LVGL's
    // software rotation: MADCTL costs one SPI command and no CPU, while
    // LV_DISP_ROT would have LVGL turn every pixel of every flush.
    //
    // The touch controller does not rotate with it - it sits on the glass -
    // so its mapped coordinates are mirrored in software to match. Because
    // that happens AFTER the stored calibration is applied, an existing
    // calibration stays valid across a rotation change. That is the reason
    // flipping the panel no longer forces a recalibration the way the old
    // fixed MADCTL value did.
    ili9341_set_rotated(g_settings.rotate_180 != 0);
    xpt2046_set_rotated(g_settings.rotate_180 != 0);
}

uint32_t pendant_settings_auto_return_ms(void)
{
    uint8_t index = g_settings.auto_return_index;
    if (index >= PENDANT_AUTO_RETURN_OPTIONS) {
        return 0;
    }

    return (uint32_t) pendant_auto_return_seconds[index] * 1000u;
}

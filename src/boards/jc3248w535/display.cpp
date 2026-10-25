#include "display.h"

#include "panel.h"

#define LCD_BL 1

// QSPI pins and the 320x480 "type1" init sequence from Arduino_GFX's known-good
// JC3248W535 definition (the driver's default init is for a 180x640 panel).
static Arduino_DataBus *bus = new Arduino_ESP32QSPI(45 /* CS */, 47 /* SCK */, 21 /* D0 */, 48 /* D1 */,
                                                    40 /* D2 */, 39 /* D3 */);

// Kept at rotation 0 / native portrait: this panel only takes whole frames, and
// rotating the driver garbles them.
Arduino_GFX *panel = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED /* RST */, 0 /* rotation */, false /* IPS */,
                                           DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, 0, 0, 0,
                                           axs15231b_320480_type1_init_operations,
                                           sizeof(axs15231b_320480_type1_init_operations));

void display_init() {
  // 40 MHz: at 80 MHz the panel shows horizontal smearing.
  if (!panel->begin(40000000)) Serial.println("[display] panel begin failed");
  panel->fillScreen(RGB565_BLACK);
  ledcAttach(LCD_BL, 5000, 8);
  ledcWrite(LCD_BL, 255);
}

void display_set_backlight(uint8_t duty) { ledcWrite(LCD_BL, duty); }

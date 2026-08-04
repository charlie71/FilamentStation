#include "drivers/DisplayDriver.h"

#include "config/BoardConfig.h"

namespace filament_station::drivers {
namespace {

class FilamentStationDisplay final : public lgfx::LGFX_Device {
 public:
  FilamentStationDisplay() {
    {
      auto cfg = bus_.config();
      cfg.port = 0;
      cfg.freq_write = config::kDisplayWriteFrequencyHz;
      cfg.pin_wr = config::kDisplayWritePin;
      cfg.pin_rd = -1;
      cfg.pin_rs = config::kDisplayCommandDataPin;
      cfg.pin_d0 = config::kDisplayDataPins[0];
      cfg.pin_d1 = config::kDisplayDataPins[1];
      cfg.pin_d2 = config::kDisplayDataPins[2];
      cfg.pin_d3 = config::kDisplayDataPins[3];
      cfg.pin_d4 = config::kDisplayDataPins[4];
      cfg.pin_d5 = config::kDisplayDataPins[5];
      cfg.pin_d6 = config::kDisplayDataPins[6];
      cfg.pin_d7 = config::kDisplayDataPins[7];
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    {
      auto cfg = panel_.config();
      cfg.pin_cs = -1;
      cfg.pin_rst = config::kDisplayResetPin;
      cfg.pin_busy = -1;
      cfg.memory_width = config::kDisplayNativeWidth;
      cfg.memory_height = config::kDisplayNativeHeight;
      cfg.panel_width = config::kDisplayNativeWidth;
      cfg.panel_height = config::kDisplayNativeHeight;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      panel_.config(cfg);
    }
    {
      auto cfg = light_.config();
      cfg.pin_bl = config::kDisplayBacklightPin;
      cfg.invert = !config::kDisplayBacklightActiveHigh;
      cfg.freq = config::kDisplayBacklightPwmFrequencyHz;
      cfg.pwm_channel = config::kDisplayBacklightPwmChannel;
      light_.config(cfg);
      panel_.setLight(&light_);
    }
    {
      auto cfg = touch_.config();
      cfg.x_min = 0;
      cfg.x_max = config::kDisplayNativeWidth - 1;
      cfg.y_min = 0;
      cfg.y_max = config::kDisplayNativeHeight - 1;
      cfg.pin_int = config::kTouchInterruptPin;
      // GPIO4 wird bereits durch den Panel-Reset geschaltet und ist mit dem
      // Touch-Reset hardwareseitig verbunden.
      cfg.pin_rst = -1;
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      cfg.i2c_port = 1;
      cfg.i2c_addr = config::kTouchI2cAddress;
      cfg.pin_sda = config::kTouchSdaPin;
      cfg.pin_scl = config::kTouchSclPin;
      cfg.freq = config::kTouchI2cFrequencyHz;
      touch_.config(cfg);
      panel_.setTouch(&touch_);
    }
    setPanel(&panel_);
  }

 private:
  lgfx::Panel_ST7796 panel_;
  lgfx::Bus_Parallel8 bus_;
  lgfx::Light_PWM light_;
  lgfx::Touch_FT5x06 touch_;
};

FilamentStationDisplay display;

}  // namespace

bool initializeDisplay() {
  if (!display.init()) {
    return false;
  }
  display.setRotation(config::kDisplayRotation);
  display.setBrightness(config::kDisplayDefaultBrightness);
  return display.width() == config::kDisplayWidth &&
         display.height() == config::kDisplayHeight;
}

void drawDisplayColorTest() {
  // TFT_* sind RGB565-Werte. uint16_t stellt sicher, dass LovyanGFX nicht die
  // RGB888-Ueberladung waehlt und die Werte um einen Farbkanal verschiebt.
  constexpr std::uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE,
                                      TFT_WHITE, TFT_BLACK};
  const std::int32_t stripeWidth = display.width() / 5;
  for (std::int32_t index = 0; index < 5; ++index) {
    const std::int32_t x = index * stripeWidth;
    const std::int32_t width =
        index == 4 ? display.width() - x : stripeWidth;
    display.fillRect(x, 0, width, display.height(), colors[index]);
  }
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.setCursor(8, display.height() - 28);
  display.print("FilamentStation display/touch test");
}

lgfx::LGFX_Device& displayDevice() { return display; }

}  // namespace filament_station::drivers

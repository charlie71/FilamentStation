#include "ui/UiBridge.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <iterator>
#include <lvgl.h>
#include <soc/soc_memory_types.h>

#include "config/AppConfig.h"
#include "config/BoardConfig.h"
#include "drivers/DisplayDriver.h"
#include "drivers/TouchDriver.h"

namespace filament_station::ui {
namespace {

void* drawBuffer1 = nullptr;
void* drawBuffer2 = nullptr;
lv_display_t* lvglDisplay = nullptr;
lv_indev_t* touchInput = nullptr;

std::uint32_t tickMilliseconds() { return millis(); }

void flushDisplay(lv_display_t* display, const lv_area_t* area,
                  std::uint8_t* pixelMap) {
  const std::int32_t width = area->x2 - area->x1 + 1;
  const std::int32_t height = area->y2 - area->y1 + 1;
  drivers::displayDevice().pushImage(
      area->x1, area->y1, width, height,
      reinterpret_cast<const lgfx::rgb565_t*>(pixelMap));
  lv_display_flush_ready(display);
}

void readTouch(lv_indev_t*, lv_indev_data_t* data) {
  std::int32_t x = 0;
  std::int32_t y = 0;
  if (drivers::readTouchCoordinates(x, y)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void createIntegrationTestScreen() {
  struct ColorSample {
    std::uint32_t color;
    std::uint32_t textColor;
    const char* label;
  };
  constexpr ColorSample kColorSamples[] = {
      {0xFF0000, 0xFFFFFF, "R"}, {0x00FF00, 0x000000, "G"},
      {0x0000FF, 0xFFFFFF, "B"}, {0x00FFFF, 0x000000, "C"},
      {0xFFFFFF, 0x000000, "W"}, {0x000000, 0xFFFFFF, "K"},
  };

  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);

  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "FilamentStation - LVGL 9");
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

  for (std::size_t index = 0; index < std::size(kColorSamples); ++index) {
    lv_obj_t* sample = lv_obj_create(screen);
    lv_obj_set_pos(sample, static_cast<lv_coord_t>(index * 80U), 64);
    lv_obj_set_size(sample, 80, 48);
    lv_obj_set_style_radius(sample, 0, 0);
    lv_obj_set_style_border_width(sample, 0, 0);
    lv_obj_set_style_bg_color(sample, lv_color_hex(kColorSamples[index].color),
                              0);
    lv_obj_remove_flag(sample, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* sampleLabel = lv_label_create(sample);
    lv_label_set_text(sampleLabel, kColorSamples[index].label);
    lv_obj_set_style_text_color(
        sampleLabel, lv_color_hex(kColorSamples[index].textColor), 0);
    lv_obj_set_style_text_font(sampleLabel, &lv_font_montserrat_18, 0);
    lv_obj_center(sampleLabel);
  }

  lv_obj_t* button = lv_button_create(screen);
  lv_obj_set_size(button, 200, 72);
  lv_obj_align(button, LV_ALIGN_CENTER, 0, 36);
  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, "Touch test");
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
  lv_obj_center(label);

  lv_obj_t* status = lv_label_create(screen);
  lv_label_set_text(status, "RGB565 | PSRAM | 480 x 320");
  lv_obj_set_style_text_color(status, lv_color_hex(0x80CBC4), 0);
  lv_obj_set_style_text_font(status, &lv_font_montserrat_18, 0);
  lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -16);
}

void releaseDrawBuffers() {
  if (drawBuffer1 != nullptr) {
    heap_caps_free(drawBuffer1);
    drawBuffer1 = nullptr;
  }
  if (drawBuffer2 != nullptr) {
    heap_caps_free(drawBuffer2);
    drawBuffer2 = nullptr;
  }
}

}  // namespace

bool initializeLvgl(UiRuntimeInfo& runtimeInfo) {
  lv_init();
  lv_tick_set_cb(tickMilliseconds);

  constexpr std::size_t kBytesPerPixel = sizeof(std::uint16_t);
  const std::size_t bufferBytes =
      static_cast<std::size_t>(config::kDisplayWidth) *
      config::kLvglDrawBufferLines * kBytesPerPixel;
  constexpr std::uint32_t kBufferCapabilities =
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
  drawBuffer1 = heap_caps_malloc(bufferBytes, kBufferCapabilities);
  drawBuffer2 = heap_caps_malloc(bufferBytes, kBufferCapabilities);
  if (drawBuffer1 == nullptr || drawBuffer2 == nullptr) {
    releaseDrawBuffers();
    return false;
  }

  lvglDisplay =
      lv_display_create(config::kDisplayWidth, config::kDisplayHeight);
  if (lvglDisplay == nullptr) {
    releaseDrawBuffers();
    return false;
  }
  lv_display_set_color_format(lvglDisplay, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(lvglDisplay, drawBuffer1, drawBuffer2, bufferBytes,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(lvglDisplay, flushDisplay);

  touchInput = lv_indev_create();
  if (touchInput == nullptr) {
    lv_display_delete(lvglDisplay);
    lvglDisplay = nullptr;
    releaseDrawBuffers();
    return false;
  }
  lv_indev_set_type(touchInput, LV_INDEV_TYPE_POINTER);
  lv_indev_set_display(touchInput, lvglDisplay);
  lv_indev_set_read_cb(touchInput, readTouch);

  createIntegrationTestScreen();
  runtimeInfo.bytesPerDrawBuffer = bufferBytes;
  runtimeInfo.totalDrawBufferBytes = bufferBytes * 2U;
  runtimeInfo.drawBuffersInPsram = esp_ptr_external_ram(drawBuffer1) &&
                                   esp_ptr_external_ram(drawBuffer2);
  return true;
}

void runLvglTimers() { lv_timer_handler(); }

}  // namespace filament_station::ui

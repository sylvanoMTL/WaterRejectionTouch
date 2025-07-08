#include <Arduino.h>
#include "hardware_conf.h"
//#include "User_Setup.h"

// IMPORTANT: Define your screen type BEFORE including WaterRejectionTouch.h
// Comment out the line that doesn't apply to your hardware:
#define RESISTIVE_SCREEN     // Using resistive touchscreen (default)
// #define CAPACITIVE_SCREEN  // Using capacitive touchscreen

// The library will automatically configure itself based on your selection:
// - RESISTIVE: Larger touch areas, debouncing, pressure-based filtering
// - CAPACITIVE: Smaller touch areas, pattern detection, multi-touch analysis

#include <lvgl.h>
#include <TFT_eSPI.h>
#include "ui/ui.h"
#include "WaterRejectionTouch.h"

#define LV_LVGL_H_INCLUDE_SIMPLE  

static const uint16_t screenWidth = 480;   
static const uint16_t screenHeight = 320;  
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];
TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */

// Water filter instance
WaterRejectionTouch waterFilter(screenWidth, screenHeight);

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);

void setup() {
  Serial.begin(115200); /* prepare for possible serial debug */
  lv_init();
  tft.begin();        /* TFT init */
  tft.setRotation(1); /* FMS Landscape orientation, flipped was 3 but was 180 degree incorrect USB cord on right side*/
  uint16_t calData[5] = { 193, 3758, 189, 3690, 7 };
  tft.setTouch(calData);                              // Let the screen use your touch values

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);
  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  ui_init();  // Init EEZ-Studio UI

  // Initialize water filtering - automatically configured for your screen type
  waterFilter.begin();
  
  // Optional: Show which screen type is configured
  Serial.print("Water filter initialized for ");
  Serial.print(waterFilter.getScreenTypeName());
  Serial.println(" touchscreen");
  
  // Optional: Enable wet mode if using outdoors
  // waterFilter.setWetModeEnabled(true);
  
  // Optional: Enable gesture mode for high security
  // waterFilter.enableGestureMode();
  
  Serial.println("Setup done");
  
  // Print configuration info
  Serial.println("Water filter active - statistics will be shown every 10 seconds");
}

void loop() {
  // Update water filter - IMPORTANT!
  waterFilter.update();
  
  lv_timer_handler(); /* let the GUI do its work */
  ui_tick();  // Update EEZ-Studio UI
  
  // Optional: Print statistics every 10 seconds
  static unsigned long lastStatsTime = 0;
  if (millis() - lastStatsTime > 10000) {
    Serial.print("Valid touches: ");
    Serial.print(waterFilter.getValidTouches());
    Serial.print(" | Rejected: ");
    Serial.print(waterFilter.getWaterDropletsRejected());
    
    uint32_t total = waterFilter.getValidTouches() + waterFilter.getWaterDropletsRejected();
    if (total > 0) {
      float rejectRate = (float)waterFilter.getWaterDropletsRejected() / total * 100;
      Serial.print(" | Rejection rate: ");
      Serial.print(rejectRate, 1);
      Serial.println("%");
    } else {
      Serial.println(" | No touches yet");
    }
    
    lastStatsTime = millis();
  }
}

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

/*Read the touchpad with water rejection filtering*/
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  uint16_t touchX, touchY;
  bool touched = tft.getTouch(&touchX, &touchY, 600);
  
  if (!touched) {
    data->state = LV_INDEV_STATE_REL;
  } else {
    // Process through water rejection filter
    // The filter automatically handles debouncing for resistive screens
    if (waterFilter.processTouch(touchX, touchY)) {
      // Valid touch - pass to LVGL
      data->state = LV_INDEV_STATE_PR;
      data->point.x = touchX;
      data->point.y = touchY;
      
      // Optional: Visual feedback for debugging
      #ifdef DEBUG_TOUCH
      Serial.print("Valid touch at: ");
      Serial.print(touchX);
      Serial.print(", ");
      Serial.println(touchY);
      #endif
    } else {
      // Touch rejected as potential water droplet
      data->state = LV_INDEV_STATE_REL;
      
      // Optional: Log rejected touches
      #ifdef DEBUG_TOUCH
      Serial.print("REJECTED touch at: ");
      Serial.print(touchX);
      Serial.print(", ");
      Serial.println(touchY);
      #endif
    }
  }
}

// Optional: Add these functions to control water filtering at runtime

void enableWetMode() {
  waterFilter.setWetModeEnabled(true);
  Serial.println("Wet mode enabled - aggressive water filtering");
}

void disableWetMode() {
  waterFilter.setWetModeEnabled(false);
  Serial.println("Wet mode disabled - normal filtering");
}

void enableGestureMode() {
  waterFilter.enableGestureMode();
  Serial.println("Gesture mode enabled - swipe from left edge to activate touch");
}

void disableGestureMode() {
  waterFilter.disableGestureMode();
  Serial.println("Gesture mode disabled");
}

void resetWaterFilterStats() {
  waterFilter.resetStatistics();
  Serial.println("Water filter statistics reset");
}
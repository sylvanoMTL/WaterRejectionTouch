/**
 * Basic Water Rejection Example
 * Shows basic usage of WaterRejectionTouch library
 * with TFT_eSPI and automatic screen type configuration
 */

// IMPORTANT: Define your screen type BEFORE including the library
// Comment out the line that doesn't match your hardware:
#define RESISTIVE_SCREEN     // Using resistive touchscreen (default)
// #define CAPACITIVE_SCREEN  // Using capacitive touchscreen

#include <TFT_eSPI.h>
#include <Adafruit_FT6206.h>
#include "WaterRejectionTouch.h"

// Display and touch objects
TFT_eSPI tft = TFT_eSPI();

// For capacitive screens, uncomment this:
// Adafruit_FT6206 touch = Adafruit_FT6206();

// Water rejection filter - automatically configured for your screen type
WaterRejectionTouch waterFilter(320, 240);  // Screen dimensions

// Touch event handler
TouchEventHandler touchHandler(&waterFilter);

// UI elements
const int BUTTON_WIDTH = 100;
const int BUTTON_HEIGHT = 50;
const int BUTTON_X = 110;
const int BUTTON_Y = 95;

bool buttonPressed = false;

void setup() {
    Serial.begin(115200);
    Serial.println("Water Rejection Touch Example");
    
    // Initialize display
    tft.init();
    tft.setRotation(1);  // Landscape
    tft.fillScreen(TFT_BLACK);
    
    #ifdef CAPACITIVE_SCREEN
    // Initialize capacitive touch controller
    if (!touch.begin(40)) {  // 40 is the I2C address
        Serial.println("Couldn't start touchscreen controller");
        while (1);
    }
    Serial.println("Capacitive touchscreen started");
    #else
    // Initialize resistive touch (built into TFT_eSPI)
    uint16_t calData[5] = { 193, 3758, 189, 3690, 7 };  // Your calibration data
    tft.setTouch(calData);
    Serial.println("Resistive touchscreen started");
    #endif
    
    // Initialize water rejection filter
    // Automatically configured based on screen type!
    waterFilter.begin();
    
    // Show which configuration is active
    Serial.print("Water filter configured for ");
    Serial.print(waterFilter.getScreenTypeName());
    Serial.println(" touchscreen");
    
    // Optional: Adjust for your environment
    // waterFilter.setWetModeEnabled(true);  // For outdoor/wet conditions
    
    // Set up touch callbacks
    touchHandler.setTouchStartCallback(onTouchStart);
    touchHandler.setTouchMoveCallback(onTouchMove);
    touchHandler.setTouchEndCallback(onTouchEnd);
    
    // Draw initial UI
    drawButton(false);
    drawStatus();
}

void loop() {
    // Update water filter (important!)
    touchHandler.update();
    
    #ifdef CAPACITIVE_SCREEN
    // Check for capacitive touches
    if (touch.touched()) {
        TS_Point p = touch.getPoint();
        
        // Map touch coordinates to screen coordinates
        // Note: Adjust these mappings based on your display orientation
        int16_t x = map(p.x, 0, 240, 0, 320);
        int16_t y = map(p.y, 0, 320, 0, 240);
        
        // Process through water rejection filter
        touchHandler.handleTouch(x, y);
    }
    #else
    // Check for resistive touches
    uint16_t x, y;
    if (tft.getTouch(&x, &y, 600)) {
        // Process through water rejection filter
        touchHandler.handleTouch(x, y);
    }
    #endif
    
    // Update statistics display every second
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 1000) {
        drawStatus();
        lastUpdate = millis();
    }
}

// Touch callbacks
void onTouchStart(int16_t x, int16_t y) {
    Serial.print("Touch Start: ");
    Serial.print(x);
    Serial.print(", ");
    Serial.println(y);
    
    // Check if button was pressed
    if (x >= BUTTON_X && x <= BUTTON_X + BUTTON_WIDTH &&
        y >= BUTTON_Y && y <= BUTTON_Y + BUTTON_HEIGHT) {
        buttonPressed = true;
        drawButton(true);
    }
    
    // Draw touch indicator
    tft.fillCircle(x, y, 10, TFT_GREEN);
}

void onTouchMove(int16_t x, int16_t y) {
    // Draw touch trail
    tft.fillCircle(x, y, 5, TFT_BLUE);
}

void onTouchEnd(int16_t x, int16_t y) {
    Serial.println("Touch End");
    
    if (buttonPressed) {
        buttonPressed = false;
        drawButton(false);
        
        // Button action
        tft.fillRect(0, 200, 320, 40, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Button Pressed!", 10, 210, 2);
    }
}

// UI drawing functions
void drawButton(bool pressed) {
    uint16_t color = pressed ? TFT_DARKGREEN : TFT_GREEN;
    tft.fillRoundRect(BUTTON_X, BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, 10, color);
    
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("PRESS", BUTTON_X + BUTTON_WIDTH/2, BUTTON_Y + BUTTON_HEIGHT/2, 4);
    tft.setTextDatum(TL_DATUM);
}

void drawStatus() {
    // Clear status area
    tft.fillRect(0, 0, 320, 30, TFT_BLACK);
    
    // Draw statistics
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Valid: " + String(waterFilter.getValidTouches()), 10, 5, 2);
    tft.drawString("Rejected: " + String(waterFilter.getWaterDropletsRejected()), 150, 5, 2);
    
    // Calculate and display rejection rate
    uint32_t total = waterFilter.getValidTouches() + waterFilter.getWaterDropletsRejected();
    if (total > 0) {
        float rate = (float)waterFilter.getWaterDropletsRejected() / total * 100;
        tft.drawString(String(rate, 1) + "%", 270, 5, 2);
    }
}
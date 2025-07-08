/**
 * Simple Integration Example
 * Shows how to add water rejection to existing TFT_eSPI projects
 * Minimal code changes required - now with automatic screen type configuration
 */

// STEP 1: Define your screen type (before includes!)
#define RESISTIVE_SCREEN     // Default for most TFT_eSPI projects
// #define CAPACITIVE_SCREEN  // Uncomment if using capacitive

#include <TFT_eSPI.h>
#include <Adafruit_FT6206.h>  // Only needed for capacitive
#include "WaterRejectionTouch.h"

// Your existing objects
TFT_eSPI tft = TFT_eSPI();

#ifdef CAPACITIVE_SCREEN
Adafruit_FT6206 touch = Adafruit_FT6206();  // For capacitive
#endif

// Add water filter (automatically configured for your screen type!)
WaterRejectionTouch waterFilter(320, 240);

void setup() {
    Serial.begin(115200);
    
    // Your existing setup code
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    
    #ifdef CAPACITIVE_SCREEN
    // Initialize capacitive touch
    touch.begin(40);
    #else
    // Initialize resistive touch (built into TFT_eSPI)
    uint16_t calData[5] = { 193, 3758, 189, 3690, 7 };
    tft.setTouch(calData);
    #endif
    
    // Add this line to initialize water filter
    waterFilter.begin();
    
    // That's it! The filter is now configured for your screen type
    Serial.print("Water filter ready for ");
    Serial.println(waterFilter.getScreenTypeName());
    
    // Optional: Enable wet mode for outdoor use
    // waterFilter.setWetModeEnabled(true);
    
    // Your UI setup here...
    drawYourUI();
}

void loop() {
    // Add this line to update the filter
    waterFilter.update();
    
    // Replace your touch handling with this:
    #ifdef CAPACITIVE_SCREEN
    if (touch.touched()) {
        TS_Point p = touch.getPoint();
        int16_t x = map(p.x, 0, 240, 0, 320);
        int16_t y = map(p.y, 0, 320, 0, 240);
        
        if (waterFilter.processTouch(x, y)) {
            handleTouch(x, y);
        }
    }
    #else
    // Resistive touch handling
    uint16_t x, y;
    if (tft.getTouch(&x, &y, 600)) {
        if (waterFilter.processTouch(x, y)) {
            handleTouch(x, y);
        }
    }
    #endif
    
    // Rest of your loop code...
}

void handleTouch(int16_t x, int16_t y) {
    // Your existing touch handling code
    Serial.print("Touch at: ");
    Serial.print(x);
    Serial.print(", ");
    Serial.println(y);
    
    // Example: Check button press
    if (x > 100 && x < 200 && y > 50 && y < 100) {
        // Button pressed
        tft.fillRect(100, 50, 100, 50, TFT_GREEN);
        delay(100);
        tft.fillRect(100, 50, 100, 50, TFT_BLUE);
    }
}

void drawYourUI() {
    // Your existing UI drawing code
    tft.fillRect(100, 50, 100, 50, TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("BUTTON", 150, 75, 2);
}

/* 
 * Summary of changes:
 * 1. Add #define RESISTIVE_SCREEN (or CAPACITIVE_SCREEN)
 * 2. Create WaterRejectionTouch object
 * 3. Call waterFilter.begin() in setup
 * 4. Call waterFilter.update() in loop
 * 5. Wrap touch processing with waterFilter.processTouch()
 * 
 * The library automatically configures itself based on screen type!
 */
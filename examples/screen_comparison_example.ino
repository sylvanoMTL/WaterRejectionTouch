/**
 * Screen Type Comparison Example
 * Shows the differences between resistive and capacitive configurations
 * Helps you understand which settings work best for your application
 */

// ========================================
// STEP 1: Choose your screen type
// ========================================
// Uncomment ONE of these lines:
#define RESISTIVE_SCREEN     // For resistive touchscreens
// #define CAPACITIVE_SCREEN  // For capacitive touchscreens

#include <TFT_eSPI.h>
#include "WaterRejectionTouch.h"

TFT_eSPI tft = TFT_eSPI();
WaterRejectionTouch waterFilter(320, 240);

void setup() {
    Serial.begin(115200);
    delay(1000);  // Allow serial monitor to connect
    
    Serial.println("=====================================");
    Serial.println("Water Rejection Configuration Demo");
    Serial.println("=====================================");
    
    // Initialize display
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    
    // Initialize water filter with automatic configuration
    waterFilter.begin();
    
    // Display current configuration
    displayConfiguration();
    
    // Show different modes
    Serial.println("\n--- Testing Different Modes ---");
    
    // Test 1: Normal mode
    Serial.println("\n1. NORMAL MODE (Indoor use):");
    waterFilter.setWetModeEnabled(false);
    printCurrentSettings();
    
    // Test 2: Wet mode
    Serial.println("\n2. WET MODE (Outdoor/rain):");
    waterFilter.setWetModeEnabled(true);
    printCurrentSettings();
    
    // Test 3: Gesture mode
    Serial.println("\n3. GESTURE MODE (High security):");
    waterFilter.enableGestureMode();
    Serial.println("   - Swipe from left edge required to activate");
    Serial.println("   - Prevents accidental activation");
    
    // Reset to normal mode
    waterFilter.setWetModeEnabled(false);
    waterFilter.disableGestureMode();
    
    // Draw UI
    drawInterface();
}

void displayConfiguration() {
    Serial.print("\nConfigured for: ");
    Serial.print(waterFilter.getScreenTypeName());
    Serial.println(" Touchscreen");
    
    #ifdef RESISTIVE_SCREEN
    Serial.println("\nRESISTIVE SCREEN FEATURES:");
    Serial.println("- Requires physical pressure");
    Serial.println("- Less sensitive to water drops");
    Serial.println("- Single touch only");
    Serial.println("- Built-in 50ms debouncing");
    Serial.println("- Pressure threshold: 300");
    
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("RESISTIVE SCREEN MODE", 10, 10, 2);
    #else
    Serial.println("\nCAPACITIVE SCREEN FEATURES:");
    Serial.println("- No pressure required");
    Serial.println("- Very sensitive to water");
    Serial.println("- Multi-touch capable");
    Serial.println("- No debouncing needed");
    Serial.println("- Detects touch patterns");
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("CAPACITIVE SCREEN MODE", 10, 10, 2);
    #endif
}

void printCurrentSettings() {
    WaterRejectionConfig config = waterFilter.getConfig();
    Serial.print("   - Max touch area: ");
    Serial.println(config.maxTouchArea);
    Serial.print("   - Min movement: ");
    Serial.println(config.minMovement);
    Serial.print("   - Max static time: ");
    Serial.print(config.maxStaticTime);
    Serial.println("ms");
    Serial.print("   - Gesture required: ");
    Serial.println(config.requireGesture ? "Yes" : "No");
    
    #ifdef RESISTIVE_SCREEN
    Serial.print("   - Debounce time: ");
    Serial.print(config.debounceTime);
    Serial.println("ms");
    Serial.print("   - Pressure threshold: ");
    Serial.println(config.pressureThreshold);
    #endif
}

void drawInterface() {
    // Title based on screen type
    tft.fillRect(0, 30, 320, 30, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    
    #ifdef RESISTIVE_SCREEN
    tft.drawString("Resistive Touch Demo", 160, 35, 4);
    #else
    tft.drawString("Capacitive Touch Demo", 160, 35, 4);
    #endif
    
    tft.setTextDatum(TL_DATUM);
    
    // Draw test areas
    tft.drawRect(20, 80, 130, 60, TFT_WHITE);
    tft.drawString("Touch Here", 45, 105, 2);
    
    tft.drawRect(170, 80, 130, 60, TFT_WHITE);
    tft.drawString("Wet Test", 200, 105, 2);
    
    // Instructions
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("Green = Valid Touch", 20, 160, 2);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Red = Rejected (Water)", 20, 180, 2);
    
    // Statistics area
    tft.drawRect(0, 200, 320, 40, TFT_BLUE);
}

void loop() {
    static unsigned long lastStatsUpdate = 0;
    
    // Update water filter
    waterFilter.update();
    
    // Check for touch
    #ifdef RESISTIVE_SCREEN
    uint16_t x, y;
    if (tft.getTouch(&x, &y, 600)) {
        handleTouch(x, y);
    }
    #else
    // For capacitive, you would read from your touch controller
    // This is just example code structure
    if (touchDetected()) {
        uint16_t x, y;
        getTouchCoordinates(&x, &y);
        handleTouch(x, y);
    }
    #endif
    
    // Update statistics every second
    if (millis() - lastStatsUpdate > 1000) {
        updateStats();
        lastStatsUpdate = millis();
    }
}

void handleTouch(uint16_t x, uint16_t y) {
    // Check if in wet test area
    bool inWetArea = (x >= 170 && x <= 300 && y >= 80 && y <= 140);
    
    if (inWetArea) {
        // Temporarily enable wet mode for this area
        waterFilter.setWetModeEnabled(true);
    }
    
    // Process touch through filter
    if (waterFilter.processTouch(x, y)) {
        // Valid touch
        tft.fillCircle(x, y, 8, TFT_GREEN);
        Serial.print("VALID: ");
    } else {
        // Rejected touch
        tft.drawCircle(x, y, 10, TFT_RED);
        tft.drawCircle(x, y, 11, TFT_RED);
        Serial.print("REJECTED: ");
    }
    
    Serial.print(x);
    Serial.print(", ");
    Serial.println(y);
    
    // Reset wet mode if we enabled it
    if (inWetArea) {
        waterFilter.setWetModeEnabled(false);
    }
}

void updateStats() {
    // Clear statistics area
    tft.fillRect(1, 201, 318, 38, TFT_BLACK);
    
    // Calculate statistics
    uint32_t valid = waterFilter.getValidTouches();
    uint32_t rejected = waterFilter.getWaterDropletsRejected();
    uint32_t total = valid + rejected;
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    char stats[50];
    sprintf(stats, "Valid:%lu Reject:%lu", valid, rejected);
    tft.drawString(stats, 10, 210, 2);
    
    if (total > 0) {
        float rejectRate = (float)rejected / total * 100;
        sprintf(stats, "Rate: %.1f%%", rejectRate);
        tft.drawString(stats, 220, 210, 2);
    }
}

// Placeholder functions for capacitive touch
// Replace with your actual touch controller code
bool touchDetected() {
    // Your capacitive touch detection code
    return false;
}

void getTouchCoordinates(uint16_t* x, uint16_t* y) {
    // Your capacitive touch reading code
    *x = 0;
    *y = 0;
}
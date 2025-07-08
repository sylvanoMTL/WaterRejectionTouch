/**
 * Advanced Water Rejection Example
 * Demonstrates gesture activation, wet mode, zone visualization,
 * and multi-touch handling
 */

#include <TFT_eSPI.h>
#include <Adafruit_FT6206.h>
#include "WaterRejectionTouch.h"

// Display and touch objects
TFT_eSPI tft = TFT_eSPI();
Adafruit_FT6206 touch = Adafruit_FT6206();
TFT_eSprite sprite = TFT_eSprite(&tft);  // For efficient updates

// Water rejection filter
WaterRejectionTouch waterFilter(320, 240);

// UI state
enum AppMode {
    MODE_NORMAL,
    MODE_WET,
    MODE_GESTURE,
    MODE_DEBUG
};

AppMode currentMode = MODE_NORMAL;
bool showZoneMap = false;
unsigned long lastDebugPrint = 0;

// Touch visualization
struct TouchVis {
    int16_t x, y;
    uint32_t timestamp;
    bool valid;
};

TouchVis touchPoints[10];
uint8_t touchIndex = 0;

void setup() {
    Serial.begin(115200);
    Serial.println("Advanced Water Rejection Example");
    
    // Initialize display
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    
    // Create sprite for status bar
    sprite.createSprite(320, 30);
    
    // Initialize touch
    if (!touch.begin(40)) {
        Serial.println("Touch controller not found");
        while (1);
    }
    
    // Configure water filter
    WaterRejectionConfig config;
    config.maxTouchArea = 40;
    config.minMovement = 3;
    config.maxStaticTime = 400;
    config.requireGesture = false;
    waterFilter.begin(config);
    
    // Initialize touch points
    memset(touchPoints, 0, sizeof(touchPoints));
    
    // Draw initial UI
    drawModeButtons();
    updateStatusBar();
    drawInstructions();
}

void loop() {
    // Update water filter
    waterFilter.update();
    
    // Handle multi-touch
    uint8_t touches = touch.touched();
    if (touches > 0) {
        TouchPoint points[5];
        
        // Read all touch points
        for (uint8_t i = 0; i < touches && i < 5; i++) {
            TS_Point p = touch.getPoint(i);
            
            // Map coordinates
            points[i].x = map(p.x, 0, 240, 0, 320);
            points[i].y = map(p.y, 0, 320, 0, 240);
            points[i].timestamp = millis();
            points[i].pressure = p.z;
            points[i].area = p.z / 5;  // Estimate
            points[i].valid = true;
        }
        
        // Process through water filter
        if (touches == 1) {
            if (waterFilter.processTouch(points[0])) {
                handleValidTouch(points[0].x, points[0].y);
            } else {
                handleRejectedTouch(points[0].x, points[0].y);
            }
        } else {
            if (waterFilter.processMultiTouch(points, touches)) {
                for (uint8_t i = 0; i < touches; i++) {
                    handleValidTouch(points[i].x, points[i].y);
                }
            } else {
                for (uint8_t i = 0; i < touches; i++) {
                    handleRejectedTouch(points[i].x, points[i].y);
                }
            }
        }
    }
    
    // Update displays
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 100) {  // 10Hz update
        updateStatusBar();
        updateTouchVisualization();
        
        if (showZoneMap && millis() - lastDebugPrint > 1000) {
            waterFilter.printZoneMap();
            waterFilter.printDebugInfo();
            lastDebugPrint = millis();
        }
        
        lastUpdate = millis();
    }
}

void handleValidTouch(int16_t x, int16_t y) {
    // Check mode button presses
    if (y > 200) {
        if (x < 80) {
            setMode(MODE_NORMAL);
        } else if (x < 160) {
            setMode(MODE_WET);
        } else if (x < 240) {
            setMode(MODE_GESTURE);
        } else {
            setMode(MODE_DEBUG);
        }
        return;
    }
    
    // Add to visualization
    touchPoints[touchIndex].x = x;
    touchPoints[touchIndex].y = y;
    touchPoints[touchIndex].timestamp = millis();
    touchPoints[touchIndex].valid = true;
    touchIndex = (touchIndex + 1) % 10;
    
    // Draw immediate feedback
    tft.fillCircle(x, y, 8, TFT_GREEN);
}

void handleRejectedTouch(int16_t x, int16_t y) {
    // Show rejected touch in red
    tft.drawCircle(x, y, 12, TFT_RED);
    tft.drawCircle(x, y, 13, TFT_RED);
}

void setMode(AppMode mode) {
    currentMode = mode;
    
    // Clear display area
    tft.fillRect(0, 40, 320, 160, TFT_BLACK);
    
    switch (mode) {
        case MODE_NORMAL:
            waterFilter.setWetModeEnabled(false);
            waterFilter.setRequireGesture(false);
            showZoneMap = false;
            break;
            
        case MODE_WET:
            waterFilter.setWetModeEnabled(true);
            waterFilter.setRequireGesture(false);
            showZoneMap = false;
            break;
            
        case MODE_GESTURE:
            waterFilter.setRequireGesture(true);
            showZoneMap = false;
            drawGestureInstructions();
            break;
            
        case MODE_DEBUG:
            showZoneMap = true;
            break;
    }
    
    drawModeButtons();
    drawInstructions();
}

void updateStatusBar() {
    sprite.fillSprite(TFT_BLACK);
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    
    // Mode indicator
    String modeStr = "Mode: ";
    switch (currentMode) {
        case MODE_NORMAL: modeStr += "Normal"; break;
        case MODE_WET: modeStr += "Wet"; break;
        case MODE_GESTURE: modeStr += "Gesture"; break;
        case MODE_DEBUG: modeStr += "Debug"; break;
    }
    sprite.drawString(modeStr, 5, 5, 2);
    
    // Statistics
    uint32_t valid = waterFilter.getValidTouches();
    uint32_t rejected = waterFilter.getWaterDropletsRejected();
    uint32_t total = valid + rejected;
    
    sprite.drawString("V:" + String(valid), 120, 5, 2);
    sprite.drawString("R:" + String(rejected), 180, 5, 2);
    
    if (total > 0) {
        float rate = (float)rejected / total * 100;
        sprite.drawString(String(rate, 1) + "%", 240, 5, 2);
    }
    
    // Gesture state indicator
    if (currentMode == MODE_GESTURE) {
        if (waterFilter.isGestureActive()) {
            sprite.fillCircle(305, 15, 8, TFT_GREEN);
        } else {
            sprite.drawCircle(305, 15, 8, TFT_RED);
        }
    }
    
    sprite.pushSprite(0, 0);
}

void updateTouchVisualization() {
    // Fade old touches
    uint32_t currentTime = millis();
    
    for (uint8_t i = 0; i < 10; i++) {
        if (touchPoints[i].valid) {
            uint32_t age = currentTime - touchPoints[i].timestamp;
            
            if (age > 2000) {
                // Clear old touch
                tft.fillCircle(touchPoints[i].x, touchPoints[i].y, 9, TFT_BLACK);
                touchPoints[i].valid = false;
            } else if (age > 500) {
                // Fade touch
                uint8_t brightness = map(age, 500, 2000, 255, 0);
                uint16_t color = tft.color565(0, brightness, 0);
                tft.drawCircle(touchPoints[i].x, touchPoints[i].y, 8, color);
            }
        }
    }
}

void drawModeButtons() {
    const char* labels[] = {"NORMAL", "WET", "GESTURE", "DEBUG"};
    const uint16_t colors[] = {TFT_BLUE, TFT_CYAN, TFT_MAGENTA, TFT_YELLOW};
    
    for (int i = 0; i < 4; i++) {
        int x = i * 80;
        uint16_t color = (currentMode == i) ? colors[i] : TFT_DARKGREY;
        
        tft.fillRect(x, 200, 79, 40, color);
        tft.drawRect(x, 200, 79, 40, TFT_WHITE);
        
        tft.setTextColor(TFT_WHITE);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(labels[i], x + 40, 220, 2);
    }
    tft.setTextDatum(TL_DATUM);
}

void drawInstructions() {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    
    switch (currentMode) {
        case MODE_NORMAL:
            tft.drawString("Normal touch mode - All touches accepted", 10, 50, 2);
            tft.drawString("Green circles = valid touches", 10, 70, 2);
            break;
            
        case MODE_WET:
            tft.drawString("Wet mode - Aggressive water filtering", 10, 50, 2);
            tft.drawString("Red circles = rejected (water detected)", 10, 70, 2);
            break;
            
        case MODE_GESTURE:
            // Handled by drawGestureInstructions()
            break;
            
        case MODE_DEBUG:
            tft.drawString("Debug mode - Check Serial Monitor", 10, 50, 2);
            tft.drawString("Zone activity map printed to serial", 10, 70, 2);
            break;
    }
}

void drawGestureInstructions() {
    tft.fillRect(0, 40, 320, 160, TFT_BLACK);
    
    // Draw swipe instruction
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Swipe from left edge to activate", 10, 50, 2);
    
    // Draw arrow
    tft.drawLine(10, 100, 60, 100, TFT_GREEN);
    tft.drawLine(60, 100, 50, 90, TFT_GREEN);
    tft.drawLine(60, 100, 50, 110, TFT_GREEN);
    
    tft.drawString("-->", 70, 90, 4);
    
    if (waterFilter.isGestureActive()) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("TOUCH ENABLED", 100, 130, 4);
    } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("TOUCH LOCKED", 100, 130, 4);
    }
}
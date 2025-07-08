/**
 * WaterRejectionTouch.cpp
 * Implementation of water rejection algorithms for capacitive touchscreens
 * 
 * Author: Assistant
 * License: MIT
 */

#include "WaterRejectionTouch.h"

// Constructor
WaterRejectionTouch::WaterRejectionTouch(uint16_t width, uint16_t height) 
    : screenWidth(width), screenHeight(height), historyIndex(0),
      gestureState(GESTURE_IDLE), gestureStateTimeout(0),
      currentTouchCount(0), waterDropletsRejected(0), validTouches(0),
      lastValidTouchTime(0) {
    
    // Set screen type based on compile-time definition
    #ifdef RESISTIVE_SCREEN
    screenType = SCREEN_RESISTIVE;
    #else
    screenType = SCREEN_CAPACITIVE;
    #endif
    
    // Initialize arrays
    memset(touchHistory, 0, sizeof(touchHistory));
    memset(zones, 0, sizeof(zones));
    memset(multiTouchPoints, 0, sizeof(multiTouchPoints));
    memset(&lastValidTouch, 0, sizeof(lastValidTouch));
}

// Initialize with default configuration
void WaterRejectionTouch::begin() {
    // Configuration is already set to defaults in struct definition
    begin(config);
}

// Initialize with custom configuration
void WaterRejectionTouch::begin(const WaterRejectionConfig& customConfig) {
    config = customConfig;
    
    // Clear all tracking data
    memset(touchHistory, 0, sizeof(touchHistory));
    memset(zones, 0, sizeof(zones));
    historyIndex = 0;
    gestureState = GESTURE_IDLE;
    waterDropletsRejected = 0;
    validTouches = 0;
}

// Main touch processing function
bool WaterRejectionTouch::processTouch(int16_t x, int16_t y) {
    TouchPoint touch;
    touch.x = x;
    touch.y = y;
    touch.timestamp = millis();
    touch.pressure = 128;  // Default pressure
    touch.area = 10;       // Default area
    touch.valid = true;
    
    return processTouch(touch);
}

bool WaterRejectionTouch::processTouch(int16_t x, int16_t y, uint8_t pressure) {
    TouchPoint touch;
    touch.x = x;
    touch.y = y;
    touch.timestamp = millis();
    touch.pressure = pressure;
    touch.area = pressure / 5;  // Estimate area from pressure
    touch.valid = true;
    
    return processTouch(touch);
}

bool WaterRejectionTouch::processTouch(const TouchPoint& touch) {
    // Bounds checking
    if (touch.x < 0 || touch.x >= screenWidth || 
        touch.y < 0 || touch.y >= screenHeight) {
        return false;
    }
    
    // Resistive screen specific processing
    if (screenType == SCREEN_RESISTIVE) {
        // Check pressure threshold (if available)
        if (config.pressureThreshold > 0 && touch.pressure < config.pressureThreshold) {
            return false;  // Too light, probably false touch
        }
        
        // Apply debouncing
        if (config.debounceTime > 0) {
            uint32_t timeSinceLastTouch = touch.timestamp - lastValidTouchTime;
            if (timeSinceLastTouch < config.debounceTime) {
                // Within debounce period - check if it's the same touch
                int16_t dx = abs(touch.x - lastValidTouch.x);
                int16_t dy = abs(touch.y - lastValidTouch.y);
                
                if (dx < config.minMovement && dy < config.minMovement) {
                    return true;  // Same touch position, allow it
                }
                return false;  // Different position within debounce time, reject
            }
        }
    }
    
    // Check if gesture is required and validate
    if (config.requireGesture && !validateGesture(touch)) {
        return false;
    }
    
    // Check for water droplet patterns
    if (isWaterPattern(touch)) {
        waterDropletsRejected++;
        return false;
    }
    
    // Check for static touch (water doesn't move)
    if (isStaticTouch(touch)) {
        waterDropletsRejected++;
        return false;
    }
    
    // Update tracking data
    updateHistory(touch);
    updateZones(touch);
    
    // Update last valid touch info
    lastValidTouchTime = touch.timestamp;
    lastValidTouch = touch;
    
    validTouches++;
    return true;
}

// Process multiple simultaneous touches
bool WaterRejectionTouch::processMultiTouch(TouchPoint* touches, uint8_t count) {
    currentTouchCount = count;
    
    // Too many simultaneous touches indicate water
    if (count > config.maxSimultaneousTouches) {
        waterDropletsRejected++;
        return false;
    }
    
    // Copy touches for analysis
    for (uint8_t i = 0; i < count && i < 5; i++) {
        multiTouchPoints[i] = touches[i];
    }
    
    // Check multi-touch patterns
    if (checkMultiTouchPattern()) {
        waterDropletsRejected++;
        return false;
    }
    
    // Process each touch individually
    bool anyValid = false;
    for (uint8_t i = 0; i < count; i++) {
        if (processTouch(touches[i])) {
            anyValid = true;
        }
    }
    
    return anyValid;
}

// Check if touch exhibits water droplet characteristics
bool WaterRejectionTouch::isWaterPattern(const TouchPoint& touch) {
    // Large touch area indicates water
    if (touch.area > config.maxTouchArea) {
        return true;
    }
    
    // Check zone activity
    uint8_t zoneX = (touch.x * ZONE_GRID_SIZE) / screenWidth;
    uint8_t zoneY = (touch.y * ZONE_GRID_SIZE) / screenHeight;
    
    if (zoneX >= ZONE_GRID_SIZE || zoneY >= ZONE_GRID_SIZE) {
        return false;
    }
    
    return checkZoneActivity(zoneX, zoneY);
}

// Check zone activity for water detection
bool WaterRejectionTouch::checkZoneActivity(uint8_t zoneX, uint8_t zoneY) {
    uint32_t currentTime = millis();
    
    // Check if this zone has suspicious activity
    if (zones[zoneX][zoneY].active) {
        uint32_t timeSinceActivation = currentTime - zones[zoneX][zoneY].activationTime;
        
        // Rapid repeated touches in same zone
        if (timeSinceActivation < 100) {
            zones[zoneX][zoneY].touchCount++;
            if (zones[zoneX][zoneY].touchCount > 3) {
                return true;  // Water detected
            }
        }
    }
    
    // Check neighboring zones (water spreads)
    uint8_t activeNeighbors = 0;
    for (int8_t dx = -1; dx <= 1; dx++) {
        for (int8_t dy = -1; dy <= 1; dy++) {
            uint8_t nx = zoneX + dx;
            uint8_t ny = zoneY + dy;
            
            if (nx < ZONE_GRID_SIZE && ny < ZONE_GRID_SIZE && 
                zones[nx][ny].active) {
                uint32_t neighborTime = currentTime - zones[nx][ny].activationTime;
                if (neighborTime < config.touchTimeout) {
                    activeNeighbors++;
                }
            }
        }
    }
    
    // Too many active neighboring zones indicates water spread
    return activeNeighbors > 4;
}

// Check multi-touch patterns for water detection
bool WaterRejectionTouch::checkMultiTouchPattern() {
    if (currentTouchCount < 2) {
        return false;
    }
    
    float clusterDensity = calculateTouchClusterDensity();
    
    // Water creates tight clusters of touches
    if (clusterDensity < 50.0f && currentTouchCount > 2) {
        return true;
    }
    
    // Check for line pattern (water running down screen)
    if (currentTouchCount >= 3) {
        // Calculate if touches form a line
        float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
        
        for (uint8_t i = 0; i < currentTouchCount && i < 5; i++) {
            sumX += multiTouchPoints[i].x;
            sumY += multiTouchPoints[i].y;
            sumXY += multiTouchPoints[i].x * multiTouchPoints[i].y;
            sumX2 += multiTouchPoints[i].x * multiTouchPoints[i].x;
        }
        
        float n = currentTouchCount;
        float correlation = (n * sumXY - sumX * sumY) / 
                           sqrt((n * sumX2 - sumX * sumX) * (n * sumY * sumY - sumY * sumY));
        
        // High correlation indicates line pattern (water streak)
        if (abs(correlation) > 0.9f) {
            return true;
        }
    }
    
    return false;
}

// Calculate density of touch cluster
float WaterRejectionTouch::calculateTouchClusterDensity() {
    if (currentTouchCount < 2) {
        return 1000.0f;  // Large value for single touch
    }
    
    float totalDistance = 0;
    uint8_t pairCount = 0;
    
    for (uint8_t i = 0; i < currentTouchCount - 1 && i < 4; i++) {
        for (uint8_t j = i + 1; j < currentTouchCount && j < 5; j++) {
            float dx = multiTouchPoints[i].x - multiTouchPoints[j].x;
            float dy = multiTouchPoints[i].y - multiTouchPoints[j].y;
            totalDistance += sqrt(dx * dx + dy * dy);
            pairCount++;
        }
    }
    
    return pairCount > 0 ? totalDistance / pairCount : 1000.0f;
}

// Check if touch is static (not moving)
bool WaterRejectionTouch::isStaticTouch(const TouchPoint& touch) {
    uint8_t staticCount = 0;
    uint32_t currentTime = touch.timestamp;
    
    for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
        if (touchHistory[i].valid && 
            currentTime - touchHistory[i].timestamp < config.maxStaticTime) {
            
            int16_t dx = abs(touchHistory[i].x - touch.x);
            int16_t dy = abs(touchHistory[i].y - touch.y);
            
            if (dx < config.minMovement && dy < config.minMovement) {
                staticCount++;
            }
        }
    }
    
    // Too many static touches in same location
    return staticCount > 5;
}

// Update zone tracking
void WaterRejectionTouch::updateZones(const TouchPoint& touch) {
    uint8_t zoneX = (touch.x * ZONE_GRID_SIZE) / screenWidth;
    uint8_t zoneY = (touch.y * ZONE_GRID_SIZE) / screenHeight;
    
    if (zoneX < ZONE_GRID_SIZE && zoneY < ZONE_GRID_SIZE) {
        zones[zoneX][zoneY].active = true;
        zones[zoneX][zoneY].activationTime = touch.timestamp;
        zones[zoneX][zoneY].lastTouchTime = touch.timestamp;
        
        if (touch.timestamp - zones[zoneX][zoneY].activationTime < 100) {
            zones[zoneX][zoneY].touchCount++;
        } else {
            zones[zoneX][zoneY].touchCount = 1;
        }
    }
}

// Update touch history
void WaterRejectionTouch::updateHistory(const TouchPoint& touch) {
    touchHistory[historyIndex] = touch;
    historyIndex = (historyIndex + 1) % HISTORY_SIZE;
}

// Clear old zone activations
void WaterRejectionTouch::clearOldZones() {
    uint32_t currentTime = millis();
    
    for (uint8_t x = 0; x < ZONE_GRID_SIZE; x++) {
        for (uint8_t y = 0; y < ZONE_GRID_SIZE; y++) {
            if (zones[x][y].active && 
                currentTime - zones[x][y].lastTouchTime > config.touchTimeout) {
                zones[x][y].active = false;
                zones[x][y].touchCount = 0;
            }
        }
    }
}

// Validate gesture for activation
bool WaterRejectionTouch::validateGesture(const TouchPoint& touch) {
    uint32_t currentTime = touch.timestamp;
    
    switch (gestureState) {
        case GESTURE_IDLE:
            // Check for gesture start (touch near edge)
            if (touch.x < config.edgeSwipeThreshold) {
                gestureState = GESTURE_WAITING;
                gestureStateTimeout = currentTime + config.gestureTimeout;
                gestureStartPoint = touch;
            }
            return false;
            
        case GESTURE_WAITING:
            // Check for timeout
            if (currentTime > gestureStateTimeout) {
                gestureState = GESTURE_IDLE;
                return false;
            }
            
            // Check for swipe completion
            if (touch.x - gestureStartPoint.x > config.swipeMinDistance) {
                gestureState = GESTURE_ACTIVE;
                gestureStateTimeout = currentTime + 30000;  // Active for 30 seconds
                return true;
            }
            return false;
            
        case GESTURE_ACTIVE:
            // Check for timeout
            if (currentTime > gestureStateTimeout) {
                gestureState = GESTURE_IDLE;
                return false;
            }
            return true;
    }
    
    return false;
}

// Update function (call in loop)
void WaterRejectionTouch::update() {
    clearOldZones();
    
    // Update gesture timeout
    if (gestureState == GESTURE_WAITING || gestureState == GESTURE_ACTIVE) {
        if (millis() > gestureStateTimeout) {
            gestureState = GESTURE_IDLE;
        }
    }
}

// Configuration methods
void WaterRejectionTouch::setConfig(const WaterRejectionConfig& newConfig) {
    config = newConfig;
}

WaterRejectionConfig WaterRejectionTouch::getConfig() const {
    return config;
}

void WaterRejectionTouch::setMaxTouchArea(uint16_t area) {
    config.maxTouchArea = area;
}

void WaterRejectionTouch::setRequireGesture(bool require) {
    config.requireGesture = require;
    if (!require) {
        gestureState = GESTURE_IDLE;
    }
}

void WaterRejectionTouch::setScreenDimensions(uint16_t width, uint16_t height) {
    screenWidth = width;
    screenHeight = height;
}

// Gesture control
void WaterRejectionTouch::enableGestureMode() {
    config.requireGesture = true;
    gestureState = GESTURE_IDLE;
}

void WaterRejectionTouch::disableGestureMode() {
    config.requireGesture = false;
    gestureState = GESTURE_IDLE;
}

bool WaterRejectionTouch::isGestureActive() const {
    return gestureState == GESTURE_ACTIVE;
}

void WaterRejectionTouch::resetGesture() {
    gestureState = GESTURE_IDLE;
}

// Statistics
uint32_t WaterRejectionTouch::getWaterDropletsRejected() const {
    return waterDropletsRejected;
}

uint32_t WaterRejectionTouch::getValidTouches() const {
    return validTouches;
}

void WaterRejectionTouch::resetStatistics() {
    waterDropletsRejected = 0;
    validTouches = 0;
}

// Calibration
void WaterRejectionTouch::calibrateForEnvironment(bool wetEnvironment) {
    if (screenType == SCREEN_RESISTIVE) {
        if (wetEnvironment) {
            // Resistive in wet conditions
            config.maxTouchArea = 60;
            config.maxStaticTime = 400;
            config.maxSimultaneousTouches = 1;
            config.requireGesture = true;  // Require gesture in wet mode
            config.pressureThreshold = 400;  // Higher pressure needed
        } else {
            // Resistive in normal conditions
            config.maxTouchArea = 80;
            config.maxStaticTime = 800;
            config.maxSimultaneousTouches = 1;
            config.requireGesture = false;
            config.pressureThreshold = 300;
        }
    } else {  // CAPACITIVE
        if (wetEnvironment) {
            // Capacitive in wet conditions - very strict
            config.maxTouchArea = 30;
            config.maxStaticTime = 300;
            config.maxSimultaneousTouches = 1;
            config.requireGesture = true;
        } else {
            // Capacitive in normal conditions
            config.maxTouchArea = 50;
            config.maxStaticTime = 500;
            config.maxSimultaneousTouches = 2;
            config.requireGesture = false;
        }
    }
}

void WaterRejectionTouch::setWetModeEnabled(bool enabled) {
    calibrateForEnvironment(enabled);
}

// Touch event detection
TouchEvent WaterRejectionTouch::getTouchEvent(const TouchPoint& current) {
    static TouchPoint lastValidTouch;
    static bool wasPressed = false;
    
    if (!current.valid) {
        if (wasPressed) {
            wasPressed = false;
            return TOUCH_END;
        }
        return TOUCH_NONE;
    }
    
    if (!wasPressed) {
        wasPressed = true;
        lastValidTouch = current;
        return TOUCH_START;
    }
    
    // Check for movement
    int16_t dx = abs(current.x - lastValidTouch.x);
    int16_t dy = abs(current.y - lastValidTouch.y);
    
    if (dx > config.minMovement || dy > config.minMovement) {
        lastValidTouch = current;
        return TOUCH_MOVE;
    }
    
    return TOUCH_NONE;
}

// Debug methods
void WaterRejectionTouch::printDebugInfo() {
    Serial.println(F("=== Water Rejection Debug Info ==="));
    Serial.print(F("Screen Type: "));
    Serial.println(getScreenTypeName());
    Serial.print(F("Valid touches: "));
    Serial.println(validTouches);
    Serial.print(F("Water droplets rejected: "));
    Serial.println(waterDropletsRejected);
    Serial.print(F("Rejection rate: "));
    if (validTouches + waterDropletsRejected > 0) {
        float rate = (float)waterDropletsRejected / (validTouches + waterDropletsRejected) * 100;
        Serial.print(rate);
        Serial.println(F("%"));
    } else {
        Serial.println(F("0%"));
    }
    Serial.print(F("Gesture state: "));
    switch (gestureState) {
        case GESTURE_IDLE: Serial.println(F("IDLE")); break;
        case GESTURE_WAITING: Serial.println(F("WAITING")); break;
        case GESTURE_ACTIVE: Serial.println(F("ACTIVE")); break;
    }
}

void WaterRejectionTouch::printZoneMap() {
    Serial.println(F("=== Zone Activity Map ==="));
    for (uint8_t y = 0; y < ZONE_GRID_SIZE; y++) {
        for (uint8_t x = 0; x < ZONE_GRID_SIZE; x++) {
            if (zones[x][y].active) {
                Serial.print(zones[x][y].touchCount);
            } else {
                Serial.print(F("."));
            }
            Serial.print(F(" "));
        }
        Serial.println();
    }
}

// TouchEventHandler implementation
TouchEventHandler::TouchEventHandler(WaterRejectionTouch* filter) 
    : waterFilter(filter), lastEvent(TOUCH_NONE),
      onTouchStart(nullptr), onTouchMove(nullptr), onTouchEnd(nullptr) {
    lastTouch.valid = false;
}

void TouchEventHandler::setTouchStartCallback(void (*callback)(int16_t, int16_t)) {
    onTouchStart = callback;
}

void TouchEventHandler::setTouchMoveCallback(void (*callback)(int16_t, int16_t)) {
    onTouchMove = callback;
}

void TouchEventHandler::setTouchEndCallback(void (*callback)(int16_t, int16_t)) {
    onTouchEnd = callback;
}

void TouchEventHandler::handleTouch(int16_t x, int16_t y) {
    TouchPoint touch;
    touch.x = x;
    touch.y = y;
    touch.timestamp = millis();
    touch.valid = true;
    
    if (waterFilter->processTouch(touch)) {
        TouchEvent event = waterFilter->getTouchEvent(touch);
        
        switch (event) {
            case TOUCH_START:
                if (onTouchStart) onTouchStart(x, y);
                break;
            case TOUCH_MOVE:
                if (onTouchMove) onTouchMove(x, y);
                break;
            case TOUCH_END:
                if (onTouchEnd) onTouchEnd(x, y);
                break;
            default:
                break;
        }
        
        lastEvent = event;
        lastTouch = touch;
    }
}

void TouchEventHandler::update() {
    waterFilter->update();
    
    // Check for touch release
    if (lastTouch.valid && millis() - lastTouch.timestamp > 100) {
        if (onTouchEnd) onTouchEnd(lastTouch.x, lastTouch.y);
        lastTouch.valid = false;
        lastEvent = TOUCH_END;
    }
}

// Screen type helper methods
const char* WaterRejectionTouch::getScreenTypeName() const {
    return screenType == SCREEN_RESISTIVE ? "Resistive" : "Capacitive";
}

bool WaterRejectionTouch::isResistiveScreen() const {
    return screenType == SCREEN_RESISTIVE;
}

bool WaterRejectionTouch::isCapacitiveScreen() const {
    return screenType == SCREEN_CAPACITIVE;
}

void WaterRejectionTouch::optimizeForScreenType() {
    // Auto-optimize settings based on screen type
    if (screenType == SCREEN_RESISTIVE) {
        Serial.println(F("Optimizing water rejection for resistive screen"));
        // Resistive screens need different handling
        config.maxTouchArea = 80;
        config.minMovement = 10;
        config.maxStaticTime = 800;
        config.debounceTime = 50;
        config.pressureThreshold = 300;
        config.maxSimultaneousTouches = 1;  // Always single touch
    } else {
        Serial.println(F("Optimizing water rejection for capacitive screen"));
        // Capacitive screens are more sensitive to water
        config.maxTouchArea = 50;
        config.minMovement = 5;
        config.maxStaticTime = 500;
        config.debounceTime = 0;  // No debouncing needed
        config.pressureThreshold = 0;  // Not used
        config.maxSimultaneousTouches = 2;
    }
}
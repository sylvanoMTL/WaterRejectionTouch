/**
 * WaterRejectionTouch.h
 * Water rejection library for capacitive and resistive touchscreens
 * Compatible with TFT_eSPI and common touch controllers
 * 
 * Author: Assistant
 * License: MIT
 */

#ifndef WATER_REJECTION_TOUCH_H
#define WATER_REJECTION_TOUCH_H

#include <Arduino.h>

// Screen type definitions - user should define one of these before including this library
// If none defined, defaults to CAPACITIVE_SCREEN
#if !defined(RESISTIVE_SCREEN) && !defined(CAPACITIVE_SCREEN)
  #define CAPACITIVE_SCREEN  // Default to capacitive if not specified
#endif

// Ensure only one screen type is defined
#if defined(RESISTIVE_SCREEN) && defined(CAPACITIVE_SCREEN)
  #error "Please define only one screen type: either RESISTIVE_SCREEN or CAPACITIVE_SCREEN"
#endif

// Touch point structure
struct TouchPoint {
    int16_t x;
    int16_t y;
    uint32_t timestamp;
    uint8_t pressure;
    uint16_t area;
    bool valid;
};

// Touch event types
enum TouchEvent {
    TOUCH_NONE,
    TOUCH_START,
    TOUCH_MOVE,
    TOUCH_END,
    TOUCH_INVALID
};

// Configuration structure with screen-type specific defaults
struct WaterRejectionConfig {
    #ifdef RESISTIVE_SCREEN
    // Resistive screen defaults - more permissive
    uint16_t maxTouchArea = 80;          // Larger area acceptable for resistive
    uint16_t minMovement = 10;           // Less precise movement detection
    uint16_t maxStaticTime = 800;        // Longer static time allowed
    uint8_t maxSimultaneousTouches = 1;  // Resistive is single touch only
    uint16_t touchTimeout = 1500;        // Longer timeout for resistive
    uint16_t gestureTimeout = 700;       // Slightly longer gesture time
    bool requireGesture = false;         // Usually not needed for resistive
    uint16_t edgeSwipeThreshold = 50;    // Same edge threshold
    uint16_t swipeMinDistance = 150;     // Same swipe distance
    uint16_t debounceTime = 50;          // Resistive needs debouncing
    uint16_t pressureThreshold = 300;    // Minimum pressure for resistive
    
    #else  // CAPACITIVE_SCREEN (default)
    // Capacitive screen defaults - stricter filtering
    uint16_t maxTouchArea = 50;          // Maximum area for valid touch
    uint16_t minMovement = 5;            // Minimum movement to be considered gesture
    uint16_t maxStaticTime = 500;        // Max time for static touch (ms)
    uint8_t maxSimultaneousTouches = 2;  // Max valid simultaneous touches
    uint16_t touchTimeout = 1000;        // Touch zone timeout (ms)
    uint16_t gestureTimeout = 500;       // Gesture completion timeout (ms)
    bool requireGesture = false;         // Require gesture to activate
    uint16_t edgeSwipeThreshold = 50;    // Pixels from edge for swipe start
    uint16_t swipeMinDistance = 150;     // Minimum swipe distance
    uint16_t debounceTime = 0;           // Capacitive doesn't need debouncing
    uint16_t pressureThreshold = 0;      // Not used for capacitive
    #endif
};

class WaterRejectionTouch {
private:
    // Touch zones for spatial filtering
    static const uint8_t ZONE_GRID_SIZE = 20;
    
    struct TouchZone {
        bool active;
        uint32_t activationTime;
        uint8_t touchCount;
        uint32_t lastTouchTime;
    };
    
    // Touch history for temporal filtering
    static const uint8_t HISTORY_SIZE = 20;
    TouchPoint touchHistory[HISTORY_SIZE];
    uint8_t historyIndex;
    
    // Zone grid
    TouchZone zones[ZONE_GRID_SIZE][ZONE_GRID_SIZE];
    
    // Screen dimensions
    uint16_t screenWidth;
    uint16_t screenHeight;
    
    // Configuration
    WaterRejectionConfig config;
    
    // Gesture state
    enum GestureState {
        GESTURE_IDLE,
        GESTURE_WAITING,
        GESTURE_ACTIVE
    };
    
    GestureState gestureState;
    uint32_t gestureStateTimeout;
    TouchPoint gestureStartPoint;
    
    // Multi-touch tracking
    uint8_t currentTouchCount;
    TouchPoint multiTouchPoints[5];
    
    // Statistics
    uint32_t waterDropletsRejected;
    uint32_t validTouches;
    
    // Screen type
    enum ScreenType {
        SCREEN_CAPACITIVE,
        SCREEN_RESISTIVE
    };
    ScreenType screenType;
    
    // Debouncing for resistive screens
    uint32_t lastValidTouchTime;
    TouchPoint lastValidTouch;
    
    // Private methods
    bool isWaterPattern(const TouchPoint& touch);
    bool checkZoneActivity(uint8_t zoneX, uint8_t zoneY);
    bool checkMultiTouchPattern();
    bool isStaticTouch(const TouchPoint& touch);
    void updateZones(const TouchPoint& touch);
    void updateHistory(const TouchPoint& touch);
    void clearOldZones();
    float calculateTouchClusterDensity();
    bool validateGesture(const TouchPoint& touch);
    
public:
    // Constructor
    WaterRejectionTouch(uint16_t width, uint16_t height);
    
    // Initialization
    void begin();
    void begin(const WaterRejectionConfig& customConfig);
    
    // Main processing functions
    bool processTouch(int16_t x, int16_t y);
    bool processTouch(int16_t x, int16_t y, uint8_t pressure);
    bool processTouch(const TouchPoint& touch);
    
    // Multi-touch processing
    bool processMultiTouch(TouchPoint* touches, uint8_t count);
    
    // Update function (call in loop)
    void update();
    
    // Configuration
    void setConfig(const WaterRejectionConfig& newConfig);
    WaterRejectionConfig getConfig() const;
    void setMaxTouchArea(uint16_t area);
    void setRequireGesture(bool require);
    void setScreenDimensions(uint16_t width, uint16_t height);
    
    // Gesture control
    void enableGestureMode();
    void disableGestureMode();
    bool isGestureActive() const;
    void resetGesture();
    
    // Statistics
    uint32_t getWaterDropletsRejected() const;
    uint32_t getValidTouches() const;
    void resetStatistics();
    
    // Debugging
    void printDebugInfo();
    void printZoneMap();
    
    // Touch event detection
    TouchEvent getTouchEvent(const TouchPoint& current);
    
    // Calibration helpers
    void calibrateForEnvironment(bool wetEnvironment);
    void setWetModeEnabled(bool enabled);
    
    // Screen type helpers
    const char* getScreenTypeName() const;
    bool isResistiveScreen() const;
    bool isCapacitiveScreen() const;
    void optimizeForScreenType();
};

// Helper class for touch event handling
class TouchEventHandler {
private:
    WaterRejectionTouch* waterFilter;
    TouchPoint lastTouch;
    TouchEvent lastEvent;
    
    void (*onTouchStart)(int16_t x, int16_t y);
    void (*onTouchMove)(int16_t x, int16_t y);
    void (*onTouchEnd)(int16_t x, int16_t y);
    
public:
    TouchEventHandler(WaterRejectionTouch* filter);
    
    void setTouchStartCallback(void (*callback)(int16_t, int16_t));
    void setTouchMoveCallback(void (*callback)(int16_t, int16_t));
    void setTouchEndCallback(void (*callback)(int16_t, int16_t));
    
    void handleTouch(int16_t x, int16_t y);
    void update();
};

#endif // WATER_REJECTION_TOUCH_H
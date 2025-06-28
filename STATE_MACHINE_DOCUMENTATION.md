# ESP32-S3 BLE Tag State Machine Documentation

## Overview
This document describes the state machine implementation for the ESP32-S3 BLE Tag system. The device operates in three distinct states: Pair, Normal, and Lost modes.

## State Diagram
```
┌─────────────┐
│   PAIR      │ ← Initial state after boot
│   MODE      │
└─────┬───────┘
      │ Device paired successfully
      ▼
┌─────────────┐
│   NORMAL    │
│   MODE      │ ← Send packets at 1Hz
└─────┬───────┘
      │ 5 consecutive packet delivery failures
      ▼
┌─────────────┐
│   LOST      │
│   MODE      │ ← Enhanced transmission/signaling
└─────────────┘
```

## States Description

### 1. PAIR MODE
- **Purpose**: Initial pairing and connection establishment
- **Entry Condition**: Device boot/startup
- **Behavior**: 
  - Advertise BLE services
  - Flash LED in a specific pattern (e.g., fast blue blink)
  - Wait for client connection and pairing
- **Exit Condition**: Successful BLE client connection established
- **Next State**: NORMAL

### 2. NORMAL MODE
- **Purpose**: Regular operation with periodic data transmission
- **Entry Condition**: From PAIR mode after successful connection
- **Behavior**:
  - Send data packets every 1 second (1Hz)
  - Flash LED in normal pattern (purple, 500ms interval)
  - Monitor packet delivery success/failure
- **Data Packet Format**: `MM/DD/YYYY, HH:MM:SS, Latitude, Longitude, Altitude, RSSI`
- **Exit Condition**: 5 consecutive packet delivery failures
- **Next State**: LOST

### 3. LOST MODE
- **Purpose**: Enhanced signaling when communication is lost
- **Entry Condition**: From NORMAL mode after 5 failed packet deliveries
- **Behavior**:
  - Enhanced LED signaling (e.g., rapid red flashing)
  - Attempt to re-establish connection
  - Continue trying to send packets (possibly at different rate)
- **Exit Condition**: TBD (manual reset, successful reconnection, etc.)

## Data Structures and Variables

### State Management
```cpp
enum DeviceState {
  STATE_PAIR,
  STATE_NORMAL,
  STATE_LOST
};

DeviceState currentState = STATE_PAIR;
```

### Timing Variables
```cpp
unsigned long lastPacketTime = 0;          // For 1Hz packet transmission
unsigned long lastLedTime = 0;             // For LED control
unsigned long lastStateChange = 0;         // Track state transitions
const long packetInterval = 1000;          // 1Hz = 1000ms
const long ledInterval = 500;              // LED flash interval
```

### Packet Delivery Tracking
```cpp
int consecutiveFailures = 0;               // Track failed deliveries
const int maxFailures = 5;                // Threshold for LOST state
bool lastPacketAck = false;                // Track last packet acknowledgment
```

### Data Packet Components
```cpp
struct PacketData {
  String timestamp;        // MM/DD/YYYY, HH:MM:SS
  float latitude;          // GPS coordinate
  float longitude;         // GPS coordinate  
  float altitude;          // GPS altitude
  int rssi;               // Bluetooth RSSI
};
```

## Key Functions

### State Management Functions
```cpp
void handlePairMode();           // Handle pairing state logic
void handleNormalMode();         // Handle normal operation state
void handleLostMode();           // Handle lost connection state
void changeState(DeviceState newState);  // State transition handler
```

### Data Functions
```cpp
String getCurrentTimestamp();    // Get RTC time in MM/DD/YYYY, HH:MM:SS format
PacketData gatherSensorData();   // Collect all sensor data
String formatPacket(PacketData data);  // Format data into packet string
bool sendPacket(String packet);  // Send packet via BLE and return success status
```

### LED Control Functions
```cpp
void updateLedForState();        // Update LED pattern based on current state
void setLedPairPattern();        // Fast blue blink for pairing
void setLedNormalPattern();      // Purple flash for normal operation
void setLedLostPattern();        // Rapid red flash for lost state
```

## Implementation Notes

### RTC Time
- Use ESP32's built-in RTC for timestamp generation
- Format: MM/DD/YYYY, HH:MM:SS
- May need to implement time synchronization mechanism

### GPS Integration
- Will require GPS module integration (not currently present)
- Placeholder values can be used during initial implementation
- GPS data: latitude, longitude, altitude

### RSSI Monitoring
- Monitor BLE connection RSSI
- Include in packet data
- Use for connection quality assessment

### Packet Delivery Confirmation
- Implement acknowledgment mechanism
- Track delivery success/failure
- Use notification callbacks to confirm delivery

### Error Handling
- Graceful handling of sensor failures
- Fallback values for missing data
- State recovery mechanisms

## Future Enhancements
- Battery level monitoring
- Sleep modes for power conservation
- Data logging to internal storage
- Remote configuration capabilities
- GPS time synchronization

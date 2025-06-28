/*
 * ESP32-S3-Zero - BLE Tag State Machine
 *
 * This program implements a 3-state machine for a BLE tracking tag:
 * 1. PAIR MODE: Initial pairing and connection establishment
 * 2. NORMAL MODE: Regular data transmission at 1Hz
 * 3. LOST MODE: Enhanced signaling when communication fails
 *
 * Data packet format: MM/DD/YYYY, HH:MM:SS, Latitude, Longitude, Altitude, RSSI
 *
 * --- HOW TO TEST BLE ---
 * 1. Upload this code to your board.
 * 2. On your phone, install a BLE scanner app (e.g., "nRF Connect for Mobile").
 * 3. Scan for devices and connect to "ESP32-S3-Zero".
 * 4. Find the custom service and characteristic.
 * 5. Enable notifications/subscribe to the characteristic to see the data packets.
 */

// Include required libraries
#include <FastLED.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <time.h>

// --- FastLED Definitions ---
#define LED_PIN       21
#define LED_TYPE      WS2812B
#define COLOR_ORDER   GRB
#define NUM_LEDS      1
#define BRIGHTNESS    50
CRGB leds[NUM_LEDS];

// --- BLE Definitions ---
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// --- State Machine Definitions ---
enum DeviceState {
  STATE_PAIR,
  STATE_NORMAL,
  STATE_LOST
};

DeviceState currentState = STATE_PAIR;

// --- Timing Variables ---
unsigned long lastPacketTime = 0;
unsigned long lastLedTime = 0;
unsigned long lastStateChange = 0;
const long packetInterval = 1000;      // 1Hz = 1000ms for data packets
const long ledInterval = 500;          // Base LED flash interval

// --- Packet Delivery Tracking ---
int consecutiveFailures = 0;
const int maxFailures = 5;
bool lastPacketAck = false;

// --- Data Packet Structure ---
struct PacketData {
  String timestamp;        // MM/DD/YYYY, HH:MM:SS
  float latitude;          // GPS coordinate (placeholder for now)
  float longitude;         // GPS coordinate (placeholder for now)
  float altitude;          // GPS altitude (placeholder for now)
  int rssi;               // Bluetooth RSSI
};

// --- LED State Variables ---
bool ledState = false;
unsigned long ledFlashRate = 500;     // Variable flash rate based on state

// Forward declarations
void changeState(DeviceState newState);

// BLE Server Callbacks to monitor connections
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Device connected");
      
      // If we were in PAIR mode, transition to NORMAL
      if (currentState == STATE_PAIR) {
        changeState(STATE_NORMAL);
      }
      
      // Reset failure counter on new connection
      consecutiveFailures = 0;
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Device disconnected");
      
      // If we were in NORMAL mode, go back to PAIR mode
      if (currentState == STATE_NORMAL) {
        changeState(STATE_PAIR);
      }
      
      // Restart advertising so a new client can connect
      pServer->getAdvertising()->start();
      Serial.println("Restarting advertising...");
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- ESP32-S3 BLE Tag State Machine ---");

  // --- Initialize FastLED ---
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  leds[0] = CRGB::Black;
  FastLED.show();
  Serial.println("FastLED Initialized.");

  // --- Initialize BLE ---
  initializeBLE();
  
  // --- Initialize RTC (basic setup) ---
  // Note: For proper time, you might want to sync with NTP or set manually
  // This is a basic setup - time will start from epoch
  
  // Start in PAIR mode
  changeState(STATE_PAIR);
  Serial.println("System initialized - Starting in PAIR mode");
}

void loop() {
  unsigned long currentTime = millis();

  // Update LED based on current state
  updateLedForState(currentTime);

  // Handle state-specific logic
  switch (currentState) {
    case STATE_PAIR:
      handlePairMode(currentTime);
      break;
    case STATE_NORMAL:
      handleNormalMode(currentTime);
      break;
    case STATE_LOST:
      handleLostMode(currentTime);
      break;
  }
}

// ========================================
// STATE MACHINE FUNCTIONS
// ========================================

void changeState(DeviceState newState) {
  if (currentState != newState) {
    Serial.print("State change: ");
    Serial.print(getStateName(currentState));
    Serial.print(" -> ");
    Serial.println(getStateName(newState));
    
    currentState = newState;
    lastStateChange = millis();
    
    // Update LED flash rate based on new state
    switch (newState) {
      case STATE_PAIR:
        ledFlashRate = 200;  // Fast blue blink
        break;
      case STATE_NORMAL:
        ledFlashRate = 500;  // Normal purple flash
        break;
      case STATE_LOST:
        ledFlashRate = 100;  // Rapid red flash
        break;
    }
  }
}

void handlePairMode(unsigned long currentTime) {
  // In PAIR mode, we just wait for a connection
  // LED will flash blue rapidly (handled in updateLedForState)
  // BLE advertising is already running
  
  // Connection handling is done in the callback functions
  // No additional logic needed here for now
}

void handleNormalMode(unsigned long currentTime) {
  // Send data packets every 1Hz (1000ms)
  if (deviceConnected && (currentTime - lastPacketTime >= packetInterval)) {
    lastPacketTime = currentTime;
    
    // Gather sensor data
    PacketData data = gatherSensorData();
    
    // Format and send packet
    String packet = formatPacket(data);
    bool success = sendPacket(packet);
    
    if (success) {
      consecutiveFailures = 0;
      Serial.print("Packet sent successfully: ");
      Serial.println(packet);
    } else {
      consecutiveFailures++;
      Serial.print("Packet send failed. Consecutive failures: ");
      Serial.println(consecutiveFailures);
      
      // Check if we should transition to LOST mode
      if (consecutiveFailures >= maxFailures) {
        changeState(STATE_LOST);
      }
    }
  }
}

void handleLostMode(unsigned long currentTime) {
  // In LOST mode, continue trying to send packets but with enhanced signaling
  // LED will flash red rapidly (handled in updateLedForState)
  
  // Continue trying to send packets (maybe at a different rate)
  if (deviceConnected && (currentTime - lastPacketTime >= packetInterval)) {
    lastPacketTime = currentTime;
    
    PacketData data = gatherSensorData();
    String packet = formatPacket(data);
    bool success = sendPacket(packet);
    
    if (success) {
      // If we successfully send a packet, consider going back to NORMAL
      consecutiveFailures = 0;
      changeState(STATE_NORMAL);
      Serial.println("Communication restored - returning to NORMAL mode");
    } else {
      Serial.println("Still in LOST mode - communication failed");
    }
  }
}

// ========================================
// DATA FUNCTIONS
// ========================================

String getCurrentTimestamp() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%m/%d/%Y, %H:%M:%S", &timeinfo);
  return String(buffer);
}

PacketData gatherSensorData() {
  PacketData data;
  
  // Get current timestamp
  data.timestamp = getCurrentTimestamp();
  
  // Placeholder GPS coordinates (replace with actual GPS module data)
  data.latitude = 40.7128;   // Example: New York City latitude
  data.longitude = -74.0060; // Example: New York City longitude
  data.altitude = 10.0;      // Example altitude in meters
  
  // Get RSSI if connected
  if (deviceConnected) {
    // Note: Getting RSSI from ESP32 as server is tricky
    // This is a placeholder - actual implementation may vary
    data.rssi = -50; // Placeholder RSSI value
  } else {
    data.rssi = 0;
  }
  
  return data;
}

String formatPacket(PacketData data) {
  // Format: MM/DD/YYYY, HH:MM:SS, Latitude, Longitude, Altitude, RSSI
  String packet = data.timestamp + ", ";
  packet += String(data.latitude, 6) + ", ";
  packet += String(data.longitude, 6) + ", ";
  packet += String(data.altitude, 2) + ", ";
  packet += String(data.rssi);
  
  return packet;
}

bool sendPacket(String packet) {
  if (!deviceConnected || !pCharacteristic) {
    return false;
  }
  
  try {
    pCharacteristic->setValue(packet.c_str());
    pCharacteristic->notify();
    return true;
  } catch (...) {
    return false;
  }
}

// ========================================
// LED CONTROL FUNCTIONS
// ========================================

void updateLedForState(unsigned long currentTime) {
  if (currentTime - lastLedTime >= ledFlashRate) {
    lastLedTime = currentTime;
    
    switch (currentState) {
      case STATE_PAIR:
        setLedPairPattern();
        break;
      case STATE_NORMAL:
        setLedNormalPattern();
        break;
      case STATE_LOST:
        setLedLostPattern();
        break;
    }
    
    ledState = !ledState;
    FastLED.show();
  }
}

void setLedPairPattern() {
  if (ledState) {
    leds[0] = CRGB::Blue;   // Blue for pairing mode
  } else {
    leds[0] = CRGB::Black;
  }
}

void setLedNormalPattern() {
  if (ledState) {
    leds[0] = CRGB::Purple; // Purple for normal operation
  } else {
    leds[0] = CRGB::Black;
  }
}

void setLedLostPattern() {
  if (ledState) {
    leds[0] = CRGB::Red;    // Red for lost connection
  } else {
    leds[0] = CRGB::Black;
  }
}

// ========================================
// UTILITY FUNCTIONS
// ========================================

String getStateName(DeviceState state) {
  switch (state) {
    case STATE_PAIR: return "PAIR";
    case STATE_NORMAL: return "NORMAL";
    case STATE_LOST: return "LOST";
    default: return "UNKNOWN";
  }
}

void initializeBLE() {
  // Create the BLE Device
  BLEDevice::init("ESP32-S3-Zero");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  // Add a descriptor to the characteristic
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("BLE Initialized and Advertising Started.");
}

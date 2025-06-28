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
BLECharacteristic* pCharacteristic = NULL;        // For sending data (notifications)
BLECharacteristic* pReceiveCharacteristic = NULL; // For receiving data (write)
bool deviceConnected = false;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // Send characteristic
#define RECEIVE_CHAR_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a9"  // Receive characteristic

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

// --- Received Data Storage ---
struct ReceivedData {
  String timestamp;        // Last received timestamp
  float latitude;          // Last received GPS latitude
  float longitude;         // Last received GPS longitude
  float altitude;          // Last received GPS altitude
  int rssi;               // Last received RSSI
  bool hasReceivedData;    // Flag to indicate if we've received valid data
};

ReceivedData receivedData = {"", 0.0, 0.0, 0.0, 0, false};

// --- LED State Variables ---
bool ledState = false;
unsigned long ledFlashRate = 500;     // Variable flash rate based on state

// Forward declarations
void changeState(DeviceState newState);
void parseReceivedPacket(String packet);

// BLE Characteristic Callbacks for receiving data
class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
      String value = String(pCharacteristic->getValue().c_str());
      
      if (value.length() > 0) {
        Serial.print("Received packet: ");
        Serial.println(value);
        
        // Parse the received packet
        parseReceivedPacket(value);
      }
    }
};

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
  
  // If we have received data, use that for GPS and timestamp
  if (receivedData.hasReceivedData) {
    data.timestamp = receivedData.timestamp;
    data.latitude = receivedData.latitude;
    data.longitude = receivedData.longitude;
    data.altitude = receivedData.altitude;
    Serial.println("Using received GPS and timestamp data");
  } else {
    // Get current timestamp
    data.timestamp = getCurrentTimestamp();
    
    // Placeholder GPS coordinates (replace with actual GPS module data)
    data.latitude = 26.3082;   // Example: UTRGV latitude
    data.longitude = -98.1740; // Example: UTRGV longitude (negative for western hemisphere)
    data.altitude = 10.0;      // Example altitude in meters
  }
  
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
// PACKET PARSING FUNCTIONS
// ========================================

void parseReceivedPacket(String packet) {
  // Expected format: MM/DD/YYYY, HH:MM:SS, Latitude, Longitude, Altitude
  // Example: "12/28/2025, 14:30:45, 40.712800, -74.006000, 10.00"
  // NOTE: RSSI is NOT accepted from incoming packets - it's determined locally
  
  int commaCount = 0;
  int lastIndex = 0;
  String parts[5]; // date, time, latitude, longitude, altitude
  
  // Count commas and split string
  for (int i = 0; i < packet.length(); i++) {
    if (packet.charAt(i) == ',') {
      if (commaCount < 5) {
        parts[commaCount] = packet.substring(lastIndex, i);
        parts[commaCount].trim(); // Remove whitespace
      }
      commaCount++;
      lastIndex = i + 1;
    }
  }
  
  // Get the last part (altitude)
  if (commaCount >= 4 && lastIndex < packet.length()) {
    parts[commaCount] = packet.substring(lastIndex);
    parts[commaCount].trim();
  }
  
  // Validate we have enough parts (should be 4: date, time, lat, lon, alt)
  if (commaCount >= 4) {
    try {
      // Combine date and time for timestamp
      receivedData.timestamp = parts[0] + ", " + parts[1];
      
      // Parse GPS coordinates and altitude
      receivedData.latitude = parts[2].toFloat();
      receivedData.longitude = parts[3].toFloat();
      receivedData.altitude = parts[4].toFloat();
      
      // RSSI is NOT updated from received packets - it remains 0 in received data
      // The actual RSSI will be determined by the ESP32's own BLE connection
      receivedData.rssi = 0;
      
      receivedData.hasReceivedData = true;
      
      Serial.println("Successfully parsed received packet:");
      Serial.println("  Timestamp: " + receivedData.timestamp);
      Serial.println("  Latitude: " + String(receivedData.latitude, 6));
      Serial.println("  Longitude: " + String(receivedData.longitude, 6));
      Serial.println("  Altitude: " + String(receivedData.altitude, 2));
      Serial.println("  RSSI will be determined locally (not from packet)");
      
    } catch (...) {
      Serial.println("Error parsing received packet - invalid format");
    }
  } else {
    Serial.println("Error: Received packet has insufficient data fields");
    Serial.println("Expected format: MM/DD/YYYY, HH:MM:SS, Latitude, Longitude, Altitude");
    Serial.println("Note: RSSI is determined locally and should not be included in incoming packets");
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

  // Create a BLE Characteristic for sending data (notifications)
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  // Add a descriptor to the characteristic
  pCharacteristic->addDescriptor(new BLE2902());

  // Create a BLE Characteristic for receiving data (write)
  pReceiveCharacteristic = pService->createCharacteristic(
                            RECEIVE_CHAR_UUID,
                            BLECharacteristic::PROPERTY_WRITE
                          );
  // Set callback for when data is written to this characteristic
  pReceiveCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

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
  Serial.println("Device can now send and receive data packets.");
}

/*
 * ESP32-S3-Zero - BLE Notifier and LED Flasher
 *
 * This program performs two tasks simultaneously:
 * 1. Flashes the onboard NeoPixel (GPIO 21) using the FastLED library.
 * 2. Creates a Bluetooth Low Energy (BLE) server that sends a "Hello World"
 * notification every second to any connected client.
 *
 * --- HOW TO TEST BLE ---
 * 1. Upload this code to your board.
 * 2. On your phone, install a BLE scanner app (e.g., "nRF Connect for Mobile").
 * 3. Scan for devices and connect to "ESP32-S3-Zero".
 * 4. Find the custom service and characteristic.
 * 5. Enable notifications/subscribe to the characteristic to see the messages.
 */

// Include required libraries
#include <FastLED.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// --- FastLED Definitions ---
#define LED_PIN       21
#define LED_TYPE      WS2812B
#define COLOR_ORDER   GRB
#define NUM_LEDS      1
#define BRIGHTNESS    50
CRGB leds[NUM_LEDS];
bool ledState = false; // To track the on/off state of the LED

// --- BLE Definitions ---
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// --- Non-Blocking Delay Timers ---
unsigned long lastLedTime = 0;
unsigned long lastBleTime = 0;
const long ledInterval = 500; // 500 ms -> flash twice a second
const long bleInterval = 1000; // 1000 ms -> send BLE notification every second

// BLE Server Callbacks to monitor connections
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Device connected");
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Device disconnected");
      // Restart advertising so a new client can connect
      pServer->getAdvertising()->start();
      Serial.println("Restarting advertising...");
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- ESP32-S3 BLE & LED Controller ---");

  // --- Initialize FastLED ---
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  leds[0] = CRGB::Black;
  FastLED.show();
  Serial.println("FastLED Initialized.");

  // --- Initialize BLE ---
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

void loop() {
  unsigned long currentTime = millis();

  // --- Task 1: Handle LED Flashing (Non-Blocking) ---
  if (currentTime - lastLedTime >= ledInterval) {
    lastLedTime = currentTime; // Update the timer

    if (ledState) {
      leds[0] = CRGB::Black; // Turn off
    } else {
      leds[0] = CRGB::Purple; // Turn on
    }
    ledState = !ledState; // Invert the state
    FastLED.show();
  }

  // --- Task 2: Send BLE Notifications (Non-Blocking) ---
  if (deviceConnected && (currentTime - lastBleTime >= bleInterval)) {
    lastBleTime = currentTime; // Update the timer

    // Create the message
    static int counter = 0;
    String message = "Hello World #" + String(counter++);
    
    // Set the characteristic value and notify the client
    pCharacteristic->setValue(message.c_str());
    pCharacteristic->notify();
    
    Serial.print("Sent notification: ");
    Serial.println(message);
  }
}

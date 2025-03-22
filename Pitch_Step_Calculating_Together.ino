#include <MPU6050_tockn.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <math.h>

#define SDA_PIN 6
#define SCL_PIN 7
#define BUTTON_PIN D0
#define LED_PIN D1

MPU6050 mpu6050(Wire);

// BLE UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;
long timer = 0;
RTC_DATA_ATTR int stepCount = 0;  // ✅ Stores step count persistently
bool stepDetected = false;

// BLE Server Callbacks
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Device connected.");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected");
        pServer->startAdvertising();
    }
};

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    Wire.begin(SDA_PIN, SCL_PIN);
    
    mpu6050.begin();
    mpu6050.calcGyroOffsets(true);
    Serial.println("MPU6050 Initialized.");

    BLEDevice::init("AlignEye AI Glass");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService* pService = pServer->createService(SERVICE_UUID);
    
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ |
                        BLECharacteristic::PROPERTY_WRITE |
                        BLECharacteristic::PROPERTY_NOTIFY
                      );

    BLE2902* pBLE2902 = new BLE2902();
    pBLE2902->setNotifications(true);
    pCharacteristic->addDescriptor(pBLE2902);

    pCharacteristic->setValue("Initializing...");
    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x00);
    BLEDevice::startAdvertising();
    
    Serial.println("BLE Service Started. Waiting for connection...");
}

void loop() {
    if (!deviceConnected) return;

    if (millis() - timer > 1000) { // Every second
        timer = millis();
        
        mpu6050.update();
        
        // Compute Pitch
        float aX = mpu6050.getAccX();
        float aY = mpu6050.getAccY();
        float aZ = mpu6050.getAccZ();
        float pitch = atan2(-aX, sqrt(aY * aY + aZ * aZ)) * 180 / PI;

        // Step detection
        float magnitude = sqrt(aX * aX + aY * aY + aZ * aZ);
        if (magnitude > 1.2 && !stepDetected) {
            stepDetected = true;
            stepCount++;  // ✅ Step count now persists
            Serial.println("Step detected!");
        } else if (magnitude < 1.1) {
            stepDetected = false;
        }

        // Send Data over BLE
        String data = "P: " + String(pitch, 2) + ", S: " + String(stepCount);
        Serial.println(data);  // Confirm it's sending correctly
        pCharacteristic->setValue(data.c_str());  
        pCharacteristic->notify();  // Ensure it's notified to the server
    }
}
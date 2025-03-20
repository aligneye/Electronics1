#include <MPU6050_tockn.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SDA_PIN 6        // Default SDA pin for Seeed XIAO ESP32C3
#define SCL_PIN 7        // Default SCL pin for Seeed XIAO ESP32C3

MPU6050 mpu6050(Wire);

// BLE UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLEDescriptor* pDescr;
BLE2902* pBLE2902;

bool deviceConnected = false;
bool oldDeviceConnected = false;
long timer = 0;

// BLE Server Callbacks
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

// BLE Characteristic Callbacks
class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        String value = pCharacteristic->getValue();
        if (value.length() > 0) {
            Serial.print("New value received: ");
            Serial.println(value.c_str());
        }
    }
};

void setup() {
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);
    
    // Initialize MPU6050
    mpu6050.begin();
    mpu6050.calcGyroOffsets(true);
    Serial.println("MPU6050 Initialized.");

    // Initialize BLE
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

    pCharacteristic->setCallbacks(new MyCallbacks());

    // Add a descriptor
    pDescr = new BLEDescriptor((uint16_t)0x2901);
    pDescr->setValue("MPU6050 Pitch Angle");
    pCharacteristic->addDescriptor(pDescr);
    
    // Enable Notifications
    pBLE2902 = new BLE2902();
    pBLE2902->setNotifications(true);
    pCharacteristic->addDescriptor(pBLE2902);

    // Start BLE service
    pCharacteristic->setValue("Hello from ESP32");
    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x00);
    BLEDevice::startAdvertising();
    
    Serial.println("BLE Service Started. Waiting for connection...");
}

void loop() {
    mpu6050.update();

    if (deviceConnected && millis() - timer > 1000) {
        timer = millis();
        
        // Compute angles
        float aX = mpu6050.getAccX();
        float aY = mpu6050.getAccY();
        float aZ = mpu6050.getAccZ();
        float pitch = atan2(-aX, sqrt(aY * aY + aZ * aZ)) * 180 / PI;

        // Print and send via BLE
        Serial.print("Pitch: ");
        Serial.println(pitch);
        String pitchStr = String(pitch);
        pCharacteristic->setValue(pitchStr.c_str());
        pCharacteristic->notify();
    }

    // Handle connection status changes
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        Serial.println("Restarting BLE advertising...");
        oldDeviceConnected = deviceConnected;
    }

    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }
}
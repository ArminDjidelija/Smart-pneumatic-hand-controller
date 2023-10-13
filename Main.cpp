/*
  Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
  Ported to Arduino ESP32 by Evandro Copercini
  updated by chegewara and MoThunderz
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include "MS5837.h"

MS5837 sensor;

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLECharacteristic* pCharacteristic_2 = NULL;
BLEDescriptor* pDescr;
BLE2902* pBLE2902;

bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

int kompresor = 16;
int ventil = 27;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR1_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR2_UUID "e3223119-9445-4e96-a4a1-85358c4046a2"

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

int brojCiklusa=0;
int brojPonavljanja = 0;
int pauza=0;
int pritisakZadani=0;
bool akt = false;

void Funkcija();

class CharacteristicCallBack : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    std::string pChar2_value_stdstr = pChar->getValue();
    String pChar2_value_string = String(pChar2_value_stdstr.c_str());
    int pChar2_value_int = pChar2_value_string.toInt();
    Serial.println("pChar2: " + String(pChar2_value_int));

    brojCiklusa=pChar2_value_int%100;
    pChar2_value_int/=100;

    brojPonavljanja=pChar2_value_int%100;
    pChar2_value_int/=100;

    pauza=pChar2_value_int%100;
    pChar2_value_int/=100;

    pritisakZadani=pChar2_value_int%100;
    pritisakZadani*=100;

    if (pChar2_value_int != 0) {
      akt = true;
    }
  }
};

void setup() {
  Serial.begin(9600);
  Wire.begin();
  while (!sensor.init()) {
    Serial.println("Init failed!");
    Serial.println("Are SDA/SCL connected correctly?");
    Serial.println("Blue Robotics Bar30: White=SDA, Green=SCL");
    Serial.println("\n\n\n");
    delay(5000);
  }
  sensor.setModel(MS5837::MS5837_30BA);
  sensor.setFluidDensity(997);  // kg/m^3 (freshwater, 1029 for seawater)
  // Create the BLE Device
  BLEDevice::init("ESP32");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService* pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
    CHAR1_UUID,
    BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic_2 = pService->createCharacteristic(
    CHAR2_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  // Create a BLE Descriptor

  pDescr = new BLEDescriptor((uint16_t)0x2901);
  pDescr->setValue("A very interesting variable");
  pCharacteristic->addDescriptor(pDescr);

  pBLE2902 = new BLE2902();
  pBLE2902->setNotifications(true);
  pCharacteristic->addDescriptor(pBLE2902);

  pCharacteristic_2->addDescriptor(new BLE2902());
  pCharacteristic_2->setCallbacks(new CharacteristicCallBack());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");

  pinMode(ventil, OUTPUT);
  digitalWrite(ventil, HIGH);
  
}

int zadnjiPritisak=1005;
int pritisak=1000;
void loop() {
  
  // notify changed value
  if (deviceConnected) {
    if (akt == false) {
      sensor.read();
      int pritisak = sensor.pressure();
      if(abs(pritisak)>2500)
      {
        pritisak=zadnjiPritisak;
      }
      pCharacteristic->setValue(pritisak);
      pCharacteristic->notify();
      zadnjiPritisak=pritisak;
      //value++;
      delay(1000);

      Serial.print("Pritisak zadani: ");
      Serial.println(pritisakZadani);
      Serial.print("Pauza: ");
      Serial.println(pauza);
      Serial.print("Broj ciklusa: ");
      Serial.println(brojCiklusa);
      Serial.print("Broj ponavljanja: ");
      Serial.println(brojPonavljanja);

      //Serial.println(pritisak);
    } else {
      for(int j=0;j<brojCiklusa;j++)
      {
        for (int i = 0; i < brojPonavljanja; i++) {
        sensor.read();
        pritisak = sensor.pressure();//k
        if(abs(pritisak)>2500)
        {
          pritisak=zadnjiPritisak;
        }
        while (pritisak < pritisakZadani) {
          digitalWrite(ventil, LOW);
          delay(100);
          sensor.read();
          pritisak = sensor.pressure();
          Serial.println(pritisak);
          if(abs(pritisak)>2500)
          {
            pritisak=zadnjiPritisak;
          }
          pCharacteristic->setValue(pritisak);
          pCharacteristic->notify();
          zadnjiPritisak=pritisak;
        }
                
        digitalWrite(ventil, HIGH);
        delay(500);
        for(int i=0;i<3;i++)
        {
          sensor.read();
          pritisak = sensor.pressure();
           if(abs(pritisak)>2500)
          {
            pritisak=zadnjiPritisak;
          }
          pCharacteristic->setValue(pritisak);
          pCharacteristic->notify();
          delay(200);
          pritisak=zadnjiPritisak;
        }
        delay(pauza*1000);

      }
        delay(pauza*2000);

      }
      akt=false;

      pritisakZadani=0;
      pauza=0;
      brojPonavljanja=0;
      brojCiklusa=0;

    }
  }
  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);                   // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();  // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
 
}



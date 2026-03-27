#include "Wireless.h"


bool WIFI_Connection = 0;
uint8_t WIFI_NUM = 0;
uint8_t BLE_NUM = 0;
bool Scan_finish = 0;
uint8_t NUM=0;
uint8_t wifi_scan()
{
  printf("/**********WiFi Test**********/\r\n");
  // Set WiFi to station mode and disconnect from an AP if it was previously connected.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  // WiFi.scanNetworks will return the number of networks found.
  int n = WiFi.scanNetworks();
  // Serial.println("Scan done");
  if (n == 0)
  {
    printf("no networks found\r\n");
  }
  else
  {
      printf("Nr | SSID                             | RSSI | CH | Encryption\r\n");
      for (int i = 0; i < 3; ++i)
      {
          // Print SSID and RSSI for each network found
          printf("%2d | ", i + 1);
          printf("%-32.32s | ", WiFi.SSID(i).c_str());
          printf("%4d | ", WiFi.RSSI(i));
          printf("%2d | ", WiFi.channel(i));
          switch (WiFi.encryptionType(i))
          {
          case WIFI_AUTH_OPEN:
              printf("open\r\n");
              break;
          case WIFI_AUTH_WEP:
              printf("WEP\r\n");
              break;
          case WIFI_AUTH_WPA_PSK:
              printf("WPA\r\n");
              break;
          case WIFI_AUTH_WPA2_PSK:
              printf("WPA2\r\n");
              break;
          case WIFI_AUTH_WPA_WPA2_PSK:
              printf("WPA+WPA2\r\n");
              break;
          case WIFI_AUTH_WPA2_ENTERPRISE:
              printf("WPA2-EAP\r\n");
              break;
          case WIFI_AUTH_WPA3_PSK:
              printf("WPA3\r\n");
              break;
          case WIFI_AUTH_WPA2_WPA3_PSK:
              printf("WPA2+WPA3\r\n");
              break;
          case WIFI_AUTH_WAPI_PSK:
              printf("WAPI\r\n");
              break;
          default:
              printf("unknown\r\n");
          }
          vTaskDelay(10);
      }
  }

  // Delete the scan result to free memory for code below.
  WiFi.scanDelete();
  WiFi.disconnect(true); 
  WiFi.mode(WIFI_OFF);   
  vTaskDelay(100);           
  printf("/*******WiFi Test Over********/\r\n\r\n");
  printf("/**********BLE Test**********/\r\n"); 

  return 1;
}
BLEScan* pBLEScan;
uint8_t ble_scan()
{

  if(NUM == 0){
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
  }
  BLEScanResults* foundDevices = pBLEScan->start(5);
  int count = foundDevices->getCount();
  printf("Found %d devices\r\n",count);
  for (int i = 0; i < count; i++) {
    BLEAdvertisedDevice device = foundDevices->getDevice(i);
    if (device.haveName()) {
      printf("Bluetooth Name      : %s\r\n",device.getName().c_str());
      printf("Device address      : %s\r\n",device.getAddress().toString().c_str());
      printf("Response Length     : %d\r\n",device.getPayloadLength());
      printf("\r\n");
      NUM++;
    }
    if(NUM == 2)
    {
      break;
    }
  }
  if(NUM == 2)
  {
    pBLEScan->stop(); 
    pBLEScan->clearResults();
    BLEDevice::deinit(true); 
    printf("/**********BLE Test Over**********/\r\n\r\n");
  }
  return NUM;
}
void Wireless_Test1(){
  uint8_t Time=0;
  pinMode(0, INPUT);                    
  while(digitalRead(0) != 0)         
  {
    vTaskDelay(10);
    Time++;
    if(Time == 100)
    {
      printf("Please press the BOOT button \r\n");
      Time=0;
    }
  }
  wifi_scan();
  uint8_t ble_NUM = ble_scan();
  while(ble_NUM != 2)
    ble_NUM = ble_scan();
    
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int wifi_scan_number()
{
  printf("/**********WiFi Test**********/\r\n");
  // Set WiFi to station mode and disconnect from an AP if it was previously connected.
  WiFi.mode(WIFI_STA);                           
  WiFi.setSleep(true);     
  // WiFi.scanNetworks will return the number of networks found.
  int count = WiFi.scanNetworks();
  if (count == 0)
  {
    printf("No WIFI device was scanned\r\n");
  }
  else{
    printf("Scanned %d Wi-Fi devices\r\n",count);
  }
  
  // Delete the scan result to free memory for code below.
  WiFi.scanDelete();
  WiFi.disconnect(true); 
  WiFi.mode(WIFI_OFF);  
  vTaskDelay(100);         
  printf("/*******WiFi Test Over********/\r\n\r\n");
  return count;
}
int ble_scan_number()
{
  printf("/**********BLE Test**********/\r\n"); 
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);

  BLEScanResults* foundDevices = pBLEScan->start(5);  
  vTaskDelay(5000);       
  int count = foundDevices->getCount();
  if (count == 0)
  {
    printf("No Bluetooth device was scanned\r\n");
  }
  else{
    printf("Scanned %d Bluetooth devices\r\n",count);
  }
  pBLEScan->clearResults(); 
  pBLEScan->stop(); 
  BLEDevice::deinit(true); 
  vTaskDelay(100);         
  printf("/**********BLE Test Over**********/\r\n\r\n");
  return count;
}

void WirelessScanTask(void *parameter) {
  WIFI_NUM = wifi_scan_number();
  BLE_NUM = ble_scan_number();
  Scan_finish = 1;
  vTaskDelete(NULL);
}
void Wireless_Test2(){
  xTaskCreatePinnedToCore(
    WirelessScanTask,     
    "WirelessScanTask",   
    4096,                
    NULL,                 
    1,                    
    NULL,                
    0                    
  );
}
#include <Preferences.h>

// UUIDs baseados no index.html
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define SSID_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define PASS_UUID    "a5b9c02d-0579-43c7-9fb6-63e843ebaf5b"
#define EMAIL_UUID   "c8659212-af91-4ad3-a995-a58d6fd26145"

bool ble_deviceConnected = false;
bool ble_provisioning_done = false;
String ble_ssid = "";
String ble_pass = "";
String ble_email = "";
BLEServer* pServer = NULL;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      ble_deviceConnected = true;
      Serial.println("[BLE] Dispositivo conectado. Interrompendo conexoes de WiFi conflitantes...");
      // Importante para n�o interferir no r�dio
      WiFi.disconnect(true);
    };

    void onDisconnect(BLEServer* pServer) {
      ble_deviceConnected = false;
      Serial.println("[BLE] Dispositivo desconectado.");
      if (ble_provisioning_done) {
          Serial.println("[BLE] Dados recebidos com sucesso. Reiniciando para aplicar...");
          delay(1000);
          ESP.restart();
      } else {
          pServer->startAdvertising(); 
      }
    }
};

class CharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = pCharacteristic->getValue();
      if (rxValue.length() > 0) {
        String data = rxValue;
        String uuidStr = String(pCharacteristic->getUUID().toString().c_str());
        
        if (uuidStr == String(SSID_UUID)) {
          ble_ssid = data;
          Serial.println("[BLE] SSID recebido.");
        } else if (uuidStr == String(PASS_UUID)) {
          ble_pass = data;
          Serial.println("[BLE] PASS recebida.");
        } else if (uuidStr == String(EMAIL_UUID)) {
          ble_email = data;
          Serial.println("[BLE] Email recebido.");
          
          if(ble_ssid.length() > 0 && ble_pass.length() > 0) {
            Preferences preferencias;
            // Salvar no NVS
            WiFi.begin(ble_ssid.c_str(), ble_pass.c_str());
            
            preferencias.begin("datalogger", false);
            preferencias.putString("email", ble_email);
            preferencias.end();
            ble_provisioning_done = true;
          }
        }
      }
    }
};

void Iniciar_BLE_Provisionamento() {
    Serial.println("[BLE] Iniciando modulo...");
    BLEDevice::init("Todesco");
    
    // Potência Máxima do S3 (Volume Trovão +9dBm)
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    BLECharacteristic *pCharSSID = pService->createCharacteristic(
                                         SSID_UUID,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
    pCharSSID->setCallbacks(new CharacteristicCallbacks());

    BLECharacteristic *pCharPASS = pService->createCharacteristic(
                                         PASS_UUID,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
    pCharPASS->setCallbacks(new CharacteristicCallbacks());

    BLECharacteristic *pCharEMAIL = pService->createCharacteristic(
                                         EMAIL_UUID,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
    pCharEMAIL->setCallbacks(new CharacteristicCallbacks());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // helps with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.printf("[BLE] Grudado no UUID: %s\n", SERVICE_UUID);
    Serial.println("[BLE] Aguardando conexao do navegador...");
}

bool is_BLE_Provisionamento_Conectado() {
    return ble_deviceConnected;
}

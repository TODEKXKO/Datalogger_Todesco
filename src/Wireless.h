#pragma once
#include <Arduino.h>
#include "WiFi.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

extern bool WIFI_Connection;
extern uint8_t WIFI_NUM;
extern uint8_t BLE_NUM;
extern bool Scan_finish;

// Funções de varredura
uint8_t wifi_scan();
uint8_t ble_scan();
void Wireless_Test1();
int wifi_scan_number();
int ble_scan_number();
void Wireless_Test2();

// Função de Provisionamento BLE
void Iniciar_BLE_Provisionamento();
bool is_BLE_Provisionamento_Conectado();

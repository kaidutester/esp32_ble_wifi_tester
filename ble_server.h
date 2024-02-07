//
// ble server
//
// TODO - missing:
// (1) factory reset - wipe NVS (nvs_erase_all(), nvs_commit())
// (2) self firmware fetching and writing
// (3) read last error code
//
// TODO - customer-id characteristic
//


#include "version.h"
#include "configuration.h"

//#define BLE_SERVER_ENABLE_LOGS 1 // this feature is not completed
//#define TEST_CUSTOM_ADV_SCAN 1   // not used

#define SERVICE_UUID_KAIDU              "8f7e1830-70b5-46b4-b09f-eda20e4b6a7b"
#define CHARACTERISTIC_UUID_SSID        "8f7e1831-70b5-46b4-b09f-eda20e4b6a7b"
#define CHARACTERISTIC_UUID_PSWD        "8f7e1832-70b5-46b4-b09f-eda20e4b6a7b"
#define CHARACTERISTIC_UUID_CERT        "8f7e1833-70b5-46b4-b09f-eda20e4b6a7b"
#define CHARACTERISTIC_UUID_DVID        "8f7e1834-70b5-46b4-b09f-eda20e4b6a7b"
#define CHARACTERISTIC_UUID_STATS       "8f7e1835-70b5-46b4-b09f-eda20e4b6a7b"
#define CHARACTERISTIC_UUID_LOGS        "8f7e1836-70b5-46b4-b09f-eda20e4b6a7b"
#define CHARACTERISTIC_UUID_CUST        "8f7e1837-70b5-46b4-b09f-eda20e4b6a7b"

#define SERVICE_UUID_OTA                "c8659210-af91-4ad3-a995-a58d6fd26145" // UART service UUID
#define CHARACTERISTIC_UUID_FW          "c8659211-af91-4ad3-a995-a58d6fd26145" // write fw bytes
#define CHARACTERISTIC_UUID_HW_VERSION  "c8659212-af91-4ad3-a995-a58d6fd26145" // read fw version, write fw bin filename
#define CHARACTERISTIC_UUID_FCTRY_RESET "c8659213-af91-4ad3-a995-a58d6fd26145" // factory-reset

#define BLE_SERVER_OTA_FULL_PACKET 512

#define BLE_SERVER_BUFSIZE 16

#define BLE_SERVER_DEVICE_STATE_INITIALIZED 0xF0
#define BLE_SERVER_DEVICE_STATE_CONFIGURED  0xF1
#define BLE_SERVER_DEVICE_STATE_OPERATIONAL 0xF2
#define BLE_SERVER_DEVICE_STATE_SSID_ERROR  0xE0
#define BLE_SERVER_DEVICE_STATE_WIFI_ERROR  0xE1
#define BLE_SERVER_DEVICE_STATE_HTTP_ERROR  0xE2

BLEServer *pServer;
BLEService *pServiceKaidu, *pServiceOta;
BLECharacteristic *pCharacteristicSsid, *pCharacteristicPswd, *pCharacteristicCert, *pCharacteristicDvid, 
    *pCharacteristicStats, *pCharacteristicLogs, *pCharacteristicCust;
BLECharacteristic *pVersionCharacteristic, *pOtaCharacteristic, *pFactoryResetCharacteristic;

bool ble_server_setup_done = false;
bool ble_server_started = false;
bool ble_server_connection_status = false;
bool ble_server_values_updated = false;
bool ble_server_ota_updated = false;
bool ble_server_ota_completed = false;
bool ble_server_ota_requested = false;
bool ble_server_log_notification_status = false;

char ble_server_ssid_value_buffer[BLE_SERVER_BUFSIZE*2 + 2] = {'\0'};
char ble_server_pswd_value_buffer[BLE_SERVER_BUFSIZE*2 + 2] = {'\0'};
char ble_server_cert_value_buffer[BLE_SERVER_BUFSIZE*8] = {'\0'};
char ble_server_dvid_value_buffer[BLE_SERVER_BUFSIZE] = {'\0'};
char ble_server_stats_value_buffer[BLE_SERVER_BUFSIZE*16] = {'\0'};
char ble_server_logs_value_buffer[BLE_SERVER_BUFSIZE*3] = {'\0'}; //note- set to BLE_SERVER_BUFSIZE*8 or more if used to return logs
char ble_server_cust_value_buffer[BLE_SERVER_BUFSIZE*8] = {'\0'};
char ble_server_fwurl_value_buffer[BLE_SERVER_BUFSIZE] = {'\0'};

String *configuration_get_ssid();
String *configuration_get_pswd();
String *configuration_get_cert();
String *configuration_get_dvid();
String *configuration_get_cust();
//String *pstatistics_string();

//void kaidu_set_mac_address(String mac);
//void stats_set_mac_address(String mac);

unsigned char ble_server_device_status = BLE_SERVER_DEVICE_STATE_INITIALIZED; //undefined
char ble_server_device_name[BLE_SERVER_BUFSIZE] = {'\0'};
unsigned long ble_server_connect_time;

#define LED_BLUE 2
void led_solid(int led);
void led_previous();
float wifi_get_avg_wifi_rssi();
void wifi_get_next_wifi_ssid (char * ssid_buffer, int ssid_len);

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      ble_server_connection_status = true;
      ble_server_connect_time = time(nullptr);
      //
      String *config_ssid = configuration_get_ssid();
      snprintf(ble_server_ssid_value_buffer, sizeof(ble_server_ssid_value_buffer), "%s", config_ssid->c_str());
      String *config_pswd = configuration_get_pswd();
      snprintf(ble_server_pswd_value_buffer, sizeof(ble_server_pswd_value_buffer), "%s", config_pswd->c_str());
      String *config_cert = configuration_get_cert();
      snprintf(ble_server_cert_value_buffer, sizeof(ble_server_cert_value_buffer), "%s", config_cert->c_str());
      String *config_dvid = configuration_get_dvid();
      snprintf(ble_server_dvid_value_buffer, sizeof(ble_server_dvid_value_buffer), "%s", config_dvid->c_str());
      //String *stats_string = pstatistics_string();
      //snprintf(ble_server_stats_value_buffer, sizeof(ble_server_stats_value_buffer), "%s", stats_string->c_str());
      String *config_cust = configuration_get_cust();
      snprintf(ble_server_cust_value_buffer, sizeof(ble_server_cust_value_buffer), "%s", config_cust->c_str());
      //
      snprintf(ble_server_logs_value_buffer, sizeof(ble_server_logs_value_buffer), ""); //blank

      Serial.print(F("ble_server: onConnect: ssid="));
      Serial.print(ble_server_ssid_value_buffer);
      Serial.print(F(" pswd="));
      Serial.print(ble_server_pswd_value_buffer);
      Serial.print(F(" dvid="));
      Serial.print(ble_server_dvid_value_buffer);
      Serial.print(F(" cert="));
      Serial.print(ble_server_cert_value_buffer);
      Serial.print(F(" stats="));
      Serial.println(ble_server_stats_value_buffer);
      Serial.print(F(" logs="));
      Serial.print(ble_server_logs_value_buffer);
      Serial.print(F(" cust="));
      Serial.print(ble_server_cust_value_buffer);

      pCharacteristicSsid ->setValue(ble_server_ssid_value_buffer);
      pCharacteristicPswd->setValue(ble_server_pswd_value_buffer);
      pCharacteristicCert->setValue(ble_server_cert_value_buffer);
      pCharacteristicDvid->setValue(ble_server_dvid_value_buffer);
      pCharacteristicStats->setValue(ble_server_stats_value_buffer);
      pCharacteristicLogs->setValue(ble_server_logs_value_buffer);
      pCharacteristicCust->setValue(ble_server_cust_value_buffer);

      led_solid(LED_BLUE);
    };

    void onDisconnect(BLEServer* pServer) {
      Serial.println(F("ble_server: disconnect"));
      ble_server_connection_status = false;
      ble_server_log_notification_status = false;
      ble_server_connect_time = 0;
      led_previous();
    }
};


class MyCharacteristicsCallbacks: public BLECharacteristicCallbacks {
    void onNotify(BLECharacteristic *pCharacteristic) {
        Serial.println(F("ble_server: onNotify:"));
        //Serial.println(pCharacteristic->getUUID().toString().c_str());
    };
    void onRead(BLECharacteristic *pCharacteristic) {
        Serial.println(F("ble_server: onRead:"));
        //Serial.println(pCharacteristic->getUUID().toString().c_str());
        std::string uuid = pCharacteristic->getUUID().toString();
        if (uuid == CHARACTERISTIC_UUID_LOGS) {
          wifi_get_next_wifi_ssid(ble_server_logs_value_buffer, sizeof(ble_server_logs_value_buffer));
          Serial.println(ble_server_logs_value_buffer);
          pCharacteristicLogs->setValue(ble_server_logs_value_buffer);
        }
    };
    void onWrite(BLECharacteristic *pCharacteristic) {
      
      std::string uuid = pCharacteristic->getUUID().toString();
      std::string rxValue = pCharacteristic->getValue();

      if (uuid == CHARACTERISTIC_UUID_SSID) {
        Serial.print(F("ble_server: write: CHARACTERISTIC_UUID_SSID = "));
        //Serial.println(rxValue.c_str());
        //sprintf(ble_server_ssid_value_buffer, "%s", rxValue);
        snprintf(ble_server_ssid_value_buffer, sizeof(ble_server_ssid_value_buffer), "%s", rxValue);
        pCharacteristicSsid ->setValue(ble_server_ssid_value_buffer);
        configuration_write(CONFIGURATION_SSID, rxValue.c_str());
        ble_server_values_updated = true;
        Serial.println(ble_server_ssid_value_buffer);
      }
      else if (uuid == CHARACTERISTIC_UUID_PSWD) {
        Serial.print(F("ble_server: write: CHARACTERISTIC_UUID_PSWD = "));
        //Serial.println(rxValue.c_str());
        //sprintf(ble_server_pswd_value_buffer, "%s", rxValue);
        snprintf(ble_server_pswd_value_buffer, sizeof(ble_server_pswd_value_buffer), "%s", rxValue);
        pCharacteristicPswd->setValue(ble_server_pswd_value_buffer);
        configuration_write(CONFIGURATION_PSWD, rxValue.c_str());
        ble_server_values_updated = true;
        Serial.println(ble_server_pswd_value_buffer);
      }
      else if (uuid == CHARACTERISTIC_UUID_CERT) {
        Serial.print(F("ble_server: write: CONFIGURATION_MQTT_CERT = "));
        Serial.println(rxValue.c_str());
        sprintf(ble_server_cert_value_buffer, "%s", rxValue);
        pCharacteristicCert->setValue(ble_server_cert_value_buffer);
        configuration_write(CONFIGURATION_MQTT_CERT, rxValue.c_str());
        ble_server_values_updated = true;
      }
      else if (uuid == CHARACTERISTIC_UUID_DVID) {
        Serial.print(F("ble_server: write: CONFIGURATION_MQTT_DVID = "));
        Serial.println(rxValue.c_str());
        sprintf(ble_server_dvid_value_buffer, "%s", rxValue);
        pCharacteristicDvid->setValue(ble_server_dvid_value_buffer);
        configuration_write(CONFIGURATION_MQTT_DVID, rxValue.c_str());
        ble_server_values_updated = true;
      }
      else if (uuid == CHARACTERISTIC_UUID_LOGS) {
        Serial.print(F("ble_server: write: CHARACTERISTIC_UUID_LOGS = "));
        Serial.println(rxValue.c_str());
        snprintf(ble_server_logs_value_buffer, sizeof(ble_server_logs_value_buffer), "%s", rxValue.c_str());
        //pCharacteristicLogs->setValue(ble_server_logs_value_buffer);
        int rxLen = rxValue.size();
        #ifdef BLE_SERVER_ENABLE_LOGS
        if (rxLen == 3 && (rxValue == "logs" || rxValue == "LOGS")) {
          ble_server_log_notification_status = !ble_server_log_notification_status;
          ble_server_connection_status = !ble_server_connection_status;
          Serial.print(F("ble_server: write: toggle connection status to enable/disable logs"));
          ble_server_values_updated = true; // do this so that after ble disconnect, scanner will reboot
        }
        #endif
      }
      else if (uuid == CHARACTERISTIC_UUID_CUST) {
        Serial.print(F("ble_server: write: CHARACTERISTIC_UUID_CUST = "));
        Serial.println(rxValue.c_str());
        snprintf(ble_server_cust_value_buffer, sizeof(ble_server_cust_value_buffer), "%s", rxValue.c_str());
        pCharacteristicCust->setValue(ble_server_cust_value_buffer);
        configuration_write(CONFIGURATION_CUST_ID, rxValue.c_str());
        ble_server_values_updated = true;
      }
      else {
        Serial.print(F("ble_server: write: unknown uuid = "));
        Serial.print(uuid.c_str());
        Serial.print(F(", values = "));
        Serial.println(rxValue.c_str());
      }
      
      // print value to console
//      if (rxValue.length() > 0) {
//        Serial.print("Received Value: ");
//        for (int i = 0; i < rxValue.length(); i++) {
//          if (rxValue[i] > 0) Serial.print(rxValue[i]);
//        }
//        Serial.println();
//      }
//      else {
//        Serial.println("Received Value: ?");
//      }
    }
};

MyCharacteristicsCallbacks *ble_server_characteristics_callback = new MyCharacteristicsCallbacks();


/////////////////////////////////////////////////////
//
//
esp_ota_handle_t ble_server_ota_handler = 0;

class OtaCharacteristicsCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      
        std::string uuid = pCharacteristic->getUUID().toString();
        std::string rxData = pCharacteristic->getValue();

        if (uuid == CHARACTERISTIC_UUID_FW) {
          if (!ble_server_ota_updated) { //If it's the first packet of OTA since bootup, begin OTA
              Serial.println(F("BeginOTA"));
              //
              // Disable WDT - OTA can starve watchdog on plug devices
              //disableCore0WDT(); disableCore1WDT();
              // Instead of disabling WDT, change timeout value temporarily
              esp_task_wdt_init(15,0);
              //
              esp_ota_begin(esp_ota_get_next_update_partition(NULL), OTA_SIZE_UNKNOWN, &ble_server_ota_handler);
              ble_server_ota_updated = true;
              Serial.println((uint32_t) ble_server_ota_handler);
          }
          if (pServer != NULL)
          {
              if (rxData.length() > 0)
              {
                  esp_ota_write(ble_server_ota_handler, rxData.c_str(), rxData.length());
                  if (rxData.length() != BLE_SERVER_OTA_FULL_PACKET)
                  {
                      esp_ota_end(ble_server_ota_handler);
                      Serial.println(F("EndOTA"));
                      if (ESP_OK == esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL))) {
                          //delay(2000);
                          //esp_restart();
                          ble_server_ota_completed = true;
                      }
                      else {
                          Serial.println(F("Ota Upload Error"));
                          ble_server_ota_updated = false;
                      }
                      //
                      // Enable WDT - turn back ON at end of OTA
                      //enableCore0WDT(); enableCore1WDT();
                      // Return WDT timeout to initial value
                      esp_task_wdt_init(5,0);
                  }
              }
          }
          uint8_t txData[5] = {1, 2, 3, 4, 5};
          //delay(1000);
          pCharacteristic->setValue((uint8_t*)txData, 5);
          pCharacteristic->notify();
        }
        else if (uuid == CHARACTERISTIC_UUID_HW_VERSION) {
        Serial.print(F("ble_server: write: CHARACTERISTIC_UUID_HW_VERSION = "));
        Serial.println(rxData.c_str());
        snprintf(ble_server_fwurl_value_buffer, sizeof(ble_server_fwurl_value_buffer), "%s", rxData.c_str());
        //pCharacteristic->setValue(ble_server_fwurl_value_buffer);
        int rxLen = rxData.size();
        if (rxLen > 4 && rxData.substr(rxLen-4) == ".bin") {
          ble_server_values_updated = true;
          ble_server_ota_requested = true;
          Serial.println(ble_server_fwurl_value_buffer);
        }
      }
    }
};

OtaCharacteristicsCallbacks *ble_server_ota_callback = new OtaCharacteristicsCallbacks();

/////////////////////////////////////////////////////
//
//

char * ble_server_get_fw_update_url() {
  //ble_server_ota_requested = false; //removed to allow watchdog to work
  return ble_server_fwurl_value_buffer;
}

#ifdef BLE_SERVER_ENABLE_LOGS
// note set size of ble_server_fwurl_value_buffer to BLE_SERVER_BUFSIZE*8 or more
// if we are using this function
//
void ble_server_update_fwurl( const char * logs ) {
  snprintf(ble_server_fwurl_value_buffer, sizeof(ble_server_fwurl_value_buffer), "%s", logs);
  if (ble_server_log_notification_status) {
    pCharacteristicLogs->setValue(ble_server_fwurl_value_buffer);
    pCharacteristicLogs->notify();
  }
}
#endif

boolean ble_server_get_fw_update_request() {
  return ble_server_ota_requested;
}

boolean ble_server_get_updated_status() {
  return ble_server_values_updated || ble_server_ota_completed || ble_server_ota_requested;
}

boolean ble_server_get_connection_status() {
  return ble_server_connection_status;
}

unsigned long ble_server_get_connect_secs() {
  if (ble_server_connection_status && ble_server_connect_time > 0) {
    return time(nullptr) - ble_server_connect_time;
  }
  return 0;
}

void ble_server_start() {

  if (!ble_server_setup_done) {
    
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    ////////////////////////// KAIDU CONFIGS ///////////////////////////////
    
    pServiceKaidu = pServer->createService(SERVICE_UUID_KAIDU);
  
    // client to read/write ssid from this characteristic
    pCharacteristicSsid = pServiceKaidu->createCharacteristic(
                           CHARACTERISTIC_UUID_SSID,
                           NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
                          );
    //pCharacteristicSsid->addDescriptor(new BLE2902());
    pCharacteristicSsid->setValue(ble_server_ssid_value_buffer); // initial value
    pCharacteristicSsid->setCallbacks(ble_server_characteristics_callback);

    // client to read/write pswd from this characteristic
    pCharacteristicPswd = pServiceKaidu->createCharacteristic(
                           CHARACTERISTIC_UUID_PSWD,
                           NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
                          );
    //pCharacteristicPswd->addDescriptor(new BLE2902());
    pCharacteristicPswd->setValue(ble_server_pswd_value_buffer); // initial value
    pCharacteristicPswd->setCallbacks(ble_server_characteristics_callback);

    // client to read/write cert from this characteristic
    pCharacteristicCert = pServiceKaidu->createCharacteristic(
                           CHARACTERISTIC_UUID_CERT,
                           NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
                          );
    //pCharacteristicCert->addDescriptor(new BLE2902());
    pCharacteristicCert->setValue(ble_server_cert_value_buffer); // initial value
    pCharacteristicCert->setCallbacks(ble_server_characteristics_callback);

    // client to read/write device-id from this characteristic
    pCharacteristicDvid = pServiceKaidu->createCharacteristic(
                           CHARACTERISTIC_UUID_DVID,
                           NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
                          );
    //pCharacteristicDvid->addDescriptor(new BLE2902());
    pCharacteristicDvid->setValue(ble_server_dvid_value_buffer); // initial value
    pCharacteristicDvid->setCallbacks(ble_server_characteristics_callback);

    // client to read statistics from this characteristic
    pCharacteristicStats = pServiceKaidu->createCharacteristic(
                           CHARACTERISTIC_UUID_STATS,
                           NIMBLE_PROPERTY::READ
                          );
    //pCharacteristicStats->addDescriptor(new BLE2902());
    pCharacteristicStats->setValue(ble_server_stats_value_buffer); // initial value
    pCharacteristicStats->setCallbacks(ble_server_characteristics_callback);

    // client to write to enable logs this characteristic
    // currently used to read wifi APs
    // use notify-property when logging messages is implemented
    #ifdef BLE_SERVER_ENABLE_LOGS
    pCharacteristicLogs = pServiceKaidu->createCharacteristic(
                           CHARACTERISTIC_UUID_LOGS,
                           NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
                          );
    #else
    pCharacteristicLogs = pServiceKaidu->createCharacteristic(
                           CHARACTERISTIC_UUID_LOGS,
                           NIMBLE_PROPERTY::READ
                          );
    #endif
    //pCharacteristicLogs->addDescriptor(new BLE2902());
    pCharacteristicLogs->setCallbacks(ble_server_characteristics_callback);

    // client to read/write cust-id from this characteristic
    pCharacteristicCust = pServiceKaidu->createCharacteristic(
                           CHARACTERISTIC_UUID_CUST,
                           NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
                          );
    //pCharacteristicDvid->addDescriptor(new BLE2902());
    pCharacteristicCust->setValue(ble_server_cust_value_buffer); // initial value
    pCharacteristicCust->setCallbacks(ble_server_characteristics_callback);

    ////////////////////////// BLE OTA ///////////////////////////////

    pServiceOta = pServer->createService(SERVICE_UUID_OTA);

    pVersionCharacteristic = pServiceOta->createCharacteristic(
                             CHARACTERISTIC_UUID_HW_VERSION,
                             NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
                           );
    //pVersionCharacteristic->addDescriptor(new BLE2902());
    uint8_t hardwareVersion[5] = {HARDWARE_VERSION_MAJOR, HARDWARE_VERSION_MINOR, SOFTWARE_VERSION_MAJOR, SOFTWARE_VERSION_MINOR, SOFTWARE_VERSION_PATCH};
    pVersionCharacteristic->setValue((uint8_t*)hardwareVersion, 5);
    pVersionCharacteristic->setCallbacks(ble_server_ota_callback);

    pOtaCharacteristic = pServiceOta->createCharacteristic(
                           CHARACTERISTIC_UUID_FW,
                           NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::WRITE
                         );
    //pOtaCharacteristic->addDescriptor(new BLE2902());
    pOtaCharacteristic->setCallbacks(ble_server_ota_callback);
    
    //pService->start(); // moved to start func
    
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

    pAdvertising->addServiceUUID(SERVICE_UUID_KAIDU);
    pAdvertising->setScanResponse(true);
    //pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    //pAdvertising->setMaxPreferred(0x12);
    pAdvertising->setMinInterval(400);  // functions that control advert rates
    pAdvertising->setMaxInterval(800);

    // setup manufacturing data to show mac-address
    // company id for now is 0xFFFF (reserved) until we get our own
    const uint8_t* rawMacAddr = NimBLEDevice::getAddress().getNative();
    const char mac0 = rawMacAddr[5] & 0xFF;
    const char mac1 = rawMacAddr[4] & 0xFF;
    const char mac2 = rawMacAddr[3] & 0xFF;
    const char mac3 = rawMacAddr[2] & 0xFF;
    const char mac4 = rawMacAddr[1] & 0xFF;
    const char mac5 = rawMacAddr[0] & 0xFF;
    //ble_server_device_status = BLE_SERVER_DEVICE_STATE_INITIALIZED; //init
    const char devstatus = ble_server_device_status & 0xFF;
    const char wifirssi  = 0x00;
    const uint8_t manData[11] = {0xFF,0xFF,mac0,mac1,mac2,mac3,mac4,mac5,devstatus,wifirssi,'\0'}; 
    //pAdvertising->setManufacturerDataBytes(&manData[0],10);
    pAdvertising->setManufacturerData(std::string((char *)&manData[0], 10));

    //kaidu_set_mac_address(NimBLEDevice::getAddress().toString().c_str());
    
    //#ifdef TEST_CUSTOM_ADV_SCAN
    //BLEAdvertisementData customAdvertisementData = BLEAdvertisementData();
    //pAdvertising->setAdvertisementData(customAdvertisementData);
    //Serial.println(F("Set custom scan response"));
    //BLEAdvertisementData customScanResponseData = BLEAdvertisementData();
    //pAdvertising->setScanResponseData(customScanResponseData);
    //#endif
    //BLEDevice::startAdvertising();      // moved to start func
  
    ble_server_setup_done = true;
  }
  
  if (!ble_server_started) {
    pServiceKaidu->start();
    pServiceOta->start();
    BLEDevice::startAdvertising();
    ble_server_started = true;
    ble_server_connect_time = 0;
  }
}

void ble_server_update_manufacture_data_device_status(unsigned char device_status, int wifi_rssi) {
  if (ble_server_device_status != device_status) {
    //
    // update device-state
    ble_server_device_status = device_status;
    //
    // if ble-server was started, then stop, update manufacturer part, and restart
    if (ble_server_started) {
      BLEDevice::stopAdvertising();
      delay(100); //allows time to stop
      BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
      const uint8_t* rawMacAddr = NimBLEDevice::getAddress().getNative();
      const char mac0 = rawMacAddr[5] & 0xFF;
      const char mac1 = rawMacAddr[4] & 0xFF;
      const char mac2 = rawMacAddr[3] & 0xFF;
      const char mac3 = rawMacAddr[2] & 0xFF;
      const char mac4 = rawMacAddr[1] & 0xFF;
      const char mac5 = rawMacAddr[0] & 0xFF;
      const char devstatus = ble_server_device_status & 0xFF;
      const char wifirssi = (char) wifi_rssi & 0xFF;
      const uint8_t manData[11] = {0xFF,0xFF,mac0,mac1,mac2,mac3,mac4,mac5,devstatus,wifirssi,'\0'}; 
      //pAdvertising->setManufacturerDataBytes(&manData[0],10);
      pAdvertising->setManufacturerData(std::string((char *)&manData[0], 10));
      BLEDevice::startAdvertising();
      Serial.println(F("BLE Server update manufacturer data..."));
    } else {
      Serial.println(F("BLE Server update device-state..."));
    }
  }
}

void ble_server_update_device_name (const char * devicename) {
    if (strcmp(ble_server_device_name, devicename) != 0) {
      //
      strcpy(ble_server_device_name, devicename);
      //
      BLEDevice::stopAdvertising();
      delay(200); //allows time to stop
  
      // do this to get NimBLE to recreate the advert-payload with new name
      BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
      pAdvertising->setName(ble_server_device_name); //change name here
      
      const uint8_t* rawMacAddr = NimBLEDevice::getAddress().getNative();
      const char mac0 = rawMacAddr[5] & 0xFF;
      const char mac1 = rawMacAddr[4] & 0xFF;
      const char mac2 = rawMacAddr[3] & 0xFF;
      const char mac3 = rawMacAddr[2] & 0xFF;
      const char mac4 = rawMacAddr[1] & 0xFF;
      const char mac5 = rawMacAddr[0] & 0xFF;
      const char devstatus = ble_server_device_status & 0xFF;
      const char wifirssi = (char) wifi_get_avg_wifi_rssi() & 0xFF;
      const uint8_t manData[11] = {0xFF,0xFF,mac0,mac1,mac2,mac3,mac4,mac5,devstatus,wifirssi,'\0'}; 
      pAdvertising->setManufacturerData(std::string((char *)&manData[0], 10));
  
      BLEDevice::startAdvertising();
      Serial.println(F("BLE Server change device-name"));
      
    } else {
      Serial.println(F("BLE Server unchanged device-name"));
    }
}

void ble_server_stop() {
  if (ble_server_started) {
    BLEDevice::stopAdvertising();
    //pServiceKaidu->stop(); //not supported in NimBLE
    //pServiceOta->stop();   //not supported in NimBLE
    ble_server_started = false;
  }
}

void ble_server_setup(const char * devicename) {
  Serial.println(F("\nBLE Server setup..."));
  //
  strcpy(ble_server_device_name, devicename);
  //
  // note these values are sticky
  // once true, we should not change them
  ble_server_values_updated = false;
  ble_server_ota_updated = false;
  ble_server_ota_requested = false;
  ble_server_log_notification_status = false;
  //
  ble_server_device_status = BLE_SERVER_DEVICE_STATE_INITIALIZED;
  //
  String *config_ssid = configuration_get_ssid();
  snprintf(ble_server_ssid_value_buffer, sizeof(ble_server_ssid_value_buffer), "%s", config_ssid->c_str());
  String *config_pswd = configuration_get_pswd();
  snprintf(ble_server_pswd_value_buffer, sizeof(ble_server_pswd_value_buffer), "%s", config_pswd->c_str());
  String *config_cert = configuration_get_cert();
  snprintf(ble_server_cert_value_buffer, sizeof(ble_server_cert_value_buffer), "%s", config_cert->c_str());
  String *config_dvid = configuration_get_dvid();
  snprintf(ble_server_dvid_value_buffer, sizeof(ble_server_dvid_value_buffer), "%s", config_dvid->c_str());
  //String *stats_string = pstatistics_string();
  //snprintf(ble_server_stats_value_buffer, sizeof(ble_server_stats_value_buffer), "%s", stats_string->c_str());
  String *config_cust = configuration_get_cust();
  snprintf(ble_server_cust_value_buffer, sizeof(ble_server_cust_value_buffer), "%s", config_cust->c_str());
  //
  snprintf(ble_server_fwurl_value_buffer, sizeof(ble_server_fwurl_value_buffer), "");
      
  if (!ble_init_done) {
    BLEDevice::init(ble_server_device_name);
    ble_init_done = true;
  }
}

void ble_server_free() {
  BLEDevice::deinit();
  ble_init_done = false;
  ble_server_setup_done = false;
}

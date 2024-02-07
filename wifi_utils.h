//
// This file contains static methods for Wifi 
//
//

#include <esp_wifi.h>
#include <Client.h>
#include <WiFi.h>
#include "esp_sntp.h"

#define WIFI_SHOW_STATS 1   // calculate wifi stattistics, can disable for production
#define WIFI_SHOW_LOGS 1
#define WIFI_SHOW_WARNING_LOGS 1

//#define WIFI_MODE_DEV 1     // uses alternate kaidu-hub password (for testing only)
//#define WIFI_MODE_DEVTEST 1 // disables kaidu-hub and deepwifi, forces to customer-wifi (for testing only)

// Wifi network details.
#define WIFI_INITIAL_SSID "SSID"
#define WIFI_INITIAL_PSWD "password"
#define WIFI_TIME_MIN_START 1696132800 // Oct 1, 2023 (EDT)
#define WIFI_CHAR_ARRAY_SIZE 64

#define WIFI_PRESET_WIFI_NUM 2
#ifdef WIFI_MODE_DEV
#ifdef WIFI_MODE_DEVTEST
// devtest mode, set presets incorrectly, forces scanner to try customer-wifi
String wifi_preset_ssid[WIFI_PRESET_WIFI_NUM] = {"BADWIFI","BAD-WIFI"};
String wifi_preset_pswd[WIFI_PRESET_WIFI_NUM] = {"badpassword","bad-password"};
#else
// dev mode, bypass kaidu-hub by setting the wrong password
String wifi_preset_ssid[WIFI_PRESET_WIFI_NUM] = {"BADWIFI","BAD-WIFI"};
String wifi_preset_pswd[WIFI_PRESET_WIFI_NUM] = {"","bad-password"};
#endif //DEVTEST
#else
// normal mode, setup kaidu-hub and deepwifi presets correctly
String wifi_preset_ssid[WIFI_PRESET_WIFI_NUM] = {"BADWIFI","BAD-WIFI"};
String wifi_preset_pswd[WIFI_PRESET_WIFI_NUM] = {"","bad-password"};
#endif
bool wifi_preset_scan[WIFI_PRESET_WIFI_NUM] = {false,false};

#define WIFI_RETRY_ORIGINAL_CREDS 1 // number of times to retry original credentials before trying presets
#ifdef KAIDU_CONFIG_SETUP_TEST
#define WIFI_CONNECT_RETRY_THRESHOLD (WIFI_PRESET_WIFI_NUM+2)*WIFI_RETRY_ORIGINAL_CREDS   // try one-round only (no retries)
#else
#define WIFI_CONNECT_RETRY_THRESHOLD (WIFI_PRESET_WIFI_NUM+2)*WIFI_RETRY_ORIGINAL_CREDS*2 // try two-rounds
#endif
#define WIFI_CONNECT_WAIT_TIMEOUT_SECS 20 // how long to wait for the wifi to connect

#define WIFI_FLOAT_MS_WEIGHT 0.5

String wifi_ssid;
String wifi_pswd;
String wifi_mac_address;
String wifi_connected_ssid;

bool wifi_use_original;
bool wifi_offline_mode = false;

// Configuration for NTP
const char* ntp_primary = "pool.ntp.org";
const char* ntp_secondary = "time.nist.gov";

// keep state
bool wifi_ntp_status = false;
bool wifi_ssid_status = false;
bool wifi_connected_status = false;
float wifi_avg_rssi = -60.0;
unsigned int wifi_num_ssids = 0;
//unsigned int wifi_reconnect_count = 0;
//unsigned int wifi_disconnect_count = 0;
unsigned int wifi_next_ssid_pointer = 0;
unsigned int wifi_fail_connect_count = 0;

//
// get wifi events here
//
static void wifi_event (WiFiEvent_t event)
{
  switch (event) {
    case SYSTEM_EVENT_STA_CONNECTED:
      #ifdef WIFI_SHOW_STATS
      if (!wifi_connected_status) {
        Serial.println(F("WIFI: SYSTEM_EVENT_STA_CONNECTED"));
        Serial.print(F("Free heap ")); Serial.println(ESP.getFreeHeap());
      }
      #endif
      wifi_connected_status = true;
      break;
      
    case SYSTEM_EVENT_STA_DISCONNECTED:
      #ifdef WIFI_SHOW_STATS
      if (wifi_connected_status) {
        Serial.println(F("WIFI: SYSTEM_EVENT_STA_DISCONNECTED"));
      }
      #endif
      wifi_connected_status = false;
      //wifi_disconnect_count++;
      break;
      
    default: 
      #ifdef WIFI_SHOW_STATS
      Serial.printf("WIFI: status %d \n", event); //normally some pre-connection state, like got-IP
      #endif
      break;
  }
}

//
//
//
void ntp_event(struct timeval *t)
{
  wifi_ntp_status = true;
  #ifdef WIFI_SHOW_STATS
  Serial.println(F("WIFI: ntp_event"));
  sntp_sync_status_t syncStatus = sntp_get_sync_status();
  switch (syncStatus) {

    case SNTP_SYNC_STATUS_COMPLETED:
      #ifdef WIFI_SHOW_LOGS
      Serial.println(F("SNTP_SYNC_STATUS_COMPLETED"));
      #endif
      break;

    #ifdef WIFI_SHOW_LOGS
    case SNTP_SYNC_STATUS_RESET:
      Serial.println(F("SNTP_SYNC_STATUS_RESET"));
      break;

    case SNTP_SYNC_STATUS_IN_PROGRESS:
      Serial.println(F("SNTP_SYNC_STATUS_IN_PROGRESS"));
      break;
    #endif

    default:
      #ifdef WIFI_SHOW_WARNING_LOGS
      Serial.println(F("Unknown Sync Status"));
      #endif
      break;
  }
  #endif
}


//void wifi_set_mac_address (String mac) {
//  wifi_mac_address = mac;
//}

// don't allow wifi to work, and di
void wifi_force_offline_mode (bool status) {
  wifi_offline_mode = status;
  if (wifi_offline_mode) {
    #ifdef WIFI_SHOW_LOGS
    Serial.println(F("Wifi offline!"));
    #endif
    if (wifi_connected_status) {
      WiFi.disconnect();// force disconnect
      wifi_connected_status = false;
    }
  }
}

void wifi_set_wifi_ssid_pswd (String ssid, String pswd) {
  wifi_ssid = ssid;
  wifi_pswd = pswd;
}

bool wifi_get_connected_status () {
  if (wifi_offline_mode && wifi_connected_status) {
    WiFi.disconnect(); // force disconnect
    wifi_connected_status = false;
  }
  return wifi_connected_status;
}

unsigned int wifi_get_fail_connect_count() {
  unsigned int temp = wifi_fail_connect_count;
  wifi_fail_connect_count = 0;
  return temp;
}

// is ssid in the list of AP detected?
bool wifi_get_ssid_status () {
  bool found_wifi = wifi_ssid_status; // customer
  for (int j=0; j<WIFI_PRESET_WIFI_NUM; j++) {
    found_wifi = found_wifi || wifi_preset_scan[j]; // preset
  }
  return wifi_ssid_status;
}

void wifi_get_next_wifi_ssid (char * ssid_buffer, int ssid_len) {
  // clear the buffer
  memset(ssid_buffer, 0, ssid_len);
  //
  if (wifi_num_ssids > 0) {
    if (wifi_next_ssid_pointer >= wifi_num_ssids) {
      wifi_next_ssid_pointer = 0;
    }
    // copy ssid and rssi into buffer
    snprintf(ssid_buffer, ssid_len, "%s\t%d", WiFi.SSID(wifi_next_ssid_pointer).c_str(), WiFi.RSSI(wifi_next_ssid_pointer));
    wifi_next_ssid_pointer += 1;
  }
}

float wifi_get_avg_wifi_rssi() {
  return wifi_avg_rssi;
}

String wifi_get_ssid() {
  return wifi_connected_ssid;
}

bool wifi_connect(bool scan_ssids_flag) {
  
  int count = 0;
  int retry_count = 0;
  unsigned int wifi_attempt = 0;

  #ifdef WIFI_MODE_DEV
  #ifdef WIFI_MODE_DEVTEST
  Serial.println(F("Connect WiFi in DEVTEST mode!"));
  #else
  Serial.println(F("Connect WiFi in DEV mode!"));
  #endif
  #else
  Serial.println(F("Connect WiFi"));
  #endif

  if (wifi_offline_mode) {
    #ifdef WIFI_SHOW_LOGS
    Serial.println(F("Wifi offline!"));
    #endif
    return false;
  }

  //wifi_start_connecting_flag = true;

  //if (wifi_ssid == NULL || wifi_ssid == "") {
  //  // if empty, then use presets
  //  wifi_ssid = wifi_preset_ssid[0];
  //  wifi_pswd = wifi_preset_pswd[0];
  //}

  //Serial.print(F("Credentials: "));
  //Serial.print(wifi_ssid);
  //Serial.print(F(" / "));
  //Serial.println(wifi_pswd);

  if (scan_ssids_flag) {
    // collect ssid names
    // check if ssid in the list of AP, repeat twice if necessary
    wifi_num_ssids = 0; //scan wifi APs, and return number of AP’s seen
    wifi_next_ssid_pointer = 0; // we are reusing this variable to hold the pointer to the next AP for ble-server to read AP list
    wifi_ssid_status = false; // did not see the AP-ssid we want
    while(wifi_ssid_status == false && count < WIFI_PRESET_WIFI_NUM+1) {
      count += 1;
      #ifdef WIFI_SHOW_LOGS
      Serial.println(F("SSID scanning"));
      #endif
      wifi_num_ssids = WiFi.scanNetworks(); //scan wifi APs, and return number of AP’s seen
      if (wifi_num_ssids > 256) {
        wifi_num_ssids = 0; //this sometimes happen WIFI in bad-state?
      } else {
        Serial.print("# Wi-Fi Seen: ");
        Serial.println(wifi_num_ssids);
        //Serial.println(F("SSID parsing"));
        for (int i=0; i< wifi_num_ssids; i++) {
          if (WiFi.SSID(i).length() > 0 && wifi_ssid.length() == WiFi.SSID(i).length() && 
              wifi_ssid == WiFi.SSID(i)) {
            wifi_ssid_status = true;
            #ifdef WIFI_SHOW_LOGS
            Serial.println(F("Customer SSID found in WiFi scan"));
            #endif
            wifi_avg_rssi = WiFi.RSSI(i); // initial value
            count += 1;
            break;
          }
        }
        // check if presets are present
        for (int j=0; j<WIFI_PRESET_WIFI_NUM; j++) {
          for (int i=0; i< wifi_num_ssids; i++) {
            if (WiFi.SSID(i).length() > 0 && wifi_preset_ssid[j].length() == WiFi.SSID(i).length() && 
                wifi_preset_ssid[j] == WiFi.SSID(i)) {
              wifi_preset_scan[j] = true;
              #ifdef WIFI_SHOW_LOGS
              Serial.println(F("SSID preset found in WiFi scan"));
              #endif
              wifi_avg_rssi = WiFi.RSSI(i); // initial value
              count += 1;
              break;
            }
          }
        }
      }
    }
  }

  wifi_use_original = true;
  retry_count = 0;
  while (wifi_connected_status == false && retry_count < WIFI_CONNECT_RETRY_THRESHOLD && !wifi_offline_mode) {
    ++retry_count; // increment our local retry counter
    //++wifi_reconnect_count; // total retry counter
    
    // determine SSID to use
    wifi_attempt = (retry_count-1) % (WIFI_PRESET_WIFI_NUM+WIFI_RETRY_ORIGINAL_CREDS);
    if (wifi_attempt == 0) {
      // try first preset aka KaiduHub
      if (wifi_preset_scan[0]) {
        #ifdef WIFI_SHOW_LOGS
        Serial.print(F("Using KaiduHub WiFi: "));
        Serial.print(wifi_preset_ssid[0]);
        Serial.print(F(" / "));
        Serial.println(wifi_preset_pswd[0]);
        #endif
        WiFi.begin(wifi_preset_ssid[0].c_str(), wifi_preset_pswd[0].c_str());
        wifi_use_original = false;
        wifi_connected_ssid = wifi_preset_ssid[0];
      } else {
        #ifdef WIFI_SHOW_WARNING_LOGS
        Serial.println(F("Missing KaiduHub WiFi!"));
        #endif
        continue;
      }
                   
    } else if (wifi_attempt <= WIFI_RETRY_ORIGINAL_CREDS) {
      // try customer wifi
      if (wifi_ssid_status) {
        #ifdef WIFI_SHOW_LOGS
        Serial.print(F("Using customer WiFi: "));
        #endif
        if (wifi_pswd == NULL || wifi_pswd == "") {
          #ifdef WIFI_SHOW_LOGS
          Serial.print(wifi_ssid); Serial.println(F(" / <blank>"));
          #endif
          WiFi.begin(wifi_ssid.c_str());
        } else {
          #ifdef WIFI_SHOW_LOGS
          Serial.print(wifi_ssid); Serial.print(F(" / ")); Serial.println(wifi_pswd);
          #endif
          WiFi.begin(wifi_ssid.c_str(), wifi_pswd.c_str());
        }
        wifi_use_original = true;
        wifi_connected_ssid = wifi_ssid;
      } else {
        #ifdef WIFI_SHOW_WARNING_LOGS
        Serial.println(F("Missing customer WiFi!"));
        #endif
        continue;
      }
      
    } else {
      // try other presets
      if (wifi_preset_scan[wifi_attempt-WIFI_RETRY_ORIGINAL_CREDS]) {
        #ifdef WIFI_SHOW_LOGS
        Serial.print(F("Using preset WiFi: "));
        Serial.print(wifi_preset_ssid[wifi_attempt-WIFI_RETRY_ORIGINAL_CREDS]);
        Serial.print(F(" / "));
        Serial.println(wifi_preset_pswd[wifi_attempt-WIFI_RETRY_ORIGINAL_CREDS]);
        #endif
        WiFi.begin(wifi_preset_ssid[wifi_attempt-WIFI_RETRY_ORIGINAL_CREDS].c_str(), 
                   wifi_preset_pswd[wifi_attempt-WIFI_RETRY_ORIGINAL_CREDS].c_str());
        wifi_connected_ssid = wifi_preset_ssid[wifi_attempt-WIFI_RETRY_ORIGINAL_CREDS];
      } else {
        #ifdef WIFI_SHOW_WARNING_LOGS
        Serial.printf("Missing %s \n", wifi_preset_ssid[wifi_attempt-WIFI_RETRY_ORIGINAL_CREDS]);
        #endif
        continue;
      }
    }
    
    wifi_connected_status = false;
    #ifdef WIFI_SHOW_LOGS
    Serial.println(F("Connecting to WiFi..."));
    #endif
    count = 0;
    while (WiFi.status() != WL_CONNECTED && count++ < WIFI_CONNECT_WAIT_TIMEOUT_SECS && !wifi_offline_mode) {
      #ifdef WIFI_SHOW_LOGS
      Serial.print(F("."));
      #endif
      delay(1000);
    }
  
    if (WiFi.status() == WL_CONNECTED) {
      // free ssid list
      //wifi_num_ssids = 0;
      //WiFi.scanDelete();

      led_blink_fast(LED_BLUE, false); //show that we are trying to connect to internet by getting time
      //
      wifi_connected_status = true;
      //
      wifi_avg_rssi = WiFi.RSSI();
      //
      wifi_ntp_status = false;
      configTime(0, 0, ntp_primary, ntp_secondary);    // get time from NTP server
      sntp_set_time_sync_notification_cb( ntp_event ); // set callback for time sync
      sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);        // sync time right away
      //
      #ifdef WIFI_SHOW_LOGS
      Serial.println(F("Wifi connected, waiting on time sync..."));
      #endif
      //
      count = 0;
      while ((time(nullptr) < WIFI_TIME_MIN_START || !wifi_ntp_status) && count++ < WIFI_CONNECT_WAIT_TIMEOUT_SECS && !wifi_offline_mode) {
        #ifdef WIFI_SHOW_LOGS
        Serial.print(F("."));
        #endif
        delay(1000);
      }
      if (time(nullptr) < WIFI_TIME_MIN_START || !wifi_ntp_status) {
        #ifdef WIFI_SHOW_LOGS
        Serial.println(F("Time sync failed, disconnect WiFi!"));
        #endif
        WiFi.disconnect();
        count = 0;
        while ((WiFi.status() == WL_CONNECTED || wifi_connected_status) && count++ < WIFI_CONNECT_WAIT_TIMEOUT_SECS) {
          delay(1000); // give time for wifi to change state
        }
        wifi_connected_status = false;
        ++wifi_fail_connect_count;
      }
      
    } else {
      #ifdef WIFI_SHOW_LOGS
      Serial.println(F("Wifi did not connect!"));
      #endif
      //WiFi.disconnect(true, true);
      WiFi.disconnect();
      //
      if (wifi_use_original) {
        // if original wifi credentials did not work, use presets
        wifi_use_original = false;
      }
      //
      ++wifi_fail_connect_count;
      wifi_connected_ssid.remove(0);
    }
  }
  #ifdef WIFI_SHOW_LOGS
  Serial.println(F("Setup wifi...DONE!"));
  #endif
  //wifi_start_connecting_flag = false;
  return wifi_connected_status;
}

void wifi_disconnect() {
  int count = 0;
  #ifdef WIFI_SHOW_LOGS
  Serial.println(F("Wifi disconnect!"));
  #endif
  //WiFi.disconnect(true, true);
  WiFi.disconnect();
  while ((WiFi.status() == WL_CONNECTED || wifi_connected_status) && count++ < WIFI_CONNECT_WAIT_TIMEOUT_SECS) {
    delay(1000); // give time for wifi to change state
  }
  WiFi.mode(WIFI_OFF);
  wifi_connected_status = false;
}

void wifi_setup() {

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("Kaidu");
  //WiFi.setSleep(true); // allow wifi to sleep to save power
  WiFi.onEvent( wifi_event );
  //
  //esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
  //esp_wifi_set_ps(WIFI_PS_NONE);
  //esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  //
  wifi_ssid.reserve(WIFI_CHAR_ARRAY_SIZE);
  wifi_pswd.reserve(WIFI_CHAR_ARRAY_SIZE);
  wifi_connected_ssid.reserve(WIFI_CHAR_ARRAY_SIZE);
  //
  //wifi_ssid = wifi_preset_ssid[0]; //WIFI_INITIAL_SSID;
  //wifi_pswd = wifi_preset_pswd[0]; //WIFI_INITIAL_PSWD;
  //
  wifi_fail_connect_count = 0;
}

void wifi_free() {
  wifi_ssid = String();
  wifi_pswd = String();
  wifi_connected_ssid = String();
}

void wifi_reset() {
  WiFi.disconnect();
  //WiFi.resetSettings(); //does not work
  //
  //wifi_reconnect_count = 0;
  //wifi_disconnect_count = 0;
  wifi_fail_connect_count = 0;
}

void wifi_loop() {
  //
  // run a small connection house-keeping loop
  //
  if (wifi_connected_status) {
    wifi_avg_rssi = WIFI_FLOAT_MS_WEIGHT * wifi_avg_rssi + (1.0-WIFI_FLOAT_MS_WEIGHT) * WiFi.RSSI();
  }
}

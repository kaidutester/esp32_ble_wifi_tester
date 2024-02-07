//
// Config-Server routines
//
// TODO- retry with increasing delay
//

#include <HTTPClient.h>

#define KAIDU_CONFIG_SERVER_DEV 1 // dev config-server, instead of production (for testing only)
//#define KAIDU_CONFIG_SERVER_FAKE_MAC 1 // fake-mac, so that there is no config-setup (for testing only)
#define KAIDU_CMD_SHOW_LOGS 1

// note: try direct-url first, then try server-url
#ifdef KAIDU_CONFIG_SERVER_DEV
#define KAIDU_CONFIG_SERVER_URL String(F("http://kaidu-configserver-devtest2.azurewebsites.net/")) //String(F("http://kaidu-pc.local:8000/")) //String(F("http://192.168.2.111:3000/"))
#define KAIDU_CONFIG_SERVER_DIRECT1 String(F("http://kaidu-configserver-devtest1.azurewebsites.net/")) //default-dev
#else
#define KAIDU_CONFIG_SERVER_URL String(F("http://kaidu-configserver-prod1.azurewebsites.net/")) //String(F("http://kaidu-pc.local:8000/")) //kaidu-hub
#define KAIDU_CONFIG_SERVER_DIRECT1 String(F("http://kaidu-config1.deeppixel.ai/")) //default-prod
#endif

#define KAIDU_CMD_MAX_COUNT 3 // retry http command this many times
#define KAIDU_CMD_TIMEOUT_MILLIS 30000 // each http routine can only take this long to run
//#define KAIDU_CMD_DELAY_MILLIS 100 // pause at least this long in between http tests (unused)

#define KAIDU_FLOAT_MS_WEIGHT 0.1

bool kaidu_config_server_url_choice = false; //selects the ip-address url
bool kaidu_http_last_status = false; //status of the last http request
float kaidu_avg_http_send_ms = 0.0;
unsigned int kaidu_http_success_count = 0;
unsigned int kaidu_http_error_count = 0;
unsigned int kaidu_http_send_count = 0;

bool kaidu_rx_fw_update_cmd_status = false;
//bool kaidu_rx_fw_update_url_status = false;
bool kaidu_rx_stop_cmd_status = false;
bool kaidu_rx_reset_cmd_status = false;
bool kaidu_rx_dfu_cmd_status = false;

char kaidu_configsetup[48] = {'\0'};
char kaidu_fw_name[48] = {'\0'};
char kaidu_device_id[24] = {'\0'};
char kaidu_nrf_version[24] = {'\0'};
String kaidu_mac_address;

//unsigned int serial_uart_get_sent_count();
//unsigned int serial_uart_get_timeout_count();

bool kaidu_cmd_find_config_server_url() {
  #ifdef KAIDU_CONFIG_SERVER_DEV
  Serial.println(F("Config-Server DEV settings!"));
  #endif
  #ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(F("Testing config-server..."));
  #endif
  String serverPath = String();
  int httpResponseCode = 0;
  HTTPClient http;
  bool done = false;
  unsigned long timeout = millis() + KAIDU_CMD_TIMEOUT_MILLIS;
  unsigned long startms = 0;
  kaidu_http_last_status = false;
  http.setReuse(true);
  for(int i=0; i<KAIDU_CMD_MAX_COUNT && !done && millis() < timeout && wifi_get_connected_status(); i++) {
    // try config-server without port number port number
    serverPath = KAIDU_CONFIG_SERVER_DIRECT1; //KAIDU_CONFIG_SERVER_URL; //
    startms = millis();
    http.begin(serverPath.c_str());
    //http.setTimeout(2);
    //http.setConnectTimeout(2000);
    //Serial.println(F("...before get#1")); Serial.flush();
    httpResponseCode = http.GET();
    //Serial.println(httpResponseCode); Serial.flush();
    //http.end();
    kaidu_avg_http_send_ms = (KAIDU_FLOAT_MS_WEIGHT * (float)(millis()-startms)) + ((1.0-KAIDU_FLOAT_MS_WEIGHT) * kaidu_avg_http_send_ms); //kaidu_update_avg_http_send(startms);
    if (httpResponseCode >= 200 && httpResponseCode < 300) {
      kaidu_http_last_status = true;
      done = true;
      kaidu_config_server_url_choice = false; //true; //
      //#ifdef KAIDU_CMD_SHOW_LOGS
      Serial.println(serverPath); //Serial.flush();
      //#endif
      return true;
    } else {
      // try config-server with port number
      kaidu_http_last_status = false;    
      serverPath = KAIDU_CONFIG_SERVER_URL; //KAIDU_CONFIG_SERVER_DIRECT1; //
      //delay(1000);
      startms = millis();
      http.begin(serverPath.c_str());
      //http.setTimeout(2);
      //http.setConnectTimeout(2000);
      //Serial.println(F("...before get#2"));
      httpResponseCode = http.GET();
      //Serial.println(httpResponseCode); Serial.flush();
      //http.end();
      kaidu_avg_http_send_ms = (KAIDU_FLOAT_MS_WEIGHT * (float)(millis()-startms)) + ((1.0-KAIDU_FLOAT_MS_WEIGHT) * kaidu_avg_http_send_ms); //kaidu_update_avg_http_send(startms);
      if (httpResponseCode >= 200 && httpResponseCode < 300) {
        kaidu_http_last_status = true;
        done = true;
        kaidu_config_server_url_choice = true; //false; //
        //#ifdef KAIDU_CMD_SHOW_LOGS
        Serial.println(serverPath); //Serial.flush();
        //#endif
        return true;
      } else {
        #ifdef KAIDU_CMD_SHOW_LOGS
        Serial.printf("...not found, i=%d \n", i); //Serial.flush();
        #endif
        //delay(1000);
      }
    }
  }
  http.end();
  if (!done) {
    #ifdef KAIDU_CMD_SHOW_LOGS
    Serial.println(F("failed!")); //Serial.flush();
    #endif
    kaidu_config_server_url_choice = false;
  }
  Serial.println(kaidu_config_server_url_choice); Serial.flush();
  return false;
}

bool kaidu_scanner_get_config() {
  #ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(F("SCANNER CONFIG"));
  #endif
  String payload;
  //int tempcount = 0;
  int httpResponseCode = 0;
  HTTPClient http;
  unsigned long timeout = millis() + KAIDU_CMD_TIMEOUT_MILLIS;
  unsigned long startms = 0;
  
  String serverPath = String();
  serverPath.reserve(128);
  
  // try getting mqtt-configuration
  httpResponseCode = 0; // init to non-success
  kaidu_http_last_status = false;
  //
  if (kaidu_config_server_url_choice) {
    serverPath = KAIDU_CONFIG_SERVER_URL;
  } else {
    serverPath = KAIDU_CONFIG_SERVER_DIRECT1;
  }
  serverPath.concat(F("kaidu_device_configuration/configstring/"));
  serverPath.concat(kaidu_mac_address);
  #ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(serverPath);
  #endif
  http.begin(serverPath.c_str());
  for(int i=0; i<KAIDU_CMD_MAX_COUNT && (httpResponseCode < 200 || httpResponseCode > 299) && millis() < timeout && wifi_get_connected_status(); i++) {
    startms = millis();
    httpResponseCode = http.GET();
    kaidu_avg_http_send_ms = (KAIDU_FLOAT_MS_WEIGHT * (float)(millis()-startms)) + ((1.0-KAIDU_FLOAT_MS_WEIGHT) * kaidu_avg_http_send_ms); //kaidu_update_avg_http_send(startms);
    ++kaidu_http_send_count;
    if (httpResponseCode > 199 && httpResponseCode < 300) {
      kaidu_http_last_status = true;
      ++kaidu_http_success_count;
      // get configuration from mac
      payload = http.getString();
      #ifdef KAIDU_CMD_SHOW_LOGS
      Serial.println(payload);
      #endif
      //
      // parse data, especially mqtt-device-id
      //tempcount = 0;
      memset(kaidu_device_id, 0, sizeof(kaidu_device_id));
      for(int j=0; j<payload.length() && payload.charAt(j) != 0x09; j++) {
        kaidu_device_id[j] = payload.charAt(j);
        //++tempcount;
      }
      Serial.println(kaidu_device_id);
      //
      http.end();
      return true;
      
    } else {
      kaidu_http_last_status = false;
      ++kaidu_http_error_count;
      #ifdef KAIDU_CMD_SHOW_LOGS
      Serial.println(F("kaidu_scanner_get_config: bad http response code!"));
      #endif
    }
  }
  http.end();
  return false;
}

bool kaidu_scanner_get_setup() {
  #ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(F("SCANNER SETUP"));
  #endif
  String payload;
  //int tempcount = 0;
  int httpResponseCode = 0;
  WiFiClient client;
  HTTPClient http;
  unsigned long timeout = millis() + KAIDU_CMD_TIMEOUT_MILLIS;
  unsigned long startms = 0;
  
  String serverPath = String();
  serverPath.reserve(128);
  
  // try getting configuration-setup
  memset(kaidu_configsetup, 0, sizeof(kaidu_configsetup));
  httpResponseCode = 0; // init to non-success
  kaidu_http_last_status = false;
  //
  if (kaidu_config_server_url_choice) {
    serverPath = KAIDU_CONFIG_SERVER_URL;
  } else {
    serverPath = KAIDU_CONFIG_SERVER_DIRECT1;
  }
  serverPath.concat(F("kaidu_device_configuration/configsetup/"));
  serverPath.concat(kaidu_mac_address);
  #ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(serverPath);
  #endif
  http.begin(serverPath.c_str());
  for(int i=0; i<KAIDU_CMD_MAX_COUNT && (httpResponseCode < 200 || httpResponseCode > 299) && millis() < timeout && wifi_get_connected_status(); i++) {
    startms = millis();
    httpResponseCode = http.GET();
    kaidu_avg_http_send_ms = (KAIDU_FLOAT_MS_WEIGHT * (float)(millis()-startms)) + ((1.0-KAIDU_FLOAT_MS_WEIGHT) * kaidu_avg_http_send_ms); //kaidu_update_avg_http_send(startms);
    ++kaidu_http_send_count;
    if (httpResponseCode > 199 && httpResponseCode < 300) {
      kaidu_http_last_status = true;
      ++kaidu_http_success_count;
      // get configuration from mac
      payload = http.getString();
      #ifdef KAIDU_CMD_SHOW_LOGS
      Serial.println(payload);
      #endif
      // store config instead of parsing
      memcpy(kaidu_configsetup, payload.c_str(), payload.length());
      //
      http.end();
      return true;
      
    } else {
      kaidu_http_last_status = false;
      ++kaidu_http_error_count;
      #ifdef KAIDU_CMD_SHOW_LOGS
      Serial.println(F("kaidu_scanner_get_setup: bad http response code!"));
      #endif
    }
  }
  http.end();
  //XXXTODO- assume 24hr if we got here
  return false;
}

int kaidu_configsetup_datalen() {
  return strlen(kaidu_configsetup);
}

bool kaidu_configsetup_realtime_flag() {
  if (kaidu_configsetup[0]=='1') return true;
  else return false;
}

unsigned long kaidu_configsetup_start_ts() {
  int tab_start = 0, tab_end = 0;
  char start_str[12] = {0};
  for(int i=0; i<strlen(kaidu_configsetup) && kaidu_configsetup[i]!='\t'; i++) {
    tab_start = i;
  }
  tab_start += 2;
  for(int i=tab_start; i<strlen(kaidu_configsetup) && kaidu_configsetup[i]!='\t'; i++) {
    tab_end = i;
  }
  tab_end += 1;
  memcpy(start_str, kaidu_configsetup+tab_start, tab_end-tab_start);
  //Serial.println(start_str);
  return atol(start_str);
}

unsigned long kaidu_configsetup_end_ts() {
  int tab_start = 0, tab_end = 0;
  char end_str[12] = {0};
  //
  // skip to second tab
  for(int j=0; j<2; j++) {
    for(int i=tab_end; i<strlen(kaidu_configsetup) && kaidu_configsetup[i]!='\t'; i++) {
      tab_end = i;
    }
    tab_end += 2;
  }
  tab_start = tab_end;
  for(int i=tab_start; i<strlen(kaidu_configsetup) && kaidu_configsetup[i]!='\t'; i++) {
    tab_end = i;
  }
  tab_end += 1;
  memcpy(end_str, kaidu_configsetup+tab_start, tab_end-tab_start);
  //Serial.println(end_str);
  return atol(end_str);
}

unsigned long kaidu_configsetup_midnight_ts() {
  int tab_start = 0, tab_end = 0;
  char end_str[12] = {0};
  //
  // skip to third tab
  for(int j=0; j<3; j++) {
    for(int i=tab_end; i<strlen(kaidu_configsetup) && kaidu_configsetup[i]!='\t'; i++) {
      tab_end = i;
    }
    tab_end += 2;
  }
  tab_start = tab_end;
  for(int i=tab_start; i<strlen(kaidu_configsetup) && kaidu_configsetup[i]!='\t'; i++) {
    tab_end = i;
  }
  tab_end += 1;
  memcpy(end_str, kaidu_configsetup+tab_start, tab_end-tab_start);
  //Serial.println(end_str);
  return atol(end_str);
}

int kaidu_configsetup_rssi_threshold() {
  int tab_start = 0, tab_end = 0;
  char end_str[12] = {0};
  //
  // skip to fourth tab
  for(int j=0; j<4; j++) {
    for(int i=tab_end; i<strlen(kaidu_configsetup) && kaidu_configsetup[i]!='\t'; i++) {
      tab_end = i;
    }
    tab_end += 2;
  }
  tab_start = tab_end;
  for(int i=tab_start; i<strlen(kaidu_configsetup) && kaidu_configsetup[i]!='\t'; i++) {
    tab_end = i;
  }
  tab_end += 1;
  memcpy(end_str, kaidu_configsetup+tab_start, tab_end-tab_start);
  Serial.printf("rssi: %s\n", end_str);
  return atoi(end_str);
}

int kaidu_configsetup_passerby_threshold() {
  int tab_start = 0, tab_end = 0;
  char end_str[12] = {0};
  //
  // skip to sixth tab
  for(int j=0; j<6; j++) {
    for(int i=tab_end; i<strlen(kaidu_configsetup) && kaidu_configsetup[i]!='\t'; i++) {
      tab_end = i;
    }
    tab_end += 2;
  }
  tab_start = tab_end;
  for(int i=tab_start; i<strlen(kaidu_configsetup) && kaidu_configsetup[i]!='\t'; i++) {
    tab_end = i;
  }
  tab_end += 1;
  memcpy(end_str, kaidu_configsetup+tab_start, tab_end-tab_start);
  Serial.printf("passerby: %s\n", end_str);
  return atoi(end_str);
}


bool kaidu_check_firmware_update() {
  #ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(F("ESP Firmware update check"));
  //int tempcount;
  #endif
  String serverPath = String();
  String payload;
  //boolean teststatus = true;
  int httpResponseCode;
  HTTPClient http;
  unsigned long timeout = millis() + KAIDU_CMD_TIMEOUT_MILLIS;
  unsigned long startms = 0;

  if (kaidu_config_server_url_choice) {
    serverPath = KAIDU_CONFIG_SERVER_URL;
  } else {
    serverPath = KAIDU_CONFIG_SERVER_DIRECT1;
  }
  serverPath.concat(F("kaidu_device_configuration/firmwareupdate/"));
  serverPath.concat(kaidu_mac_address);
  serverPath.concat("/");
  serverPath.concat(VERSION_STRING);
  #ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(serverPath);
  #endif
  kaidu_http_last_status = false;
  http.begin(serverPath.c_str());
  //tempcount = 0;
  for(int i=0; i<KAIDU_CMD_MAX_COUNT && (httpResponseCode < 200 || httpResponseCode > 299) && millis() < timeout && wifi_get_connected_status(); i++) {
    startms = millis();
    httpResponseCode = http.GET();
    kaidu_avg_http_send_ms = (KAIDU_FLOAT_MS_WEIGHT * (float)(millis()-startms)) + ((1.0-KAIDU_FLOAT_MS_WEIGHT) * kaidu_avg_http_send_ms); //kaidu_update_avg_http_send(startms);
    ++kaidu_http_send_count;
    //#ifdef KAIDU_CMD_SHOW_LOGS
    //++tempcount;
    //#endif
    if (httpResponseCode > 199 && httpResponseCode < 300) {
      kaidu_http_last_status = true;
      ++kaidu_http_success_count;
      //check payload, eg. "v03.000.bin"
      payload = http.getString();
      Serial.println(payload);
      if(payload.length() > 0) {
        if (payload.endsWith(".bin")) {
          kaidu_rx_fw_update_cmd_status = true;
          kaidu_rx_dfu_cmd_status = false; // do this because we are sharing kaidu-fw-name
          snprintf(kaidu_fw_name, sizeof(kaidu_fw_name), "%s", payload.c_str());
          //Serial.println(F("FIRMWARE check: found update!"));
          #ifdef KAIDU_CMD_SHOW_LOGS
          Serial.println(kaidu_fw_name);
          #endif
        }
      }
      return true;
    } else {
      kaidu_http_last_status = false;
      ++kaidu_http_error_count;
      #ifdef KAIDU_CMD_SHOW_LOGS
      Serial.println(F("kaidu_check_firmware_update: bad http response code!"));
      #endif
      //teststatus = false;
    }
  }
  #ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(F("api-error"));
  #endif
  return false;
}

bool kaidu_check_slave_firmware_update() {
  #ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(F("NRF Firmware update check"));
  //int tempcount;
  #endif
  String serverPath = String();
  String payload;
  //boolean teststatus = true;
  int httpResponseCode;
  HTTPClient http;
  unsigned long timeout = millis() + KAIDU_CMD_TIMEOUT_MILLIS;
  unsigned long startms = 0;

  if (kaidu_config_server_url_choice) {
    serverPath = KAIDU_CONFIG_SERVER_URL;
  } else {
    serverPath = KAIDU_CONFIG_SERVER_DIRECT1;
  }
  serverPath.concat(F("kaidu_device_configuration/slave_firmware_update/"));
  serverPath.concat(kaidu_mac_address);
  serverPath.concat("/");
  serverPath.concat(kaidu_nrf_version);
  #ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(serverPath);
  #endif
  kaidu_http_last_status = false;
  http.begin(serverPath.c_str());
  //tempcount = 0;
  for(int i=0; i<KAIDU_CMD_MAX_COUNT && (httpResponseCode < 200 || httpResponseCode > 299) && millis() < timeout && wifi_get_connected_status(); i++) {
    startms = millis();
    httpResponseCode = http.GET();
    kaidu_avg_http_send_ms = (KAIDU_FLOAT_MS_WEIGHT * (float)(millis()-startms)) + ((1.0-KAIDU_FLOAT_MS_WEIGHT) * kaidu_avg_http_send_ms); //kaidu_update_avg_http_send(startms);
    ++kaidu_http_send_count;
    //#ifdef KAIDU_CMD_SHOW_LOGS
    //++tempcount;
    //#endif
    if (httpResponseCode > 199 && httpResponseCode < 300) {
      kaidu_http_last_status = true;
      ++kaidu_http_success_count;
      //check payload, eg. "v02000.dat"
      payload = http.getString();
      Serial.println(payload);
      if(payload.length() > 0) {
        if (payload.endsWith(".dat")) {
          kaidu_rx_dfu_cmd_status = true;
          kaidu_rx_fw_update_cmd_status = false; // do this because we are sharing kaidu-fw-name
          snprintf(kaidu_fw_name, sizeof(kaidu_fw_name), "%s", payload.c_str());
          //Serial.println(F("FIRMWARE check: found update!"));
          #ifdef KAIDU_CMD_SHOW_LOGS
          Serial.println(kaidu_fw_name);
          #endif
        }
      }
      return true;
    } else {
      kaidu_http_last_status = false;
      ++kaidu_http_error_count;
      #ifdef KAIDU_CMD_SHOW_LOGS
      Serial.println(F("kaidu_check_slave_firmware_update: bad http response code!"));
      #endif
      //teststatus = false;
    }
  }
  #ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(F("api-error"));
  #endif
  return false;
}


void kaidu_send_http_telemetry(String telemetrymsg) {
  #ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(F("HTTP TELEMETRY"));
  #endif
  unsigned int sendcount, errorcount;
  int httpResponseCode;
  HTTPClient http;
  unsigned long timeout = millis() + KAIDU_CMD_TIMEOUT_MILLIS;
  unsigned long startms = 0;
  //
  String serverPath = String();
  serverPath.reserve(256);
  //
  // create the URL
  if (kaidu_config_server_url_choice) {
    serverPath = KAIDU_CONFIG_SERVER_URL;
  } else {
    serverPath = KAIDU_CONFIG_SERVER_DIRECT1;
  }
  serverPath.concat(F("kaidu_device_cmd/"));
  serverPath.concat(kaidu_mac_address);
  serverPath.concat(F("/"));
  serverPath.concat(kaidu_device_id);
  serverPath.concat(F("/{\"m\":\""));
  serverPath.concat(kaidu_mac_address);
  serverPath.concat(F("\",\"i\":\""));
  serverPath.concat(telemetrymsg);
  serverPath.concat(F("\",\"v\":\""));
  serverPath.concat(VERSION_STRING);
  serverPath.concat(F("\",\"n\":\""));
  serverPath.concat(kaidu_nrf_version);
  serverPath.concat(F("\",\"t\":"));
  serverPath.concat(time(nullptr));
  serverPath.concat(F(",\"h\":"));
  serverPath.concat(ESP.getFreeHeap());
  serverPath.concat(F(",\"u\":0,\"z\":0,\"s\":\""));
  serverPath.concat(wifi_get_ssid());
  serverPath.concat(F("\",\"w\":"));
  serverPath.concat(wifi_get_avg_wifi_rssi());
  serverPath.concat(F(",\"x\":"));
  serverPath.concat(wifi_get_fail_connect_count());
  serverPath.concat(F(",\"a\":"));
  serverPath.concat(kaidu_http_success_count);
  serverPath.concat(F(",\"b\":"));
  serverPath.concat(kaidu_http_send_count);
  serverPath.concat(F(",\"c\":"));
  serverPath.concat(kaidu_avg_http_send_ms);
  serverPath.concat(F("}"));
  //
  // try to send
  #ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(serverPath);
  #endif
  kaidu_http_last_status = false;
  startms = millis();
  http.begin(serverPath.c_str());
  httpResponseCode = http.GET();
  kaidu_avg_http_send_ms = (KAIDU_FLOAT_MS_WEIGHT * (float)(millis()-startms)) + ((1.0-KAIDU_FLOAT_MS_WEIGHT) * kaidu_avg_http_send_ms); //kaidu_update_avg_http_send(startms);
  sendcount=1; errorcount=0;
  for(int i=0; i<KAIDU_CMD_MAX_COUNT && (httpResponseCode < 200 || httpResponseCode > 299) && millis() < timeout && wifi_get_connected_status(); i++) {
    ++errorcount;
    startms = millis();
    httpResponseCode = http.GET();
    kaidu_avg_http_send_ms = (KAIDU_FLOAT_MS_WEIGHT * (float)(millis()-startms)) + ((1.0-KAIDU_FLOAT_MS_WEIGHT) * kaidu_avg_http_send_ms); //kaidu_update_avg_http_send(startms);
    ++sendcount;
  }
  if (httpResponseCode > 199 && httpResponseCode < 300) {
    kaidu_http_last_status = true;
    // upload was success, then reset counters
    kaidu_http_success_count = 1;
    kaidu_http_error_count = errorcount;
    kaidu_http_send_count = sendcount;
    
  } else {
    kaidu_http_last_status = false;
    // upload failed, update counters
    kaidu_http_error_count += errorcount;
    kaidu_http_send_count += sendcount;
  }
}

void kaidu_get_command(char * cmdmem, int cmdlen) {
  String serverPath = String();
  String payload;
  int httpResponseCode;
  HTTPClient http;
  unsigned long startms = 0;

  if (kaidu_config_server_url_choice) {
    serverPath = KAIDU_CONFIG_SERVER_URL;
  } else {
    serverPath = KAIDU_CONFIG_SERVER_DIRECT1;
  }
  serverPath.concat(F("kaidu_device_cmd/"));
  serverPath.concat(kaidu_mac_address);
  //Serial.println(serverPath);
  startms = millis();
  http.begin(serverPath.c_str());
  httpResponseCode = http.GET();
  kaidu_avg_http_send_ms = (KAIDU_FLOAT_MS_WEIGHT * (float)(millis()-startms)) + ((1.0-KAIDU_FLOAT_MS_WEIGHT) * kaidu_avg_http_send_ms); //kaidu_update_avg_http_send(startms);
  ++kaidu_http_send_count;
  if (httpResponseCode > 199 && httpResponseCode < 300) {
    kaidu_http_last_status = true;
    ++kaidu_http_success_count;
    //check payload, eg. "cmd-name, cmd-params"
    payload = http.getString();
    //Serial.println(payload);
    payload.toCharArray(cmdmem, cmdlen);
  } else {
    kaidu_http_last_status = false;
    ++kaidu_http_error_count;
    #ifdef KAIDU_CMD_SHOW_LOGS
    Serial.println(F("kaidu_get_command: bad http response code!"));
    #endif
  }
  http.end();
}

#define KAIDU_CMD_MEM_LEN 36
bool kaidu_process_command() {
  char cmdmem[KAIDU_CMD_MEM_LEN];
  char * cmdname;
  char * cmdparam;
  #ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(F("KAIDU CMD"));
  #endif
  memset(cmdmem, 0, KAIDU_CMD_MEM_LEN);
  kaidu_get_command(cmdmem, KAIDU_CMD_MEM_LEN);
  // XXXDC
  // note we assume that cmd is 2-chars, but this may change in the future
  // we should just search for the comma, instead of making assumptions
  //
  if (cmdmem[0] > 0 && cmdmem[1] > 0 && (cmdmem[2] == 0 || cmdmem[2] == ',')) {
    //Serial.println(cmdmem);
    cmdmem[KAIDU_CMD_MEM_LEN-1] = 0; // zero
    cmdmem[2] = 0; //replace comma with 0
    cmdname = cmdmem;
    cmdparam = cmdmem + 3;
    #ifdef KAIDU_CMD_SHOW_LOGS
    Serial.println(cmdname);
    Serial.println(cmdparam);
    #endif
    //
    if (cmdname[0]=='R' && cmdname[1]=='S') {
      // change to reset state
      kaidu_rx_reset_cmd_status = true;
      #ifdef KAIDU_CMD_SHOW_LOGS
      Serial.println(F("reset command"));
      #endif
      //mqtt_confirm_kaidu_command(cmdname, cmdname);
      kaidu_send_http_telemetry("RS");
      return true;
    }
    else if (cmdname[0]=='V' && cmdname[1]=='E') {
      // return version
      #ifdef KAIDU_CMD_SHOW_LOGS
      Serial.println(F("esp version command"));
      #endif
      cmdmem[2] = '=';
      snprintf(cmdparam, KAIDU_CMD_MEM_LEN-3, VERSION_STRING);
      //mqtt_confirm_kaidu_command(cmdname, cmdparam);
      kaidu_send_http_telemetry(cmdmem);
      return true;
    }
    else if (cmdname[0]=='V' && cmdname[1]=='N') {
      // return version
      #ifdef KAIDU_CMD_SHOW_LOGS
      Serial.println(F("nrf version command"));
      #endif
      cmdmem[2] = '=';
      snprintf(cmdparam, KAIDU_CMD_MEM_LEN-3, kaidu_nrf_version);
      //mqtt_confirm_kaidu_command(cmdname, cmdparam);
      kaidu_send_http_telemetry(cmdmem);
      return true;
    }
    else if (cmdname[0]=='P' && cmdname[1]=='I') {
      // respond to ping command
      #ifdef KAIDU_CMD_SHOW_LOGS
      Serial.println(F("ping command"));
      #endif
      //cmdparam[0] = 'P';
      //cmdparam[1] = 'O';
      //cmdparam[2] = 0;
      //mqtt_confirm_kaidu_command(cmdname, cmdparam);
      kaidu_send_http_telemetry("PO");
      return true;
    }
    else if (cmdname[0]=='D' && cmdname[1]=='F') {
      // dfu command
      // check fw name
      for(int i=0; i<KAIDU_CMD_MEM_LEN-6; i++) {
        if(cmdparam[i]=='.' && cmdparam[i+1]=='d' && cmdparam[i+2]=='a' && cmdparam[i+3]=='t') {
          // found .dat , will derive .bin from this name
          cmdmem[2] = '=';
          kaidu_send_http_telemetry(cmdmem);
          snprintf(kaidu_fw_name, sizeof(kaidu_fw_name), "%s", cmdparam);
          kaidu_rx_dfu_cmd_status = true;
          #ifdef KAIDU_CMD_SHOW_LOGS
          Serial.println(F("dfu command with valid fw filename"));
          Serial.println(kaidu_fw_name);
          #endif
          break;
        }
      }
      #ifdef KAIDU_CMD_SHOW_LOGS
      if (!kaidu_rx_dfu_cmd_status) {
        Serial.println(F("dfu command but invalid fw filename"));        
      }
      #endif
      return true;
    }
    else if (cmdname[0]=='O' && cmdname[1]=='T') {
      // ota command
      // check fw name
      for(int i=0; i<KAIDU_CMD_MEM_LEN-6; i++) {
        if(cmdparam[i]=='.' && cmdparam[i+1]=='b' && cmdparam[i+2]=='i' && cmdparam[i+3]=='n') {
          // found .bin
          cmdmem[2] = '=';
          kaidu_send_http_telemetry(cmdmem);
          snprintf(kaidu_fw_name, sizeof(kaidu_fw_name), "%s", cmdparam);
          kaidu_rx_fw_update_cmd_status = true;
          #ifdef KAIDU_CMD_SHOW_LOGS
          Serial.println(F("ota command with valid fw filename"));
          Serial.println(kaidu_fw_name);
          #endif
          break;
        }
      }
      #ifdef KAIDU_CMD_SHOW_LOGS
      if (!kaidu_rx_fw_update_cmd_status) {
        Serial.println(F("ota command but invalid fw filename"));        
      }
      #endif
      return true;
    }
    else {
      // unknown
      #ifdef KAIDU_CMD_SHOW_LOGS
      Serial.println(F("unknown command"));
      #endif
      //cmdparam[0] = '%'; cmdparam[1] = '3'; cmdparam[2] = 'F';
      //cmdparam[3] = '%'; cmdparam[4] = '3'; cmdparam[5] = 'F';
      //cmdparam[6] = 0;
      //mqtt_confirm_kaidu_command(cmdname, cmdparam); //respond with '??'
      kaidu_send_http_telemetry("??");
    }
  } else {
    if (cmdmem[0] > 0) {
      #ifdef KAIDU_CMD_SHOW_LOGS
      Serial.println(F("unknown command"));
      Serial.println(cmdmem);
      #endif
      cmdname = cmdmem;
      cmdparam = cmdmem;
      for (int i=0; i<KAIDU_CMD_MEM_LEN; i++) {
        if(cmdmem[i] == ',') {
          cmdmem[i] = 0;
          cmdparam = cmdmem + i + 1;
          break;
        }
      }
      #ifdef KAIDU_CMD_SHOW_LOGS
      Serial.println(cmdname);
      Serial.println(cmdparam);
      #endif
      //mqtt_confirm_kaidu_command(cmdname, NULL);
      kaidu_send_http_telemetry(F("??"));
    }
  }
  return false;
}

void kaidu_set_mac_address (uint8_t* mac_buf) {
  uint8_t mac0 = mac_buf[5] & 0xFF;
  uint8_t mac1 = mac_buf[4] & 0xFF;
  uint8_t mac2 = mac_buf[3] & 0xFF;
  uint8_t mac3 = mac_buf[2] & 0xFF;
  uint8_t mac4 = mac_buf[1] & 0xFF;
  uint8_t mac5 = mac_buf[0] & 0xFF;
  char mac_str[20];
  sprintf(mac_str,"%02x:%02x:%02x:%02x:%02x:%02x", mac0, mac1, mac2, mac3, mac4, mac5);
  kaidu_mac_address = String(mac_str);
  #ifdef KAIDU_CONFIG_SERVER_FAKE_MAC
  kaidu_mac_address.concat(":XX");
  #endif
  //#ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(kaidu_mac_address);
  //#endif
  // default device-id, unless override
  sprintf(kaidu_device_id,"%02x%02x%02x%02x%02x%02x", mac0, mac1, mac2, mac3, mac4, mac5);
  //#ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(kaidu_device_id);
  //#endif
}

bool kaidu_rx_last_http_status () {
  return kaidu_http_last_status;
}

bool kaidu_rx_stop_cmd () {
  bool cmd_status = kaidu_rx_stop_cmd_status;
  kaidu_rx_stop_cmd_status = false;
  return cmd_status;
}

bool kaidu_rx_reset_cmd () {
  bool cmd_status = kaidu_rx_reset_cmd_status;
  kaidu_rx_reset_cmd_status = false;
  return cmd_status;
}

bool kaidu_rx_fw_update_cmd () {
  // note- will reset after reading fw filename
  return kaidu_rx_fw_update_cmd_status;
}

bool kaidu_rx_dfu_cmd () {
  // note- will reset after reading fw filename
  return kaidu_rx_dfu_cmd_status;
}

//bool kaidu_rx_fw_update_url () {
//  return kaidu_rx_fw_update_url_status;
//}

char * kaidu_fw_update_name () {
  // when getting url, set state to false
  kaidu_rx_fw_update_cmd_status = false;
  kaidu_rx_dfu_cmd_status = false;
  return kaidu_fw_name;
}

//String kaidu_fw_update_get_url () {
//  return kaidu_fw_update_url_uri;
//}

void kaidu_set_nrf_version (char * nrfversion) {
  #ifdef KAIDU_CMD_SHOW_LOGS
  Serial.println(nrfversion);
  #endif
  memset(kaidu_nrf_version, 0, sizeof(kaidu_nrf_version));
  memcpy(kaidu_nrf_version, nrfversion, strlen(nrfversion));
}

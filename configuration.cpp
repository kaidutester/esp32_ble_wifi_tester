//
// read configuration from preferences
// write configuration by ble-server
//

#include "Preferences.h"
#include "configuration.h"

#define CONFIGURATION_STRING_SIZE 16

Preferences preferences;
String configuration_ssid;
String configuration_pswd;
String configuration_mqtt_cert;
String configuration_mqtt_devid;
String configuration_cust_id;
unsigned int configuration_reboot_count = 0;
//boolean configuration_check_failed = false; // assume all is well

//void ble_scan_set_devid (String devid);
//void wifi_probe_set_devid (String devid);
void wifi_set_wifi_ssid_pswd (String ssid, String pswd);
//void mqtt_set_mqtt_cert (String cert);
//void mqtt_set_mqtt_dvid (String devid);
//void stats_set_reboot_counter (unsigned int lcount);
//void stats_set_device_id (String dvid);

/////////////////////////////////////////////////////
//
//

String* configuration_get_ssid() {
  return &configuration_ssid;
}

String* configuration_get_pswd() {
  return &configuration_pswd;
}

String* configuration_get_dvid() {
  return &configuration_mqtt_devid;
}

String* configuration_get_cert() {
  return &configuration_mqtt_cert;
}

String* configuration_get_cust() {
  return &configuration_cust_id;
}

unsigned int configuration_get_reboot() {
  return configuration_reboot_count;
}

boolean configuration_is_empty() {
  boolean config_status = false;
  if (configuration_ssid == "" && configuration_cust_id == "" && 
    configuration_mqtt_cert == "" && configuration_mqtt_devid == "") {
    config_status = true;
  }
  Serial.print(F("configuration_is_empty: "));
  Serial.println(String(config_status));
  return config_status;
}

String configuration_read (const char* configuration_string_name) {
  preferences.begin(CONFIGURATION_STORE, true);
  String configuration_data = preferences.getString(configuration_string_name, "");
  preferences.end();
  Serial.print(F("configuration_read: "));
  Serial.print(String(configuration_string_name));
  Serial.print(F(" = "));
  Serial.println(configuration_data);
  return configuration_data;
}

void configuration_write (const char* configuration_string_name, String configuration_string_data) {
  Serial.print(F("configuration_write: "));
  Serial.print(String(configuration_string_name));
  Serial.print(F(" = "));
  Serial.println(configuration_string_data);
  preferences.begin(CONFIGURATION_STORE, false);
  preferences.putString(configuration_string_name, configuration_string_data);
  preferences.end();
}

void configuration_write_int (const char* configuration_string_name, int configuration_int_data) {
  Serial.print(F("configuration_write: "));
  Serial.print(String(configuration_string_name));
  Serial.print(F(" = "));
  Serial.println(String(configuration_int_data));
  preferences.begin(CONFIGURATION_STORE, false);
  preferences.putInt(configuration_string_name, configuration_int_data);
  preferences.end();
}

boolean configuration_check_all () {
  boolean config_status = false;
  preferences.begin(CONFIGURATION_STORE, true);
  
  configuration_ssid = preferences.getString(CONFIGURATION_SSID, "");
  Serial.print(F("configuration: CONFIGURATION_SSID = "));
  Serial.println(configuration_ssid);
  
  configuration_pswd = preferences.getString(CONFIGURATION_PSWD, "");
  Serial.print(F("configuration: CONFIGURATION_PSWD = "));
  Serial.println(configuration_pswd);
  
  configuration_mqtt_cert = preferences.getString(CONFIGURATION_MQTT_CERT, "");
  Serial.print(F("configuration: CONFIGURATION_MQTT_CERT = "));
  Serial.println(configuration_mqtt_cert);
  
  configuration_mqtt_devid = preferences.getString(CONFIGURATION_MQTT_DVID, "");
  Serial.print(F("configuration: CONFIGURATION_MQTT_DVID = "));
  Serial.println(configuration_mqtt_devid);

  configuration_reboot_count = preferences.getInt(CONFIGURATION_REBOOT_COUNT, 0);
  Serial.print(F("configuration: CONFIGURATION_REBOOT_COUNT = "));
  Serial.println(configuration_reboot_count);

  configuration_cust_id = preferences.getString(CONFIGURATION_CUST_ID, "");
  Serial.print(F("configuration: CONFIGURATION_CUST_ID = "));
  Serial.println(configuration_cust_id);

  preferences.end();
  
  if (configuration_ssid != "") {
    wifi_set_wifi_ssid_pswd(configuration_ssid, configuration_pswd);
  }
//  if (configuration_mqtt_cert != "") mqtt_set_mqtt_cert(configuration_mqtt_cert);
//  if (configuration_mqtt_devid != "") {
//    //ble_scan_set_devid(configuration_mqtt_devid);
//    //wifi_probe_set_devid(configuration_mqtt_devid);
//    mqtt_set_mqtt_dvid(configuration_mqtt_devid);
//    //stats_set_device_id(configuration_mqtt_devid);
//  }
//  stats_set_reboot_counter(configuration_reboot_count);
  
  //if (configuration_ssid != "" && configuration_mqtt_cert != "" && configuration_mqtt_devid != "") {
  if (configuration_ssid != "") {
    config_status = true;
    Serial.println(F("configuration_check_all: pass"));
  } else {
    //configuration_check_failed = true;
    config_status = false;
    Serial.println(F("configuration_check_all: fail"));
  }
  
  return config_status;
}

void configuration_setup() {
  // setup String sizes
  configuration_ssid.reserve(CONFIGURATION_STRING_SIZE);
  configuration_pswd.reserve(CONFIGURATION_STRING_SIZE);
  configuration_mqtt_devid.reserve(CONFIGURATION_STRING_SIZE);
  configuration_mqtt_cert.reserve(8*CONFIGURATION_STRING_SIZE);
  configuration_cust_id.reserve(4*CONFIGURATION_STRING_SIZE);   //at least 36 for uuid4
}

void configuration_loop() {
  // TODO - when not configured, check internet, pull configuration, write to nvram, reboot
  // only write to nvram if configuration differs from existing
  // if (configuration_check_failed) {...}
}

//
// configuration defines and protos
// note that nvr/preferences library have key name length limitations
//

#define CONFIGURATION_STORE "kaidu"

#define CONFIGURATION_SSID "ssid"
#define CONFIGURATION_PSWD "pswd"
#define CONFIGURATION_MQTT_CERT "cert"
#define CONFIGURATION_MQTT_DVID "dvid"
#define CONFIGURATION_CUST_ID "cust"

#define CONFIGURATION_REBOOT_COUNT "boot"

void configuration_write (const char* configuration_string_name, String configuration_string_data);
void configuration_write_int (const char* configuration_string_name, int configuration_int_data);
boolean configuration_check_all ();
void configuration_setup ();
void configuration_loop ();
unsigned int configuration_get_reboot ();
String *configuration_get_ssid ();

// OTA Auth Setup
// No authentication by default
const char* ota_password = "pd2023";
// Hostname defaults to esp8266-[ChipID]
const char* ota_hostname = "PerfectDraft";

// MQTT Setup
// Uncomment the define __MQTT_ENABLED__ to enable MQTT implementation
// and ensure to setup MQTT credentials ...
//
//#define __MQTT_ENABLED__
#if defined(__MQTT_ENABLED__)
  const char* mqtt_hostname = "_BROKER_";
  #define MQTT_PORT 1886
  const char* mqtt_username = "_BROKER_USER_";
  const char* mqtt_password = "_BROKER_PASSWORD_";
#endif

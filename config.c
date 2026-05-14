//this is a sample config.cpp for the ESP8266 Remote, see:
//https://github.com/judasgutenberg/Esp8266_RemoteControl
#include "config.h"
#include "globals.h"

char* cs[CONFIG_STRING_COUNT]; //used for string configurations
int ci[CONFIG_TOTAL_COUNT]; //used for int configurations

char* css[CONFIG_SLAVE_STRING_COUNT]; // string slave configuration values
int cis[CONFIG_SLAVE_TOTAL_COUNT];    // integer slave configuration values

void initMasterDefaults() {
  cs[STORAGE_PASSWORD] = "supersecret";  // must match storage_password in backend
  cs[URL_GET] = "/weather/server.php";
  cs[HOST_GET] = "yourserver.com";
  cs[ENCRYPTION_SCHEME] = "71FF867AC7733913"; // 16-char hex string
  cs[SENSOR_CONFIG_STRING] = "0*-1*1*99*NULL*NULL*12v*15*1"; // "14*-1*2301*11*0*NULL*stank*1";
  cs[PINS_TO_START_LOW] = ""; //comma-delimited integers of pins to start low
  cs[WIFI_SSID] = "your_ssid";
  cs[WIFI_PASSWORD] = "secret";
  cs[WIFI_SSID_1] = "another_ssid";
  cs[WIFI_PASSWORD_1] = "";
  cs[WIFI_SSID_2] = "yet_another_ssid";
  cs[WIFI_PASSWORD_2] = "";
  
  ci[SENSOR_ID] = 680;//280;          // e.g. 2301 = DHT2301
  ci[SENSOR_I2C] = 0x77;
  ci[CONSOLIDATE_ALL_SENSORS_TO_ONE_RECORD] = 1;
  ci[DEVICE_ID] = 16;
  ci[POLLING_GRANULARITY] = 4;
  ci[DATA_LOGGING_GRANULARITY] = 300;
  ci[OFFLINE_LOG_GRANULARITY] = 100;
  ci[WIFI_TIMEOUT] = 100;
  ci[OFFLINE_RECONNECT_INTERVAL] = 20;
  ci[FRAM_INDEX_SIZE] = 256;
  ci[FRAM_LOG_TOP] = 28672;
  ci[CONNECTION_FAILURE_RETRY_SECONDS] = 4;
  ci[CONNECTION_RETRY_NUMBER] = 22;
  ci[GRANULARITY_WHEN_IN_MOXEE_PHASE_0] = 3;
  ci[GRANULARITY_WHEN_IN_MOXEE_PHASE_1] = 17;
  ci[NUMBER_OF_HOTSPOT_REBOOTS_OVER_LIMITED_TIMEFRAME_BEFORE_ESP_REBOOT] = 4;
  ci[HOTSPOT_LIMITED_TIME_FRAME] = 340;
  ci[MOXEE_POWER_SWITCH] = 0;
  ci[DEEP_SLEEP_TIME_PER_LOOP] = 0;
  ci[LIGHT_SLEEP_TIME_PER_LOOP] = 0;
  ci[SENSOR_DATA_PIN] = -1;
  ci[SENSOR_POWER_PIN] = -1;
  ci[SENSOR_SUB_TYPE] = 21;
  ci[IR_PIN] = 0;
  ci[INA219_ADDRESS] = 0x40;//0x40;
  ci[FRAM_ADDRESS] = 0;//0x50;
  ci[RTC_ADDRESS] = 0;//0x68;
  ci[SLAVE_I2C] = 20;
  ci[SLAVE_PET_WATCHDOG_COMMAND] = 203;
  ci[DEBUG] = 2;
  ci[POLLING_SKIP_LEVEL] = 50;
  ci[LOCAL_WEB_SERVICE_RESPONSIVENESS] = 1;
  ci[BAUD_RATE_LEVEL] = 9;
  ci[CONFIG_PERSIST_METHOD] = 0;
  ci[SEND_TELEMETRY_TYPE_IN_RESERVED] = 1;
  ci[ANOMALY_LOG_LEVEL] = 1;

  //these were originally for the BE680 but for other sensors that take a complex configuration, these could be made params for those as well:
  ci[SENSOR_PARAM_1] = 5; // for the first three: 5 is  BME68X_OS_16X, 4 is 8X, 3 is 2X, 1 is 1X, 0 is none
  ci[SENSOR_PARAM_2] = 5;
  ci[SENSOR_PARAM_3] = 5;
  ci[SENSOR_PARAM_4] = 1; //should be in the range of 0 for no filter to 7 for BME68X_FILTER_SIZE_127                    
  ci[SENSOR_PARAM_5] = 0; //0 for 50 degrees, 34 for 100 degrees, 100 for 200, 233 for 400 degrees, 300 for 500 degrees
  ci[SENSOR_PARAM_6] = 0x59; //0x59 is 89 ms, 200 is 200 ms
  
  ci[SERIAL_SWAP] = 1;
  ci[SERIAL_DEBUG_LEVEL] = 0;
  ci[SERIAL_FOR_COMMANDS_ONLY] = 0;
  ci[SERIAL_BUFFER] = 2048;
  ci[SERIAL_PARSE_MODE] = 0;
}

void initSlaveDefaults() {                                      
  css[0] = "Characteristic #2;0x3ffbb61c;0x3ffbb5fc;4;4;6;7;8;9;10;11;0x3ffbb60c;0;1";
  css[1] = "Characteristic #7;I (318800102);0x3ffbb5bc;6;7";
  css[2] = "Characteristic #9;I (28318992);0x3ffbb64c;0;1";

  cis[SLAVE_POWER_MODE] = 2;
  cis[SLAVE_PARSE_MODE] = 0;
  cis[SLAVE_BAUD_RATE_LEVEL] = 9;
  cis[SLAVE_I2C_ADDRESS] = 0;
  cis[SLAVE_REBOOT_PIN] = 7;
  cis[SLAVE_SERIAL_MODE] = 2;
 
}

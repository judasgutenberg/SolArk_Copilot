
const char* wifi_ssid = "your_wifi_ssid"; //mine was Moxee Hotspot83_2.4G
const char* wifi_password = "your_wifi_password";
const char* storage_password = "your_secret"; //to ensure someone doesn't store bogus data on your server. should match value in the storage_password column in you tenant record
//data posted to remote server so we can keep a historical record
//url will be in the form: http://your-server.com:80/weather/data.php?data=
const char* url_get = "/your_directory/data.php";
const char* host_get = "your-domain.com";
const char* sensor_config_string = "";// "14*-1*2301*11*NULL*NULL*stank*1|0*-1*1*3*NULL*NULL*gassy*1"; //an easy way to specify multiple sensors. the format is: dataPin*powerPin*sensorType*sensorSubType*i2c_address*device_feature_id*name*consolidateAllSensorsToOneRecord|next_sensor...
const int sensor_id = 280; //2301;//2301;//680; //SENSORS! -- we support these: 75 for LM75, 85 for BMP085, 180 for BMP180, 2301 for DHT 2301, 680 for BME680.  0 for no sensor.  No support for multiple sensors for now.
const int sensor_i2c = 0x76;
const int consolidate_all_sensors_to_one_record = 1;
const int device_id = the_id_in_device_table; 
int polling_granularity = 4; //how often to poll backend in seconds, 4 makes sense
int data_logging_granularity = 300; //how often to store data in backend, 300 makes sense
const int connection_failure_retry_seconds = 4;
const int connection_retry_number = 22;

const int granularity_when_in_connection_failure_mode = 5; //40 was too little time for everything to come up and start working reliably, at least with my sketchy cellular connection
const int hotspot_limited_time_frame = 340; //seconds

int deep_sleep_time_per_loop = 0;  //in seconds. saves energy.  set to zero if unneeded. GPIO16/D0 needs to be conneted to RST in hardware first. can be changed remotely

//if you are using a DHT hygrometer/temperature probe, these values will be important
//particularly if you reflashed a MySpool temperature probe (https://myspool.com/) with custom firmware
//on those, the sensorData is 14 and sensorPower is 12
const int sensor_data_pin = -1; 
const int sensor_power_pin = 12;
// #define dhType DHT11 // DHT 11
// #define dhType DHT22 // DHT 22, AM2302, AM2321
const int sensor_sub_type = 21; // DHT 21, AM2301

const char pins_to_start_low[] = {12, 13, -1}; //so when the device comes up it doesn't immediately turn on attached devices

const int ir_pin = 14; //set to the value of an actual data pin if you want to send data via ir from your controller on occasion
const int ina219_address = 0x40; //set to -1 if you have no power voltage to monitor
const int ina219_address_a = -1; //set to -1 if you have no power voltage a to monitor
const int ina219_address_b = -1; //set to -1 if you have no power voltage b to monitor

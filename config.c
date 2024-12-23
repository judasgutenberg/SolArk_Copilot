
const char* wifi_ssid = "yourr_ssid"; //mine was Moxee Hotspot83_2.4G
const char* wifi_password = "your_wifi_password";
const char* storage_password = "your_storage_password"; //to ensure someone doesn't store bogus data on your server. should match value in the storage_password column in you user table for your user
//data posted to remote server so we can keep a historical record
//url will be in the form: http://your-server.com:80/weather/data.php?data=
const char* url_get = "/weather/data.php";
const char* host_get = "randomsprocket.com";
const int device_id = 16;
const int connection_failure_retry_seconds = 4;
const int connection_retry_number = 22;
const char pins_to_start_low[] = {12, 13, -1}; //so when the device comes up it doesn't immediately turn on attached devices

const int ir_pin = 14; //set to the value of an actual data pin if you want to send data via ir from your controller on occasion
const int ina219_address = 0x40; //set to -1 if you have no power voltage to monitor
const int ina219_address_a = -1; //set to -1 if you have no power voltage a to monitor
const int ina219_address_b = -1; //set to -1 if you have no power voltage b to monitor

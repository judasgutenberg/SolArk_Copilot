

//Gus Mueller, June 29, 2024
//uses an ESP8266 wired with the swap serial pins (D8 as TX and D7 as RX) connected to the exposed serial header on the ESP32 in the SolArk's WiFi dongle.
//this intercepts the communication data between the SolArk and the dongle to get frequent updates (that is, every few seconds) of the power and battery levels.
//the data still makes it to PowerView (now MySolArk) but you have access to it much sooner, locally, and at much finer granularity
//this is similar to the ESP8266Remte, but has the SolArk monitoring but no Moxee Rebooting functionality
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <Adafruit_INA219.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_ADT7410.h>

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SimpleMap.h>

#include "config.h"

#include "Zanshin_BME680.h"  // Include the BME680 Sensor library
#include <DHT.h>
#include <Adafruit_AHTX0.h>
#include <SFE_BMP180.h>
#include <Adafruit_BMP085.h>
#include <Temperature_LM75_Derived.h>
#include <Adafruit_BMP280.h>
#include <Wire.h>

#include "index.h" //Our HTML webpage contents with javascriptrons

//i created 12 of each sensor object in case we added lots more sensors via device_features
//amusingly, this barely ate into memory at all
//since many I2C sensors only permit two sensors per I2C bus, you could reduce the size of these object arrays
//and so i've dropped some of these down to 2
Adafruit_ADT7410 adt7410[4];
DHT* dht[6];
Adafruit_AHTX0 AHT[2];
SFE_BMP180 BMP180[2];
BME680_Class BME680[2];
Adafruit_BMP085 BMP085d[2];
Generic_LM75 LM75[2];
Adafruit_BMP280 BMP280[2];
IRsend irsend(ir_pin);
Adafruit_INA219* ina219;
Adafruit_INA219* ina219a;
Adafruit_INA219* ina219b;

Adafruit_VL53L0X lox[4];

String serialContent = "";
String ipAddress;
String ipAddressAffectingChange;
int changeSourceId = 0;
String goodData;
String dataToDisplay;
String deviceName = "";
long lastDataLogTime = 0;
long connectionFailureTime;
bool connectionFailureMode = false;
ESP8266WebServer server(80); //Server on port 80
SoftwareSerial feedbackSerial(3, 1);
WiFiUDP ntpUDP; //i guess i need this for time lookup
NTPClient timeClient(ntpUDP, "pool.ntp.org");


long lastCommandId = 0;

String additionalSensorInfo; //we keep it stored in a delimited string just the way it came from the server and unpack it periodically to get the data necessary to read sensors
int pinTotal = 12;
String pinList[12]; //just a list of pins
String pinName[12]; //for friendly names
bool localSource = false;
float measuredVoltage = 0;
float measuredAmpage = 0;
bool canSleep = false;
long latencySum = 0;
long latencyCount = 0;
bool debug = false;
uint8_t outputMode = 0;
String responseBuffer = "";

SimpleMap<String, int> *pinMap = new SimpleMap<String, int>([](String &a, String &b) -> int {
  if (a == b) return 0;      // a and b are equal
  else if (a > b) return 1;  // a is bigger than b
  else return -1;            // a is smaller than b
});
SimpleMap<String, int> *sensorObjectCursor = new SimpleMap<String, int>([](String &a, String &b) -> int {
  if (a == b) return 0;      // a and b are equal
  else if (a > b) return 1;  // a is bigger than b
  else return -1;            // a is smaller than b
});
bool glblRemote = false;

int totalSerialChars = 0;
bool goodDataMode = false;
long localChangeTime = 0;
int requestNonJsonPinInfo = 1;
 
void setup() {
  Serial.begin(115200);
  for(int i=0; i<10; i++) {
    if((int)pins_to_start_low[i] == -1) {
      break;
    }
    pinMode((int)pins_to_start_low[i], OUTPUT);
    digitalWrite((int)pins_to_start_low[i], LOW);
  }
  wiFiConnect();
  Wire.begin();
  delay(2000);
  server.on("/energy", handleInverterData);
  server.on("/", handleRoot);      //Displays a form where devices can be turned on and off and the outputs of sensors
  server.on("/readLocalData", localShowData);
  server.on("/weatherdata", handleWeatherData); //This page is called by java Script AJAX
  server.on("/writeLocalData", localSetData);
  server.begin();
  startWeatherSensors(sensor_id, sensor_sub_type, sensor_i2c, sensor_data_pin, sensor_power_pin);
  timeClient.begin();
  timeClient.setTimeOffset(0);
  if(ina219_address > 0) {
    ina219 = new Adafruit_INA219(ina219_address);
    if (!ina219->begin()) {
      Serial.println("Failed to find INA219 chip");
    } else {
      ina219->setCalibration_16V_400mA();
    }
  }
  if(ina219_address_a > -1) {
    ina219a = new Adafruit_INA219(ina219_address_a);
    if (!ina219a->begin()) {
    }
  }
  if(ina219_address_b > -1) {
    ina219b = new Adafruit_INA219(ina219_address_b);
    if (!ina219b->begin()) {
    }
  }
 
  Serial.println("about to swap");
  delay(4000);
  Serial.swap();
  feedbackSerial.begin(115200);
}

void lookupLocalPowerData() {//sets the globals with the current reading from the ina219
  if(ina219_address < 1) { //if we don't have a ina219 then do not bother
    return;
  }
  float shuntvoltage = 0;
  float busvoltage = 0;
  float current_mA = 0;
  float loadvoltage = 0;
  float power_mW = 0;
  cleanup();
  shuntvoltage = ina219->getShuntVoltage_mV();
  busvoltage = ina219->getBusVoltage_V();
  current_mA = ina219->getCurrent_mA();
  power_mW = ina219->getPower_mW();
  loadvoltage = busvoltage + (shuntvoltage / 1000);
  measuredVoltage = loadvoltage;
  measuredAmpage = current_mA;
}

String additionalPowerData() {//used for reading the voltages on the solar strings, since i can't get those from the solark itself
  float shuntvoltagea = 0;
  float busvoltagea = 0;
  float current_mAa = 0;
  float loadvoltagea = 0;
  float power_mWa = 0;
  float shuntvoltageb = 0;
  float busvoltageb = 0;
  float current_mAb = 0;
  float loadvoltageb = 0;
  float power_mWb = 0;

  if(ina219_address_a > 0) {
    shuntvoltagea = ina219a->getShuntVoltage_mV();
    busvoltagea = ina219a->getBusVoltage_V();
    current_mAa = ina219a->getCurrent_mA();
    power_mWa = ina219a->getPower_mW();
    loadvoltagea = busvoltagea + (shuntvoltagea / 1000);
  }
  if(ina219_address_b > 0) {
    shuntvoltageb = ina219b->getShuntVoltage_mV();
    busvoltageb = ina219b->getBusVoltage_V();
    current_mAb = ina219b->getCurrent_mA();
    power_mWb = ina219b->getPower_mW();
    loadvoltageb = busvoltageb + (shuntvoltagea / 1000);
  }
 
  String out = (String)loadvoltagea + "*" + (String)current_mAa + "*" + (String)loadvoltageb + "*" + (String)current_mAb;
  return out;
}

void startWeatherSensors(int sensorIdLocal, int sensorSubTypeLocal, int i2c, int pinNumber, int powerPin) {
  //i've made all these inputs generic across different sensors, though for now some apply and others do not on some sensors
  //for example, you can set the i2c address of a BME680 or a BMP280 but not a BMP180.  you can specify any GPIO as a data pin for a DHT
  int objectCursor = 0;
  if(sensorObjectCursor->has((String)sensor_id)) {
    objectCursor = sensorObjectCursor->get((String)sensorIdLocal);;
  } 
  if(sensorIdLocal == 1) { //simple analog input
    //all we need to do is turn on power to whatever the analog device is
    if(powerPin > -1) {
      pinMode(powerPin, OUTPUT);
      digitalWrite(powerPin, LOW);
    }
  } else if(sensorIdLocal == 680) {
    Serial.print(F("Initializing BME680 sensor...\n"));
    while (!BME680[objectCursor].begin(I2C_STANDARD_MODE, i2c)) {  // Start B DHTME680 using I2C, use first device found
      Serial.print(F(" - Unable to find BME680. Trying again in 5 seconds.\n"));
      delay(5000);
    }  // of loop until device is located
    Serial.print(F("- Setting 16x oversampling for all sensors\n"));
    BME680[objectCursor].setOversampling(TemperatureSensor, Oversample16);  // Use enumerated type values
    BME680[objectCursor].setOversampling(HumiditySensor, Oversample16);     // Use enumerated type values
    BME680[objectCursor].setOversampling(PressureSensor, Oversample16);     // Use enumerated type values
    //Serial.print(F("- Setting IIR filter to a value of 4 samples\n"));
    BME680[objectCursor].setIIRFilter(IIR4);  // Use enumerated type values
    //Serial.print(F("- Setting gas measurement to 320\xC2\xB0\x43 for 150ms\n"));  // "?C" symbols
    BME680[objectCursor].setGas(320, 150);  // 320?c for 150 milliseconds
  } else if (sensorIdLocal == 2301) {
    Serial.print(F("Initializing DHT AM2301 sensor at pin: "));
    if(powerPin > -1) {
      pinMode(powerPin, OUTPUT);
      digitalWrite(powerPin, LOW);
    }
    dht[objectCursor] = new DHT(pinNumber, sensorSubTypeLocal);
    dht[objectCursor]->begin();
  } else if(sensorIdLocal == 2320) { //AHT20
    if (AHT[objectCursor].begin()) {
      Serial.println("Found AHT20");
    } else {
      Serial.println("Didn't find AHT20");
    } 
  } else if (sensorIdLocal == 7410) { //adt7410
    adt7410[objectCursor].begin(i2c);
    adt7410[objectCursor].setResolution(ADT7410_16BIT);
  } else if (sensorIdLocal == 180) { //BMP180
    BMP180[objectCursor].begin();
  } else if (sensorIdLocal == 85) { //BMP085
    Serial.print(F("Initializing BMP085...\n"));
    BMP085d[objectCursor].begin(); 
  } else if (sensorIdLocal == 280) {
    Serial.print("Initializing BMP280 at i2c: ");
    Serial.print((int)i2c);
    Serial.print(" objectcursor:");
    Serial.print((int)objectCursor);
    Serial.println();
    if(!BMP280[objectCursor].begin(i2c)){
      Serial.println("Couldn't find BMX280!");
    }
  }
  sensorObjectCursor->put((String)sensorIdLocal, objectCursor + 1); //we keep track of how many of a particular sensor_id we use
}

//returns a "*"-delimited string containing weather data, starting with temperature and ending with deviceFeatureId,    a url-encoded sensorName, and consolidateAllSensorsToOneRecord
//we might send multiple-such strings (separated by "!") to the backend for multiple sensors on an ESP8266
//i've made this to handle all the weather sensors i have so i can mix and match, though of course there are many others
String weatherDataString(int sensor_id, int sensor_sub_type, int dataPin, int powerPin, int i2c, int deviceFeatureId, char objectCursor, String sensorName, int ordinalOfOverwrite, int consolidateAllSensorsToOneRecord) {
  double humidityValue = NULL;
  double temperatureValue = NULL;
  double pressureValue = NULL;
  double gasValue = NULL;
  int32_t humidityRaw = NULL;
  int32_t temperatureRaw = NULL;
  int32_t pressureRaw = NULL;
  int32_t gasRaw = NULL;
  int32_t alt  = NULL;
  static char buf[16];
  static uint16_t loopCounter = 0;  
  String transmissionString = "";
  String sensorValue;
  double humidityFromSensor = NULL;
  double temperatureFromSensor = NULL;
  double pressureFromSensor = NULL;
  double gasFromSensor = NULL;
 
  if(deviceFeatureId == NULL) {
    objectCursor = 0;
  }
  if(sensor_id == 1) { //simple analog input. we can use subType to decide what kind of sensor it is!
    //an even smarter system would somehow be able to put together multiple analogReads here
    if(powerPin > -1) {
      digitalWrite(powerPin, HIGH); //turn on sensor power. 
    }
    delay(10);
    double value = NULL;
    if(i2c){
      //i forget how we read a pin on an i2c slave. lemme see:
      sensorValue = String(getPinValueOnSlave((char)i2c, (char)dataPin));
    } else {
      sensorValue = String(analogRead(dataPin));
    }
    /*
    for(char i=0; i<12; i++){ //we have 12 separate possible sensor functions:
      //temperature*pressure*humidity*gas*windDirection*windSpeed*windIncrement*precipitation*reserved1*reserved2*reserved3*reserved4
      //if you have some particular sensor communicating through a pin and want it to be one of these
      //you set sensor_sub_type to be the 0-based value in that *-delimited string
      //i'm thinking i don't bother defining the reserved ones and just let them be application-specific and different in different implementations
      //a good one would be radioactive counts per unit time
      if((int)i == ordinalOfOverwrite) { //had been using sensor_sub_type
        transmissionString = transmissionString + nullifyOrNumber(value);
      }
      transmissionString = transmissionString + "*";
    }
    */
    //note, if temperature ends up being NULL, the record won't save. might want to tweak data.php to save records if it contains SOME data
    
    if(powerPin > -1) {
      digitalWrite(powerPin, LOW);
    }
  } else if (sensor_id == 53) { //distance sensor, does not produce weather data
    VL53L0X_RangingMeasurementData_t measure;
    lox[objectCursor].rangingTest(&measure, false); // pass in 'true' to get debug data printout!
    if (measure.RangeStatus != 4) {  // phase failures have incorrect data
      sensorValue = String(measure.RangeMilliMeter);
    } else {
      sensorValue = "-1";
    }
    
  } else if (sensor_id == 680) { //this is the primo sensor chip, so the trouble is worth it
    //BME680 code:
    BME680[objectCursor].getSensorData(temperatureRaw, humidityRaw, pressureRaw, gasRaw);
    //i'm not sure what all this is about, since i just copied it from the BME680 example:
    sprintf(buf, "%4d %3d.%02d", (loopCounter - 1) % 9999,  // Clamp to 9999,
            (int8_t)(temperatureRaw / 100), (uint8_t)(temperatureRaw % 100));   // Temp in decidegrees
    //Serial.print(buf);
    sprintf(buf, "%3d.%03d", (int8_t)(humidityRaw / 1000),
            (uint16_t)(humidityRaw % 1000));  // Humidity milli-pct
    //Serial.print(buf);
    sprintf(buf, "%7d.%02d", (int16_t)(pressureRaw / 100),
            (uint8_t)(pressureRaw % 100));  // Pressure Pascals
    //Serial.print(buf);                                     
 
    //Serial.print(buf);
    sprintf(buf, "%4d.%02d\n", (int16_t)(gasRaw / 100), (uint8_t)(gasRaw % 100));  // Resistance milliohms
    //Serial.print(buf);

    humidityFromSensor = (double)humidityRaw/1000;
    temperatureFromSensor = (double)temperatureRaw/100;
    pressureFromSensor = (double)pressureRaw/100;
    gasFromSensor = (double)gasRaw/100;  //all i ever get for this is 129468.6 and 8083.7
  } else if (sensor_id == 2301) { //i love the humble DHT
    if(powerPin > -1) {
      digitalWrite(powerPin, HIGH); //turn on DHT power, in case you're doing that. 
    }
    delay(10);
    humidityFromSensor = (double)dht[objectCursor]->readHumidity();
    temperatureFromSensor = (double)dht[objectCursor]->readTemperature();
    if(powerPin > -1) {
      digitalWrite(powerPin, LOW);//turn off DHT power. maybe it saves energy, and that's why MySpool did it this way
    }
  } else if(sensor_id == 280) {
    temperatureFromSensor = BMP280[objectCursor].readTemperature();
    pressureFromSensor = BMP280[objectCursor].readPressure()/100;
  } else if(sensor_id == 2320) { //AHT20
    sensors_event_t humidity, temp;
    AHT[objectCursor].getEvent(&humidity, &temp);
    humidityFromSensor = humidity.relative_humidity;
    temperatureFromSensor = temp.temperature;
  } else if(sensor_id == 7410) {  
    temperatureFromSensor = adt7410[objectCursor].readTempC();
  } else if(sensor_id == 180) { //so much trouble for a not-very-good sensor 
    char status;
    double p0,a;
    status = BMP180[objectCursor].startTemperature();
    if (status != 0)
    {
      // Wait for the measurement to complete:
      delay(status);   
      // Retrieve the completed temperature measurement:
      // Note that the measurement is stored in the variable T.
      // Function returns 1 if successful, 0 if failure.
      status = BMP180[objectCursor].getTemperature(temperatureFromSensor);
      if (status != 0)
      {
        status = BMP180[objectCursor].startPressure(3);
        if (status != 0)
        {
          // Wait for the measurement to complete:
          delay(status);
          // Retrieve the completed pressure measurement:
          // Note that the measurement is stored in the variable P.
          // Note also that the function requires the previous temperature measurement (temperatureValue).
          // (If temperature is stable, you can do one temperature measurement for a number of pressure measurements.)
          // Function returns 1 if successful, 0 if failure.
          status = BMP180[objectCursor].getPressure(pressureFromSensor,temperatureFromSensor);
          if (status == 0) {
            //Serial.println("error retrieving pressure measurement\n");
          }
        } else {
          //Serial.println("error starting pressure measurement\n");
        }
      } else {
        //Serial.println("error retrieving temperature measurement\n");
      }
    } else {
      //Serial.println("error starting temperature measurement\n");
    }
  } else if (sensor_id == 85) {
    //https://github.com/adafruit/Adafruit-BMP085-Library
    temperatureFromSensor = BMP085d[objectCursor].readTemperature();
    pressureFromSensor = BMP085d[objectCursor].readPressure()/100; //to get millibars!
  } else if (sensor_id == 75) { //LM75, so ghetto
    //https://electropeak.com/learn/interfacing-lm75-temperature-sensor-with-arduino/
    temperatureFromSensor = LM75[objectCursor].readTemperatureC();
  } else { //either sensor_id is NULL or 0
    //no sensor at all
  }
  //ordinalOfOverwrite allows us to take the temperature (and other values) from one of the previous sensors and place it in an arbitrary position of the delimited string
  if(ordinalOfOverwrite < 0) {
    temperatureValue = temperatureFromSensor;
    pressureValue = pressureFromSensor;
    humidityValue = humidityFromSensor;
    gasValue = gasFromSensor;
  } else {
    if(sensorValue == NULL) {
      if(sensor_sub_type == 1){
        sensorValue = String(pressureFromSensor);
      } else if (sensor_sub_type == 2){
        sensorValue = String(humidityFromSensor);
      } else if (sensor_sub_type == 3){
        sensorValue = String(gasFromSensor);
      } else {
        sensorValue = String(temperatureFromSensor); 
      }
    }
    
  }
  //
  if(sensor_id > 0) {
    transmissionString = nullifyOrNumber(temperatureValue) + "*" + nullifyOrNumber(pressureValue);
    transmissionString = transmissionString + "*" + nullifyOrNumber(humidityValue);
    transmissionString = transmissionString + "*" + nullifyOrNumber(gasValue);
    transmissionString = transmissionString + "*********"; //for esoteric weather sensors that measure wind and precipitation.  the last four are reserved for now
  }
  //using delimited data instead of JSON to keep things simple
  transmissionString = transmissionString + nullifyOrInt(sensor_id) + "*" + nullifyOrInt(deviceFeatureId) + "*" + sensorName + "*" + nullifyOrInt(consolidateAllSensorsToOneRecord); 
  if(ordinalOfOverwrite > -1) {
    transmissionString = replaceNthElement(transmissionString, ordinalOfOverwrite, sensorValue, '*');
  }
  return transmissionString;
}

void postData(String datastring){
  WiFiClient clientGet;
  const int httpGetPort = 80;
  HTTPClient http;
  String url;
  String mode = "debug";
  String encryptedStoragePassword = encryptStoragePassword(datastring);
  String allData = "k2=" + encryptedStoragePassword + "&locationId=" + device_id + "&mode=debug" + mode + "&data=" + urlEncode(datastring, true);
  url = "http://" + (String)host_get + ":" + String(httpGetPort) + url_get;
  
  http.begin(clientGet, url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST(allData);
 }

 
void loop() {
  // send data only when you receive data:
  
  char incomingByte = ' ';
  String startValidIndication = "MB_real data,seg_cnt:3\r\r";
  long nowTime = millis();

  if((nowTime - lastDataLogTime)/1000 > 1000 || (lastDataLogTime > nowTime && lastDataLogTime >0)){
    //if we overflow millis() or it's been more than 1000 seconds since communication, reboot ESP
    rebootEsp();
  }
  if(!goodDataMode) {
    for(int i=0; i <4; i++) { //doing this four times here is helpful to make web service reasonably responsive. once is not enough
      server.handleClient();
    }
    //lookupLocalPowerData();
 
  }
 
    
  if (Serial.available() > 0) {
    // read the incoming byte:
    incomingByte = Serial.read();

    serialContent += incomingByte;
    totalSerialChars++;
    if(totalSerialChars > 5000) {
      totalSerialChars = 0;
      goodDataMode = false;
      serialContent = "";
    }
    if(goodDataMode) {
      if(ipAddress.indexOf(' ') > 0) { //i was getting HTML header info mixed in for some reason
        ipAddress = ipAddress.substring(0, ipAddress.indexOf(' '));
      }
      bool includesWeatherData = false;
      if (incomingByte == '\r'){
        //dataToDisplay = goodData;
        int packetSize = goodData.length();
        dataToDisplay =  parseInverterData(goodData) + "*" + (String)packetSize;
        dataToDisplay = dataToDisplay + "*" + additionalPowerData(); //*volta*ampa*voltb*ampb after packetSize
        if(millis() - lastDataLogTime > data_logging_granularity * 1000 || lastDataLogTime == 0) {
          includesWeatherData = true;
          if(sensor_id > -1) {
            glblRemote = true;
            String weatherData = weatherDataString(sensor_id, sensor_sub_type, sensor_data_pin, sensor_power_pin, sensor_i2c, NULL, 0, deviceName, -1, consolidate_all_sensors_to_one_record);
            glblRemote = false;
            dataToDisplay += "!" +  weatherData;
            
            String additionalSensorData = handleDeviceNameAndAdditionalSensors((char *)additionalSensorInfo.c_str(), false);
            dataToDisplay +=  additionalSensorData;
            
          }
        }
        String ipAddressToUse = ipAddress;
        if(ipAddressAffectingChange != "") {
           ipAddressToUse = ipAddressAffectingChange;
           changeSourceId = 1;
        }
        
        dataToDisplay = dataToDisplay + "||" + joinMapValsOnDelimiter(pinMap, "*") + "|" + (int)lastCommandId + "**" + (int)localSource + "*" + ipAddressToUse + "*" + requestNonJsonPinInfo + "*1*" + changeSourceId;
        dataToDisplay = dataToDisplay + "*" +  timeClient.getEpochTime()  + "*" + millis();
        dataToDisplay = dataToDisplay + "*";
        if(latencyCount > 0) {
          dataToDisplay = dataToDisplay + (1000 * latencySum)/latencyCount;
        }
        dataToDisplay = dataToDisplay + "|*" + measuredVoltage + "*" + measuredAmpage; //if this device could timestamp data from its archives, it would put the numeric timetamp before measuredVoltage
        //dataToDisplay += + "*" + latitude + "*" + longitude; //not yet supported. might also include accelerometer data some day
        feedbackPrint(packetSize);
        feedbackPrint("\n");
        feedbackPrint(dataToDisplay); 
        feedbackPrint("\n");
        if(packetSize == 745) {
          sendRemoteData(dataToDisplay, "saveLocallyGatheredSolarData", includesWeatherData);
        } else {
          sendRemoteData(dataToDisplay, "suspectLocallyGatheredSolarData", includesWeatherData);
        }
        goodData = "";
        goodDataMode = false;
      } else {
        goodData += incomingByte;
      } 
    }
    if(endsWith(serialContent, startValidIndication)){ //!goodDataMode && 
      //dataToDisplay += "foundone\n";
      //postData(dataToDisplay + "\n" + serialContent);
      serialContent = "";
      totalSerialChars = 0;
      goodDataMode = true;   
    }
    
    if(canSleep) {
      //this will only work if GPIO16 (D2 on a WEMOS D1) and EXT_RSTB are wired together. see https://www.electronicshub.org/esp8266-deep-sleep-mode/
      if(deep_sleep_time_per_loop > 0) {
        printLine("sleeping...");
        ESP.deepSleep(deep_sleep_time_per_loop * 1e6); 
      }
    }
    //this will only work if GPIO16 and EXT_RSTB are wired together. see https://www.electronicshub.org/esp8266-deep-sleep-mode/
    if(light_sleep_time_per_loop > 0) {
      printLine("snoozing...");
      sleepForSeconds(light_sleep_time_per_loop);
      printLine("awakening...");
      wiFiConnect();
    }
  }
  
  if(responseBuffer != "") {
    sendRemoteData(responseBuffer, "commandout", true);
  }
  
  timeClient.update();
}

void sleepForSeconds(int seconds) {
    wifi_set_opmode(NULL_MODE);            // Turn off Wi-Fi for lower power
    wifi_set_sleep_type(LIGHT_SLEEP_T);    // Enable Light Sleep Mode

    uint32_t sleepEndTime = millis() + seconds * 1000;
    while (millis() < sleepEndTime) {
        delay(10); // Short delays allow CPU to periodically enter light sleep
    }
    // GPIO states are preserved during this period
}


//this is the easiest way I could find to read querystring parameters on an ESP8266. ChatGPT was suprisingly unhelpful
void localSetData() {
  localChangeTime = millis();
  String id = "";
  int onValue = 0;
  for (int i = 0; i < server.args(); i++) {
    if(server.argName(i) == "id") {
      id = server.arg(i);
      //Serial.print(id);
      //Serial.print( " : ");
    } else if (server.argName(i) == "on") {
      onValue = (int)(server.arg(i) == "1");  
    }
    //Serial.print(onValue);
    //Serial.println();
  } 
  for (int i = 0; i < pinMap->size(); i++) {
    String key = pinList[i];
    //Serial.print(key);
    //Serial.print(" ?= ");
    //Serial.println(id);
    if(key == id) {
      pinMap->remove(key);
      pinMap->put(key, onValue);
      //Serial.print("LOCAL SOURCE TRUE :");
      //Serial.println(onValue);
      localSource = true; //sets the NodeMCU into a mode it cannot get out of until the server sends back confirmation it got the data
    }
  }
  server.send(200, "text/plain", "Data received");
}

void localShowData() {
  if(millis() - localChangeTime < 1000) {
    return;
  }
  String deviceNameToUse = deviceName;
  if(deviceNameToUse == "") {
    deviceNameToUse = "Your Device";
  }
  String out = "{\"device\":\"" + deviceNameToUse + "\", \"pins\": [";
  for (int i = 0; i < pinMap->size(); i++) {
    out = out + "{\"id\": \"" + pinList[i] +  "\",\"name\": \"" + pinName[i] +  "\", \"value\": \"" + (String)pinMap->getData(i) + "\"}";
    if(i < pinMap->size()-1) {
      out = out + ", ";
    }
  }
  out += "]}";
  server.send(200, "text/plain", out); 
}


void handleRoot() {
 String s = MAIN_page; //Read HTML contents
 server.send(200, "text/html", s); //Send web page
}

void handleInverterData() {
 String s = dataToDisplay; //Read HTML contents
 server.send(200, "text/plain", s);//Send web page
}


void wiFiConnect() {
  WiFi.persistent(false); //hopefully keeps my flash from being corrupted, see: https://rayshobby.net/wordpress/esp8266-reboot-cycled-caused-by-flash-memory-corruption-fixed/
  WiFi.begin(wifi_ssid, wifi_password);     //Connect to your WiFi router
  Serial.println();
  // Wait for connection
  int wiFiSeconds = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    wiFiSeconds++;

  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(wifi_ssid);
  Serial.print("IP address: ");
  ipAddress =  WiFi.localIP().toString();
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP
}

bool endsWith(const String& str1, const String& str2) {
  if (str2.length() > str1.length()) {
    return false;
  }
  return str1.substring(str1.length() - str2.length()) == str2;
}

String removeSpaces(String str) {
  String result = "";
  for (int i = 0; i < str.length(); i++) {
    if (str[i] != ' ') {
      result += str[i];
    }
  }
  return result;
}

int generateDecimalFromStringPositions(String inData, int start, int stop) {
  String hexValue = removeSpaces(inData.substring(start, stop));
  int out = strtol(hexValue.c_str(), nullptr, 16);
  if (out & 0x8000) {
    // If the MSB is set, adjust the value for two's complement
    out -= 0x10000;
  }
  return out;
}

String parseInverterData(String inData){
  //mysteryValues and changers are values whose meanings I haven't yet determined. i log them on the backend and try to figure them out by context
  int firstChanger = generateDecimalFromStringPositions(inData, 7, 13);
  int secondChanger = generateDecimalFromStringPositions(inData, 25, 31);
  int thirdChanger = generateDecimalFromStringPositions(inData, 67, 73);
  int fourthChanger = generateDecimalFromStringPositions(inData, 75, 81);
  int fifthChanger = generateDecimalFromStringPositions(inData, 91, 97);
  int sixthChanger =  generateDecimalFromStringPositions(inData, 385, 391);
  int seventhChanger =  generateDecimalFromStringPositions(inData, 481, 487);
  int batteryPercent = generateDecimalFromStringPositions(inData, 604, 606);
  int loadPower = generateDecimalFromStringPositions(inData, 607, 613);
  int solarString1  = generateDecimalFromStringPositions(inData, 613, 619);
  int solarString2 = generateDecimalFromStringPositions(inData, 619, 625);
  int mysteryValue1 = generateDecimalFromStringPositions(inData, 625, 631);
  int mysteryValue2 = generateDecimalFromStringPositions(inData, 631, 637);
  int batteryPower = generateDecimalFromStringPositions(inData, 637, 643);
  int gridPower = generateDecimalFromStringPositions(inData, 121, 127);
  int batteryVoltage = generateDecimalFromStringPositions(inData, 595, 601);
  int mysteryValue3 = generateDecimalFromStringPositions(inData, 643, 649);
  //1st is gridPower, 2nd is batteryPercentage, 3rd loadPower, 4th is battery power  (2's complement for negative), 5th and 6th are solar strings
  String out = (String)millis() + "*" + String(gridPower) + "*" + String(batteryPercent) + "*" + String(batteryPower) + "*" + String(loadPower) + "*" + String(solarString1) + "*" + (String)solarString2;
  out += "*" + (String)batteryVoltage;
  out += "*" + (String)mysteryValue3;
  out += "*" + (String)mysteryValue1 + "*" + (String)mysteryValue2;
  out += "*" + (String)firstChanger;
  out += "*" + (String)secondChanger;
  out += "*" + (String)thirdChanger;
  out += "*" + (String)fourthChanger;
  out += "*" + (String)fifthChanger;
  out += "*" + (String)sixthChanger;
  out += "*" + (String)seventhChanger;
  
  //feedbackSerial.println(out);
  return out;
}

//SEND DATA TO A REMOTE SERVER TO STORE IN A DATABASE----------------------------------------------------
void sendRemoteData(String datastring, String mode, bool includesWeatherData){
  printLine(datastring);
  WiFiClient clientGet;
  const int httpGetPort = 80;
  String url;
  postData(datastring + "\n" + goodData);
  if(deviceName == "") {
    mode = "getInitialDeviceInfo";
  }

  String encryptedStoragePassword = encryptStoragePassword(datastring);
  url =  (String)url_get + "?k2=" + encryptedStoragePassword + "&device_id=" + device_id + "&mode=" + mode + "&data=" + urlEncode(datastring, true);
  //Serial.println(host_get);
  int attempts = 0;
  while(!clientGet.connect(host_get, httpGetPort) && attempts < connection_retry_number) {
    attempts++;
    delay(20);
  }

  if (attempts >= connection_retry_number) {
    connectionFailureTime = millis();
    connectionFailureMode = true;
  } else {
     connectionFailureTime = 0;
     connectionFailureMode = false;
     clientGet.println("GET " + url + " HTTP/1.1");
     clientGet.print("Host: ");
     clientGet.println(host_get);
     clientGet.println("User-Agent: ESP8266/1.0");
     clientGet.println("Accept-Encoding: identity");
     clientGet.println("Connection: close\r\n\r\n");
     unsigned long timeoutP = millis();
     while (clientGet.available() == 0) {
       if (millis() - timeoutP > 10000) {
 
        //clientGet.stop();
        if(clientGet.connect(host_get, httpGetPort)){
         //timeOffset = timeOffset + timeSkewAmount; //in case two probes are stepping on each other, make this one skew a 20 seconds from where it tried to upload data
         clientGet.println("GET / HTTP/1.1");
         clientGet.print("Host: ");
         clientGet.println(host_get);
         clientGet.println("User-Agent: ESP8266/1.0");
         clientGet.println("Accept-Encoding: identity");
         clientGet.println("Connection: close\r\n\r\n");
        }//if (clientGet.connect(
        //clientGet.stop();
        return;
       } //if( millis() -  
     }
     
    delay(4); //see if this improved data reception. OMG IT TOTALLY WORKED!!!
    bool receivedData = false;
    bool receivedDataJson = false;
    if(clientGet.available() && ipAddressAffectingChange != "") { //don't turn these globals off until we have data back from the server
       ipAddressAffectingChange = "";
       changeSourceId = 0;
    }
    while(clientGet.available()){
      receivedData = true;
      String retLine = clientGet.readStringUntil('\n');
      retLine.trim();
      if(retLine.indexOf("\"error\":") < 0 && includesWeatherData && (retLine.charAt(0)== '{' || retLine.charAt(0)== '*' || retLine.charAt(0)== '|')) {
       
        lastDataLogTime = millis();
        canSleep = true;  //canSleep is a global and will not be set until all the tasks of the device are finished.
        //also we can switch outputMode to 0 and clear responseBuffer
        outputMode = 0;
        responseBuffer = "";
      }
      //Here the code is designed to be able to handle either JSON or double-delimited data from data.php
      //I started with just JSON, but that's a notoriously bulky data format, what with the names of all the
      //entities embedded and the overhead of quotes and brackets.  This is a problem because when the 
      //amount of data being sent by my server reached some critical threshold (I'm not sure what it is!)
      //it automatically gzipped the data, which I couldn't figure out how to unzip on a ESP8266.
      //So then I made a system of sending only some of the data at a time via JSON.  That introduced a lot of
      //complexity and also made the system less responsive, since you now had to wait for the device_feature to
      //get its turn in a fairly slow round-robin (on a slow internet connection, it would take ten seconds per item).
      //So that's why I implemented the non-JSON data format, which can easily specify the values for all 
      //device_features in one data object (assuming it's not too big). The ESP8266 still can respond to data in the
      //JSON format, which it will assume if the first character of the data is a '{' -- but if the first character
      //is a '|' then it assumes the data is non-JSON. Otherwise it assumes it's HTTP boilerplate and ignores it.
      printLine(retLine);
      if(retLine.charAt(0) == '*') { //getInitialDeviceInfo
        //Serial.print("Initial Device Data: ");
        //Serial.println(retLine);
        //set the global string; we'll just use that to store our data about addtional sensors
        if(sensor_config_string != "") {
          retLine = replaceFirstOccurrenceAtChar(retLine, String(sensor_config_string), '|');
          //retLine = retLine + "|" + String(sensor_config_string); //had been doing it this way; not as good!
        }
        additionalSensorInfo = retLine;
        //once we have it
        handleDeviceNameAndAdditionalSensors((char *)additionalSensorInfo.c_str(), true);
        break;
      } else if(retLine.charAt(0) == '*') { //getInitialDeviceInfo
        //set the global string; we'll just use that to store our data about addtional sensors
        /*
        if(sensor_config_string != "") {
          retLine = replaceFirstOccurrenceAtChar(retLine, String(sensor_config_string), '|');
          //retLine = retLine + "|" + String(sensor_config_string); //had been doing it this way; not as good!
        }
        */
        //additionalSensorInfo = retLine;
        //once we have it
        handleDeviceNameAndAdditionalSensors((char *)additionalSensorInfo.c_str(), true);
        break;
      } else if(retLine.charAt(0) == '{') { //we don't handle JSON so no need for this
        //setLocalHardwareToServerStateFromJson((char *)retLine.c_str());
        //receivedDataJson = true;
        break; 
      } else if(retLine.charAt(0) == '|') {
        String serverCommandParts[2];
        splitString(retLine, '!', serverCommandParts, 2);
        setLocalHardwareToServerStateFromNonJson((char *)serverCommandParts[0].c_str());
        if(retLine.indexOf("!") > -1) {
          if(serverCommandParts[1].length()>5) { //just has latency data
            //Serial.print("COMMAND (beside pin data): ");
            //Serial.println(serverCommandParts[1]);
          } 
          runCommandsFromNonJson((char *)("!" + serverCommandParts[1]).c_str());
        }
        receivedDataJson = true;
        break;   
      } else if(retLine.charAt(0) == '!') { //it's a command, so an exclamation point seems right
        //Serial.print("COMMAND: ");
        printLine(retLine);
        runCommandsFromNonJson((char *)retLine.c_str());
        break;         
      } else {
      }
    }
   
  } //if (attempts >= connection_retry_number)....else....    
  clientGet.stop();
}

void runCommandsFromNonJson(char * nonJsonLine){
  //can change the default values of some config data for things like polling
  String command;
  int commandId;
  String commandData;
  String commandArray[4];
  int latency;
  //first get rid of the first character, since all it does is signal that we are receiving a command:
  nonJsonLine++;
  splitString(nonJsonLine, '|', commandArray, 3);
  commandId = commandArray[0].toInt();
  command = commandArray[1];
  commandData = commandArray[2];
  latencyCount++;
  latency = commandArray[3].toInt();
  latencySum += latency;
  if(commandId == -2) {
    outputMode = 2;
  }
  if(commandId) {
    if(command == "reboot") {
      rebootEsp();
    } else if(command == "one pin at a time") {
      //onePinAtATimeMode = (boolean)commandData.toInt(); //setting a global.
    } else if(command == "sleep seconds per loop") {
      deep_sleep_time_per_loop = commandData.toInt(); //setting a global.
    } else if(command == "snooze seconds per loop") {
      light_sleep_time_per_loop = commandData.toInt(); //setting a global
    } else if(command == "polling granularity") {
      //polling_granularity = commandData.toInt(); //setting a global.
    } else if(command == "logging granularity") {
      data_logging_granularity = commandData.toInt(); //setting a global.
    } else if(command == "clear latency average") {
      latencyCount = 0;
      latencySum = 0;
    } else if (command ==  "get uptime") {
      textOut("Last booted: " + timeAgo("") + "\n");
    } else if (command ==  "get wifi uptime") {
      textOut("WiFi up: " + msTimeAgo(wifiOnTime) + "\n");
    } else if (command ==  "get lastpoll") {
      //textOut("Last poll: " + msTimeAgo(lastPoll) + "\n");
    } else if (command ==  "get lastdatalog") {
      textOut("Last data: " + msTimeAgo(lastDataLogTime) + "\n");
    } else if (command == "get memory") {
      dumpMemoryStats(0);
    } else if (command == "set debug") {
      debug = true;
    } else if (command == "clear debug") {
      debug = false;
    } else if(command == "ir") {
      sendIr(commandData); //ir data must be comma-delimited
    }
    lastCommandId = commandId;
  }
}

void sendIr(String rawDataStr) {
  irsend.begin();
  //Example input string
  //rawDataStr = "500,1500,500,1500,1000";
  size_t rawDataLength = 0;

  // Count commas to determine array length
  for (size_t i = 0; i < rawDataStr.length(); i++) {
    if (rawDataStr[i] == ',') rawDataLength++;
  }
  rawDataLength++; // Account for the last value
  // Allocate array
  uint16_t* rawData = (uint16_t*)malloc(rawDataLength * sizeof(uint16_t));

  // Parse the string into the array
  size_t index = 0;
  int start = 0;
  for (size_t i = 0; i <= rawDataStr.length(); i++) {
    if (rawDataStr[i] == ',' || rawDataStr[i] == '\0') {
      rawData[index++] = rawDataStr.substring(start, i).toInt();
      start = i + 1;
    }
  }
  // Send the parsed raw data
  irsend.sendRaw(rawData, rawDataLength, 38);
  //Serial.println("IR signal sent!");
  free(rawData); // Free memory
}


String handleDeviceNameAndAdditionalSensors(char * sensorData, bool intialize){
  feedbackPrint(sensorData);
  String additionalSensorArray[12];
  String specificSensorData[8];
  int i2c;
  int pinNumber;
  int powerPin;
  int sensorIdLocal;
  int sensorSubTypeLocal;
  int deviceFeatureId;
  int consolidateAllSensorsToOneRecord = 0;
  int ordinalOfOverwrite;
  String out = "";
  int objectCursor = 0;
  int oldsensor_id = -1;
  String sensorName;
  splitString(sensorData, '|', additionalSensorArray, 12);
  deviceName = additionalSensorArray[0].substring(1);
  requestNonJsonPinInfo = 1; //set this global
  for(int i=1; i<12; i++) {
    String sensorDatum = additionalSensorArray[i];
    if(sensorDatum.indexOf('*')>-1) {
      splitString(sensorDatum, '*', specificSensorData, 8);
      pinNumber = specificSensorData[0].toInt();
      powerPin = specificSensorData[1].toInt();
      sensorIdLocal = specificSensorData[2].toInt();
      sensorSubTypeLocal = specificSensorData[3].toInt();
      i2c = specificSensorData[4].toInt();
      deviceFeatureId = specificSensorData[5].toInt();
      sensorName = specificSensorData[6];
      ordinalOfOverwrite = specificSensorData[7].toInt();
      consolidateAllSensorsToOneRecord = specificSensorData[8].toInt();
      if(oldsensor_id != sensorIdLocal) { //they're sorted by sensor_id, so the objectCursor needs to be set to zero if we're seeing the first of its type
        objectCursor = 0;
      }
      if(sensorIdLocal == sensor_id) { //this particular additional sensor is the same type as the base (non-additional) sensor, so we have to pre-start it higher
        objectCursor++;
      }
      if(intialize) {
        startWeatherSensors(sensorIdLocal, sensorSubTypeLocal, i2c, pinNumber, powerPin); //guess i have to pass all this additional info
      } else {
        //otherwise do a weatherDataString
        out = out + "!" + weatherDataString(sensorIdLocal, sensorSubTypeLocal, pinNumber, powerPin, i2c, deviceFeatureId, objectCursor, sensorName, ordinalOfOverwrite, consolidateAllSensorsToOneRecord);
      }
      objectCursor++;
      oldsensor_id = sensorIdLocal;
    }
  }
 return out;
}

void handleWeatherData() {
  String transmissionString;
  if(sensor_id > -1) {
     transmissionString = weatherDataString(sensor_id, sensor_sub_type, sensor_data_pin, sensor_power_pin, sensor_i2c, NULL, 0, deviceName, -1, consolidate_all_sensors_to_one_record);
  }
  server.send(200, "text/plain", transmissionString); //Send values only to client ajax request
}

//if the backend sends too much text data at once, it is likely to get gzipped, which is hard to deal with on a microcontroller with limited resources
//so a better strategy is to send double-delimited data instead of JSON, with data consistently in known ordinal positions
//thereby making the data payloads small enough that the server never gzips them
//i've made it so the ESP8266 can receive data in either format. it takes the lead on specifying which format it prefers
//but if it misbehaves, i can force it to be one format or the other remotely 
void setLocalHardwareToServerStateFromNonJson(char * nonJsonLine){
  int pinNumber = 0;
  String key;
  int value = -1;
  int canBeAnalog = 0;
  int enabled = 0;
  int pinCounter = 0;
  int serverSaved = 0;
  String friendlyPinName = "";
  String nonJsonPinArray[12];
  String nonJsonDatumString;
  String nonJsonPinDatum[5];
  String pinIdParts[2];
  char i2c = 0;
  splitString(nonJsonLine, '|', nonJsonPinArray, 12);
  int foundPins = 0;
  //pinMap->clear();
  for(int i=1; i<12; i++) {
    nonJsonDatumString = nonJsonPinArray[i];
    if(nonJsonDatumString.indexOf('*')>-1) {  
      splitString(nonJsonDatumString, '*', nonJsonPinDatum, 5);
      key = nonJsonPinDatum[1];
      friendlyPinName = nonJsonPinDatum[0];
      value = nonJsonPinDatum[2].toInt();
      pinName[foundPins] = friendlyPinName;
      canBeAnalog = nonJsonPinDatum[3].toInt();
      serverSaved = nonJsonPinDatum[4].toInt();

      if(key.indexOf('.')>0) {
        splitString(key, '.', pinIdParts, 2);
        i2c = pinIdParts[0].toInt();
        pinNumber = pinIdParts[1].toInt();
      } else {
        pinNumber = key.toInt();
      }
      //Serial.println("!ABOUT TO TURN OFF localsource: " + (String)localSource +  " serverSAVED: " + (String)serverSaved);
      if(!localSource || serverSaved == 1){
        if(serverSaved == 1) {//confirmation of serverSaved, so localSource flag is no longer needed
          if(debug) {
            Serial.println("SERVER SAVED==1!!");
          }
          localSource = false;
        } else {
          pinMap->remove(key);
          pinMap->put(key, value);
          //Serial.print(key);
          //Serial.print(": ");
          //Serial.println(pinMap->getIndex(key));
        }
      }
      pinList[foundPins] = key;
      pinMode(pinNumber, OUTPUT);
      if(i2c > 0) {
        //Serial.print("Non-JSON i2c: ");
        //Serial.println(key);
        setPinValueOnSlave(i2c, (char)pinNumber, (char)value); 
      } else {
        if(canBeAnalog) {
          analogWrite(pinNumber, value);
        } else {
          //Serial.print("Non-JSON reg: ");
          //Serial.println(key);
          if(value > 0) {
            digitalWrite(pinNumber, HIGH);
          } else {
            digitalWrite(pinNumber, LOW);
          }
        }
      }
      foundPins++;
    }
    
  }
  pinTotal = foundPins;
}

///////////////////////////////////////////////
void rebootEsp() {
  feedbackSerial.print("Rebooting ESP");
  ESP.restart();
}

 
//////////////////////////////////////////////
long getPinValueOnSlave(char i2cAddress, char pinNumber) { //might want a user-friendlier API here
  //reading an analog or digital value from the slave:
  Wire.beginTransmission(i2cAddress);
  Wire.write(pinNumber); //addresses greater than 64 are the same as AX (AnalogX) where X is 64-value
  Wire.endTransmission();
  //delay(100); 
  Wire.requestFrom(i2cAddress, 4); //we only ever get back four-byte long ints
  long totalValue = 0;
  int byteCursor = 1;
  while (Wire.available()) {
    byte receivedValue = Wire.read(); // Read the received value from slave
    totalValue = totalValue + receivedValue * pow(256, 4-byteCursor);
    //Serial.println(receivedValue); // Print the received value
    byteCursor++;
  }
  return totalValue;
}

void setPinValueOnSlave(char i2cAddress, char pinNumber, char pinValue) {
  //if you have a slave Arduino set up with this code:
  //https://github.com/judasgutenberg/Generic_Arduino_I2C_Slave
  //and a device_type_feature specifies an i2c address
  //then this code will send the data to that slave Arduino
  Wire.beginTransmission(i2cAddress);
  Wire.write(pinNumber);
  Wire.write(pinValue);
  Wire.endTransmission();
}

time_t parseDateTime(String dateTime) {
    int year, month, day, hour, minute, second;
    
    // Convert String to C-style char* for parsing
    if (sscanf(dateTime.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
        return 0; // Return 0 if parsing fails
    }

    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = second;
    
    return mktime(&t); // Convert struct tm to Unix timestamp
}


String msTimeAgo(long millisFromPast) {
  return humanReadableTimespan((uint32_t) (millis() - millisFromPast)/1000);
}
 
// Overloaded version: Uses NTP time as the default comparison
String timeAgo(String sqlDateTime) {
    return timeAgo(sqlDateTime, timeClient.getEpochTime());
}

// Returns a human-readable "time ago" string
String timeAgo(String sqlDateTime, time_t compareTo) {
    time_t past;
    time_t nowTime;

    if (sqlDateTime.length() == 0) {
        // If an empty string is passed, use millis() for uptime
        unsigned long uptimeSeconds = millis() / 1000;
        nowTime = uptimeSeconds;
        past = 0;
    } else {
        past = parseDateTime(sqlDateTime);
        nowTime = compareTo;
    }

    if (past == -1 || past > nowTime) {
        return "Invalid time";
    }

    time_t diffInSeconds = nowTime - past;
    return humanReadableTimespan((uint32_t) diffInSeconds);
}

String humanReadableTimespan(uint32_t diffInSeconds) {
    int seconds = diffInSeconds % 60;
    int minutes = (diffInSeconds / 60) % 60;
    int hours = (diffInSeconds / 3600) % 24;
    int days = diffInSeconds / 86400;

    if (days > 0) {
        return String(days) + (days == 1 ? " day ago" : " days ago");
    }
    if (hours > 0) {
        return String(hours) + (hours == 1 ? " hour ago" : " hours ago");
    }
    if (minutes > 0) {
        return String(minutes) + (minutes == 1 ? " minute ago" : " minutes ago");
    }
    return String(seconds) + (seconds == 1 ? " second ago" : " seconds ago");
}


/////////////////////////////////////////////
//utility functions
/////////////////////////////////////////////
void textOut(String data){
  if(outputMode == 2) {
    //sendRemoteData(data, "commandout", 0xFFFF); //do this in loop, not now
    responseBuffer += data;
  } else {
    feedbackPrint(data);
  }
}


int freeMemory() {
    return ESP.getFreeHeap();
}

String makeAsteriskString(uint8_t number){
  String out = "";
  for(uint8_t i=0; i<number; i++) {
    out += "*";
  }
  return out;
}


void dumpMemoryStats(int marker){
  char buffer[80]; 
  sprintf(buffer, "Memory (%d): Free=%d, MaxBlock=%d, Fragmentation=%d%%\n", marker,
                  ESP.getFreeHeap(), ESP.getMaxFreeBlockSize(),
                  100 - (ESP.getMaxFreeBlockSize() * 100 / ESP.getFreeHeap()));
  textOut(String(buffer));
}

String urlEncode(String str, bool minimizeImpact) {
  String encodedString = "";
  char c;
  char hexDigits[] = "0123456789ABCDEF"; // Hex conversion lookup
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += "%20";
    } else if (isalnum(c) || c == '.' || (minimizeImpact && ( c == '|'  || c == '*' ||  c == ','))) {
      encodedString += c;
    } else {
      encodedString += '%';
      encodedString += hexDigits[(c >> 4) & 0xF];
      encodedString += hexDigits[c & 0xF];
    }
  }
  return encodedString;
}

String joinValsOnDelimiter(long vals[], String delimiter, int numberToDo) {
  String out = "";
  for(int i=0; i<numberToDo; i++){
    out = out + (String)vals[i];
    if(i < numberToDo-1) {
      out = out + delimiter;
    }
  }
  return out;
}

String joinMapValsOnDelimiter(SimpleMap<String, int> *pinMap, String delimiter) {
  String out = "";
  for (int i = 0; i < pinMap->size(); i++) {
    //i had to rework this because pinMap was saving out of order in some cases
    String key = pinList[i];
    out = out + (String)pinMap->get(key);
    if(i < pinMap->size()- 1) {
      out = out + delimiter;
    }
  }
  return out;
}

String nullifyOrNumber(double inVal) {
  if(inVal == NULL) {
    return "";
  } else {
    return String(inVal);
  }
}

String nullifyOrInt(int inVal) {
  if(inVal == NULL) {
    return "";
  } else {
    return String(inVal);
  }
}

void shiftArrayUp(long array[], long newValue, int arraySize) {
    // Shift elements down by one index
    for (int i =  1; i < arraySize ; i++) {
        array[i - 1] = array[i];
    }
    // Insert the new value at the beginning
    array[arraySize - 1] = newValue;
}

void splitString(const String& input, char delimiter, String* outputArray, int arraySize) {
  int lastIndex = 0;
  int count = 0;
  for (int i = 0; i < input.length(); i++) {
    if (input.charAt(i) == delimiter) {
      // Extract the substring between the last index and the current index
      outputArray[count++] = input.substring(lastIndex, i);
      lastIndex = i + 1;
      if (count >= arraySize) {
        break;
      }
    }
  }
  // Extract the last substring after the last delimiter
  outputArray[count++] = input.substring(lastIndex);
}

String replaceFirstOccurrenceAtChar(String str1, String str2, char atChar) { //thanks ChatGPT!
    int index = str1.indexOf(atChar);
    if (index == -1) {
        // If there's no '|' in the first string, return it unchanged.
        return str1;
    }
    String beforeDelimiter = str1.substring(0, index); // Part before the first '|'
    String afterDelimiter = str1.substring(index + 1); // Part after the first '|'

    // Construct the new string with the second string inserted.
    String result = beforeDelimiter + "|" + str2 + "|" + afterDelimiter;
    return result;
}

String replaceNthElement(const String& input, int n, const String& replacement, char delimiter) {
  String result = "";
  int currentIndex = 0;
  int start = 0;
  int end = input.indexOf(delimiter);

  while (end != -1 || start < input.length()) {
    if (currentIndex == n) {
      // Replace the nth element
      result += replacement;
    } else {
      // Append the current element
      result += input.substring(start, end != -1 ? end : input.length());
    }
    if (end == -1) break; // No more delimiters
    result += delimiter; // Append the delimiter
    start = end + 1;     // Move to the next element
    end = input.indexOf(delimiter, start);
    currentIndex++;
  }

  // If n is out of range, return the original string
  if (n >= currentIndex) {
    return input;
  }
  return result;
}


byte calculateChecksum(String input) {
    byte checksum = 0;
    for (int i = 0; i < input.length(); i++) {
        checksum += input[i];
    }
    return checksum;
}

byte countSetBitsInString(const String &input) {
    byte bitCount = 0;
    // Iterate over each character in the string
    for (size_t i = 0; i < input.length(); ++i) {
        char c = input[i];
        
        // Count the set bits in the ASCII value of the character
        for (int bit = 0; bit < 8; ++bit) {
            if (c & (1 << bit)) {
                bitCount++;
            }
        }
    }
    return bitCount;
}


byte countZeroes(const String &input) {
    byte zeroCount = 0;
    // Iterate over each character in the string
    for (size_t i = 0; i < input.length(); ++i) {
        char c = input[i];
        // Count the zeroes
        if (c == 48) {
            zeroCount++;
        }
   
    }
    return zeroCount;
}

/*
String oldEncryptStoragePassword(String datastring) {
  int timeStamp = timeClient.getEpochTime();
  char buffer[10];
  itoa(timeStamp, buffer, 10);  // Base 10 conversion
  String timestampString = String(buffer);
  byte checksum = calculateChecksum(datastring);
  String encryptedStoragePassword = urlEncode(simpleEncrypt(simpleEncrypt((String)storage_password, timestampString.substring(1,9), salt), String((char)countSetBitsInString(datastring)), String((char)checksum)), false);
  return encryptedStoragePassword;
}
*/

uint8_t rotateLeft(uint8_t value, uint8_t count) {
    return (value << count) | (value >> (8 - count));
}

uint8_t rotateRight(uint8_t value, uint8_t count) {
    return (value >> count) | (value << (8 - count));
}

//let's define how encryption_scheme works:
//a data_string is passed in as is the storage_password.  we want to be able to recreate the storage_password server-side, so no can be lossy on the storage_password
//it's all based on nibbles. two operations are performed by byte from these two nibbles, upper nibble first, then lower nibble
//00: do nothing (byte unchanged)
//01: xor storage_password at this position with count_of_zeroes in dataString
//02: xor between storage_password at this position and timestampString at this position.
//03: xor between storage_password at this position and data_string  at this position.
//04: xor storage_password with ROL data_string at this position
//05: xor storage_password with ROR data_string at this position
//06: xor storage_password with checksum of entire data_string;
//07: xor storage_password with checksum of entire stringified timestampString
//08: xor storage_password with set_bits of entire data_string
//09  xor storage_password with set_bits in entire timestampString
//10: ROL this position of storage_password 
//11: ROR this position of storage_password 
//12: ROL this position of storage_password twice
//13: ROR this position of storage_password twice
//14: invert byte of storage_password at this position (zeroes become ones and ones become zeroes)
//15: xor storage_password at this position with count_of_zeroes in full timestampString
String encryptStoragePassword(String datastring) {
    int timeStamp = timeClient.getEpochTime();
    char buffer[10];
    itoa(timeStamp, buffer, 10);  // Convert timestamp to string
    String timestampStringInterim = String(buffer);
    String timestampString = timestampStringInterim.substring(1,8); //second param is OFFSET
    //Serial.print(timestampString);
    //Serial.println("<=known to frontend");
    //timestampString = "0123227";
    // Convert encryption_scheme into an array of nibbles
    uint8_t nibbles[16];  // 64-bit number has 16 nibbles
    for (int i = 0; i < 16; i++) {
        nibbles[i] = (encryption_scheme >> (4 * (15 - i))) & 0xF;  // Extract nibble
    }
    // Process nibbles
    String processedString = "";
    int counter = 0;
    uint8_t thisByteResult;
    uint8_t thisByteOfStoragePassword;
    uint8_t thisByteOfDataString;
    uint8_t thisByteOfTimestampString;
    uint8_t dataStringChecksum = calculateChecksum(datastring);
    uint8_t timestampStringChecksum = calculateChecksum(timestampString);
    uint8_t dataStringSetBits = countSetBitsInString(datastring);
    uint8_t timestampStringSetBits = countSetBitsInString(timestampString);
    uint8_t dataStringZeroCount = countZeroes(datastring);
    uint8_t timestampStringZeroCount = countZeroes(timestampString);
    for (int i = 0; i < strlen(storage_password) * 2; i++) {
  
        thisByteOfDataString = datastring[counter % datastring.length()];
        thisByteOfTimestampString = timestampString[counter % timestampString.length()];
        /*
        Serial.print(timestampString);
        Serial.print(":");
        Serial.print(timestampString[counter % timestampString.length()]);
        Serial.print(":");
        Serial.print(counter);
        Serial.print(":");
        Serial.println(counter % timestampString.length());
        */
        if(i % 2 == 0) {
          thisByteOfStoragePassword = storage_password[counter];
        } else {
          thisByteOfStoragePassword = thisByteResult;
        }
        uint8_t nibble = nibbles[i % 16];
        if(nibble == 0) {
          //do nothing
        } else if(nibble == 1) { 
          thisByteResult = thisByteOfStoragePassword ^ dataStringZeroCount;
        } else if(nibble == 2) { 
          thisByteResult = thisByteOfStoragePassword ^ thisByteOfTimestampString;
        } else if(nibble == 3) { 
          thisByteResult = thisByteOfStoragePassword ^ thisByteOfDataString;
        } else if(nibble == 4) { 
          thisByteResult = thisByteOfStoragePassword ^ rotateLeft(thisByteOfDataString, 1);
        } else if(nibble == 5) { 
          thisByteResult = thisByteOfStoragePassword ^ rotateRight(thisByteOfDataString, 1);
        } else if(nibble == 6) { 
          thisByteResult = thisByteOfStoragePassword ^ dataStringChecksum;
        } else if(nibble == 7) { 
          thisByteResult = thisByteOfStoragePassword ^ timestampStringChecksum;
        } else if(nibble == 8) { 
          thisByteResult = thisByteOfStoragePassword ^ dataStringSetBits;
        } else if(nibble == 9) { 
          thisByteResult = thisByteOfStoragePassword ^ timestampStringSetBits;
        } else if(nibble == 10) { 
          thisByteResult = rotateLeft(thisByteOfStoragePassword, 1);
        } else if(nibble == 11) { 
          thisByteResult = rotateRight(thisByteOfStoragePassword, 1);
        } else if(nibble == 12) { 
          thisByteResult = rotateLeft(thisByteOfStoragePassword, 2);
        } else if(nibble == 13) { 
          thisByteResult = rotateRight(thisByteOfStoragePassword, 2);
        } else if(nibble == 14) { 
          thisByteResult = ~thisByteOfStoragePassword; //invert the byte
        } else if(nibble == 15) { 
          thisByteResult = thisByteOfStoragePassword ^ timestampStringZeroCount;
        }
        
        // Advance the counter every other nibble
        
        if (i % 2 == 1) {
            char hexStr[3];
            sprintf(hexStr, "%02X", thisByteResult); 
            String paddedHexStr = String(hexStr); 
            processedString += paddedHexStr;  // Append nibble as hex character
            /*
            Serial.print("%");
            Serial.print(thisByteOfStoragePassword);
            Serial.print(",");
            Serial.print(thisByteOfDataString);
            Serial.print("|");
            Serial.print(thisByteOfTimestampString);
            Serial.print("|");
            Serial.print(dataStringZeroCount);
            Serial.print("|");
            Serial.print(timestampStringZeroCount);
            Serial.print(" via: ");
            Serial.print(nibble);
            Serial.print("=>");
            Serial.println(thisByteResult);
            */
            counter++;
        } else {
            /*
            Serial.print("&");
            Serial.print(thisByteOfStoragePassword);
            Serial.print(",");
            Serial.print(thisByteOfDataString);
            Serial.print("|");
            Serial.print(thisByteOfTimestampString);
            Serial.print("|");
            Serial.print(dataStringZeroCount);
            Serial.print("|");
            Serial.print(timestampStringZeroCount);
            Serial.print(" via: ");
            Serial.print(nibble);
            Serial.print("=>");
            Serial.println(thisByteResult);
            */
            
        }
        //Serial.print(":");
    }
    return processedString;
}



void cleanup(){
  ESP.getFreeHeap(); // This sometimes triggers internal cleanup
  yield();    
}

///print utils -- comment-out as needed to keep serial line pure

void feedbackPrint(int value){
  feedbackSerial.print((String)value);
  //Serial.println(value);
}

void feedbackPrint(String value){
  feedbackSerial.print(value);
}

void printLine(String value){
 //normally commented-out
 //Serial.println(value);
}

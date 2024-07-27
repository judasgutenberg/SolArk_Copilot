//Gus Mueller, June 29, 2024
//uses an ESP8266 wired with the swap serial pins (D8 as TX and D7 as RX) connected to the exposed serial header on the ESP32 in the SolArk's WiFi dongle.
//this intercepts the communication data between the SolArk and the dongle to get frequent updates (that is, every few seconds) of the power and battery levels.
//the data still makes it to PowerView (now MySolArk) but you have access to it much sooner, locally, and at much finer granularity
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <SoftwareSerial.h>


#include <SimpleMap.h>


#include "config.h"

#include "Zanshin_BME680.h"  // Include the BME680 Sensor library
#include <DHT.h>
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
DHT* dht[6];
SFE_BMP180 BMP180[2];
BME680_Class BME680[2];
Adafruit_BMP085 BMP085d[2];
Generic_LM75 LM75[12];
Adafruit_BMP280 BMP280[2];


String serialContent = "";
String ipAddress;
String goodData;
String dataToDisplay;
String deviceName = "";
long lastDataLogTime = 0;
long connectionFailureTime;
bool connectionFailureMode = false;
ESP8266WebServer server(80); //Server on port 80
SoftwareSerial feedbackSerial(3, 1);

String additionalSensorInfo; //we keep it stored in a delimited string just the way it came from the server and unpack it periodically to get the data necessary to read sensors
int pinTotal = 12;
String pinList[12]; //just a list of pins
String pinName[12]; //for friendly names
bool localSource = false;
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
int moxeeRebootCount = 0;
long moxeeRebootTimes[] = {0,0,0,0,0,0,0,0,0,0,0};

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
  
  Serial.println("about to swap");
  delay(4000);
  Serial.swap();
  feedbackSerial.begin(115200);
  
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
    //Serial.print(F("- Setting gas measurement to 320\xC2\xB0\x43 for 150ms\n"));  // "�C" symbols
    BME680[objectCursor].setGas(320, 150);  // 320�c for 150 milliseconds
  } else if (sensorIdLocal == 2301) {
    Serial.print(F("Initializing DHT AM2301 sensor at pin: "));
    if(powerPin > -1) {
      pinMode(powerPin, OUTPUT);
      digitalWrite(powerPin, LOW);
    }
    dht[objectCursor] = new DHT(pinNumber, sensorSubTypeLocal);
    dht[objectCursor]->begin();
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
String weatherDataString(int sensor_id, int sensor_sub_type, int dataPin, int powerPin, int i2c, int deviceFeatureId, char objectCursor, String sensorName,  int consolidateAllSensorsToOneRecord) {
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
  if(glblRemote) {
    sensorName = urlEncode(sensorName);
  }
  if(deviceFeatureId == NULL) {
    objectCursor = 0;
  }
  if(sensor_id == 1) { //simple analog input. we can use subType to decide what kind of sensor it is!
    //an even smarter system would somehow be able to put together multiple analogReads here
    if(powerPin > -1) {
      digitalWrite(powerPin, HIGH); //turn on sensor power. 
    }
    //delay(10);
    double value = NULL;
    if(i2c){
      //i forget how we read a pin on an i2c slave. lemme see:
      value = (double)getPinValueOnSlave((char)i2c, (char)dataPin);
    } else {
      value = (double)analogRead(dataPin);
    }
    for(char i=0; i<12; i++){ //we have 12 separate possible sensor functions:
      //temperature*pressure*humidity*gas*windDirection*windSpeed*windIncrement*precipitation*reserved1*reserved2*reserved3*reserved4
      //if you have some particular sensor communicating through a pin and want it to be one of these
      //you set sensor_sub_type to be the 0-based value in that *-delimited string
      //i'm thinking i don't bother defining the reserved ones and just let them be application-specific and different in different implementations
      //a good one would be radioactive counts per unit time
      if((int)i == sensor_sub_type) {
        transmissionString = transmissionString + nullifyOrNumber(value);
      }
      transmissionString = transmissionString + "*";
    }
    //note, if temperature ends up being NULL, the record won't save. might want to tweak data.php to save records if it contains SOME data
    
    if(powerPin > -1) {
      digitalWrite(powerPin, LOW);
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
    humidityValue = (double)humidityRaw/1000;
    temperatureValue = (double)temperatureRaw/100;
    pressureValue = (double)pressureRaw/100;
    gasValue = (double)gasRaw/100;  //all i ever get for this is 129468.6 and 8083.7
  } else if (sensor_id == 2301) { //i love the humble DHT
    if(powerPin > -1) {
      digitalWrite(powerPin, HIGH); //turn on DHT power, in case you're doing that. 
    }
    //delay(10);
    humidityValue = (double)dht[objectCursor]->readHumidity();
    temperatureValue = (double)dht[objectCursor]->readTemperature();
    pressureValue = NULL; //really should set unknown values as null
    if(powerPin > -1) {
      digitalWrite(powerPin, LOW);//turn off DHT power. maybe it saves energy, and that's why MySpool did it this way
    }
  } else if(sensor_id == 280) {
    humidityValue = NULL;
    temperatureValue = BMP280[objectCursor].readTemperature();
    pressureValue = BMP280[objectCursor].readPressure()/100;
  } else if(sensor_id == 180) { //so much trouble for a not-very-good sensor 
    //BMP180 code:
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
      status = BMP180[objectCursor].getTemperature(temperatureValue);
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
          status = BMP180[objectCursor].getPressure(pressureValue,temperatureValue);
          if (status == 0) {
            Serial.println("error retrieving pressure measurement\n");
          }
        } else {
          Serial.println("error starting pressure measurement\n");
        }
      } else {
        Serial.println("error retrieving temperature measurement\n");
      }
    } else {
      Serial.println("error starting temperature measurement\n");
    }
    humidityValue = NULL; //really should set unknown values as null
  } else if (sensor_id == 85) {
    //https://github.com/adafruit/Adafruit-BMP085-Library
    temperatureValue = BMP085d[objectCursor].readTemperature();
    pressureValue = BMP085d[objectCursor].readPressure()/100; //to get millibars!
    humidityValue = NULL; //really should set unknown values as null
  } else if (sensor_id == 75) { //LM75, so ghetto
    //https://electropeak.com/learn/interfacing-lm75-temperature-sensor-with-arduino/
    temperatureValue = LM75[objectCursor].readTemperatureC();
    pressureValue = NULL;
    humidityValue = NULL;
  } else { //either sensor_id is NULL or 0
    //no sensor at all
    temperatureValue = NULL;//don't want to save a record in weather_data from an absent sensor, so force temperature NULL
    pressureValue = NULL;
    humidityValue = NULL;
  }
  //
  if(sensor_id > 1) {
    transmissionString = nullifyOrNumber(temperatureValue) + "*" + nullifyOrNumber(pressureValue);
    transmissionString = transmissionString + "*" + nullifyOrNumber(humidityValue);
    transmissionString = transmissionString + "*" + nullifyOrNumber(gasValue);
    transmissionString = transmissionString + "*********"; //for esoteric weather sensors that measure wind and precipitation.  the last four are reserved for now
  }
  //using delimited data instead of JSON to keep things simple
  transmissionString = transmissionString + nullifyOrInt(sensor_id) + "*" + nullifyOrInt(deviceFeatureId) + "*" + sensorName + "*" + nullifyOrInt(consolidateAllSensorsToOneRecord); 
  return transmissionString;
}

void postData(String datastring){
  WiFiClient clientGet;
  const int httpGetPort = 80;
  HTTPClient http;
  String url;
  String mode = "debug";
  String allData =  "storagePassword=" + (String)storage_password + "&locationId=" + device_id + "&mode=debug" + mode + "&data=" + datastring;
  url = "http://" + (String)host_get + ":" + String(httpGetPort) + url_get;
  
  http.begin(clientGet, url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST(allData);
 }

 
void loop() {
  // put your main code here, to run repeatedly:
  // send data only when you receive data:
  char incomingByte = ' ';
  String startValidIndication = "MB_real data,seg_cnt:3\r\r";
  long nowTime = millis();
  if(nowTime > 1000 * 86400 * 7 || nowTime < hotspot_limited_time_frame * 1000  && moxeeRebootCount >= number_of_hotspot_reboots_over_limited_timeframe_before_esp_reboot) {
    feedbackSerial.print("MOXEE REBOOT COUNT: ");
    feedbackSerial.print(moxeeRebootCount);
    feedbackSerial.println();
    rebootEsp();
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
      if (incomingByte == '\r'){
        //dataToDisplay = goodData;
        int packetSize = goodData.length();
        dataToDisplay =  parseData(goodData);
        if(millis() - lastDataLogTime > data_logging_granularity * 1000) {
          if(sensor_id > -1) {
            glblRemote = true;
            String weatherData = weatherDataString(sensor_id, sensor_sub_type, sensor_data_pin, sensor_power_pin, sensor_i2c, NULL, 0, deviceName, consolidate_all_sensors_to_one_record);
            glblRemote = false;
            dataToDisplay += "*" + (String)packetSize + "!" +  weatherData;
            feedbackSerial.println(packetSize);
            String additionalSensorData = handleDeviceNameAndAdditionalSensors((char *)additionalSensorInfo.c_str(), false);
            dataToDisplay +=  additionalSensorData;
            lastDataLogTime = millis();
          }
        }
        dataToDisplay += "||" + joinMapValsOnDelimiter(pinMap, "*", pinTotal) + "|***" + ipAddress + "*" + requestNonJsonPinInfo + "*0";
        feedbackSerial.println(dataToDisplay); 
        if(packetSize == 160) {
          sendRemoteData(dataToDisplay, "saveLocallyGatheredSolarData");
        } else {
          sendRemoteData(dataToDisplay, "suspectLocallyGatheredSolarData");
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
  }
  if(!goodDataMode) {
    server.handleClient();
  }

}


//this is the easiest way I could find to read querystring parameters on an ESP8266. ChatGPT was suprisingly unhelpful
void localSetData() {
  localChangeTime = millis();
  String id = "";
  int onValue = 0;
  for (int i = 0; i < server.args(); i++) {
    if(server.argName(i) == "id") {
      id = server.arg(i);
      Serial.print(id);
      Serial.print( " : ");
    } else if (server.argName(i) == "on") {
      onValue = (int)(server.arg(i) == "1");  
    }
    Serial.print(onValue);
    Serial.println();
  } 
  for (int i = 0; i < pinMap->size(); i++) {
    String key = pinList[i];
    Serial.print(key);
    Serial.print(" ?= ");
    Serial.println(id);
    if(key == id) {
      pinMap->remove(key);
      pinMap->put(key, onValue);
      Serial.print("LOCAL SOURCE TRUE :");
      Serial.println(onValue);
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

String parseData(String inData){

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
  int solPotential = generateDecimalFromStringPositions(inData, 643, 649);
  //1st is gridPower, 2nd is batteryPercentage, 3rd loadPower, 4th is battery power  (2's complement for negative), 5th and 6th are solar strings
  String out = (String)millis() + "*" + String(gridPower) + "*" + String(batteryPercent) + "*" + String(batteryPower) + "*" + String(loadPower) + "*" + String(solarString1) + "*" + (String)solarString2;
  out += "*" + (String)batteryVoltage;
  out += "*" + (String)solPotential;
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
void sendRemoteData(String datastring, String mode){
  WiFiClient clientGet;
  const int httpGetPort = 80;
  String url;
  postData(datastring + "\n" + goodData);
  if(deviceName == "") {
    mode = "getInitialDeviceInfo";
  }
  url =  (String)url_get + "?storagePassword=" + (String)storage_password + "&locationId=" + device_id + "&mode=" + mode + "&data=" + datastring;
  //Serial.println(host_get);
  int attempts = 0;
  while(!clientGet.connect(host_get, httpGetPort) && attempts < connection_retry_number) {
    attempts++;
    delay(20);
  }

  if (attempts >= connection_retry_number) {
    connectionFailureTime = millis();
    connectionFailureMode = true;
    //rebootMoxee();
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
        //let's try a simpler connection and if that fails, then reboot moxee
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
    while(clientGet.available()){
      receivedData = true;
      String retLine = clientGet.readStringUntil('\n');
      retLine.trim();
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
        setLocalHardwareToServerStateFromNonJson((char *)retLine.c_str());
        receivedDataJson = true;
        break;         
      } else {
      }
    }
   
  } //if (attempts >= connection_retry_number)....else....    
  clientGet.stop();
}


String handleDeviceNameAndAdditionalSensors(char * sensorData, bool intialize){
  feedbackSerial.println(sensorData);
  String additionalSensorArray[12];
  String specificSensorData[8];
  int i2c;
  int pinNumber;
  int powerPin;
  int sensorIdLocal;
  int sensorSubTypeLocal;
  int deviceFeatureId;
  int consolidateAllSensorsToOneRecord = 0;
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
      consolidateAllSensorsToOneRecord = specificSensorData[7].toInt();
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
        out = out + "!" + weatherDataString(sensorIdLocal, sensorSubTypeLocal, pinNumber, powerPin, i2c, deviceFeatureId, objectCursor, sensorName, consolidateAllSensorsToOneRecord);
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
    transmissionString = weatherDataString(sensor_id, sensor_sub_type, sensor_data_pin, sensor_power_pin, sensor_i2c, NULL, 0, deviceName, consolidate_all_sensors_to_one_record);
  }
  server.send(200, "text/plain", transmissionString); //Send values only to client ajax request
}

//if the backend sends too much text data at once, it is likely to get gzipped, which is hard to deal with on a microcontroller with limited resources
//so a better strategy is to send double-delimited data instead of JSON, with data consistently in known ordinal positions
//thereby making the data payloads small enough that the server never gzips them
//i've made it so the ESP8266 can receive data in either format. it takes the lead on specifying which format it prefers
//but if it misbehaves, i can force it to be one format or the other remotely 
void setLocalHardwareToServerStateFromNonJson(char * nonJsonLine){
  //return;
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
  feedbackSerial.println(nonJsonLine);
  //return;
  splitString(nonJsonLine, '|', nonJsonPinArray, 12);
  int foundPins = 0;
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
      
      //Serial.println("!ABOUT TO TURN OF localsource: " + (String)localSource +  " serverSAVED: " + (String)serverSaved);
      if(!localSource || serverSaved == 1){
        if(serverSaved == 1) {//confirmation of serverSaved, so localSource flag is no longer needed
          //Serial.println("SERVER SAVED==1!!");
          localSource = false;
        } else {
          pinMap->remove(key);
          pinMap->put(key, value);
        }
      }
      
      pinList[foundPins] = key;
      //return; //pinMode causes serial probs!!
      pinMode(pinNumber, OUTPUT);
      //return;
      if(i2c > 0) {
        setPinValueOnSlave(i2c, (char)pinNumber, (char)value); 
      } else {
        if(canBeAnalog) {
          analogWrite(pinNumber, value);
        } else {
          if(value > 0) {
            digitalWrite(pinNumber, HIGH);
          } else {
            digitalWrite(pinNumber, LOW);
          }
        }
      }
    }
    foundPins++;
  }
  pinTotal = foundPins;
}


///////////////////////////////////////////////
void rebootEsp() {
  Serial.println("Rebooting ESP");
  ESP.restart();
}

void rebootMoxee() {  //moxee hotspot is so stupid that it has no watchdog.  so here i have a little algorithm to reboot it.
  if(moxee_power_switch > -1) {
    digitalWrite(moxee_power_switch, LOW);
    delay(7000);
    digitalWrite(moxee_power_switch, HIGH);
  }
  //only do one reboot!  it usually takes two, but this thing can be made to cycle so fast that this same function can handle both reboots, which is important if the reboot happens to 
  //be out of phase with the cellular hotspot
  shiftArrayUp(moxeeRebootTimes,  millis(), 10);
  moxeeRebootCount++;
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

/////////////////////////////////////////////
//utility functions
/////////////////////////////////////////////
String urlEncode(String str) {
  String encodedString = "";
  char c;
  for (int i =0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += "%20";
    } else if (isalnum(c)) {
      encodedString+= c;
    } else {
      encodedString += '%';
      encodedString += (byte)c >> 4;
      encodedString += (byte)c & 0xf;
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

String joinMapValsOnDelimiter(SimpleMap<String, int> *mapIn, String delimiter, int numberToDo) {
  String out = "";
  for (int i = 0; i < mapIn->size(); i++) {
    out = out + (String)mapIn->getData(i);
    if(i < numberToDo-1) {
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

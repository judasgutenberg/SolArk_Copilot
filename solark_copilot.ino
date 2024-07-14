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
String serialContent = "";
String ipAddress;
String goodData;
String dataToDisplay;
String deviceName = "Your device";
long lastDataLogTime;
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
int totalSerialChars = 0;

bool goodDataMode = false;

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
  server.begin();
  
  
  Serial.println("about to swap");
  delay(4000);
  Serial.swap();
  feedbackSerial.begin(115200);
  
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
      if (incomingByte == '\r'){
        //dataToDisplay = goodData;
        dataToDisplay =  parseData(goodData);
        sendRemoteData(dataToDisplay);
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
  if(ipAddress.indexOf(' ') > 0) { //i was getting HTML header info mixed in for some reason
    ipAddress = ipAddress.substring(0, ipAddress.indexOf(' '));
  }
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
  out += "||" + joinMapValsOnDelimiter(pinMap, "*", pinTotal) + "|***" + ipAddress + "*1*0";
  //feedbackSerial.println(out);
  return out;
}

//SEND DATA TO A REMOTE SERVER TO STORE IN A DATABASE----------------------------------------------------
void sendRemoteData(String datastring){
  WiFiClient clientGet;
  const int httpGetPort = 80;
  String url;
  String mode = "getDeviceData";
  //most of the time we want to getDeviceData, not saveData. the former picks up remote control activity. the latter sends sensor data
  postData(datastring + "\n" + goodData);
  mode = "saveLocallyGatheredSolarData";
  lastDataLogTime = millis();
 
  if(deviceName == "") {
    mode = "getInitialDeviceInfo";
  }
  url =  (String)url_get + "?storagePassword=" + (String)storage_password + "&locationId=" + device_id + "&mode=" + mode + "&data=" + datastring;
  //Serial.println(host_get);
  int attempts = 0;
  while(!clientGet.connect(host_get, httpGetPort) && attempts < connection_retry_number) {
    attempts++;
    delay(200);
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
    delay(1); //see if this improved data reception. OMG IT TOTALLY WORKED!!!
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
        //handleDeviceNameAndAdditionalSensors((char *)additionalSensorInfo.c_str(), true);
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
        //handleDeviceNameAndAdditionalSensors((char *)additionalSensorInfo.c_str(), true);
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
//////////////////////////////////////////////
long getPinValueOnSlave(char i2cAddress, char pinNumber) { //might want a user-friendlier API here
  //reading an analog or digital value from the slave:
  Wire.beginTransmission(i2cAddress);
  Wire.write(pinNumber); //addresses greater than 64 are the same as AX (AnalogX) where X is 64-value
  Wire.endTransmission();
  delay(100); 
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

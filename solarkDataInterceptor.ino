//Gus Mueller, June 29, 2024
//uses an ESP8266 wired with the swap serial pins (D8 as TX and D7 as RX) connected to the exposed serial header on the ESP32 in the SolArk's WiFi dongle.
//this intercepts the communication data between the SolArk and the dongle to get frequent updates (that is, every few seconds) of the power and battery levels.
//the data still makes it to PowerView (now MySolArk) but you have access to it much sooner, locally, and at much finer granularity
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
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

bool goodDataMode = false;

void setup() {
  Serial.begin(115200);
  wiFiConnect();
  delay(2000);
  server.on("/", handleRoot);
  server.begin();
  
  
  Serial.println("about to swap");
  delay(4000);
  Serial.swap();

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
      serialContent = "";
      goodDataMode = true;   
    }
  }
  server.handleClient();

}

void handleRoot() {
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
  int batteryPercent = generateDecimalFromStringPositions(inData, 604, 606);
  int loadPower = generateDecimalFromStringPositions(inData, 607, 613);
  int solarString2  = generateDecimalFromStringPositions(inData, 613, 619);
  int solarString1 = generateDecimalFromStringPositions(inData, 619, 625);
  int mysteryValue = generateDecimalFromStringPositions(inData, 625, 631);
  int mysteryValue3 = generateDecimalFromStringPositions(inData, 631, 637);
  int batteryPower = generateDecimalFromStringPositions(inData, 637, 643);
  int mysteryValue4 = generateDecimalFromStringPositions(inData, 643, 649);
  int gridPower = generateDecimalFromStringPositions(inData, 121, 127);
  //1st is gridPower, 2nd is batteryPercentage, 3rd loadPower, 4th is battery power  (2's complement for negative), 5th and 6th are solar strings
  String out = String(gridPower) + "*" + String(batteryPercent) + "*" + String(batteryPower) + "*" + String(loadPower) + "*" + String(solarString1) + "*" + solarString2 +  "*" + mysteryValue + "*" + mysteryValue3  + "*" + mysteryValue4;
  out += "|||***" + ipAddress;
  return out;
}

//SEND DATA TO A REMOTE SERVER TO STORE IN A DATABASE----------------------------------------------------
void sendRemoteData(String datastring){
  WiFiClient clientGet;
  const int httpGetPort = 80;
  String url;
  String mode = "getDeviceData";
  //most of the time we want to getDeviceData, not saveData. the former picks up remote control activity. the latter sends sensor data
 
  mode = "saveLocallyGatheredSolarData";
  lastDataLogTime = millis();
 
  if(deviceName == "") {
    //mode = "getInitialDeviceInfo";
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
      } else if(retLine.charAt(0) == '{') {
        //setLocalHardwareToServerStateFromJson((char *)retLine.c_str());
        //receivedDataJson = true;
        break; 
      } else if(retLine.charAt(0) == '|') {
        //setLocalHardwareToServerStateFromNonJson((char *)retLine.c_str());
        //receivedDataJson = true;
        break;         
      } else {
      }
    }
   
  } //if (attempts >= connection_retry_number)....else....    
  clientGet.stop();
}

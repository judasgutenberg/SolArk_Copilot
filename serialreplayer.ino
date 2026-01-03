#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#define BAUD_RATE       115200
#define LINE_DELAY_MS   20
#define LOOP_DELAY_MS   2000

//you will need to change these to your local ssid/password and wherever on the web you put mocksolar.php
const char* ssid     = "your_ssid";
const char* password = "your_secret_password";
const char* url = "http://your_site.com/weather/mocksolark.php";

WiFiClient client;
HTTPClient http;

void setup() {
  Serial.begin(BAUD_RATE);
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
}


void loop() {
  streamEndpointLinewise();
}



void streamEndpointLinewise() {

  if (!http.begin(client, url)) {
    Serial.println("HTTP begin failed");
    delay(LOOP_DELAY_MS);
    return;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP GET failed: %d\n", httpCode);
    http.end();
    delay(LOOP_DELAY_MS);
    return;
  }

  WiFiClient* stream = http.getStreamPtr();

  while (http.connected()) {

    while (stream->available()) {
      char c = stream->read();
      Serial.write(c);

      if (c == '\n') {
        delay(LINE_DELAY_MS);
        yield();   // keep the watchdog happy
      }
    }

    yield();
  }

  http.end();

  delay(LOOP_DELAY_MS);
}

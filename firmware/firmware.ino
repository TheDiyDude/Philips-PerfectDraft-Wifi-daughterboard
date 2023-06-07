// Philips-PerfectDraft-Wifi-daughterboard
// Firmware to add Wifi  
// to your Perfect Draft Beer dispenser 
// and set cooling temperaure to a desired
// value, i.e 6°C, 7°C .. 
//
// Author: Marc-Oliver Blumenauer 
//         marc@l3c.de
//
// License: MIT
//  
//
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <LapX9C10X.h>

// Do not forget to replace your network credentials in this file
#include "Credentials.h"

// X9C103 pins
#define CSPIN  D1
#define INCPIN D2
#define UDPIN  D3

LapX9C10X digPot(INCPIN, UDPIN, CSPIN, LAPX9C10X_X9C103);

// Port of our Web Server 
WiFiServer server(80);

// For HTTP Request
String header;
char buffer[255];

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.setPassword(ota_password);
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Prepare WebServer
  server.begin();

  // Prepare X9C103 set wiper - to 0 use digPot.begin(-1)
  digPot.begin();
  delay(5000);
  
  // Done!
  
  Serial.println("Setup completed");
}

void loop() {
  
  WiFiClient client = server.available(); 
  if (client) {                             
    Serial.println("Client available");
    String currentLine = "";                

    while (client.connected()) { 

      if (client.available()) {
        char c = client.read();             
        Serial.write(c);                    
        header += c;
        if (c == '\n') {                    
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            if (header.indexOf("GET /Temp/inc") >= 0) {
              Serial.println("Temp: Incrementing");
              digPot.offset(+1);
              delay(100);
            } else if (header.indexOf("GET /Temp/dec") >= 0) {
              Serial.println("Temp: Decrementing");
              digPot.offset(-1);
              delay(100);
            } else if (header.indexOf("GET /Temp/bypass") >= 0) {
              Serial.println("Temp: Bypass digital potentiometer");
              digPot.set(0);
              delay(100);
            } else if (header.indexOf("GET /Temp/store") >= 0) {
              Serial.println("Temp: store current value in NVRAM");
              digPot.writeNVM();
              delay(100);
            } else if (header.indexOf("GET /Temp/6deg") >= 0) {
              Serial.println("Temp: set to 6°C ");
              digPot.set((float)5.66);
              delay(100);
            } else if (header.indexOf("GET /Temp/7deg") >= 0) {
              Serial.println("Temp: set to 7°C ");
              digPot.set((float)6.8);
              delay(100);
            }

            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #333344; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #888899;}</style></head>");

            client.println("<body><h1 align=\"center\">Perfect Draft Wifi v0.1</h1>");
            sprintf(buffer,"<p><h2 align=\"center\">Digital Pot R = %4.2f KOhm</h2></p>",digPot.getK());
            client.println(buffer);
            client.println("<p><a href=\"/Temp/bypass\"><button class=\"button\">Bypass</button></a></p>");
            client.println("<p><a href=\"/Temp/6deg\"><button class=\"button\">6&deg;C</button></a>");
            client.println("<a href=\"/Temp/7deg\"><button class=\"button\">7&deg;C</button></a></p>");
            client.println("<p><a href=\"/Temp/inc\"><button class=\"button\">+</button></a></p>");
            client.println("<p><a href=\"/Temp/dec\"><button class=\"button\">-</button></a></p>");
            client.println("<p><a href=\"/Temp/store\"><button class=\"button\">Save</button></a></p>");
            client.println("</body></html>");

            client.println();
            break;
          } else { 
            currentLine = "";
          }
        } else if (c != '\r') {  
          currentLine += c;      
        }
      }
    }

    header = "";

    client.stop();
    Serial.println("Client disconnected");
    Serial.println("");
  }

  ArduinoOTA.handle();
}

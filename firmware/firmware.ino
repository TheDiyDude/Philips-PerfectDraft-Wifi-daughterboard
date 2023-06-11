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

// MQTT
#if defined(__MQTT_ENABLED__)
  #include <PubSubClient.h>
  WiFiClient espClient;
  PubSubClient client(espClient);
  bool useMQTT = true;
  int mqttRetryCount = 0;
#endif

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

#if defined(__MQTT_ENABLED__)
char kOhm[30];
#endif

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
  
  // MQTT 
  // initialize mqtt client
  #if defined(__MQTT_ENABLED__)
  Serial.print("connecting to MQTT host ");
  client.setServer(mqtt_hostname, MQTT_PORT);
  mqttRetryCount=0;
  if (!client.connected()) {
        while (!client.connected() && useMQTT) {
            client.connect(ota_hostname,mqtt_username,mqtt_password);
            Serial.print(".");
            mqttRetryCount++;
            delay(1000);
            if (mqttRetryCount>=10) { useMQTT=false; }
        }
  }
  //useMQTT = client.connected();
  if (useMQTT) {
    Serial.println(". connected!");
    pub("status", "alive");
  }
  if (!useMQTT && mqttRetryCount>=10) {
    Serial.println(". failed!");
  }
  #endif
  // Done!
  
  Serial.println("Setup completed");
}

void loop() {
  #if defined(__MQTT_ENABLED__)
    client.loop();
  #endif
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
              #if defined(__MQTT_ENABLED__)
                sprintf(kOhm,"%4.2f",digPot.getK());
                pub("kOhm",kOhm);
              #endif
              delay(100);
            } else if (header.indexOf("GET /Temp/dec") >= 0) {
              Serial.println("Temp: Decrementing");
              digPot.offset(-1);
              #if defined(__MQTT_ENABLED__)
                sprintf(kOhm,"%4.2f",digPot.getK());
                pub("kOhm",kOhm);
              #endif
              delay(100);
            } else if (header.indexOf("GET /Temp/bypass") >= 0) {
              Serial.println("Temp: Bypass digital potentiometer");
              digPot.set(0);
              #if defined(__MQTT_ENABLED__)
                pub("BeerTemp","3");
              #endif
              delay(100);
            } else if (header.indexOf("GET /Temp/store") >= 0) {
              Serial.println("Temp: store current value in NVRAM");
              digPot.writeNVM();
              delay(100);
            } else if (header.indexOf("GET /Temp/6deg") >= 0) {
              Serial.println("Temp: set to 6°C ");
              digPot.set((float)4.8);
              #if defined(__MQTT_ENABLED__)
                pub("BeerTemp","6");
              #endif
              delay(100);
            }else if (header.indexOf("GET /Temp/10deg") >= 0) {
              Serial.println("Temp: set to 10°C ");
              digPot.set((float)5.66);
              #if defined(__MQTT_ENABLED__)
                pub("BeerTemp","10");
              #endif
              delay(100);
            } else if (header.indexOf("GET /Temp/11deg") >= 0) {
              Serial.println("Temp: set to 11°C ");
              digPot.set((float)6.8);
              #if defined(__MQTT_ENABLED__)
                pub("BeerTemp","11");
              #endif
              delay(100);
            }

            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #333344; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #888899;}</style></head>");

            client.println("<body><h1 align=\"center\">Perfect Draft Wifi v1.1</h1>");
            sprintf(buffer,"<p><h2 align=\"center\">Digital Pot R = %4.2f kOhm</h2></p>",digPot.getK());
            client.println(buffer);
            client.println("<p><a href=\"/Temp/bypass\"><button class=\"button\">Bypass</button></a></p>");
            client.println("<p><a href=\"/Temp/6deg\"><button class=\"button\">6&deg;C</button></a>");
            client.println("<a href=\"/Temp/10deg\"><button class=\"button\">10&deg;C</button></a>");
            client.println("<a href=\"/Temp/11deg\"><button class=\"button\">11&deg;C</button></a></p>");
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
#if defined(__MQTT_ENABLED__)
void pub(const char* topic, const char* value) {
  if (client.connected() && useMQTT) {
    sprintf(buffer,"/PerfectDraft/%s/%s",ota_hostname,topic);
    client.publish(buffer,value);
    Serial.println("Sending to MQTT Broker:");
    Serial.println(buffer);
    Serial.println(value);
  }
}
#endif

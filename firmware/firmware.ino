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
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <LapX9C10X.h>
#include <EEPROM.h>

#include "Credentials.h"

// MQTT
#if defined(__MQTT_ENABLED__)
  #include <PubSubClient.h>
  WiFiClient espClient;
  PubSubClient mqtt(espClient);
  bool useMQTT = true;
  int mqttRetryCount = 0;
#endif

// X9C103 pins
#define CSPIN  D1
#define INCPIN D2
#define UDPIN  D3

LapX9C10X digPot(INCPIN, UDPIN, CSPIN, LAPX9C10X_X9C103);

// Port of our Web Server 
ESP8266WebServer server(80);

// For HTTP Request
char buffer[255];
String ssid, pass, content;
bool apmode = false;                              
String redirect = "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"0; url='/'\" /></head><body></body></html>";
float beerTemp = 0.0;

#if defined(__MQTT_ENABLED__)
char kOhm[30];
#endif

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  readData();                                     
  if (ssid.length() == 0) {                       
    ap_mode();
  } else {
    if (testWifi()) {                             
      Serial.println("WiFi Connected!!!");
      apmode = false;
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
      EEPROM_Read(&beerTemp);
      if (beerTemp > 0.01 && beerTemp < 12.00) {
          digPot.set(beerTemp);
      } else {
          digPot.begin();
      }
      delay(5000);
      // MQTT 
      // initialize mqtt client
      #if defined(__MQTT_ENABLED__)
      Serial.print("connecting to MQTT host ");
      mqtt.setServer(mqtt_hostname, MQTT_PORT);
      mqttRetryCount=0;
      if (!mqtt.connected()) {
            while (!mqtt.connected() && useMQTT) {
                mqtt.connect(ota_hostname,mqtt_username,mqtt_password);
                Serial.print(".");
                mqttRetryCount++;
                delay(1000);
                if (mqttRetryCount>=10) { useMQTT=false; }
            }
      }
      if (useMQTT) {
        Serial.println(". connected!");
        pub("status", "alive");
      }
      if (!useMQTT && mqttRetryCount>=10) {
        Serial.println(". failed!");
      }
      #endif  
      launchWeb(1);   
    } else {                                       
      ap_mode();
    }
  }
  Serial.println("Setup completed");
}

void loop() {
  ArduinoOTA.handle();
  #if defined(__MQTT_ENABLED__)
    mqtt.loop(); 
  #endif
  if (apmode) {
    server.handleClient();                        
  } else {
    if (WiFi.status() == WL_CONNECTED) {          
      server.handleClient();
      #if defined(__MQTT_ENABLED__)
        mqtt.loop(); 
      #endif
    }
  }
}
#if defined(__MQTT_ENABLED__)
void pub(const char* topic, const char* value) {
  if (mqtt.connected() && useMQTT) {
    sprintf(buffer,"/PerfectDraft/%s/%s",ota_hostname,topic);
    mqtt.publish(buffer,value);
    Serial.println("Sending to MQTT Broker:");
    Serial.println(buffer);
    Serial.println(value);
  }
}
#endif

void ap_mode() {                                  
  Serial.println("AP Mode. Please connect to http://192.168.4.1 to configure");
  apmode = true;
  const char* ssidap = "PerfectDraft-Setup";
  const char* passap = "";
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssidap, passap);
  Serial.println(WiFi.softAPIP());
  apmode = true;
  launchWeb(0);                                   
}

void launchWeb(int webtype) {
  createWebServer(webtype);
  server.begin();
}

void createWebServer(int webtype) {             
  if (webtype == 0) { 
    server.on("/", []() {
      content = "<html><head><style>.button {background-color: #3CBC8D;";
      content += "color: white;padding: 5px 10px;text-align: center;text-decoration: none;";
      content += "display: inline-block;font-size: 14px;margin: 4px 2px;cursor: pointer;}";
      content += "input[type=text],[type=password]{width: 100%;padding: 5px 10px;";
      content += "margin: 5px 0;box-sizing: border-box;border: none;";
      content += "background-color: #3CBC8D;color: white;}</style></head><body>";
      content += "<h1>PerfectDraft Wifi v1.2</h1><br>";
      content += "<h3>Current Settings</h3>";
      content += "<table><tr><td><label> WiFi SSID</label></td><td><label>" + ssid + "</label></td></tr>";
      content += "<tr><td><label> WiFi Pasword</label></td><td><label>" + pass + "</label></td></tr></table><br><br>";
      content += "<form method='get' action='setting'>";
      content += "<h3>New WiFi Settings</h3>";
      content += "<table><tr><td><label>WiFi SSID</label></td><td><input type='text' name = 'ssid' lenght=32 ></td></tr>";
      content += "<tr><td><label> WiFi Password</label></td><td><input type='password' name = 'password' lenght=32></td></tr>";
      content += "<tr><td></td><td><input class=button type='submit'></td></tr></table></form>";
      content += "</body></html>";
      server.send(200, "text/html", content);
    });
    server.on("/setting", []() {                  
      String ssidw = server.arg("ssid");
      String passw = server.arg("password");
      writeData(ssidw, passw);                    
      content = "Success..please reboot";
      server.send(200, "text/html", content);
      delay(2000);
      ESP.restart();
    });
    server.on("/clear", []() {                      
      clearData();
      delay(2000);
      ESP.restart();
    });
  }
  if (webtype == 1) {   
    server.on("/", []() {
      content  = "<!DOCTYPE html><html>";
      content += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
      content += "<link rel=\"icon\" href=\"data:,\">";
      content += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
      content += ".button { background-color: #333344; border: none; color: white; padding: 16px 40px;";
      content += "text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}";
      content += ".button2 {background-color: #888899;}</style></head>";
      content += "<body><h1 align=\"center\">Perfect Draft Wifi v1.2</h1>";
      sprintf(buffer,"<p><h2 align=\"center\">Digital Pot R = %4.2f kOhm</h2></p>",digPot.getK());
      content += buffer;
      content += "<p><a href=\"/Temp/bypass\"><button class=\"button\">Bypass</button></a></p>";
      content += "<p><a href=\"/Temp/6deg\"><button class=\"button\">6&deg;C</button></a>";
      content += "<a href=\"/Temp/10deg\"><button class=\"button\">10&deg;C</button></a>";
      content += "<a href=\"/Temp/11deg\"><button class=\"button\">11&deg;C</button></a></p>";
      content += "<p><a href=\"/Temp/inc\"><button class=\"button\">+</button></a></p>";
      content += "<p><a href=\"/Temp/dec\"><button class=\"button\">-</button></a></p>";
      content += "<p><a href=\"/Temp/store\"><button class=\"button\">Save</button></a></p>";
      content += "<p><a href=\"/clear\"><button class=\"button\">Erase Wifi Settings</button></a></p>";
      content += "</body></html>";
      server.send(200, "text/html", content);
    });
    server.on("/clear", []() {                      
      clearData();
      delay(2000);
      ESP.restart();
    });
    server.on("/Temp/inc", []() {
      Serial.println("Temp: Incrementing");
      digPot.offset(+1);
      #if defined(__MQTT_ENABLED__)
        sprintf(kOhm,"%4.2f",digPot.getK());
        pub("kOhm",kOhm);
      #endif
      delay(100);
      server.send(200, "text/html", redirect);
    }); 
    server.on("/Temp/dec", []() {
      Serial.println("Temp: Decrementing");
      digPot.offset(-1);
      #if defined(__MQTT_ENABLED__)
        sprintf(kOhm,"%4.2f",digPot.getK());
        pub("kOhm",kOhm);
      #endif
      delay(100);
      server.send(200, "text/html", redirect);
    });
    server.on("/Temp/bypass", []() {
      Serial.println("Temp: Bypass digital potentiometer");
      digPot.set(0);
      #if defined(__MQTT_ENABLED__)
        pub("BeerTemp","3");
        sprintf(kOhm,"%4.2f",digPot.getK());
        pub("kOhm",kOhm);
      #endif
      delay(100);
      server.send(200, "text/html", redirect);
    });
    server.on("/Temp/store", []() {
      Serial.println("Temp: store current value in NVRAM");
      digPot.writeNVM();
      delay(100);
      beerTemp = digPot.getK();
      EEPROM_Write(&beerTemp);
      server.send(200, "text/html", redirect);
    }); 
    server.on("/Temp/6deg", []() {
      Serial.println("Temp: set to 6°C ");
      digPot.set((float)4.8);
      #if defined(__MQTT_ENABLED__)
        pub("BeerTemp","6");
        sprintf(kOhm,"%4.2f",digPot.getK());
        pub("kOhm",kOhm);
      #endif
      delay(100);
      server.send(200, "text/html", redirect);
    });
    server.on("/Temp/10deg", []() {
      Serial.println("Temp: set to 10°C ");
      digPot.set((float)5.66);
      #if defined(__MQTT_ENABLED__)
        pub("BeerTemp","10");
        sprintf(kOhm,"%4.2f",digPot.getK());
        pub("kOhm",kOhm);
      #endif
      delay(100);
      server.send(200, "text/html", redirect);
    });
    server.on("/Temp/11deg", []() {
      Serial.println("Temp: set to 11°C ");
      digPot.set((float)6.8);
      #if defined(__MQTT_ENABLED__)
        pub("BeerTemp","11");
        sprintf(kOhm,"%4.2f",digPot.getK());
        pub("kOhm",kOhm);
      #endif
      delay(100);
      server.send(200, "text/html", redirect);
    });
  }
}
void EEPROM_Write(float *num)
{
  EEPROM.begin(512);
  byte ByteArray[4];
  int memPos = 100;
  memcpy(ByteArray, num, 4);
  for(int x = 0; x < 4; x++) { EEPROM.write((memPos++ + 4) + x, ByteArray[x]); }  
  EEPROM.end();
  Serial.print("Stored EEPROM Beer Temp: ");
  Serial.println(*num,2);
}
void EEPROM_Read(float *num)
{
  EEPROM.begin(512);
  byte ByteArray[4];
  int memPos = 100;
  for(int x = 0; x < 4; x++) { ByteArray[x] = EEPROM.read((memPos++ + 4) + x); }
  memcpy(num, ByteArray, 4);
  EEPROM.end();
  Serial.print("Read EEPROM Beer Temp: ");
  Serial.println(*num,2);
}
void readData() {                                 
  EEPROM.begin(512);
  Serial.println("Reading From EEPROM..");
  for (int i = 0; i < 50; i++) {                  
    if (char(EEPROM.read(i)) > 0) {
      ssid += char(EEPROM.read(i));
    }
  }
  for (int i = 50; i < 100; i++) {                 
    if (char(EEPROM.read(i)) > 0) {
      pass += char(EEPROM.read(i));
    }
  }
  Serial.println("Wifi ssid: " + ssid);
  Serial.println("Wifi password: " + pass);
  EEPROM.end();
}
void writeData(String a, String b) {              
  clearData();                                    
  EEPROM.begin(512);                              
  Serial.println("Writing to EEPROM...");
  for (int i = 0; i < 50; i++) {                  
    EEPROM.write(i, a[i]);
  }
  for (int i = 50; i < 100; i++) {                 
    EEPROM.write(i, b[i - 50]);
    Serial.println(b[i]);
  }
  EEPROM.commit();
  EEPROM.end();
  Serial.println("Write Successfull");
}
void clearData() {                                
  EEPROM.begin(512);
  Serial.println("Clearing EEPROM ");
  for (int i = 0; i < 512; i++) {
    Serial.print(".");
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  EEPROM.end();
}
boolean testWifi() {                            
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  int c = 0;
  while (c < 30) {
    if (WiFi.status() == WL_CONNECTED) {
      WiFi.setAutoReconnect(true);
      WiFi.persistent(true);
      Serial.println(WiFi.status());
      Serial.println(WiFi.localIP());
      delay(100);
      return true;
    }
    Serial.print(".");
    delay(900);
    c++;
  }
  Serial.println("Connection time out...");
  return false;
}

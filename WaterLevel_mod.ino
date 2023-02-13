
  #include <ESP8266WiFi.h>
  #include <WiFiManager.h>
  #include <EEPROM.h>
  #include <HCSR04.h>
  
  #include <FS.h>
  #include "Hash.h"
  #include <ArduinoJson.h>
  
  #define WEBSERVER_H
  #include <ESPAsyncTCP.h>
  #include <ESPAsyncWebServer.h>

//#include <ESPAsyncUDP.h>          
// https://github.com/me-no-dev/ESPAsyncUDP.git

#include <uri/UriRegex.h>
#include <coredecls.h>
#include <time.h>

  //#include <WiFiUdp.h>

  #include <stdlib.h>

  IPAddress local_IP(192, 168, 1, 196);
  // Set your Gateway IP address
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 0, 0);
  IPAddress primaryDNS(8, 8, 8, 8);   //optional
  IPAddress secondaryDNS(8, 8, 4, 4); //optional
  
  const char* www_username = "admin";
  const char* www_password = "esp8266";
  
  const int trigPin = 12;
  const int echoPin = 13;
  int wifiPin = 15;
  int motorLED = 4; // Assign LED pin i.e: D8 on NodeMCU
  #define MOTOR_CONTROL_PIN 14
  
  long duration;
  int distance;
  int percent;
  
  char* host = "Smart_Wifi";
  WiFiManager wm;
  
  ESP8266WebServer httpServer(80);
  
  AsyncWebServer server(80);
  void serverRouting();
   
  HCSR04 distanceSensor(12, 13); // trig, echo D6,D7

  uint8_t tankTopCm = 45;
  uint8_t tankBottomCm = 155;
  
  int waterLevelLowerThreshold = 20;
  int waterLevelUpperThreshold = 90;
  
  //float volume = 0;
  int liters = 0;
  int waterLevelDownCount = 0, waterLevelUpCount = 0;
  
  #define EE_MOTOR_STATUS 10
  bool motorEnabled;
  bool isMotorEnabledAuto;
  
  // Timer variables
  unsigned long lastTime = 0;
  unsigned long timerDelay = 5000;


void motorAutoOn();

#define MyTimeZoneSec (5*3600+30*60)
bool isTimeSynchronized = false;

unsigned long timeOfDayLastMS = 0L;
static uint32_t tods_old = 0;

long onAlarm1TODsec = 18000L; // 5:00AM
long onAlarm2TODsec = 61200L; // 5:00PM
bool onAlarm1Enabled = false;
bool onAlarm2Enabled = false;

long timerMotorCooldownStart = 59 * 60 * 1000L; // 59 minutes of motor cool down

long timerMotorAutoOff = 0; // 
long timerMotorCooldown = 0; // 


#define EE_motorEnabled     0
#define EE_onAlarm1Enabled  (EE_motorEnabled    + sizeof(motorEnabled))
#define EE_onAlarm1TODsec   (EE_onAlarm1Enabled + sizeof(onAlarm1Enabled))
#define EE_onAlarm2Enabled  (EE_onAlarm1TODsec  + sizeof(onAlarm1TODsec))
#define EE_onAlarm2TODsec   (EE_onAlarm2Enabled + sizeof(onAlarm2Enabled))
#define EE_tankTopCm        (EE_onAlarm2TODsec  + sizeof(onAlarm2TODsec))
#define EE_tankBottomCm     (EE_tankTopCm       + sizeof(tankTopCm))

void SaveToEEPROM(int operation) {
  switch (operation) {
        //case EE_motorEnabled:   EEPROM.put(EE_motorEnabled, motorEnabled);      EEPROM.commit();  break;
        case EE_onAlarm1Enabled:  EEPROM.put(EE_onAlarm1Enabled, onAlarm1Enabled);    EEPROM.commit();  break;
        case EE_onAlarm1TODsec:   EEPROM.put(EE_onAlarm1TODsec, onAlarm1TODsec);      EEPROM.commit();  break;
        case EE_onAlarm2Enabled:  EEPROM.put(EE_onAlarm2Enabled, onAlarm2Enabled);    EEPROM.commit();  break;
        case EE_onAlarm2TODsec:   EEPROM.put(EE_onAlarm2TODsec, onAlarm2TODsec);      EEPROM.commit();  break;
        //case EE_tankTopCm:      EEPROM.put(EE_tankTopCm, tankTopCm);            EEPROM.commit();  break;
        case EE_tankBottomCm:   EEPROM.put(EE_tankBottomCm, tankBottomCm);      EEPROM.commit();  break;
  }
}
void LoadFromEEPROM() {
  //EEPROM.get(EE_motorEnabled, motorEnabled);
  EEPROM.get(EE_onAlarm1Enabled, onAlarm1Enabled);
  EEPROM.get(EE_onAlarm1TODsec, onAlarm1TODsec);
  EEPROM.get(EE_onAlarm2Enabled, onAlarm2Enabled);
  EEPROM.get(EE_onAlarm2TODsec, onAlarm2TODsec);
  tankTopCm = 0;//EEPROM.get(EE_tankTopCm, tankTopCm);
  EEPROM.get(EE_tankBottomCm, tankBottomCm);
}

  // Initialize WiFi
  void initWiFi() {
  
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      Serial.println("STA Failed to configure");
    }

  Serial.print("initializing Wi-Fi");
  WiFi.hostname(host);
  wm.setConnectTimeout(20); // how long to try to connect for before continuing
  wm.setConfigPortalTimeout(60); // auto close configportal after n seconds
  wm.setAPClientCheck(true); // avoid timeout if client connected to softap
  bool res = wm.autoConnect(host);

    if (!res) {
      Serial.println("Failed to connect or hit timeout");
      digitalWrite(wifiPin, LOW);
    }
    else  Serial.println("connected with Wi-Fi");
    digitalWrite(wifiPin, HIGH);
  }
  
  // speed of the sound on meter per second (m/s)
  #define SPEED_OF_SOUND 340
  #define CM_PER_USEC_HALF (0.5 * SPEED_OF_SOUND / 1e4)
  //
  void ultrasonic () ///for calculating distance
  {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    duration = pulseIn(echoPin, HIGH);
    //Serial.println("Trigger for 10 \n\n");
    //Serial.println("duration calculated \n\n");
    //Serial.println("assembling sonar\n\n");
  
    distance = duration * CM_PER_USEC_HALF;
    float delta = tankTopCm - tankBottomCm;
    delta = delta == 0 ? 0 : 1 / delta;
    percent = 100 * (distance - tankBottomCm) * delta;

    // debuging:
    //Serial.println(String("durationX ") + duration + ", distance "+distance + " delta " +delta + ", percent " + percent);
    //distance = 140;
    //percent = 100 * (distance - tankBottomCm) * delta;
    //Serial.println(String("durationX ") + duration + ", distance "+distance + " delta " +delta + ", percent " + percent + ", onAlarm1TODsec " + onAlarm1TODsec + ", onAlarm2TODsec " + onAlarm2TODsec);
  }
  void measure_Volume()
  {
    ultrasonic();
    liters = percent;
    //Serial.println(liters);
  
    if (liters <= waterLevelLowerThreshold)
      waterLevelDownCount++;
    else waterLevelDownCount = 0;
  
    if (liters >= waterLevelUpperThreshold)
      waterLevelUpCount++;
    else waterLevelUpCount = 0;
  
    if (waterLevelDownCount >= 3)
      motorAutoOn();
    if (waterLevelUpCount >= 3)
      motorOff();
    //if (waterLevelDownCount >= 3)
    //{ //TURN ON RELAY
    //  waterLevelDownCount = 0;
    //  Serial.println("motor turned on");
    //  digitalWrite(MOTOR_CONTROL_PIN, HIGH); //Relay is active HIGH
    //  digitalWrite(LED, HIGH); // turn the LED on
    //  motorEnabled = true;
    //  timerMotorCooldown = timerMotorCooldownStart; // reset motor cooldown
    //}
    //if (waterLevelUpCount >= 3)
    //{ //TURN OFF RELAY
    //  waterLevelUpCount = 0;
    //  Serial.println("motor turned off");
    //  digitalWrite(MOTOR_CONTROL_PIN, LOW); //Relay is active LOW
    //  digitalWrite(LED, LOW); // turn the LED on
    //  if (motorEnabled) timerMotorCooldown = timerMotorCooldownStart; // reset motor cooldown
    //  motorEnabled = false;
    //}
  }
  void runPeriodicFunc()
  {
    static const unsigned long REFRESH_INTERVAL1 = 1000;
    static unsigned long lastRefreshTime1 = 0;
    //*****
    if (isTimeSynchronized) {
      timeval tod;
      if (gettimeofday(&tod, nullptr) == 0) {
        uint32_t tods = (tod.tv_sec + MyTimeZoneSec) % 86400;
        if (onAlarm1Enabled && tods_old != 0 && tods_old < onAlarm1TODsec && tods >= onAlarm1TODsec)
          motorAutoOn();
        else if (onAlarm2Enabled && tods_old != 0 && tods_old < onAlarm2TODsec && tods >= onAlarm2TODsec)
          motorAutoOn();
        tods_old = tods;
      } else {
        Serial.println("Bad time");
      }
    }
    //*****
    unsigned long currentTimeMS = millis(), deltaMS = currentTimeMS - timeOfDayLastMS;
    if (timerMotorCooldown > 0) {
      timerMotorCooldown -= deltaMS;
      if (timerMotorCooldown < 0 )
        timerMotorCooldown = 0;
    }
    if (motorEnabled) {
        if (isMotorEnabledAuto)
          timerMotorCooldown = timerMotorCooldownStart; // reset motor cooldown
        if (timerMotorAutoOff > 0) {
          timerMotorAutoOff -= deltaMS;
          if (timerMotorAutoOff < 1) {
            timerMotorAutoOff = 0;
            motorOff();
          }
        }
    } else
      timerMotorAutoOff = 0;
    timeOfDayLastMS = currentTimeMS;
    //*****
    if (millis() - lastRefreshTime1 >= REFRESH_INTERVAL1)
    {
      measure_Volume();
      lastRefreshTime1 = millis();
    }
    if (!motorEnabled) 
    digitalWrite(MOTOR_CONTROL_PIN, LOW);
  
   }
  

void setup() {
  EEPROM.begin(512);
  delay(10);
  //if (EEPROM.read(EE_MOTOR_STATUS) == 1)motorEnabled = true;
  //else motorEnabled = false;
  pinMode(MOTOR_CONTROL_PIN, OUTPUT);
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);// Sets the echoPin as an Input
  
  pinMode(wifiPin, OUTPUT); 
  pinMode(motorLED, OUTPUT); 
  Serial.begin(115200);
  initWiFi();
  Serial.print(F("Inizializing FS..."));
  if (SPIFFS.begin()) {
    Serial.println(F("done."));
  } else {
    Serial.println(F("fail."));
  }

  Serial.println("Set routing for http server!");
  serverRouting();

  httpServer.begin();
  Serial.println("HTTP server started");

  IPAddress ip = WiFi.localIP();
  Serial.print("This is my ip: ");
  Serial.println(ip);

  //
  isTimeSynchronized = false;
  settimeofday_cb([&isTimeSynchronized] (bool from_sntp) {if (from_sntp && !isTimeSynchronized) isTimeSynchronized = true; });
  configTime(0, 0, "time1.google.com", "time2.google.com", "time3.google.com");

  LoadFromEEPROM();
}

void checkLEDstatus(){
  if (WiFi.status() == WL_CONNECTED){
    digitalWrite(wifiPin, HIGH);
  }else {
    digitalWrite(wifiPin, LOW);
  }
  if (motorEnabled){ 
    digitalWrite(motorLED, HIGH);
   } else {
    digitalWrite(motorLED, LOW);
   }
  
}
void loop(void) {
  
  runPeriodicFunc();
  checkLEDstatus();
  httpServer.handleClient();
   
}
void motorOn(bool autoOn) {
  isMotorEnabledAuto = autoOn;
  Serial.println("motor turned on");
  digitalWrite(MOTOR_CONTROL_PIN, HIGH); //Relay is active HIGH
  digitalWrite(motorLED, HIGH); // turn the LED on
  if (!motorEnabled) {
    if (liters <= 20)       timerMotorAutoOff = 35L * 60L * 1000L;
    else if (liters <= 50)  timerMotorAutoOff = 25L * 60L * 1000L;
    else if (liters <= 75)  timerMotorAutoOff = 15L * 60L * 1000L;
    else                    timerMotorAutoOff = 10L * 60L * 1000L;
  }
  motorEnabled = true;
  SaveToEEPROM(EE_motorEnabled);
  if (isMotorEnabledAuto) timerMotorCooldown = timerMotorCooldownStart; // reset motor cooldown
}

void motorAutoOn() {
  if (!motorEnabled && timerMotorCooldown == 0 && liters <= waterLevelUpperThreshold) {// only enable motor if is not enabled and is not in cooldown time
    motorOn(true);
  }
}

void motorOff() {
  if (motorEnabled && isMotorEnabledAuto) timerMotorCooldown = timerMotorCooldownStart; // reset motor cooldown
  Serial.println("motor turned off");
  digitalWrite(MOTOR_CONTROL_PIN, LOW); //Relay is active LOW
  digitalWrite(motorLED, LOW); // turn the LED off
  motorEnabled = false;
  SaveToEEPROM(EE_motorEnabled);
}

String getContentType(String filename) {
  if (filename.endsWith(F(".htm"))) return F("text/html");
  else if (filename.endsWith(F(".html"))) return F("text/html");
  else if (filename.endsWith(F(".css"))) return F("text/css");
  else if (filename.endsWith(F(".js"))) return F("application/javascript");
  else if (filename.endsWith(F(".json"))) return F("application/json");
  else if (filename.endsWith(F(".png"))) return F("image/png");
  else if (filename.endsWith(F(".gif"))) return F("image/gif");
  else if (filename.endsWith(F(".jpg"))) return F("image/jpeg");
  else if (filename.endsWith(F(".jpeg"))) return F("image/jpeg");
  else if (filename.endsWith(F(".ico"))) return F("image/x-icon");
  else if (filename.endsWith(F(".xml"))) return F("text/xml");
  else if (filename.endsWith(F(".pdf"))) return F("application/x-pdf");
  else if (filename.endsWith(F(".zip"))) return F("application/x-zip");
  else if (filename.endsWith(F(".gz"))) return F("application/x-gzip");
  return F("text/plain");
}

bool handleFileRead(String path) {
  Serial.print(F("handleFileRead: "));
  Serial.println(path);

  if (!is_authenticated()) {
    Serial.println(F("Go on not login!"));
    path = F("/login.html");
  } else {
    if (path.endsWith("/")) path += F("index.html"); // If a folder is requested, send the index file
  }
  String contentType = getContentType(path);              // Get the MIME type
  String pathWithGz = path + F(".gz");
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz)) { // If there's a compressed version available
      path += F(".gz");                      // Use the compressed version
    }
    fs::File file = SPIFFS.open(path, "r");                 // Open the file
    size_t sent = httpServer.streamFile(file, contentType); // Send it to the client
    file.close();                                    // Close the file again
    Serial.println(
      String(F("\tSent file: ")) + path + String(F(" of size "))
      + sent);
    return true;
  }
  Serial.println(String(F("\tFile Not Found: ")) + path);
  return false;                     // If the file doesn't exist, return false
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += (httpServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";

  for (uint8_t i = 0; i < httpServer.args(); i++) {
    message += " " + httpServer.argName(i) + ": " + httpServer.arg(i)
               + "\n";
  }

  httpServer.send(404, "text/plain", message);
}

void handleLogin() {
  Serial.println("Handle login");
  String msg;
  if (httpServer.hasHeader("Cookie")) {
    // Print cookies
    Serial.print("Found cookie: ");
    String cookie = httpServer.header("Cookie");
    Serial.println(cookie);
  }

  if (httpServer.hasArg("username") && httpServer.hasArg("password")) {
    Serial.print("Found parameter: ");

    if (httpServer.arg("username") == String(www_username) && httpServer.arg("password") == String(www_password)) {
      httpServer.sendHeader("Location", "/");
      httpServer.sendHeader("Cache-Control", "no-cache");

      String token = sha1(String(www_username) + ":" + String(www_password) + ":" + httpServer.client().remoteIP().toString());
      httpServer.sendHeader("Set-Cookie", "ESPSESSIONID=" + token);

      httpServer.send(301);
      Serial.println("Log in Successful");
      return;
    }
    msg = "Wrong username/password! try again.";
    Serial.println("Log in Failed");
    httpServer.sendHeader("Location", "/login.html?msg=" + msg);
    httpServer.sendHeader("Cache-Control", "no-cache");
    httpServer.send(301);
    return;
  }
}

/**
   Manage logout (simply remove correct token and redirect to login form)
*/
void handleLogout() {
  Serial.println("Disconnection");
  httpServer.sendHeader("Location", "/login.html?msg=User disconnected");
  httpServer.sendHeader("Cache-Control", "no-cache");
  httpServer.sendHeader("Set-Cookie", "ESPSESSIONID=0");
  httpServer.send(301);
  return;
}

/**
   Retrieve temperature humidity realtime data
*/
void handleTemperatureHumidity() {
  //Serial.println("handleTemperatureHumidity");

  //manageSecurity();

  //Serial.println("handleTemperatureHumidity security pass!");

  const size_t capacity = 1024;
  DynamicJsonDocument doc(capacity);

  doc["waterLevel"] = liters;
  doc["motorStatus"] =  motorEnabled;
  doc["isTimeSynchronized"] = isTimeSynchronized;
  doc["timerMotorAutoOff"] = timerMotorAutoOff / 1000;
  doc["timerMotorCooldown"] = timerMotorCooldown / 1000;
  doc["tod"] = tods_old;
  // If you don't have a DHT12 put only the library
  // comment upper line and decomment this line
  //  doc["humidity"] = random(10,80);
  //  doc["temp"] = random(1000,3500)/100.;

  String buf;
  serializeJson(doc, buf);
  httpServer.send(200, F("application/json"), buf);
}

//Check if header is present and correct
bool is_authenticated() {
  Serial.println("Enter is_authenticated");
  if (httpServer.hasHeader("Cookie")) {
    Serial.print("Found cookie: ");
    String cookie = httpServer.header("Cookie");
    Serial.println(cookie);

    String token = sha1(String(www_username) + ":" +
                        String(www_password) + ":" +
                        httpServer.client().remoteIP().toString());

    if (cookie.indexOf("ESPSESSIONID=" + token) != -1) {
      Serial.println("Authentication Successful");
      return true;
    }
  }
  Serial.println("Authentication Failed");
  return false;
}

bool manageSecurity() {
  if (!is_authenticated()) {
    httpServer.send(401, F("application/json"), "{\"msg\": \"You must authenticate!\"}");
    return false;
  }
  return true;
}

bool manageSecuritySend(String fileName, String contentType, HTTPMethod requestMethod) {
  if (!is_authenticated()) {
    httpServer.sendHeader("Location", "/login.html?msg=Authentication required");
    httpServer.sendHeader("Cache-Control", "no-cache");
    httpServer.send(301);
    return false;
  }
  File file = SPIFFS.open(fileName, "r");
  httpServer.streamFile(file, contentType, requestMethod);
  return true;
}

void restEndPoint() {
  // External rest end point (out of authentication)

  //// added security checking before send index or setup
  //httpServer.on("/", HTTP_GET, [&httpServer](){ manageSecuritySend("/index.html", "text/html", HTTP_GET);});
  //httpServer.on("/index.html", HTTP_GET, [&httpServer](){ manageSecuritySend("/index.html", "text/html", HTTP_GET);});
  //httpServer.on("/setup.html", HTTP_GET, [&httpServer](){ manageSecuritySend("/setup.html", "text/html", HTTP_GET);});
  
  httpServer.on("/login", HTTP_POST, handleLogin);
  httpServer.on("/logout", HTTP_GET, handleLogout);

  httpServer.on("/temperatureHumidity", HTTP_GET, handleTemperatureHumidity);
  httpServer.on("/on", HTTP_GET,  [](){motorOn(false);});
  httpServer.on("/off", HTTP_GET, motorOff);
  //
  httpServer.on(         "/alarm1Enabled",                        HTTP_GET, [&httpServer, &onAlarm1Enabled] (){ /*if (!manageSecurity()) return;*/ httpServer.send(200, F("application/json"), onAlarm1Enabled ? "true" : "false"); });
  httpServer.on(         "/alarm2Enabled",                        HTTP_GET, [&httpServer, &onAlarm2Enabled] (){ /*if (!manageSecurity()) return;*/ httpServer.send(200, F("application/json"), onAlarm2Enabled ? "true" : "false"); });
  httpServer.on(UriRegex("^\\/alarm1Enabled\\/(true|false)$" ),   HTTP_GET, [&httpServer, &onAlarm1Enabled] (){ /*if (!manageSecurity()) return;*/ onAlarm1Enabled = httpServer.pathArg(0) == "true";                                      SaveToEEPROM(EE_onAlarm1Enabled); });
  httpServer.on(UriRegex("^\\/alarm2Enabled\\/(true|false)$" ),   HTTP_GET, [&httpServer, &onAlarm2Enabled] (){ /*if (!manageSecurity()) return;*/ onAlarm2Enabled = httpServer.pathArg(0) == "true";                                      SaveToEEPROM(EE_onAlarm2Enabled); });
  httpServer.on(         "/alarm1TODsec",                         HTTP_GET, [&httpServer, &onAlarm1TODsec]  (){ /*if (!manageSecurity()) return;*/ httpServer.send(200, F("application/json"), String(onAlarm1TODsec, DEC)); });
  httpServer.on(         "/alarm2TODsec",                         HTTP_GET, [&httpServer, &onAlarm2TODsec]  (){ /*if (!manageSecurity()) return;*/ httpServer.send(200, F("application/json"), String(onAlarm2TODsec, DEC)); });
  httpServer.on(UriRegex("^\\/alarm1TODsec\\/([0-9]+)$" ),        HTTP_GET, [&httpServer, &onAlarm1TODsec]  (){ /*if (!manageSecurity()) return;*/ onAlarm1TODsec = strtoul(httpServer.pathArg(0).c_str(), nullptr, 10);                   SaveToEEPROM(EE_onAlarm1TODsec); });
  httpServer.on(UriRegex("^\\/alarm2TODsec\\/([0-9]+)$" ),        HTTP_GET, [&httpServer, &onAlarm2TODsec]  (){ /*if (!manageSecurity()) return;*/ onAlarm2TODsec = strtoul(httpServer.pathArg(0).c_str(), nullptr, 10);                   SaveToEEPROM(EE_onAlarm2TODsec); });
  httpServer.on(         "/tankTopCm",                            HTTP_GET, [&httpServer, &tankTopCm]       (){ /*if (!manageSecurity()) return;*/ httpServer.send(200, F("application/json"), String(tankTopCm, DEC)); });
  httpServer.on(         "/tankBottomCm",                         HTTP_GET, [&httpServer, &tankBottomCm]    (){ /*if (!manageSecurity()) return;*/ httpServer.send(200, F("application/json"), String(tankBottomCm, DEC)); });
  httpServer.on(UriRegex("^\\/tankTopCm\\/([0-9]+)$" ),           HTTP_GET, [&httpServer, &tankTopCm]       (){ /*if (!manageSecurity()) return;*/ tankTopCm = strtoul(httpServer.pathArg(0).c_str(), nullptr, 10);                        SaveToEEPROM(EE_tankTopCm); });
  httpServer.on(UriRegex("^\\/tankBottomCm\\/([0-9]+)$" ),        HTTP_GET, [&httpServer, &tankBottomCm]    (){ /*if (!manageSecurity()) return;*/ tankBottomCm = strtoul(httpServer.pathArg(0).c_str(), nullptr, 10);                     SaveToEEPROM(EE_tankBottomCm); });

}

void serverRouting() {
  restEndPoint();

  // Manage Web Server
  Serial.println(F("Go on not found!"));
  httpServer.onNotFound([]() {               // If the client requests any URI
    Serial.println(F("On not found"));
    if (!handleFileRead(httpServer.uri())) { // send it if it exists
      handleNotFound();// otherwise, respond with a 404 (Not Found) error
    }
  });

  Serial.println(F("Set cache!"));
  // Serve a file with no cache so every tile It's downloaded
  httpServer.serveStatic("/configuration.json", SPIFFS, "/configuration.json", "no-cache, no-store, must-revalidate");
  // Server all other page with long cache so browser chaching they
  httpServer.serveStatic("/", SPIFFS, "/", "max-age=31536000");

  //here the list of headers to be recorded
  const char * headerkeys[] = { "User-Agent", "Cookie" };
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);
  //ask server to track these headers
  httpServer.collectHeaders(headerkeys, headerkeyssize);
}

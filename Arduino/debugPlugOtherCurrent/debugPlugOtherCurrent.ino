#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiSTA.h>
#include <ESP8266WiFiType.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <NtpClientLib.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include "FS.h"
#include <Scheduler.h>

#define HISTORY_FILE "/WiFiConf.txt"
StaticJsonBuffer<288> JSONbuffer;   //Declaring static JSON buffer
int addrSSID = 0;
int addrPWD = 1;

ESP8266WebServer server(80);
IPAddress apIP(10, 10, 10, 1);
const byte DNS_PORT = 53;          // Capture DNS requests on port 53
DNSServer dnsServer;

String newHTML;
String responseHTML = "<!DOCTYPE html>"
  "<html lang=\"fr\">"
  "<head>"
    "<meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>AFG Autisme</title>"
  "</head>"
  "<body>"
  "<h1>AFG Autisme</h1>"
  "<h2>Interface de provision WiFi</h2>"
  "<form action='/' method='post'>"
    "<div>"
      "<label for='name'>SSID : </label>"
      "<SELECT name='user_ssid' size='1'>";


String device_id = "jh3A@itTbQ";

boolean isConfigure = false;
int APtime = 0;
int ConnectTime = 0;
const char* WiFiSSID; // = "testraspb"; //"micasa";
const char* WiFiPWD; // = "Raspberry2017"; //"1122334455";
const char* reWiFiSSID; 
const char* reWiFiPWD;

/////// Capteur PIR ///////
/////// Mode de mesure ////////
int sensorMode = 0;  // 0 : simple mouvement /// 1 : mouvement continu (possibilité de connaître la durée du mouvement)
// Pin digital connecté à la sortie du capteur PIR
int pirPin = 4;
//Temps de calibration (10-60 secs selon la datasheet)
int calibrationTime = 40;        
// Le temps que le capteur mets pour donner une impulsion LOW en output
long unsigned int lowIn;         
// Temps en millisecondes que le capteur doit être low pour que le mouvement s'arrête
long unsigned int pause = 10000; //60000; //5000;
// Conditions à vérifier
boolean lockLow = true;
boolean takeLowTime;

/////// Génération de la date avec NTP et algo pour économiser la batterie ///////
boolean syncOnce = false;
// Heure
long date;
long addSpringDate;
// Temps depuis le démarrage du nodeMCU
long dateSinceStart;
// Variable temp pour calculer l'heure à chaque mesure
long tmp;
long tmpMillis;


void setup() {
  Serial.begin(9600);

  newHTML = addScanToHTML (responseHTML, newHTML);

  SPIFFS.begin();  

  delay(1000);
  //WiFi.softAP(ssid, password);
  WiFi.mode(WIFI_AP);
 // WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("Ericsson Sleep Monitoring WiFi"); // WiFi name

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);

  // replay to all requests with same HTML
  server.onNotFound([]() {
    if (server.hasArg("user_ssid")) {
      handleSubmit();
    } else {
      handleRoot();
    }
  });
  server.begin(); 
  Serial.println("Server listening");

  /////////// WiFi provision ////////////
  //APtime = millis();
  Serial.print("Attente de la provision du WiFi : ");
  //while (millis() < 120000 || isConfigure == false) {
  while (APtime < 120000) {
    if (WiFiSSID == NULL && WiFiPWD == NULL) {
      dnsServer.processNextRequest();
      server.handleClient();
      delay(500);
      Serial.print(".");
      APtime = millis();
    } else {
      APtime = 120000;
    }
  }
  // Déconnexion de l'AP
  WiFi.softAPdisconnect(true);
  delay(1000);



  // Lecture du fichier WiFiConf
  loadHistory();

  /////////// Connexion WiFi ////////////
  delay(1000);
  wifi_connect(WiFiSSID, WiFiPWD);

  /////////// Serveur NTP ////////////
  Serial.println();
  Serial.println("Connexion au serveur NTP : ");
  NTP.onNTPSyncEvent([](NTPSyncEvent_t error) {
    if (error) {
      Serial.print("Time Sync error : ");
      if (error == noResponse){
        Serial.println("NTP server not reachable");
      }else if (error == invalidAddress){
        Serial.println("Invalid NTP server address");
      }
    } else {
      if (syncOnce == false) {
        syncOnce = true;
        // Affichages pour demonstration des méthodes NTP
//        Serial.print("Voici la date et l'heure : ");
//        Serial.println(NTP.getTimeDateString(NTP.getLastNTPSync()));
//        Serial.print("Voici la date : ");
//        Serial.println(NTP.getDateStr(NTP.getLastNTPSync()));
//        Serial.print("Voici l'heure : ");
//        Serial.println(NTP.getTimeStr(NTP.getLastNTPSync()));
        Serial.print("Timestamp reçu : "); 
        Serial.println(NTP.getLastNTPSync());
//        Serial.print("Voici le TimeZone : ");
//        Serial.println(NTP.getTimeZone());
//        Serial.print("Heure d'été ? : ");
//        Serial.println(NTP.isSummerTimePeriod(NTP.getLastNTPSync()));//isSummerTimePeriod(NTP.getLastNTPSync()));
//        Serial.println();
//        if (NTP.isSummerTimePeriod(NTP.getLastNTPSync()) == 0) {
//          addSpringDate = 1;
//        } else {
//          addSpringDate = 0;
//        }
        // Code pour algo d'incrémentation de l'heure
        //date = NTP.getLastNTPSync() - NTP.getTimeZone()*3600 - addSpringDate*3600; // date <== timestamp
        date = NTP.getLastNTPSync();
        dateSinceStart = millis();
      }
    }
  });

  while (syncOnce == false) {
    NTP.begin("pool.ntp.org"); //, 1, true);
    delay(1000);
  }

  // Arrêt du serveur NTP
  NTP.stop();
  delay(1000);

  // Déconnection du WiFi
  //wifi_disconnect();

  /////////// Calibration du capteur PIR ////////////
  pinMode(pirPin, INPUT_PULLUP);
  //digitalWrite(pirPin, LOW);
  // Le temps de calibrer le capteur PIR ...
  delay(1000);
  Serial.println();
  Serial.print("Calibration du capteur ");
  for(int i = 0; i < calibrationTime; i++){
    Serial.print(".");
    delay(1000);
  }
  Serial.println(" Terminé");
  Serial.println("Capteur actif");
  delay(50);
  
}



void loop() {
  if (sensorMode == 0) {
    if(lockLow){
      if(digitalRead(pirPin) == LOW){
        // lockLow permet d'assurer la transition à LOW avant de mesurer tout mouvements en output
        lockLow = false; 
        tmp = date + ((millis() - dateSinceStart)/1000);  
        tmpMillis = millis();         
        Serial.println("---");
        Serial.print("Mouvement détecté : ");
        Serial.println(tmp);
        delay(50);
        send_message(tmp, 1);
        Serial.print(pause/1000);
        Serial.print(" secondes d'attente avant le prochain mouvement : ");
      } 
    } else {
      delay(1000);
      Serial.print(".");
      if((tmp + (millis()-tmpMillis)/1000) >= (tmp + pause/1000)) {
        Serial.println();
        Serial.println("Un nouveau mouvement peut maintenant être mesuré");
        lockLow = true;
      }
    }   
  } else {
    if(digitalRead(pirPin) == LOW){
      if(lockLow){  
        // lockLow permet d'assurer la transition à LOW avant de mesurer tout mouvements en output
        lockLow = false; 
        tmp = date + ((millis() - dateSinceStart)/1000);           
        Serial.println("---");
        Serial.print("Mouvement détecté à ");
        Serial.println(NTP.getTimeDateString(tmp));
        delay(50);
        send_message(tmp, 1);
      }         
      takeLowTime = true;
    } 
  
    if(digitalRead(pirPin) == HIGH){       
      if(takeLowTime){
        lowIn = millis();          // Compte le temps de transition entre HIGH et LOW
        takeLowTime = false;       // Assure que cela est réalisé au départ d'une phase LOW
      }
      // Si le capteur reste à LOW pendant plus longtemps que la pause fixée (5sec), alors on assume qu'il n'y a plus de mouvements
      if(!lockLow && millis() - lowIn > pause){  
        // Ce code s'effectue seulement après un mouvement
        lockLow = true;
        tmp = date + ((millis() - dateSinceStart)/1000);                     
        Serial.print("Mouvement terminé à ");
        Serial.println(NTP.getTimeDateString(tmp - (pause/1000)));
        delay(50);
      }
    }
  }
}





void handleRoot() {
  //server.send(200, "text/html", responseHTML);
  server.send(200, "text/html", newHTML);
}

void handleSubmit() {
  String SSIDvalue;
  String PWDvalue;

  if (!server.hasArg("user_ssid")) return returnFail("BAD ARGS");
  
  if (server.args() > 0 ) {
    // SSID
    SSIDvalue = server.arg("user_ssid");
    WiFiSSID = strdup(SSIDvalue.c_str());

    // Password
    PWDvalue = server.arg("user_pwd");
    WiFiPWD = strdup(PWDvalue.c_str());

    // Create JSON
    //StaticJsonBuffer<288> JSONbuffer;   //Declaring static JSON buffer
    JsonObject& JSONencoder = JSONbuffer.createObject(); 
    JSONencoder["SSID"] = WiFiSSID;
    JSONencoder["PWD"] = WiFiPWD;
    char JSONWiFiBuffer[288];
    JSONencoder.prettyPrintTo(JSONWiFiBuffer, sizeof(JSONWiFiBuffer));

    // Store JSON in WiFiConf file
    saveHistory(JSONencoder);
    
    // Send page OK
    returnOK();

    //isConfigure = true;
  }
}

void returnFail(String msg) {
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(500, "text/plain", msg + "\r\n");
}

void returnOK() {
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Configuration du WiFi prise en compte.\r\n Si l'AP disparait : Connexion WiFi OK.\r\n Si l'AP persiste : Connexion WiFi NOK.\r\n");
}






void wifi_connect(const char* WiFiSSID, const char* WiFiPWD) {
  Serial.println();
  Serial.println("Connexion au WiFi : ");
  
  // Déconnexion des précédentes connexions
  //wifi_disconnect();
  //delay(1000);
  
  // Scan des réseaux
  Serial.print("Nombre de réseaux : ");
  Serial.println(WiFi.scanNetworks());
  delay(500);
  
  // Affichage des SSIDs
  Serial.println("Liste des SSIDs : ");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++){
    Serial.println(WiFi.SSID(i));
  }
  
  Serial.println();
  // Connexion au SSID souhaité
  WiFi.begin(WiFiSSID, WiFiPWD);
  Serial.print("En attente de connexion ");
  //while (WiFi.status() != WL_CONNECTED){
  int thisTime = millis();
  while (ConnectTime < thisTime + 10000) {
    if (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      ConnectTime = millis();
    } else {
      ConnectTime = thisTime + 10000;
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    //WiFi.softAPdisconnect(true);
    // Connexion réussie : affichage des paramètres
    Serial.println(" Connecté !");
    Serial.print("SSID : ");
    Serial.println(WiFi.SSID());
    Serial.print("Addresse IP : ");
    Serial.println(WiFi.localIP());
    Serial.print("Adresse MAC : ");
    Serial.println(WiFi.macAddress());
    Serial.print("Puissance (dB) : ");
    Serial.println(WiFi.RSSI());
  } else {
    //WiFi.softAPdisconnect(true);
    newHTML = addScanToHTML (responseHTML, newHTML);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("Ericsson Sleep Monitoring WiFi"); // WiFi name
    dnsServer.start(DNS_PORT, "*", apIP);
    server.onNotFound([]() {
      if (server.hasArg("user_ssid")) {
        handleSubmit();
      } else {
        handleRoot();
      }
    });
    server.begin(); 
    Serial.println("Server listening");
  
    /////////// WiFi provision ////////////
    //APtime = millis();
    APtime = 0;
    int errorTime = millis();
    const char* testSSID = WiFiSSID;
    const char* testPWD = WiFiPWD;
    Serial.print("Attente de la provision du WiFi : ");
    //while (millis() < 120000 || isConfigure == false) {
    while (APtime < errorTime + 60000) {
      if (WiFiSSID == testSSID && WiFiPWD == testPWD) {
        dnsServer.processNextRequest();
        server.handleClient();
        delay(500);
        Serial.print(".");
        APtime = millis();
      } else {
        APtime = errorTime + 60000;
      }
    }

    WiFi.softAPdisconnect(true);
    
    // Lecture du fichier WiFiConf
    delay(1000);
    reloadHistory();
    /////////// Connexion WiFi ////////////
    delay(1000);
    Serial.print(reWiFiSSID);
    Serial.print(reWiFiPWD);
    wifi_connect(reWiFiSSID, reWiFiPWD);
  }
}


void wifi_disconnect() {
  if(WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
    delay(1000);
    while(WiFi.status() == WL_CONNECTED){
      delay(500);
      Serial.print(".");
    }
    Serial.println("WiFi déconnecté !");
    delay(1000);
  }
  return;
}

void send_message(long date, int value) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error in WiFi connection");
    wifi_connect(WiFiSSID, WiFiPWD);
  }
  
  if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
    
    StaticJsonBuffer<288> JSONbuffer;   //Declaring static JSON buffer
    JsonObject& JSONencoder = JSONbuffer.createObject(); 
 
    //JSONencoder["serial"] = [0, 0, 0, 0, 0, 5, 210, 197];
    JsonArray& serial = JSONencoder.createNestedArray("serial");
    serial.add(0);
    serial.add(0);
    serial.add(0);
    serial.add(0);
    serial.add(0);
    serial.add(5);
    serial.add(210);
    serial.add(197);
    JSONencoder["type"] = "motion";
    JSONencoder["date"] = date;
    JSONencoder["dValue"];
    JSONencoder["binValue"] = value;
  
    char JSONmessageBuffer[288];
    JSONencoder.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
    //Serial.println(JSONmessageBuffer);
 
    HTTPClient http;    //Declare object of class HTTPClient
 
    http.begin("http://35.187.36.227/api/event");      //Specify request destination
    http.addHeader("Content-Type", "application/json");  //Specify content-type header
 
    int httpCode = http.POST(JSONmessageBuffer);   //Send the request
    //String payload = http.getString();                                        //Get the response payload
    
    //Serial.println(httpCode);   //Print HTTP return code
    //Serial.println(payload);    //Print request response payload

    if(httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if(httpCode != 201) {
          String payload = http.getString();
          Serial.println(payload);
      } else {
        Serial.println("Data sended");
      }
    } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    } 
    http.end();  //Close connection
  } else {
    Serial.println("Error in WiFi connection");
  }
}




void saveHistory(JsonObject& WiFiConf){
  Serial.println("Ecriture du fichier WiFiConf");
  File historyFile = SPIFFS.open(HISTORY_FILE, "w");
  WiFiConf.printTo(historyFile); // Exporte et enregsitre le JSON dans la zone SPIFFS - Export and save JSON object to SPIFFS area
  historyFile.close();  
}

void loadHistory(){
  File file = SPIFFS.open(HISTORY_FILE, "r");
  if (!file){
    Serial.println("Aucun fichier WiFiConf existe");
  } else {
    size_t size = file.size();
    if ( size == 0 ) {
      Serial.println("Fichier WiFiConf vide !");
    } else {
      std::unique_ptr<char[]> buf (new char[size]);
      file.readBytes(buf.get(), size);
      JsonObject& root = JSONbuffer.parseObject(buf.get());
      if (!root.success()) {
        Serial.println("Impossible de lire le JSON WiFiConf");
      } else {
        Serial.println("JSON WiFiConf à été chargé : ");
        root.printTo(Serial);
        WiFiSSID = strdup(root["SSID"]);
        WiFiPWD = strdup(root["PWD"]);
      }
    }
    file.close();
  }
}

void reloadHistory(){
  File file = SPIFFS.open(HISTORY_FILE, "r");
  if (!file){
    Serial.println("Aucun fichier WiFiConf existe");
  } else {
    size_t size = file.size();
    if ( size == 0 ) {
      Serial.println("Fichier WiFiConf vide !");
    } else {
      std::unique_ptr<char[]> buf (new char[size]);
      file.readBytes(buf.get(), size);
      JsonObject& root = JSONbuffer.parseObject(buf.get());
      if (!root.success()) {
        Serial.println("Impossible de lire le JSON WiFiConf");
      } else {
        Serial.println("JSON WiFiConf à été chargé : ");
        root.printTo(Serial);
        reWiFiSSID = strdup(root["SSID"]);
        reWiFiPWD = strdup(root["PWD"]);
      }
    }
    file.close();
  }
}


String addScanToHTML (String responseHTML, String newHTML) {
  int n = WiFi.scanNetworks();
  newHTML = responseHTML;
  for (int j = 0; j < n; j++){
        newHTML += "<OPTION>";
        newHTML += WiFi.SSID(j);
      }
      newHTML += "</SELECT>"
      //"<input type='text' id='ssid' name='user_ssid'>"
    "</div>"
    "<div>"
      "<label for='mail'>Password : </label>"
        "<input type='text' id='pwd' name='user_pwd'>"
    "</div>"
    "<div class='button'>"
      "<button type='submit'>Enregistrer</button>"
  "</form>"
  "</body>"
  "</html>";
  
  return newHTML;
}


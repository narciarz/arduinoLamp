#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <credentials.h>

/*Add sensitive mappings */
const char* ssid = mySSID;
const char* password = myPASSWORD;
const char* otapass = myOTAPASS;
const char* apikey = myAPIKEY;
const char* jssecret = myJSSECRET;

const char* host = "esp8266-LivingRoomBcp"; /*Add OTA deviceName */

ESP8266WebServer server(80);   //instantiate server at port 80 (http port)

String acsCtrlAllowOrigin = "*";

String apiKeyValue = myAPIKEY;
String jsSecretResponse = "{\"key\":\""+(String)myJSSECRET+"\"}";

//enter your file name
const char* file = "/config.json";
DynamicJsonDocument cfg(1024);


void updateMemoryConfig(String VarName, String VarValue) {
  Serial.printf("Saving:%s=%s\n", VarName, VarValue);
  cfg[VarName] = VarValue;
}

void handleServerHeaders() {
  const char * headerkeys[] = {"X-ApiKey"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  server.collectHeaders(headerkeys, headerkeyssize);
}

bool is_authentified(){
  String apiHeaderName = "X-ApiKey";
  if (server.hasHeader(apiHeaderName)){
    Serial.printf("Found %s: ", apiHeaderName);
    String apiKey = server.header(apiHeaderName);
    Serial.println(apiKey);
    if (apiKey == apiKeyValue) {
      Serial.println("Authentification Successful");
      return true;
    }
  }
  Serial.println("Authentification Failed");
  return false;
}

void saveMemoryConfigToDisk() {
  File configFile = SPIFFS.open(file, "w");
  serializeJson(cfg, configFile);
  serializeJson(cfg, Serial);
  configFile.close();
  Serial.println("Config saved to disk");
}

void loadMemoryConfigFromDisk() {
  Serial.println("loadMemoryConfigFromDisk");
  
  SPIFFS.begin();
  File configFile = SPIFFS.open(file, "r");
  if (!configFile) {
    Serial.println("- failed to open config file for reading");
    deserializeJson(cfg, "{}");
  } else {
    deserializeJson(cfg, configFile);
  }
  Serial.println("Loading");
  serializeJson(cfg, Serial);
  configFile.close();

  JsonObject root = cfg.as<JsonObject>();

  for (JsonPair kv  : root) {
    String key = (String)kv.key().c_str();
    String value = kv.value().as<char*>();
      handlePin(key.toInt(), value);
  }
  Serial.println("Config loaded");
}

void cleanMemoryConfigDisk() {
  SPIFFS.format();
  Serial.println("SPIFFS formatted");
}

boolean handlePin(int gpPin, String newStatus) {
  Serial.printf("handlePin gpPin %d newStatus %s\n", gpPin, newStatus);
  if ((gpPin >= 0 && gpPin <= 5) || (gpPin >= 12 && gpPin <= 16)) {
    pinMode(gpPin, OUTPUT);
    if (newStatus == "on") {
      Serial.printf("digitalWrite %s\n", newStatus);
      digitalWrite(gpPin, LOW);
    } else {
      digitalWrite(gpPin, HIGH);
    }
    return true;
  } else {
    Serial.println("Bad PIN");
  }
  return false;
}

void handleNotFound() {
  String message = "Page not found";
  server.send(404, "text/plain", message);
}

void handleCors() {
  Serial.println("Options...");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Origin", acsCtrlAllowOrigin);
  server.send(204);
}

void setup() {
  delay(1000);
  Serial.begin(115200);  
  Serial.println("Booting");
  
  loadMemoryConfigFromDisk();

  WiFi.begin(ssid, password); //begin WiFi connection
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
  
  Serial.print("OTA host: ");
  ArduinoOTA.setHostname(host);
  ArduinoOTA.setPassword(otapass);
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

  server.on("/", []() {
    Serial.println("Root...");
    server.send(200, "text/html", "OK");
  });
  server.on("/config/saveMemoryToDisk", HTTP_OPTIONS, []() {
    handleCors();
  });
  server.on("/config/saveMemoryToDisk", []() {
    server.sendHeader("Access-Control-Allow-Origin", acsCtrlAllowOrigin);
    Serial.println("Save memory to disk...");
    if (!is_authentified()) {
      server.send(401, "text/html", "Access Forbidden");
      return;
    }
    if (cfg.isNull()) {
      Serial.println("Empty memory - nothing to save");
      server.send(204, "text/html", "Empty memory, nothing to save");
      return;
    }
    String output;
    serializeJson(cfg, output);
    saveMemoryConfigToDisk();
    server.send(200, "application/json", output);
  });
  server.on("/config/getFromMemory", HTTP_OPTIONS, []() {
    handleCors();
  });
  server.on("/config/getFromMemory", []() {
    server.sendHeader("Access-Control-Allow-Origin", acsCtrlAllowOrigin);
    Serial.println("Get config from memory...");
    if (!is_authentified()) {
      server.send(401, "text/html", "Access Forbidden");
      return;
    }
    if (cfg.isNull()) {
      Serial.println("Empty memory");
      server.send(204, "text/html", "Empty memory");
      return;
    }
    String output;
    serializeJson(cfg, output);
    server.send(200, "application/json", output);
  });
  server.on("/config/loadFromDiskToMemory", []() {
    Serial.println("Load config from memory to disk...");
    if (!is_authentified()) {
      server.send(401, "text/html", "Access Forbidden");
      return;
    }
    loadMemoryConfigFromDisk();
    server.send(200, "application/json", "OK");
  });
  server.on("/config/cleanDisk", HTTP_OPTIONS, []() {
    handleCors();
  });
  server.on("/config/cleanDisk", []() {
    server.sendHeader("Access-Control-Allow-Origin", acsCtrlAllowOrigin);
    Serial.println("Clean disk...");
    if (!is_authentified()) {
      server.send(401, "text/html", "Access Forbidden");
      return;
    }
    cleanMemoryConfigDisk();
    server.send(200, "text/html", "OK");
  });
  server.on("/config/getToken", HTTP_OPTIONS, []() {
    handleCors();
  });
  server.on("/config/getToken", []() {
    server.sendHeader("Access-Control-Allow-Origin", acsCtrlAllowOrigin);
    server.send(200, "application/json", jsSecretResponse);
  });
  server.on("/utils/restart", HTTP_OPTIONS, []() {
    handleCors();
  });
  server.on("/utils/restart", []() {
    server.sendHeader("Access-Control-Allow-Origin", acsCtrlAllowOrigin);
    Serial.println("Restart...");
    if (!is_authentified()) {
      server.send(401, "text/html", "Access Forbidden");
      return;
    }
    server.send(200, "text/html", "OK");
    delay(1000);
    ESP.restart();
  });
  server.on("/utils/getWifiStatus", HTTP_OPTIONS, []() {
    handleCors();
  });
  server.on("/utils/getWifiStatus", []() {
    server.sendHeader("Access-Control-Allow-Origin", acsCtrlAllowOrigin);
    Serial.println("Obtain Wifi Status...");
    if (!is_authentified()) {
      server.send(401, "text/html", "Access Forbidden");
      return;
    }
    long wifiSignal = WiFi.RSSI();
    Serial.printf("RSSI: %d dBm\n", wifiSignal);
    server.send(200, "text/html", "{\"wifi\":"+(String)wifiSignal+"}");
  });
  
  server.onNotFound(handleNotFound);

  server.on("/gpio/update", HTTP_OPTIONS, []() {
    handleCors();
  });
  
  server.on("/gpio/update", HTTP_POST, []() {
    server.sendHeader("Access-Control-Allow-Origin", acsCtrlAllowOrigin);
    Serial.println("Gpio update...");
    if (!is_authentified()) {
      server.send(401, "text/html", "Access Forbidden");
      return;
    }
    StaticJsonDocument<200> doc;
    auto error = deserializeJson(doc, server.arg("plain"));
    if (error) {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(error.c_str());
      return;
    }
    serializeJson(doc, Serial);
    int id = doc["id"];
    String expectedStatus = doc["status"];
    boolean changeResult=handlePin(id, expectedStatus);
    Serial.printf("\nResult handlePin=%d\n", changeResult);
    if (changeResult) {
      Serial.println("succeed");
      updateMemoryConfig((String) id, expectedStatus);
      server.send ( 200, "application/json", "{success:true}" );
    } else {
      Serial.println("failed");
      server.send ( 400, "text/html", "Request payload error" );
    }
  });
  handleServerHeaders();
  server.begin();
  Serial.println("Web server started!");
}

void loop(void) {
  ArduinoOTA.handle();
  server.handleClient();
}

#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <FS.h>



// Replace with your network credentials
const char* ssid = "######"; /* Add your router's SSID */
const char* password = "######"; /*Add the password */

ESP8266WebServer server(80);   //instantiate server at port 80 (http port)

String page = "";
String okPage = "{status: \"OK\"}";
int LEDPin = 2;
String apiKeyValue = "secretKey";

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
  if (gpPin != 0) {
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
void initializeFileSystem() {
  //Initialize File system
//  SPIFFS.begin();
  Serial.println("File System Initialized");
  //open file for reading
  File dataFile = SPIFFS.open(file, "r");
  Serial.println("Reading Data from File:");
  //Data from file
  //Read upto complete size
  for (int i = 0; i < dataFile.size(); i++)
  {
    Serial.print((char)dataFile.read());
  }

}
void setup() {
  delay(1000);
  Serial.begin(115200);


//  initializeFileSystem();
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

  server.on("/", []() {
    Serial.println("Root...");
    server.send(200, "text/html", "OK");
  });
  server.on("/saveMemoryToDisk", []() {
    Serial.println("Save memory to disk...");
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
  server.on("/config/getFromMemory", []() {
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
    loadMemoryConfigFromDisk();
    server.send(200, "application/json", "OK");
  });
  server.on("/cleanDisk", []() {
    Serial.println("Clean Disk...");
    cleanMemoryConfigDisk();
    server.send(200, "text/html", "OK");
  });
  server.on("/utils/restart", []() {
    Serial.println("Restart...");
    server.send(200, "text/html", "OK");
    delay(1000);
    ESP.restart();
  });
  server.on("/GPIO4OFF", []() {
    server.send(200, "application/json", okPage);
    digitalWrite(LEDPin, HIGH);
//    handlePin(16, "off");
    delay(1000);
  });
  server.on("/GPIO4ON", []() {
    server.send(200, "application/json", okPage);
    digitalWrite(LEDPin, LOW);
//    handlePin(16, "on");
    delay(1000);
  });
  server.onNotFound(handleNotFound);

  server.on("/gpio/update", HTTP_POST, []() {
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

//  server.on("/config/set", HTTP_POST, []() {
//    StaticJsonDocument<200> doc;
//    //    JsonObject& newjson = doc.parseObject(server.arg("plain"));
//    auto error = deserializeJson(doc, server.arg("plain"));
//    if (error) {
//      Serial.print(F("deserializeJson() failed with code "));
//      Serial.println(error.c_str());
//      return;
//    }
//    serializeJson(doc, Serial);
//    String value = doc["lol"];
//    Serial.println(value);
//    server.send ( 200, "text/json", "{success:true}" );
//    //    StaticJsonBuffer<200> newBuffer;
//    //    JsonObject& newjson = newBuffer.parseObject(server.arg("plain"));
//    //
//    //    Serial.println(newjson);
//    //
//    //    server.send ( 200, "text/json", "{success:true}" );
//  });

  handleServerHeaders();
  server.begin();
  Serial.println("Web server started!");
}

void loop(void) {
  server.handleClient();
}

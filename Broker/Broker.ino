/** How2 connect 2 the DtHack Cloud server
Author: Mrs Christin Koss

please also check the wiki:

cloud:
GUI:    https://130.61.84.222:4443
REST:   https://130.61.84.222:4442/rest/trackathon

[
{
"site":      "yourUserAccoutName",
"collector": "123",          // must be unique for several devices
"timestamp": "1536151106",      // e.g  epochTime format
"device":    "d4e6b7d51bd5",    // e.g. MAC
"point":     "staticmac",
"value":     1
},
….
]

*/

// Include packet id feature
// the sender is the receiver on the PJON line
// we use bitbannging on port D3
#define PJON_PIN D7

#define PJON_INCLUDE_PACKET_ID true

// Uncomment to run SoftwareBitBang in MODE 2
// #define SWBB_MODE 2
// Uncomment to run SoftwareBitBang in MODE 3
// #define SWBB_MODE 3

#include <PJON.h>


#include <time.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>



float PJON_test;
float PJON_mistakes;
int PJON_busy;
int PJON_fail;

// <Strategy name> bus(selected device id)
PJON<SoftwareBitBang> bus(2);

char jsonBuffer[1000] = { 0 }; 

//wlanRouter @ Home  / e.g. the AP
char ssid[] = "TelekomOS3";
char pass[] = "#dthack18";

WiFiClientSecure client;

//epochtime / time 
int timezone = 0;   // 2 wäre genau passend für D-land...wird aber in der cloud gemacht deshalb 0
int dst = 0;
struct tm * timeinfo;
time_t now;


unsigned long OUTER_LOOP_LONG_TIMER = 20000;
unsigned long refreshTheLoopTimer_passedMillies = 0;

bool postDataToCloudServer() {
  bool weGotGoodData = false;                           
  client = WiFiClientSecure();
  if (WiFi.status() == WL_CONNECTED) {
    const char* fingerprint = "7e37b7b83766474ceec4a2144fd77b4799f276b3";
    if (client.verify(fingerprint, "130.61.84.222")) {
      //    
    }
    else {
      //  
    }
    if (client.connect("130.61.84.222", 4442)) {
        Serial.println(F("postDataToCloudServer() / connected 2 cloud")); // EveryThing is fine         
    }
    else
    {
        Serial.println(F("Failed to connect to cloudServer.."));            
        return false;
    }   
    String data_length = String(strlen(jsonBuffer) + 1);
    client.println(F("POST /rest/trackathon HTTP/1.1"));    
    client.println(F("Host: 130.61.84.222"));
    client.println("Connection: close");
    client.println(F("Cache-Control: no-cache"));
    client.println(F("Content-Type: application/json"));
    client.print(F("Content-Length: "));  client.println(data_length);
    client.println();
    client.println(jsonBuffer);
        
    while (!!!client.available()) {
      delay(50);
      Serial.print(".!");   
    }   
    String respLine = "";
    while (client.available()) {
      respLine = client.readStringUntil('\r');
      Serial.print(respLine);
      if (respLine.indexOf("200 OK")) weGotGoodData = true;     
    }
    if (weGotGoodData == true) {
      Serial.print(F("\nTransMission-OK: ")); Serial.println("200 OK");
    }
    else {
      Serial.println(F("TransMission-Error! "));      
    }       
    client.stop();
  }
  else
  {
    Serial.println(F("Failure: Failed to connect to local HotSpot.."));
  } 
  return true;
}

void WIFI_Connect()
{
  if (WiFi.status() != WL_CONNECTED) {                   
    WiFi.mode(WIFI_STA);
    delay(300);
    Serial.println(F("\Connecting to Wifi"));
    WiFi.begin(ssid, pass);   
    Serial.print(".");  
  }
  while (WiFi.status() != WL_CONNECTED) {
    delay((uint32_t)500);
    Serial.print(F("."));
  }
}

void setup() {
setupPJON();
  Serial.begin(115200);   
  WIFI_Connect(); 

  configTime(timezone * 3600, dst * 0, "pool.ntp.org", "time.nist.gov");
  Serial.println(F("\nWaiting for time"));
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("");
  now = time(nullptr);  //LOGLN(ctime(&now));
  struct tm * timeinfo;
  time(&now);
  timeinfo = localtime(&now); 
}


void loop() {

  unsigned long currentMillis = millis(); // grab current time  -  check if "interval" time has passed (eg. some sec to do something )
  
  loopPJON();

  return;

  if ((unsigned long)(currentMillis - refreshTheLoopTimer_passedMillies) >= OUTER_LOOP_LONG_TIMER) {
    refreshTheLoopTimer_passedMillies = millis();
        
    long mac = 0x0800271F4800 | random(0, 30);
    char macc[13];
    sprintf(macc, "%x", mac);
    Serial.print("MAC: "); Serial.println(macc);

    now = time(nullptr);  struct tm * timeinfo; time(&now); timeinfo = localtime(&now);
    char time_c[13]; sprintf(time_c, "%d", now);
    strcat(jsonBuffer, "[{\"site\":");  strcat(jsonBuffer, "\"arduino\"");
    strcat(jsonBuffer, ",");
    // Important: Every collector must have its own Identifier
    strcat(jsonBuffer, "\"collector\":");   strcat(jsonBuffer, "1");
    strcat(jsonBuffer, ",");    
    strcat(jsonBuffer, "\"timestamp\":");   strcat(jsonBuffer, "\""); strcat(jsonBuffer, time_c);  strcat(jsonBuffer, "\"");
    strcat(jsonBuffer, ",");
    strcat(jsonBuffer, "\"device\":");    strcat(jsonBuffer, "\"");  strcat(jsonBuffer,macc); strcat(jsonBuffer, "\"");
    strcat(jsonBuffer, ",");
    strcat(jsonBuffer, "\"point\":");   strcat(jsonBuffer, "\"staticmac\"");
    strcat(jsonBuffer, ",");
    strcat(jsonBuffer, "\"value\":");   strcat(jsonBuffer, "1");
    strcat(jsonBuffer, "}]");
    Serial.print("filled jsonBuffer: "); Serial.println(jsonBuffer);
    
    postDataToCloudServer();
    //refresh the jsonBuffer
    memset(jsonBuffer, 0, sizeof(jsonBuffer));
    jsonBuffer[0] = '\0'; 
  }

}



void setupPJON() {
  bus.strategy.set_pin(PJON_PIN);
  bus.set_receiver(receiver_function);
  bus.begin();

  Serial.begin(115200);
};

void receiver_function(
  uint8_t *payload, 
  uint16_t length, 
  const PJON_Packet_Info &packet_info
) {
  if (length > 0) {
    char * dataString = NULL;
    dataString = (char *)malloc(length + 1);
    strncpy (dataString, (char *)payload, length);
    dataString[length] = 0;
    Serial.print (dataString);
    Serial.print ("\n");
    free(dataString);  
  }
    digitalWrite(13, HIGH);
    delay(30);
    digitalWrite(13, LOW);
}

/* void receiver_function(uint8_t *payload, uint16_t length, const PJON_Packet_Info &packet_info) {
  Serial.print("Packet id: ");
  Serial.println(packet_info.id);
  Serial.flush();
} */

void loopPJON() {
  bus.receive(1000);
};

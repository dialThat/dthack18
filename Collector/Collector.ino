/** WifiScan + OLED +  cloud
Author: Mrs Christin Koss

please also check the wiki:

Challenge:

- setup and cofigure the LIBs in Arduino Properly
- check the serial Monitor
- build / deploy the application to the NodeMcu
- Understand the basic Concept - What  the methods do?
- make some basic tests concerning OLED Display

*/
// Include packet id feature
#define PJON_INCLUDE_PACKET_ID true

// Uncomment to run SoftwareBitBang in MODE 2
// #define SWBB_MODE 2
// Uncomment to run SoftwareBitBang in MODE 3
// #define SWBB_MODE 3

#include <PJON.h>

// <Strategy name> bus(selected device id)
PJON<SoftwareBitBang> bus(45);

int packet;
#define PJON_PIN D3

char content[] = "01234567890123456789";


#define DISABLE 0
#define ENABLE  1


//  wifi
#define DATA_LENGTH           112
#define TYPE_MANAGEMENT       0x00
#define TYPE_CONTROL          0x01
#define TYPE_DATA             0x02
#define SUBTYPE_PROBE_REQUEST 0x04
#define CHANNEL_HOP_INTERVAL_MS   2000


//All u NEED for the OLED DISPLAY I2C 
#include <Wire.h>    // 
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h> // Original .h  zZ ein 64 Bit Display  - https://github.com/adafruit/Adafruit_SSD1306/issues/57
#include <user_interface.h>
#include <WiFiManager.h>


static os_timer_t channelHop_timer;


int oledRowCount = 0;
#define I2C_ADDRESS 0x3C    // Define proper RST_PIN if required.
#define RST_PIN -1
SSD1306AsciiWire oled;

void setup() {
  setupPJON();
  setupWifi();
  setupOled();
}


/**
* Callback for promiscuous mode
*Meaning of ICACHE_FLASH_ATTR: http://bbs.espressif.com/viewtopic.php?t=1183
*Die bedeutung ist wohl nicht ganz klar!?
*/
static void ICACHE_FLASH_ATTR sniffer_callback(uint8_t *buffer, uint16_t length) {
  struct SnifferPacket *snifferPacket = (struct SnifferPacket*) buffer;
  showMetadata(snifferPacket);
}

void setupOled() {

  Serial.begin(115200);   // OLED
  Wire.begin(D1, D4);     // sda, scl ok: (D1,D2)
  Wire.setClock(400000L);
  oled.begin(&Adafruit128x64, I2C_ADDRESS);
  oled.setFont(Adafruit5x7);
  oled.clear();
  oled.set1X();
  oledRowCount = 0;
  oled.clear();
  
}

void CheckOledMessage() {
  //Display  ** Display  ** Dosplay ************************
  if (oledRowCount >= 8) {  //bei 128 x 32  sind das 4   or bei 128 x 64  sind das 8
    oledRowCount = 0;
    oled.clear();   //oled.set1X();  //  alles ist auf 1 .. 
    oled.println("dthack18");
  }
  ++oledRowCount; // jedes CRLF ist eine Zeile und zählt als rowCount 
}

void loop() {

  CheckOledMessage();
    if(!bus.update()) packet = bus.send(44, content, 20);
  delay(200);
}

/**
* Callback for channel hoping
*/
void channelHop()
{
  // hoping channels 1-14
  uint8 new_channel = wifi_get_channel() + 1;
  if (new_channel > 14)
    new_channel = 1;
  wifi_set_channel(new_channel);
}


void setupWifi() {
  // set the WiFi chip to "promiscuous" mode aka monitor mode
  Serial.begin(115200);
  delay(10);
  wifi_set_opmode(STATION_MODE);
  wifi_set_channel(1);
  wifi_promiscuous_enable(DISABLE);
  delay(10);
  wifi_set_promiscuous_rx_cb(sniffer_callback);
  delay(10);
  wifi_promiscuous_enable(ENABLE);

  // setup the channel hoping callback timer
  os_timer_disarm(&channelHop_timer);
  os_timer_setfn(&channelHop_timer, (os_timer_func_t *)channelHop, NULL);
  os_timer_arm(&channelHop_timer, CHANNEL_HOP_INTERVAL_MS, 1);
}



struct RxControl {
  signed rssi : 8; // signal intensity of packet
  unsigned rate : 4;
  unsigned is_group : 1;
  unsigned : 1;
  unsigned sig_mode : 2; // 0:is 11n packet; 1:is not 11n packet;
  unsigned legacy_length : 12; // if not 11n packet, shows length of packet.
  unsigned damatch0 : 1;
  unsigned damatch1 : 1;
  unsigned bssidmatch0 : 1;
  unsigned bssidmatch1 : 1;
  unsigned MCS : 7; // if is 11n packet, shows the modulation and code used (range from 0 to 76)
  unsigned CWB : 1; // if is 11n packet, shows if is HT40 packet or not
  unsigned HT_length : 16;// if is 11n packet, shows length of packet.
  unsigned Smoothing : 1;
  unsigned Not_Sounding : 1;
  unsigned : 1;
  unsigned Aggregation : 1;
  unsigned STBC : 2;
  unsigned FEC_CODING : 1; // if is 11n packet, shows if is LDPC packet or not.
  unsigned SGI : 1;
  unsigned rxend_state : 8;
  unsigned ampdu_cnt : 8;
  unsigned channel : 4; //which channel this packet in.
  unsigned : 12;
};

struct SnifferPacket {
  struct RxControl rx_ctrl;
  uint8_t data[DATA_LENGTH];
  uint16_t cnt;
  uint16_t len;
};


static void printDataSpan(uint16_t start, uint16_t size, uint8_t* data) {
  for (uint16_t i = start; i < DATA_LENGTH && i < start + size; i++) {
    Serial.write(data[i]);
  }
}

static void getMAC(char *addr, uint8_t* data, uint16_t offset) {
  sprintf(addr, "%02x:%02x:%02x:%02x:%02x:%02x", data[offset + 0], data[offset + 1], data[offset + 2], data[offset + 3], data[offset + 4], data[offset + 5]);
}


static void showMetadata(struct SnifferPacket *snifferPacket) {

  unsigned int frameControl = ((unsigned int)snifferPacket->data[1] << 8) + snifferPacket->data[0];

  uint8_t version = (frameControl & 0b0000000000000011) >> 0;
  uint8_t frameType = (frameControl & 0b0000000000001100) >> 2;
  uint8_t frameSubType = (frameControl & 0b0000000011110000) >> 4;
  uint8_t toDS = (frameControl & 0b0000000100000000) >> 8;
  uint8_t fromDS = (frameControl & 0b0000001000000000) >> 9;

    char addr[] = "00:00:00:00:00:00";


  // Only look for probe request packets
  if (frameType != TYPE_MANAGEMENT ||
    frameSubType != SUBTYPE_PROBE_REQUEST)
    return;
  getMAC(addr, snifferPacket->data, 10);

  oled.print("RSSI: ");  oled.print(snifferPacket->rx_ctrl.rssi, DEC); oled.print(" CH ");  oled.println(wifi_get_channel()); oled.print("MAC "); oled.println(addr); 


}




void setupPJON() {
  bus.set_packet_id(true);
  bus.strategy.set_pin(12);
  bus.set_error(error_handler);
  bus.begin();

  packet = bus.send(44, content, 20);
  Serial.begin(115200);
}

void error_handler(uint8_t code, uint16_t data, void *custom_pointer) {
  if(code == PJON_CONNECTION_LOST) {
    Serial.print("Connection lost with device id ");
    Serial.println(bus.packets[data].content[0], DEC);
  }
};


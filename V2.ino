/*
 * Pico LoRa APRS Tracker
 * 
 * Code written by Ludwik SP6MPL with the assistance of Artificial Intelligence (AI).
 * All configuration settings are located at the top of this file.
 */

#include <SPI.h>
#include <LoRa.h>
#include <TinyGPSPlus.h>

// ==========================================
// CONFIGURATION AND SETTINGS (TRACKER SETUP)
// ==========================================

// LoRa SPI Pin Definitions for Raspberry Pi Pico
#define LORA_SCK   10
#define LORA_MOSI  11
#define LORA_MISO  12
#define LORA_SS    13
#define LORA_RST   14
#define LORA_DIO0  15

// Hardware Pin Configuration
#define GPS_UART_TX_PIN    0   // Raspberry Pi Pico UART TX pin connected to GPS RX
#define GPS_UART_RX_PIN    1   // Raspberry Pi Pico UART RX pin connected to GPS TX

// LoRa Transmit Power & Radio Parameters
#define LORA_TX_POWER     20   // Maximum transmit power in dBm (20 dBm = 100mW)
#define LORA_CODING_RATE   5   // Coding rate parameter for LoRa (4/5)

// Frequency 1 Settings (Primary Frequency)
const long FREQ_1_HZ      = 434855000; // Primary frequency in Hz (434.855 MHz)
const int FREQ_1_SF       = 9;         // Spreading Factor for primary frequency
const long FREQ_1_BW      = 125000;    // Bandwidth for primary frequency in Hz (125 kHz)

// Frequency 2 Settings (Alternate Frequency)
const long FREQ_2_HZ      = 433775000; // Alternate frequency in Hz (433.775 MHz)
const int FREQ_2_SF       = 12;        // Spreading Factor for alternate frequency
const long FREQ_2_BW      = 125000;    // Bandwidth for alternate frequency in Hz (125 kHz)

// APRS Packet & Timing Parameters
const unsigned long txInterval = 40000;     // Time interval between transmissions in milliseconds (40 seconds)
const String aprsHeader        = "SP6MPL-9>APZ52M:"; // APRS Source Callsign, SSID and Destination path
const int gpsTimeoutMs         = 5000;      // Maximum age of GPS data to be considered valid (5 seconds)

// ==========================================
// END OF CONFIGURATION
// ==========================================


TinyGPSPlus gps;
unsigned long lastTxTime = 0;
bool useAlternateFreq = false;
unsigned long packetCounter = 0;

void setup() {
  Serial.begin(115200);


  Serial1.setTX(GPS_UART_TX_PIN);
  Serial1.setRX(GPS_UART_RX_PIN);
  Serial1.begin(9600);

  Serial.println(F("Pico LoRa APRS Tracker"));


  SPI1.setSCK(LORA_SCK);
  SPI1.setTX(LORA_MOSI);
  SPI1.setRX(LORA_MISO);
  SPI1.begin();

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  LoRa.setSPI(SPI1);


  setupLoRa(FREQ_1_HZ, FREQ_1_SF, FREQ_1_BW); 
}

void loop() {

  while (Serial1.available()) {
    gps.encode(Serial1.read());
  }


  if (millis() - lastTxTime >= txInterval) {
    sendAprsPacket();
    lastTxTime = millis();
  }
}

void setupLoRa(long frequency, int sf, long bw) {
  LoRa.end(); 
  
  if (!LoRa.begin(frequency)) {
    Serial.println(F("LoRa initialization failed! Check SPI connections."));
    while (1);
  }
  
  LoRa.setSpreadingFactor(sf);
  LoRa.setSignalBandwidth(bw);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.enableCrc();
  LoRa.setTxPower(LORA_TX_POWER);
}

void sendAprsPacket() {
  char latStr[] = "0000.00N";
  char lonStr[] = "00000.00E";
  char altStr[] = "000000";
  
  int satCount = 0;

 
  if (gps.location.isValid() && gps.location.age() < gpsTimeoutMs) {
    satCount = gps.satellites.value();


    double lat = gps.location.lat();
    char latDir = (lat >= 0) ? 'N' : 'S';
    lat = abs(lat);
    int latDeg = (int)lat;
    double latMin = (lat - latDeg) * 60.0;
    sprintf(latStr, "%02d%05.2f%c", latDeg, latMin, latDir);

   
    double lon = gps.location.lng();
    char lonDir = (lon >= 0) ? 'E' : 'W';
    lon = abs(lon);
    int lonDeg = (int)lon;
    double lonMin = (lon - lonDeg) * 60.0;
    sprintf(lonStr, "%03d%05.2f%c", lonDeg, lonMin, lonDir);

  
    if (gps.altitude.isValid()) {
      long altFeet = (long)(gps.altitude.meters() * 3.28084);
      if (altFeet < 0) altFeet = 0;
      if (altFeet > 999999) altFeet = 999999;
      sprintf(altStr, "%06ld", altFeet);
    }
  }

  int freqId = 1;


  if (useAlternateFreq) {
    Serial.println(F("TX on: 433.775MHz, SF12, BW125k"));
    setupLoRa(FREQ_2_HZ, FREQ_2_SF, FREQ_2_BW);
    freqId = 2;
  } else {
    Serial.println(F("TX on: 434.855MHz, SF9, BW125k"));
    setupLoRa(FREQ_1_HZ, FREQ_1_SF, FREQ_1_BW);
    freqId = 1;
  }
  useAlternateFreq = !useAlternateFreq; 

  int cpuTemp = (int)analogReadTemp();


  String aprsComment = "P" + String(packetCounter) + 
                       "S" + String(satCount) + 
                       "F" + String(freqId) + 
                       "T" + String(cpuTemp);


  String aprsPayload = "!" + String(latStr) + "/" + String(lonStr) + "O/A=" + String(altStr) + " " + aprsComment;
  String fullPacket = aprsHeader + aprsPayload;


  LoRa.beginPacket();
  LoRa.write('<');    
  LoRa.write(0xFF);   
  LoRa.write(0x01);  
  LoRa.print(fullPacket);
  LoRa.endPacket();

  Serial.print(F("Sent over RF: "));
  Serial.println(fullPacket);

  packetCounter++;
}

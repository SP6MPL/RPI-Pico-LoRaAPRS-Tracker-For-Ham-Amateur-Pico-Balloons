#include <SPI.h>
#include <LoRa.h>
#include <TinyGPSPlus.h>


#define LORA_SCK   10
#define LORA_MOSI  11
#define LORA_MISO  12
#define LORA_SS    13
#define LORA_RST   14
#define LORA_DIO0  15


#define GPS_RESET_JUMPER 5


TinyGPSPlus gps;


unsigned long lastTxTime = 0;
const unsigned long txInterval = 40000; // 40 sekund
bool useAlternateFreq = false;


unsigned long packetCounter = 0;

void setup() {

  Serial.begin(115200);
  

  pinMode(GPS_RESET_JUMPER, INPUT_PULLUP);

  
  Serial1.setTX(0);
  Serial1.setRX(1);
  Serial1.begin(9600);

  Serial.println(F("Pico LoRa APRS Tracker"));


  SPI1.setSCK(LORA_SCK);
  SPI1.setTX(LORA_MOSI);
  SPI1.setRX(LORA_MISO);
  SPI1.begin();


  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  LoRa.setSPI(SPI1);


  setupLoRa(434855000, 9, 125000); 
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
    Serial.println(F("Błąd inicjalizacji LoRa! Sprawdź połączenia SPI."));
    while (1);
  }
  
  LoRa.setSpreadingFactor(sf);
  LoRa.setSignalBandwidth(bw);
  LoRa.setCodingRate4(5);
  LoRa.enableCrc();
  LoRa.setTxPower(20); // Maksymalna moc nadawania
}


void sendAprsPacket() {
  char latStr[] = "0000.00N";
  char lonStr[] = "00000.00E";
  char altStr[] = "000000";
  
  int satCount = 0;

 
  if (gps.location.isValid() && gps.location.age() < 5000) {
   
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
    Serial.println(F("TX na: 433.775MHz, SF12, BW125k"));
    setupLoRa(433775000, 12, 125000);
    freqId = 2;
  } else {
    Serial.println(F("TX na: 434.855MHz, SF9, BW125k"));
    setupLoRa(434855000, 9, 125000);
    freqId = 1;
  }
  useAlternateFreq = !useAlternateFreq; 


  int cpuTemp = (int)analogReadTemp();

  
  String aprsHeader = "SP6MPL-9>APZ52M:";
  

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

  Serial.print(F("Wysłano na RF: "));
  Serial.println(fullPacket);


  packetCounter++;
}

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <DHT.h>
#include <SensirionI2CSen5x.h>

static const uint32_t GPSBaud = 9600;

TinyGPSPlus gps;
DHT dht(4, DHT11); // pin 4, sensor DHT11
SensirionI2CSen5x sen55; // instancja czujnika SEN5X

// definiujemy strukturę na dane odczytane z gps
struct GPSData {
  bool connected = false;
  bool valid = false;
  double lat = 0.0;
  double lng = 0.0;
  double altitude = 0.0;
  double speedKmph = 0.0;
  uint32_t satellites = 0;
  uint32_t lastUpdateMs = 0;

  bool timeValid = false;
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
};

struct DHTData {
  float temperature = 0.0;
  float humidity = 0.0;
};

struct SEN55Data {
  bool connected = false;
  bool valid = false;
  float pm1p0 = 0.0;
  float pm2p5 = 0.0;
  float pm4p0 = 0.0;
  float pm10p0 = 0.0;
  float humidity = 0.0;
  float temperature = 0.0;
  float vocIndex = 0.0;
  float noxIndex = 0.0;
};
GPSData gpsData; // globalna zmienna do przechowywania danych GPS

DHTData dhtData; // globalna zmienna do przechowywania danych DHT

SEN55Data sen55Data; // globalna zmienna do przechowywania danych SEN55

// put function declarations here:
// Definiujemy funkcję do odczytu danych z GPS, logiga na dole pliku
bool readGPS(GPSData &data);

// Definiujemy funkcję do odczytu danych z DHT, logiga na dole pliku
bool readDHT(DHTData &data); 


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); // Inicjalizacja portu szeregowego dla komunikacji z komputerem
  Serial1.begin(GPSBaud); // Inicjalizacja portu szeregowego dla komunikacji z modułem GPS

  dht.begin(); // Inicjalizacja czujnika DHT
}

void loop() {
  static uint32_t lastPrintMs = 0;
  if (millis() - lastPrintMs >= 3000) {
    lastPrintMs = millis();
    if (readGPS(gpsData)) {
      Serial.print("Connected: ");
      Serial.print(gpsData.connected);
      Serial.print(", Valid: ");
      Serial.print(gpsData.valid);
      Serial.print(", Lat: ");
      Serial.print(gpsData.lat, 6);
      Serial.print(", Lng: ");
      Serial.print(gpsData.lng, 6);
      Serial.print(", Altitude: ");
      Serial.print(gpsData.altitude);
      Serial.print(", Speed (km/h): ");
      Serial.print(gpsData.speedKmph);
      Serial.print(", Satellites: ");
      Serial.print(gpsData.satellites);
      char buf[32];
      snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u UTC",
         gpsData.year, gpsData.month, gpsData.day,
         gpsData.hour, gpsData.minute, gpsData.second);
      Serial.print(", DateTime: ");
      Serial.println(gpsData.timeValid ? buf : "n/a");
    }
    if (readDHT(dhtData)) {
      Serial.print("Temperature: ");
      Serial.print(dhtData.temperature);
      Serial.print(" °C, Humidity: ");
      Serial.print(dhtData.humidity);
      Serial.println(" %");
    }
    else {
      Serial.println("Failed to read from DHT sensor!");
    }
  }
}

// put function definitions here:


bool readGPS(GPSData &data) {
  bool newSentence = false;

  while (Serial1.available() > 0) {
    char c = Serial1.read();
    data.lastUpdateMs = millis();
    data.connected = true;

    if (gps.encode(c)) {
      newSentence = true;
    }
  }

  // Mark disconnected if no bytes arrive for more than 2.5 seconds
  if (millis() - data.lastUpdateMs > 2500) {
    data.connected = false;
    data.valid = false;
  }

  if (newSentence) {
    data.valid = gps.location.isValid();

    if (gps.location.isValid()) {
      data.lat = gps.location.lat();
      data.lng = gps.location.lng();
    }
    if (gps.altitude.isValid()) data.altitude = gps.altitude.meters();
    if (gps.speed.isValid())    data.speedKmph = gps.speed.kmph();
    data.satellites = gps.satellites.value();

    data.timeValid = gps.date.isValid() && gps.time.isValid();
    if (data.timeValid) {
      data.year   = gps.date.year();
      data.month  = gps.date.month();
      data.day    = gps.date.day();
      data.hour   = gps.time.hour();
      data.minute = gps.time.minute();
      data.second = gps.time.second();
    }
  }
  return newSentence;
}

bool readDHT(DHTData &data) {
  data.temperature = dht.readTemperature();
  data.humidity = dht.readHumidity();

  // Check if any reads failed and exit early (to try again).
  if (isnan(data.temperature) || isnan(data.humidity)) {
    return false;
  }
  return true;
}

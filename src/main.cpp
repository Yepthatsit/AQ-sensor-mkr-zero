#include <Arduino.h>
#include <TinyGPSPlus.h>

static const uint32_t GPSBaud = 9600;

TinyGPSPlus gps;

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
};

// put function declarations here:
// Dane z GPS
GPSData getGPSData();


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial1.begin(GPSBaud);
}

void loop() {
  // put your main code here, to run repeatedly:
}

// put function definitions here:

GPSData getGPSData() {
  GPSData data;
  // Implementation for getting GPS data
  return data;
}
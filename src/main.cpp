#include <Arduino.h>
#include <TinyGPSPlus.h>

static const uint32_t GPSBaud = 9600;

TinyGPSPlus gps;

GPSData gpsData;
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
bool readGPS(GPSData &data);


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial1.begin(GPSBaud);
}

void loop() {
  bool newData = readGPS(gpsData);
  Serial.print("Connected: ");
  Serial.print(gpsData.connected ? "Yes" : "No");
  Serial.print(", Valid: ");
  Serial.print(gpsData.valid ? "Yes" : "No");
  Serial.print(", Lat: ");
  Serial.print(gpsData.lat, 6);
  Serial.print(", Lng: ");
  Serial.print(gpsData.lng, 6);
  Serial.print(", Altitude: ");
  Serial.print(gpsData.altitude, 2);
  Serial.print(", Speed: ");
  Serial.print(gpsData.speedKmph, 2);
  Serial.print(", Satellites: ");
  Serial.print(gpsData.satellites);
  Serial.print(", Last Update: ");
  Serial.print(gpsData.lastUpdateMs);
  Serial.print(" ms ago");
  Serial.println();
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
    data.lat = gps.location.lat();
    data.lng = gps.location.lng();
    data.altitude = gps.altitude.meters();
    data.speedKmph = gps.speed.kmph();
    data.satellites = gps.satellites.value();
  }

  return newSentence;
}

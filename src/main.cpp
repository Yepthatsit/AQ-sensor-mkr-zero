#include <Arduino.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include <SensirionI2CSen5x.h>

static const uint32_t GPSBaud = 9600;

TinyGPSPlus gps;
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


SEN55Data sen55Data; // globalna zmienna do przechowywania danych SEN55

// put function declarations here:
// Definiujemy funkcję do odczytu danych z GPS, logiga na dole pliku
bool readGPS(GPSData &data);

// Definiujemy funkcję do odczytu danych z SEN55, logiga na dole pliku
bool readSEN55(SEN55Data &data);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); // Inicjalizacja portu szeregowego dla komunikacji z komputerem
  Serial1.begin(GPSBaud); // Inicjalizacja portu szeregowego dla komunikacji z modułem GPS
  Wire.begin(); // Inicjalizacja magistrali I2C
  sen55.begin(Wire); // Inicjalizacja czujnika SEN55
  char errorMessage[256];
  uint16_t error = sen55.deviceReset();
  if (error) {
    errorToString(error, errorMessage, sizeof(errorMessage));
    Serial.print("SEN55 deviceReset failed: ");
    Serial.println(errorMessage);
    return;
  }

  error = sen55.startMeasurement();
  if (error) {
    errorToString(error, errorMessage, sizeof(errorMessage));
    Serial.print("SEN55 startMeasurement failed: ");
    Serial.println(errorMessage);
    return;
  }

  sen55Data.connected = true;
  Serial.println("SEN55 initialized and fan started.");
}

void loop() {
  static uint32_t lastPrintMs = 0;
  if (millis() - lastPrintMs >= 3000) {
    lastPrintMs = millis();
    readSEN55(sen55Data);
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
    if (sen55Data.valid) {
      Serial.print("SEN55 -> PM1.0: "); Serial.print(sen55Data.pm1p0, 1);
      Serial.print(" | PM2.5: "); Serial.print(sen55Data.pm2p5, 1);
      Serial.print(" | PM4.0: "); Serial.print(sen55Data.pm4p0, 1);
      Serial.print(" | PM10: "); Serial.print(sen55Data.pm10p0, 1);
      Serial.print(" ug/m3 | Temp: "); Serial.print(sen55Data.temperature, 2);
      Serial.print(" C | RH: "); Serial.print(sen55Data.humidity, 1);
      Serial.print(" % | VOC: "); Serial.print(sen55Data.vocIndex, 0);
      Serial.print(" | NOx: "); Serial.println(sen55Data.noxIndex, 0);
    } else {
      Serial.println("SEN55 -> Oczekiwanie na dane lub blad odczytu...");
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


bool readSEN55(SEN55Data &data) {
  if (!data.connected) {
    data.valid = false;
    return false;
  }

  uint16_t error = sen55.readMeasuredValues(
      data.pm1p0, data.pm2p5, data.pm4p0, data.pm10p0,
      data.humidity, data.temperature, data.vocIndex, data.noxIndex);

  if (error) {
    data.valid = false;
    return false;
  }

  // Uwaga: Na starcie (pierwsze ~1-10 s) VOC i NOx są niewiarygodne zanim algorytm się ustabilizuje
  data.valid = !isnan(data.temperature) && !isnan(data.pm2p5);
  return data.valid;
}

#include <Arduino.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include <SensirionI2CSen5x.h>
#include <LiquidCrystal_I2C.h>
#include <avr/dtostrf.h>
#include <SPI.h>
#include <SD.h>

bool sdReady = false; // zmienna do przechowywania stanu karty SD
const char* INDEX_FILENAME = "index.dat"; // plik do przechowywania numeru ostatniego pliku logu
char logFilename[16]; // Przechowa wygenerowaną nazwę np. "log_0.csv", "log_1.csv"

static const uint32_t GPSBaud = 9600;

TinyGPSPlus gps; // instancja obiektu TinyGPSPlus do obsługi danych GPS
SensirionI2CSen5x sen55; // instancja czujnika SEN5X
LiquidCrystal_I2C lcd(0x27, 16, 2); // Adres I2C i rozmiar wyświetlacza LCD

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
  
  float* values[8] = {&pm1p0, &pm2p5, &pm4p0, &pm10p0, &temperature, &humidity, &vocIndex, &noxIndex}; //wskaźniki do wartości danych SEN55

  const char* labels[8] = { // etykiety do wyświetlania danych
    "PM1.0: %s ug    ",
    "PM2.5: %s ug    ",
    "PM4.0: %s ug    ",
    "PM10 : %s ug    ",
    "Temp : %s C     ",
    "Hum  : %s %%     ",
    "VOC  : %s       ",
    "NOx  : %s       "
  };
};
GPSData gpsData; // globalna zmienna do przechowywania danych GPS

SEN55Data sen55Data; // globalna zmienna do przechowywania danych SEN55

int page = 1; // zmienna do przechowywania aktualnej strony wyświetlacza LCD
// put function declarations here:
// Definiujemy funkcję do odczytu danych z GPS, logiga na dole pliku
bool readGPS(GPSData &data);

// Definiujemy funkcję do odczytu danych z SEN55, logiga na dole pliku
bool readSEN55(SEN55Data &data);

// Definiujemy funkcję do inicjalizacji karty SD
bool initSD();

// Definiujemy funkcję do logowania danych do pliku CSV na karcie SD
bool logMeasurementToSD(const GPSData &gps, const SEN55Data &sen);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); // Inicjalizacja portu szeregowego dla komunikacji z komputerem
  Serial1.begin(GPSBaud); // Inicjalizacja portu szeregowego dla komunikacji z modułem GPS
  Wire.begin(); // Inicjalizacja magistrali I2C
  sen55.begin(Wire); // Inicjalizacja czujnika SEN55
  lcd.init(); // Inicjalizacja wyświetlacza LCD
  lcd.backlight(); // Włączenie podświetlenia wyświetlacza LCD

  char errorMessage[256];
  uint16_t error = sen55.deviceReset();
  if (error) {
    errorToString(error, errorMessage, sizeof(errorMessage));
    Serial.print("SEN55 reset nie powiódł się: ");
    Serial.println(errorMessage);
    return;
  }

  error = sen55.startMeasurement();
  if (error) {
    errorToString(error, errorMessage, sizeof(errorMessage));
    Serial.print("SEN55 startMeasurement nie powiódł się: ");
    Serial.println(errorMessage);
    return;
  }

  sen55Data.connected = true;
  Serial.println("SEN55 gotowy.");

  // Inicjalizacja karty SD
  sdReady = initSD();
}

void loop() {
  static uint32_t lastPrintMs = 0;
  if (millis() - lastPrintMs >= 3000) {
    lastPrintMs = millis();
    
    if (readGPS(gpsData)) { // odczyt danych z GPS i wyświetlenie ich na Serial Monitor jeżeli odczyt się powiódł
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

    if (readSEN55(sen55Data)) { // odczyt danych z SEN55 i wyświetlenie ich na Serial Monitor jeżeli odczyt się powiódł
      Serial.print("SEN55 -> PM1.0: "); Serial.print(sen55Data.pm1p0, 1);
      Serial.print(" | PM2.5: "); Serial.print(sen55Data.pm2p5, 1);
      Serial.print(" | PM4.0: "); Serial.print(sen55Data.pm4p0, 1);
      Serial.print(" | PM10: "); Serial.print(sen55Data.pm10p0, 1);
      Serial.print(" ug/m3 | Temp: "); Serial.print(sen55Data.temperature, 2);
      Serial.print(" C | RH: "); Serial.print(sen55Data.humidity, 1);
      Serial.print(" % | VOC: "); Serial.print(sen55Data.vocIndex, 0);
      Serial.print(" | NOx: "); Serial.println(sen55Data.noxIndex, 0);

      // Wyświetlanie danych z SEN55 w formacie etykieta: wartość na LCD
      char line1[17]; // 16 znaków + 1 na znak null. Zmienne do przechowywania linii wyświetlacza LCD
      char line2[17];
      char val1[8];
      char val2[8];

      // 0 miejsc po przecinku dla VOC (index 6) i NOx (index 7), 1 miejsc po przecinku dla pozostałych wartości
      dtostrf(*sen55Data.values[page - 1], 1, (page - 1 >= 6) ? 0 : 1, val1);
      dtostrf(*sen55Data.values[page],     1, (page >= 6)     ? 0 : 1, val2);

      snprintf(line1, sizeof(line1), sen55Data.labels[page - 1], val1); // Formatowanie linii wyświetlacza LCD z etykietą i wartością
      snprintf(line2, sizeof(line2), sen55Data.labels[page],     val2);

      lcd.setCursor(0, 0); // Ustawienie kursora na początek pierwszej linii wyświetlacza LCD
      lcd.print(line1); // Wyświetlenie pierwszej linii na wyświetlaczu LCD
      lcd.setCursor(0, 1);
      lcd.print(line2);
      page+= 2; // przejście do następnej strony danych (2 wartości na stronę)
      if (page > 7) page = 1; // reset strony po wyświetleniu wszystkich danych
      logMeasurementToSD(gpsData, sen55Data); // logowanie danych do pliku CSV na karcie SD
    } else { // jeżeli odczyt danych z SEN55 się nie powiódł, wyświetlenie komunikatu o błędzie na Serial Monitor i LCD
      Serial.println("SEN55 -> Oczekiwanie na dane lub blad odczytu...");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("SEN55 error");
    }
  }
}

// put function definitions here:


bool readGPS(GPSData &data) { // odczyt danych z GPS, zwraca true jeżeli odczyt się powiódł, false jeżeli nie
  bool newSentence = false;

  while (Serial1.available() > 0) { // Jeżeli są dostępne dane z GPS, odczytujemy je i przekazujemy do obiektu TinyGPSPlus
    char c = Serial1.read();
    data.lastUpdateMs = millis();
    data.connected = true;

    if (gps.encode(c)) {
      newSentence = true;
    }
  }

  // Jeżeli żadna informacja nie została odebrana z GPS przez 2,5 sekundy, oznaczamy GPS jako niepodłączony i dane jako nieprawidłowe
  if (millis() - data.lastUpdateMs > 2500) {
    data.connected = false;
    data.valid = false;
  }

  if (newSentence) { // Jeżeli odebrano nową informację z GPS, aktualizujemy dane w strukturze GPSData
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


bool readSEN55(SEN55Data &data) { // odczyt danych z SEN55, zwraca true jeżeli odczyt się powiódł, false jeżeli nie
  if (!data.connected) { // jeżeli SEN55 nie jest podłączony, zwracamy false
    data.valid = false;
    return false;
  }

  uint16_t error = sen55.readMeasuredValues( // odczyt danych z SEN55, zapisanie ich do struktury SEN55Data
      data.pm1p0, data.pm2p5, data.pm4p0, data.pm10p0,
      data.humidity, data.temperature, data.vocIndex, data.noxIndex);

  if (error) { // jeżeli odczyt danych z SEN55 się nie powiódł, wyświetlamy komunikat o błędzie na Serial Monitor i zwracamy false
    data.valid = false;
    return false;
  }

  // Uwaga: Na starcie (pierwsze pare minut Max 2) VOC i NOx są niewiarygodne zanim algorytm się ustabilizuje
  data.valid = !isnan(data.temperature) && !isnan(data.pm2p5);
  return data.valid;
}


bool initSD() {
  if (!SD.begin(SDCARD_SS_PIN)) {
    Serial.println("Blad karty SD lub brak karty!");
    return false;
  }

  Serial.println("Karta SD zainicjalizowana.");
  uint32_t fileIndex = 0;

  if (SD.exists(INDEX_FILENAME)) {
    File idxFile = SD.open(INDEX_FILENAME, FILE_READ);
    if (idxFile) {
      fileIndex = idxFile.parseInt() + 1;
      idxFile.close();
    }
    SD.remove(INDEX_FILENAME); // Usunięcie starego pliku przed zapisem nowego indeksu
  }

  File idxFile = SD.open(INDEX_FILENAME, FILE_WRITE);
  if (!idxFile) {
    Serial.println("Blad zapisu pliku index.dat!");
    return false;
  }
  idxFile.println(fileIndex);
  idxFile.close();

  // Utworzenie nazwy pliku CSV w formacie ( log_0.csv)
  snprintf(logFilename, sizeof(logFilename), "log_%lu.csv", fileIndex);
  Serial.print("Plik pomiarowy: ");
  Serial.println(logFilename);

  File logFile = SD.open(logFilename, FILE_WRITE);
  if (!logFile) {
    Serial.println("Blad utworzenia pliku pomiarowego!");
    return false;
  }

  logFile.println("uptime_ms,gps_valid,gps_datetime_utc,lat,lng,alt_m,speed_kmph,satellites,pm1_0,pm2_5,pm4_0,pm10_0,temp_C,hum_pct,voc,nox");
  logFile.close();

  return true;
}

bool logMeasurementToSD(const GPSData &gps, const SEN55Data &sen) {
  if (!sdReady) {
    return false;
  }

  File logFile = SD.open(logFilename, FILE_WRITE);
  if (!logFile) {
    Serial.println("Blad otwarcia pliku pomiarowego do zapisu!");
    return false;
  }

  // 1. Czas systemowy (ms od uruchomienia)
  logFile.print(millis());
  logFile.print(',');

  // 2. Dane z GPS
  logFile.print(gps.valid ? 1 : 0);
  logFile.print(',');

  if (gps.timeValid) {
    char dtBuf[24];
    snprintf(dtBuf, sizeof(dtBuf), "%04u-%02u-%02u %02u:%02u:%02u",
             gps.year, gps.month, gps.day,
             gps.hour, gps.minute, gps.second);
    logFile.print(dtBuf);
  } else {
    logFile.print("n/a");
  }
  logFile.print(',');

  logFile.print(gps.lat, 6);        logFile.print(',');
  logFile.print(gps.lng, 6);        logFile.print(',');
  logFile.print(gps.altitude, 1);   logFile.print(',');
  logFile.print(gps.speedKmph, 1);  logFile.print(',');
  logFile.print(gps.satellites);    logFile.print(',');

  // 3. Dane z Sensirion SEN55
  logFile.print(sen.pm1p0, 1);       logFile.print(',');
  logFile.print(sen.pm2p5, 1);       logFile.print(',');
  logFile.print(sen.pm4p0, 1);       logFile.print(',');
  logFile.print(sen.pm10p0, 1);      logFile.print(',');
  logFile.print(sen.temperature, 2); logFile.print(',');
  logFile.print(sen.humidity, 1);    logFile.print(',');
  logFile.print(sen.vocIndex, 0);    logFile.print(',');
  logFile.println(sen.noxIndex, 0);

  logFile.close();
  return true;
}
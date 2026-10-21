
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_sleep.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>

// =====================================================
// WOW-BE INSTELLINGEN
// =====================================================

const char* authKey = "yourKey";

// Erasmus_Creascola_HeiligGraf_0
const char* name = "yourname";
const char* siteID =  "yoursiteID";

const char* wowURL =
  "http://wow.meteo.be/api/v2/send";

// =====================================================
// ALGEMENE INSTELLINGEN
// =====================================================

// true = opgeslagen WiFi wissen bij elke start
const bool TEST_MODE = false;

// Elke 5 minuten een meting
const uint64_t SLEEP_TIME_SECONDS = 300;

// OLED 5 seconden tonen voor deep sleep
const unsigned long RESULT_DISPLAY_TIME = 5000;

// WiFi en HTTP timeouts
const unsigned long WIFI_CONNECT_TIMEOUT = 15000;
const int HTTP_TIMEOUT = 8000;

// =====================================================
// SENSORCONFIGURATIE
// =====================================================

// Beide sensortypes gebruiken dezelfde pin.
// Sluit slechts EEN sensor tegelijk aan.

#define SENSOR_PIN 23

#define DHTTYPE DHT22

DHT dht(SENSOR_PIN, DHTTYPE);

OneWire oneWire(SENSOR_PIN);
DallasTemperature ds18b20(&oneWire);

enum SensorType {
  SENSOR_NONE,
  SENSOR_DHT22,
  SENSOR_DS18B20
};

SensorType activeSensor = SENSOR_NONE;

float temperatureC = NAN;
float humidity = NAN;

bool sensorOK = false;

// =====================================================
// OLED
// =====================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_RESET -1

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

bool oledOK = false;

// =====================================================
// WIFI RESETKNOP
// =====================================================

// Knop tussen GPIO13 en GND
#define WIFI_RESET_PIN 13

const unsigned long RESET_HOLD_TIME = 5000;

// =====================================================
// STATUS
// =====================================================

String wowStatus = "WAIT";

// =====================================================
// SENSORNAAM
// =====================================================

const char* sensorName() {
  switch (activeSensor) {
    case SENSOR_DHT22:
      return "DHT22";

    case SENSOR_DS18B20:
      return "DS18B20";

    default:
      return "NONE";
  }
}

// =====================================================
// ALGEMEEN OLED-SCHERM
// =====================================================

void showMessage(
  String line1,
  String line2 = "",
  String line3 = "",
  String line4 = ""
) {
  if (!oledOK) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println(line1);

  display.setCursor(0, 16);
  display.println(line2);

  display.setCursor(0, 32);
  display.println(line3);

  display.setCursor(0, 48);
  display.println(line4);

  display.display();
}

// =====================================================
// WIFI CONFIGURATIESCHERM
// =====================================================

void showWiFiSetup() {
  if (!oledOK) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("WiFi SETUP - ");
  display.println(name);

  display.drawLine(
    0, 10, 127, 10, SSD1306_WHITE
  );

  display.setCursor(0, 16);
  display.println("Connect to:");

  display.setCursor(0, 28);
  display.println("WOW-WeatherStation");

  display.setCursor(0, 42);
  display.println("Then open:");

  display.setCursor(0, 54);
  display.println("192.168.4.1");

  display.display();
}

// =====================================================
// RESULTAAT OP OLED
// =====================================================

void showResult() {
  if (!oledOK) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Titel
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("WOW Weather Station");

  display.drawLine(
    0, 10, 127, 10, SSD1306_WHITE
  );

  // Temperatuur
  display.setCursor(0, 16);

  if (sensorOK) {
    display.setTextSize(2);
    display.print(temperatureC, 1);
    display.print(" C");
  } else {
    display.setTextSize(1);
    display.println("Sensor error");
  }

  // Sensor en vochtigheid
  display.setTextSize(1);
  display.setCursor(0, 38);

  if (activeSensor == SENSOR_DHT22) {
    display.print("DHT22 RH: ");
    display.print(humidity, 1);
    display.print("%");
  }
  else if (activeSensor == SENSOR_DS18B20) {
    display.print("Sensor: DS18B20");
  }
  else {
    display.print("No sensor found");
  }

  // WiFi status
  display.setCursor(0, 53);
  display.print("WiFi:");

  if (WiFi.status() == WL_CONNECTED) {
    display.print("OK");
  } else {
    display.print("OFF");
  }

  // WOW status
  display.setCursor(65, 53);
  display.print("WOW:");
  display.print(wowStatus);

  display.display();
}

// =====================================================
// OLED UITSCHAKELEN
// =====================================================

void turnOffOLED() {
  if (!oledOK) return;

  display.clearDisplay();
  display.display();

  display.ssd1306_command(
    SSD1306_DISPLAYOFF
  );
}

// =====================================================
// DHT22 PROBEREN
// =====================================================

bool tryDHT22() {
  Serial.println();
  Serial.println("Checking DHT22 on GPIO23...");

  // De DHT22 moet na opstarten stabiliseren.
  delay(2200);

  for (int attempt = 1; attempt <= 2; attempt++) {

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    Serial.print("DHT22 attempt ");
    Serial.println(attempt);

    // Communicatie en plausibele waarden
    if (
      isfinite(t) &&
      isfinite(h) &&
      t >= -40.0 &&
      t <= 80.0 &&
      h >= 0.0 &&
      h <= 100.0
    ) {
      temperatureC = t;
      humidity = h;

      activeSensor = SENSOR_DHT22;
      sensorOK = true;

      Serial.println("DHT22 FOUND!");

      Serial.print("Temperature: ");
      Serial.print(temperatureC, 2);
      Serial.println(" C");

      Serial.print("Humidity: ");
      Serial.print(humidity, 1);
      Serial.println(" %");

      return true;
    }

    if (attempt < 2) {
      delay(2200);
    }
  }

  Serial.println("DHT22 not detected.");
  return false;
}

// =====================================================
// DS18B20 PROBEREN
// =====================================================

bool tryDS18B20() {
  Serial.println();
  Serial.println("Checking DS18B20 on GPIO23...");

  // OneWire opnieuw initialiseren omdat de DHT22
  // dezelfde GPIO eerder gebruikt heeft.
  oneWire.begin(SENSOR_PIN);

  ds18b20.begin();

  int count = ds18b20.getDeviceCount();

  Serial.print("OneWire devices found: ");
  Serial.println(count);

  if (count == 0) {
    Serial.println("DS18B20 not detected.");
    return false;
  }

  ds18b20.requestTemperatures();

  float t = ds18b20.getTempCByIndex(0);

  if (
    !isfinite(t) ||
    t == DEVICE_DISCONNECTED_C ||
    t < -55.0 ||
    t > 125.0
  ) {
    Serial.println("DS18B20 read failed.");
    return false;
  }

  temperatureC = t;
  humidity = NAN;

  activeSensor = SENSOR_DS18B20;
  sensorOK = true;

  Serial.println("DS18B20 FOUND!");

  Serial.print("Temperature: ");
  Serial.print(temperatureC, 2);
  Serial.println(" C");

  return true;
}

// =====================================================
// AUTOMATISCHE SENSORDETECTIE
// =====================================================

void readSensor() {
  sensorOK = false;
  activeSensor = SENSOR_NONE;

  temperatureC = NAN;
  humidity = NAN;

  showMessage(
    "WOW Weather Station",
    name,
    "Detecting sensor...",
    "GPIO23"
  );

  // Stap 1: DHT22 proberen
  if (tryDHT22()) {
    return;
  }

  // Stap 2: DS18B20 proberen
  showMessage(
    "WOW Weather Station",
    name,
    "Checking DS18B20..."
  );

  if (tryDS18B20()) {
    return;
  }

  // Geen sensor gevonden
  Serial.println();
  Serial.println("ERROR: No working sensor!");

  showMessage(
    "SENSOR ERROR",
    "GPIO23",
    "No DHT22",
    "No DS18B20"
  );
}

// =====================================================
// WIFI CONFIGURATIEPORTAAL
// =====================================================

bool startConfigurationPortal() {
  WiFiManager wm;

  wm.setConfigPortalTimeout(600);

  wm.setAPCallback(
    [](WiFiManager* wm) {
      showWiFiSetup();
    }
  );

  showWiFiSetup();

  Serial.println(
    "Starting WiFi configuration portal."
  );

  bool connected = wm.startConfigPortal(
    "WOW-WeatherStation"
  );

  if (!connected) {
    showMessage(
      "WiFi SETUP",
      "",
      "Setup timed out",
      "Restart device"
    );

    delay(5000);
    return false;
  }

  showMessage(
    "WiFi connected!",
    "",
    WiFi.SSID(),
    "Setup complete"
  );

  delay(3000);
  return true;
}

// =====================================================
// VERBINDEN MET OPGESLAGEN WIFI
// =====================================================

bool connectToSavedWiFi() {
  showMessage(
    "WOW Weather Station",
    "",
    "Connecting to",
    "WiFi..."
  );

  WiFi.mode(WIFI_STA);
  WiFi.begin();

  unsigned long startTime = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - startTime < WIFI_CONNECT_TIMEOUT
  ) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected.");

    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    return true;
  }

  Serial.println("Saved WiFi unavailable.");
  return false;
}

// =====================================================
// WIFI RESETKNOP CONTROLEREN
// =====================================================

bool checkWiFiResetRequest() {
  if (digitalRead(WIFI_RESET_PIN) != LOW) {
    return false;
  }

  showMessage(
    "WiFi RESET",
    "",
    "Keep holding...",
    "5 seconds"
  );

  unsigned long startTime = millis();

  while (digitalRead(WIFI_RESET_PIN) == LOW) {

    if (
      millis() - startTime >= RESET_HOLD_TIME
    ) {
      WiFiManager wm;
      wm.resetSettings();

      Serial.println("WiFi settings erased.");

      showMessage(
        "WiFi RESET",
        "",
        "Settings erased",
        "Release button"
      );

      while (
        digitalRead(WIFI_RESET_PIN) == LOW
      ) {
        delay(50);
      }

      delay(1000);
      return true;
    }

    delay(50);
  }

  return false;
}

// =====================================================
// GEGEVENS NAAR WOW-BE STUREN
// =====================================================

void uploadToWOW() {
  if (!sensorOK) {
    wowStatus = "SENS";

    Serial.println(
      "Upload skipped: sensor error."
    );

    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    wowStatus = "OFF";

    Serial.println(
      "Upload skipped: no WiFi."
    );

    return;
  }

  showMessage(
    "WOW Weather Station",
    name,
    "Uploading data...",
    sensorName()
  );

  // Celsius omzetten naar Fahrenheit
  float temperatureF =
    temperatureC * 9.0 / 5.0 + 32.0;

  // WOW-BE URL samenstellen
  String url =
    String(wowURL) +
    "?siteid=" + String(siteID) +
    "&siteAuthenticationKey=" + String(authKey) +
    "&dateutc=now" +
    "&tempf=" + String(temperatureF, 1) +
    "&softwaretype=ESP32";

  // Luchtvochtigheid wordt momenteel NIET verstuurd.
  // Dat was ook zo in je oorspronkelijke programma.
  //
  // Indien gewenst, kun je voor de DHT22 toevoegen:
  //
  // if (activeSensor == SENSOR_DHT22) {
  //   url += "&humidity=" + String(humidity, 1);
  // }

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT);

  http.begin(url);

  int httpCode = http.GET();

  if (httpCode == 200) {
    wowStatus = "OK";
    Serial.println("WOW upload successful.");
  }
  else if (httpCode > 0) {
    wowStatus = "E" + String(httpCode);

    Serial.print("WOW HTTP error: ");
    Serial.println(httpCode);
  }
  else {
    wowStatus = "ERR";

    Serial.print("HTTP connection error: ");
    Serial.println(
      http.errorToString(httpCode)
    );
  }

  http.end();
}

// =====================================================
// DEEP SLEEP
// =====================================================

void goToSleep() {
  showResult();

  delay(RESULT_DISPLAY_TIME);

  // WiFi uitschakelen
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  delay(100);

  // OLED uitschakelen
  turnOffOLED();

  // Automatisch ontwaken na 5 minuten
  esp_sleep_enable_timer_wakeup(
    SLEEP_TIME_SECONDS * 1000000ULL
  );

  // Ook ontwaken met knop op GPIO13
  esp_sleep_enable_ext0_wakeup(
    (gpio_num_t)WIFI_RESET_PIN,
    0
  );

  Serial.println("Entering deep sleep.");
  Serial.flush();

  esp_deep_sleep_start();
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("=========================");
  Serial.println("WOW-BE WEATHER STATION");
  Serial.println("Automatic sensor detection");
  Serial.println("=========================");

  // Resetknop
  pinMode(
    WIFI_RESET_PIN,
    INPUT_PULLUP
  );

  // OLED initialiseren
  Wire.begin(OLED_SDA, OLED_SCL);

  oledOK = display.begin(
    SSD1306_SWITCHCAPVCC,
    0x3C
  );

  if (oledOK) {
    display.ssd1306_command(
      SSD1306_DISPLAYON
    );

    showMessage(
      "WOW Weather Station",
      "",
      "Starting..."
    );
  } else {
    Serial.println(
      "OLED initialization failed."
    );
  }

  // DHT22 initialiseren
  dht.begin();

  // DS18B20 wordt pas na een mislukte
  // DHT22-meting geinitialiseerd.

  // TEST MODE
  if (TEST_MODE) {
    WiFiManager wm;
    wm.resetSettings();

    Serial.println(
      "TEST MODE: WiFi erased."
    );
  }

  // Resetknop controleren
  bool resetRequested =
    checkWiFiResetRequest();

  // Reden van ontwaken bepalen
  esp_sleep_wakeup_cause_t wakeReason =
    esp_sleep_get_wakeup_cause();

  bool buttonWake =
    wakeReason == ESP_SLEEP_WAKEUP_EXT0;

  // WiFi
  bool wifiConnected = false;

  if (
    resetRequested ||
    buttonWake ||
    TEST_MODE
  ) {
    wifiConnected =
      startConfigurationPortal();
  }
  else {
    wifiConnected =
      connectToSavedWiFi();

    if (!wifiConnected) {
      wowStatus = "OFF";
    }
  }

  // Eerste installatie zonder opgeslagen WiFi
  if (
    !wifiConnected &&
    WiFi.SSID().length() == 0
  ) {
    wifiConnected =
      startConfigurationPortal();
  }

  // Automatische sensordetectie
  readSensor();

  // Uploaden naar WOW-BE
  if (wifiConnected) {
    uploadToWOW();
  } else {
    wowStatus = "OFF";
  }

  // Deep sleep
  goToSleep();
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  // Leeg: alles gebeurt in setup().
  // Na deep sleep begint setup() opnieuw.
}

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C  // Dirección I2C de la pantalla OLED

#define BUTTON_PIN 12  // Pin donde está conectado el botón

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Replace with your network credentials
const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";

// API URL for the bus stop
const char* apiUrl = "https://api.xor.cl/red/bus-stop/PC836";

int currentMenu = 0;
bool lastButtonState = HIGH;
bool currentButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
bool buttonPressed = false;

unsigned long previousMillis = 0;
const long interval = 15000;  // 15 seconds

String serviceInfoC27 = "";
String serviceInfo421 = "";
String serviceInfoC02 = "";
String serviceInfoC09 = "";

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Configura el pin del botón con una resistencia pull-up interna

  // Configura la pantalla OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.display();
  delay(2000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Connecting to WiFi...");
  display.display();
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    display.setCursor(0, 10);
    display.println("Connecting...");
    display.display();
  }
  Serial.println("Connected to WiFi");

  // Display connection success
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connected to WiFi");
  display.display();
  delay(2000); // Display "Connected" for 2 seconds

  // Configure time for Chile timezone
  configTime(-4 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // Display initial message
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Awaiting updates...");
  display.display();

  // Inicializar la información del bus en el inicio
  updateBusInfo();
  showMenu(currentMenu); // Mostrar el menú inicial
}

void loop() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != currentButtonState) {
      currentButtonState = reading;

      if (currentButtonState == LOW && !buttonPressed) { // botón presionado
        buttonPressed = true;
        currentMenu++;
        if (currentMenu > 3) { // Asumiendo que tienes 4 menús ahora
          currentMenu = 0;
        }
        showMenu(currentMenu); // Cambiar de menú sin llamar a la API
      } else if (currentButtonState == HIGH) {
        buttonPressed = false;
      }
    }
  }

  lastButtonState = reading;

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    updateBusInfo(); // Llamar a la API cada 15 segundos
  }
}

void updateBusInfo() {
  // Make the API call
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(apiUrl);

    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println("HTTP Response Code: " + String(httpResponseCode));
      Serial.println("Payload: " + payload);

      // Parse JSON
      DynamicJsonDocument doc(8192); // Increase buffer size to handle larger JSON responses
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print("JSON deserialization failed: ");
        Serial.println(error.c_str());
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("JSON Failed");
        display.display();
        return;
      }

      // Extract the services we are interested in
      JsonArray services = doc["services"];
      serviceInfoC27 = "";
      serviceInfo421 = "";
      serviceInfoC02 = "";
      serviceInfoC09 = "";

      for (JsonObject service : services) {
        const char* id = service["id"];
        String serviceInfo = "";

        JsonArray buses = service["buses"];
        if (buses.size() == 0) {
          serviceInfo += "No hay micro disponible\n";
        } else {
          for (JsonObject bus : buses) {
            int meters_distance = bus["meters_distance"];
            int min_arrival_time = bus["min_arrival_time"];
            serviceInfo += "Dist: " + String(meters_distance) + "m, ";
            serviceInfo += "Time: " + String(min_arrival_time) + "min\n";
          }
        }

        if (strcmp(id, "C27") == 0) {
          serviceInfoC27 = serviceInfo;
        } else if (strcmp(id, "421") == 0) {
          serviceInfo421 = serviceInfo;
        } else if (strcmp(id, "C02") == 0) {
          serviceInfoC02 = serviceInfo;
        } else if (strcmp(id, "C09") == 0) {
          serviceInfoC09 = serviceInfo;
        }
      }

      showMenu(currentMenu); // Actualizar el menú actual con la nueva información
    } else {
      Serial.print("Error on HTTP request: ");
      Serial.println(httpResponseCode);
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("HTTP Error");
      display.display();
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Error");
    display.display();
  }
}

void showMenu(int menu) {
  display.clearDisplay();
  switch(menu) {
    case 0:
      display.setCursor(0, 0);
      display.println(F("C27 Info:"));
      display.setCursor(0, 10);
      display.println(serviceInfoC27);
      break;
    case 1:
      display.setCursor(0, 0);
      display.println(F("421 Info:"));
      display.setCursor(0, 10);
      display.println(serviceInfo421);
      break;
    case 2:
      display.setCursor(0, 0);
      display.println(F("C02 Info:"));
      display.setCursor(0, 10);
      display.println(serviceInfoC02);
      break;
    case 3:
      display.setCursor(0, 0);
      display.println(F("C09 Info:"));
      display.setCursor(0, 10);
      display.println(serviceInfoC09);
      break;
  }
  display.display();
}
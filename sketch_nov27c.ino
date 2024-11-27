#include <ESP8266WebServer.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "DHT.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Definitions
#define DHTTYPE DHT11
#define DHTPIN 14  // GPIO14 (D5)
#define DS18B20 2  // GPIO2 (D4)
#define REPORTING_PERIOD_MS 1000
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // Reset pin (-1 if not used)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi Credentials
const char* ssid = "Abhijeet";       // Your WiFi SSID
const char* password = "SRMVIT@1806"; // Your WiFi password

// Sensor Instances
DHT dht(DHTPIN, DHTTYPE);
PulseOximeter pox;
OneWire oneWire(DS18B20);
DallasTemperature sensors(&oneWire);

// Web Server
ESP8266WebServer server(80);

// Global Variables
float temperature = -1, humidity = -1, BPM = -1, SpO2 = -1, bodytemperature = -1;
unsigned long lastDHTMillis = 0, lastDS18B20Millis = 0, lastMAXMillis = 0;
const long DHTInterval = 2000;       // 2 seconds
const long DS18B20Interval = 5000;   // 5 seconds
uint32_t tsLastReport = 0;
unsigned long lastTempReadMillis = 0;

// Function Prototypes
void handle_OnConnect();
void handle_NotFound();
String SendHTML(float temperature, float humidity, float BPM, float SpO2, float bodytemperature);

// Setup Function
void setup() {
  Serial.begin(115200);
  delay(100);

  // Sensor Initializations
  dht.begin();
  sensors.begin();

  // WiFi Initialization
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Web Server Routes
  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);
  server.begin();

  // Initialize the OLED with detected I2C address
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // 0x3C is the I2C address for the SSD1306
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);  // Don't proceed further if OLED fails to initialize
  }
  display.display();
  delay(2000);  // Pause for 2 seconds to show the splash screen
  display.clearDisplay();

  // MAX30100 Initialization
  Serial.print("Initializing MAX30100 sensor...");
  if (!pox.begin()) {
    Serial.println("FAILED to initialize MAX30100.");
  } else {
    Serial.println("SUCCESS");
  }
}

// Main Loop
void loop() {
  unsigned long currentMillis = millis();
  server.handleClient();

  // MAX30100 Updates (frequent)
  if (currentMillis - lastMAXMillis >= 100) {  // Every 100 ms
    lastMAXMillis = currentMillis;
    pox.update();
    BPM = pox.getHeartRate();
    SpO2 = pox.getSpO2();
    if (BPM == 0 || SpO2 == 0) {
      
      BPM = -1;
      SpO2 = -1;
    }
  }

  // DHT11 Updates (every 2 seconds)
  if (currentMillis - lastDHTMillis >= DHTInterval) {
    lastDHTMillis = currentMillis;
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    if (isnan(temperature)) temperature = -1;  // Error handling
    if (isnan(humidity)) humidity = -1;
  }

  // DS18B20 Updates (asynchronous temperature conversion)
  if (currentMillis - lastDS18B20Millis >= DS18B20Interval) {
    lastDS18B20Millis = currentMillis;
    // Request temperature conversion asynchronously
    sensors.setWaitForConversion(false);
    sensors.requestTemperatures();
    // Schedule reading for after conversion time (750ms max for DS18B20)
    if (currentMillis - lastTempReadMillis >= 750) { // Wait for conversion
      lastTempReadMillis = currentMillis;
      float temp = sensors.getTempCByIndex(0);
      if (temp == DEVICE_DISCONNECTED_C) {
        Serial.println("Error reading DS18B20 body temperature.");
        bodytemperature = -1;
      } else {
        bodytemperature = temp;
      }
    }
  }

  // Display on OLED
  display.clearDisplay();
  display.setTextSize(1);  // Text size
  display.setTextColor(SSD1306_WHITE);  // Text color
  display.setCursor(0, 0);  // Start at top-left corner
  display.print("Heart Rate: ");
  display.print((BPM == -1) ? "--" : String(BPM));
  display.println(" BPM");

  display.print("SpO2: ");
  display.print((SpO2 == -1) ? "--" : String(SpO2));
  display.println(" %");

  display.print("Body Temp: ");
  display.print((bodytemperature == -1) ? "--" : String(bodytemperature, 1));
  display.println(" C");

  display.display();

  delay(1000);  // Update every second (you can change this as needed)

  // Debugging Outputs
  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
    tsLastReport = millis();
    Serial.println("*********************************");
    Serial.print("Room Temperature: ");
    Serial.println((temperature == -1) ? "N/A" : String(temperature) + " °C");
    Serial.print("Room Humidity: ");
    Serial.println((humidity == -1) ? "N/A" : String(humidity) + " %");
    Serial.print("Heart Rate (BPM): ");
    Serial.println((BPM == -1) ? "N/A" : String(BPM));
    Serial.print("SpO2: ");
    Serial.println((SpO2 == -1) ? "N/A" : String(SpO2) + " %");
    Serial.print("Body Temperature: ");
    Serial.println((bodytemperature == -1) ? "N/A" : String(bodytemperature) + " °C");
    Serial.println("*********************************");
  }
}

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML(temperature, humidity, BPM, SpO2, bodytemperature));
}

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

String SendHTML(float temperature, float humidity, float BPM, float SpO2, float bodytemperature) {
  String html = "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  html += "<title>Patient Health Monitoring</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.7.2/css/all.min.css'>";
  html += "<style>";
  
  // CSS Styles
  html += "body { font-family: Arial, sans-serif; background-color: #f7f7f7; margin: 0; padding: 0; color: #333; }";
  html += ".header { background-color: #008080; color: #fff; padding: 20px; text-align: center; }";
  html += ".header h1 { margin: 0; font-size: 24px; }";
  html += ".container { padding: 20px; }";
  html += ".row { display: flex; flex-wrap: wrap; justify-content: center; margin-bottom: 20px; }";
  html += ".card { background: #fff; border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); margin: 10px; padding: 20px; text-align: center; width: 300px; }";
  html += ".card .icon { font-size: 50px; margin-bottom: 10px; color: #008080; }";
  html += ".card .value { font-size: 30px; font-weight: bold; }";
  html += ".card .label { font-size: 14px; color: #666; }";
  html += ".status-bar { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }";
  html += ".status-item { font-size: 14px; display: flex; align-items: center; }";
  html += ".status-item i { margin-right: 5px; }";
  html += ".button { background-color: #28a745; color: white; padding: 10px 20px; text-decoration: none; border-radius: 5px; font-size: 16px; display: inline-block; }";
  html += ".button:hover { background-color: #218838; }";

  html += "</style>";

  //Ajax Code Start
  html += "<script>";
html += "setInterval(loadDoc,1000);";
html += "function loadDoc() {";
html += "var xhttp = new XMLHttpRequest();";
html += "xhttp.onreadystatechange = function() {";
html += "if (this.readyState == 4 && this.status == 200) {";
html += "document.body.innerHTML = this.responseText;";
html += "}";
html += "};";
html += "xhttp.open('GET', '/', true);";
html += "xhttp.send();";
html += "}";
html += "</script>";

  //Ajax Code END

  html += "</head>";
  html += "<body>";

  // Header Section
  html += "<div class='header'>";
  html += "<h1>Patient Health Monitoring</h1>";
  html += "</div>";

  // Status Section
  html += "<div class='container'>";
  html += "<div class='status-bar'>";
  html += "<div class='status-item'><i class='fas fa-user-circle'></i> <strong>Mr. Abhijeet</strong></div>";
  html += "<div class='status-item'><i class='fas fa-thermometer-half'></i> Room: " + String((int)temperature) + "°C</div>";
  html += "<div class='status-item'><i class='fas fa-tint'></i> Humidity: " + String((int)humidity) + "%</div>";
  html += "</div>";

  // Sensor Cards
  html += "<div class='row'>";
  
    // Heart Rate
  html += "<div class='card'>";
  html += "<div class='icon'><i class='fas fa-heartbeat'></i></div>";
  html += "<div class='value'>" + ((BPM == -1) ? String("--") : String((int)BPM)) + " BPM</div>";
  html += "<div class='label'>Heart Rate</div>";
  html += "</div>";
  
  // SpO2
  html += "<div class='card'>";
  html += "<div class='icon'><i class='fas fa-burn'></i></div>";
  html += "<div class='value'>" + ((SpO2 == -1) ? String("--") : String((int)SpO2)) + " %</div>";
  html += "<div class='label'>SpO2</div>";
  html += "</div>";
  
  // Body Temperature
  html += "<div class='card'>";
  html += "<div class='icon'><i class='fas fa-thermometer-full'></i></div>";
  html += "<div class='value'>" + ((bodytemperature == -1) ? String("--") : String(bodytemperature, 1)) + "°C</div>";
  html += "<div class='label'>Body Temperature</div>";
  html += "</div>";

  
  // Sleep Hours
  html += "<div class='card'>";
  html += "<div class='icon'><i class='fas fa-bed'></i></div>";
  html += "<div class='value'>12 hrs</div>";
  html += "<div class='label'>Sleep</div>";
  html += "</div>";

  html += "</div>"; // End of Row

  // Documents Section
  html += "<div style='text-align: center; margin-top: 20px;'>";
  html += "<a href='#' class='button'>View Documents</a>";
  html += "</div>";

  html += "</div>"; // End of Container
  html += "</body>";
  html += "</html>";

  return html;
}
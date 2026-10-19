#include <Arduino.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <WiFiS3.h>
#include <ArduinoHttpClient.h>

// WiFi credentials - CHANGE THESE TO YOUR NETWORK
const char* ssid = "InternetOfThings";
const char* password = "WTC_InternetOfThingsProgramq";

// API endpoint configuration - CHANGE THESE TO YOUR API
const char* serverAddress = "temeraturemonitoring-fcctdzecfehhchbr.northcentralus-01.azurewebsites.net";
const int serverPort = 443;  // Use 443 for HTTPS
const char* apiEndpoint = "/api/sensors";  // Your API endpoint path

// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define LED_PIN 6  // LED on digital pin 6

// Update interval (milliseconds)
#define UPDATE_INTERVAL 30000  // Send data every 30 seconds
#define HTTP_RESPONSE_TIMEOUT 1000
#define TLS_CONNECTION_TIMEOUT 3000
#define DISPLAY_UPDATE_INTERVAL 1000

// Create sensor and display objects
Adafruit_AHTX0 aht;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WiFiSSLClient wifiClient;  // Use WiFiSSLClient for HTTPS
HttpClient httpClient = HttpClient(wifiClient, serverAddress, serverPort);

unsigned long lastUpdateTime = 0;
unsigned long lastDisplayUpdateTime = 0;
bool networkNeedsReset = false;

enum ApiStatus {
  API_IDLE,
  API_SENDING,
  API_SUCCESS,
  API_FAILED
};

ApiStatus apiStatus = API_IDLE;

void drawStatusIcons() {
  display.fillRect(94, 53, 34, 11, SSD1306_BLACK);

  if (WiFi.status() == WL_CONNECTED) {
    display.drawLine(96, 57, 101, 53, SSD1306_WHITE);
    display.drawLine(101, 53, 106, 57, SSD1306_WHITE);
    display.drawLine(98, 59, 101, 57, SSD1306_WHITE);
    display.drawLine(101, 57, 104, 59, SSD1306_WHITE);
    display.drawPixel(101, 62, SSD1306_WHITE);
  } else {
    display.drawLine(97, 54, 105, 62, SSD1306_WHITE);
    display.drawLine(105, 54, 97, 62, SSD1306_WHITE);
  }

  display.drawCircle(119, 58, 5, SSD1306_WHITE);
  if (apiStatus == API_SENDING) {
    display.fillCircle(119, 58, 2, SSD1306_WHITE);
  } else if (apiStatus == API_SUCCESS) {
    display.drawLine(116, 58, 118, 60, SSD1306_WHITE);
    display.drawLine(118, 60, 122, 55, SSD1306_WHITE);
  } else if (apiStatus == API_FAILED) {
    display.drawLine(116, 55, 122, 61, SSD1306_WHITE);
    display.drawLine(122, 55, 116, 61, SSD1306_WHITE);
  }
}

void setApiStatus(ApiStatus status) {
  apiStatus = status;
  drawStatusIcons();
  display.display();
}

bool connectToWiFi() {
  Serial.println("\n========== WiFi Connection Start ==========");
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();
  
  WiFi.begin(ssid, password);
  Serial.println("WiFi.begin() called");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[SUCCESS] WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.println("==========================================\n");
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.print("IP: ");
    display.println(WiFi.localIP());
    display.display();
    delay(2000);
    return true;
  } else {
    Serial.println("[FAILED] WiFi Connection Failed!");
    Serial.print("Final WiFi Status: ");
    Serial.println(WiFi.status());
    Serial.println("==========================================\n");
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Failed!");
    display.display();
    return false;
  }
}

void resetNetworkConnection() {
  Serial.println("Resetting WiFi after API transport failure...");
  httpClient.stop();
  WiFi.disconnect();
  delay(1000);
  connectToWiFi();
}

void sendDataToAPI(float tempF, float tempC, float humidity) {
  Serial.println("\n========== API Request Start ==========");
  setApiStatus(API_SENDING);

  if (networkNeedsReset) {
    resetNetworkConnection();
    networkNeedsReset = false;
  }
  
  // Check WiFi status
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERROR] WiFi not connected!");
    Serial.print("WiFi Status Code: ");
    Serial.println(WiFi.status());
    Serial.println("Attempting to reconnect...");
    if (!connectToWiFi()) {
      setApiStatus(API_FAILED);
      return;
    }
  }
  Serial.println("[OK] WiFi connection verified");
  Serial.print("Signal Strength (RSSI): ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  
  // Create JSON payload
  Serial.println("Creating JSON payload...");
  String jsonPayload = "{";
  jsonPayload += "\"deviceId\":\"RileyF\",";
  jsonPayload += "\"temperature_f\":" + String(tempF, 2) + ",";
  jsonPayload += "\"temperature_c\":" + String(tempC, 2) + ",";
  jsonPayload += "\"humidity\":" + String(humidity, 2);
  jsonPayload += "}";
  
  Serial.println("[OK] JSON payload created");
  Serial.print("Payload Size: ");
  Serial.print(jsonPayload.length());
  Serial.println(" bytes");
  Serial.print("Payload: ");
  Serial.println(jsonPayload);
  
  // Make HTTP POST request
  Serial.println("\nInitiating HTTP POST request...");
  Serial.print("Server: ");
  Serial.print(serverAddress);
  Serial.print(":");
  Serial.println(serverPort);
  Serial.print("Endpoint: ");
  Serial.println(apiEndpoint);

  // Start every HTTPS request with a fresh TLS connection.
  httpClient.stop();
  
  Serial.println("Calling httpClient.beginRequest()...");
  httpClient.beginRequest();
  Serial.println("[OK] beginRequest() completed");
  
  Serial.println("Calling httpClient.post()...");
  int requestResult = httpClient.post(apiEndpoint);
  if (requestResult != 0) {
    Serial.print("[ERROR] Could not start HTTP POST: ");
    Serial.println(requestResult);
    httpClient.stop();
    networkNeedsReset = true;
    setApiStatus(API_FAILED);
    return;
  }
  Serial.println("[OK] post() completed");
  
  Serial.println("Sending headers...");
  httpClient.sendHeader("Content-Type", "application/json");
  httpClient.sendHeader("Content-Length", jsonPayload.length());
  Serial.println("[OK] Headers sent");
  
  Serial.println("Sending body...");
  httpClient.beginBody();
  httpClient.print(jsonPayload);
  httpClient.endRequest();
  Serial.println("[OK] Request sent, waiting for response...");
  
  // Get response
  Serial.println("\nReading response status code...");
  int statusCode = httpClient.responseStatusCode();
  Serial.print("Status Code: ");
  Serial.println(statusCode);
  
  // Interpret status code
  if (statusCode > 0) {
    Serial.println("[SUCCESS] Received HTTP response");
  } else {
    Serial.println("[ERROR] HTTP request failed");
    switch(statusCode) {
      case -1:
        Serial.println("Error: HTTP_ERROR_CONNECTION_FAILED (-1)");
        Serial.println("Could not connect to server");
        break;
      case -2:
        Serial.println("Error: HTTP_ERROR_API (-2)");
        Serial.println("Incorrect API usage");
        break;
      case -3:
        Serial.println("Error: HTTP_ERROR_TIMED_OUT (-3)");
        Serial.println("Request timed out - server not responding");
        break;
      case -4:
        Serial.println("Error: HTTP_ERROR_INVALID_RESPONSE (-4)");
        Serial.println("Invalid response from server");
        break;
      default:
        Serial.println("Error: Unknown error code");
    }
  }
  
  if (statusCode > 0) {
    Serial.println("Reading response body...");
    String response = httpClient.responseBody();
    Serial.print("Response Length: ");
    Serial.print(response.length());
    Serial.println(" bytes");
    Serial.print("Response Body: ");
    Serial.println(response);
  } else {
    Serial.println("Skipping response body because no HTTP response was received");
  }
  httpClient.stop();

  if (statusCode < 0) {
    networkNeedsReset = true;
  }
  Serial.println("==========================================\n");
  
  if (statusCode == 200 || statusCode == 201) {
    Serial.println("[SUCCESS] Data sent successfully!");
    setApiStatus(API_SUCCESS);
  } else if (statusCode > 0) {
    Serial.print("[WARNING] Unexpected status code: ");
    Serial.println(statusCode);
    setApiStatus(API_FAILED);
  } else {
    Serial.println("[FAILED] API request failed");
    setApiStatus(API_FAILED);
  }
}

void setup() {
  Serial.begin(9600);
  while (!Serial && millis() < 3000); // Wait for Serial or timeout
  Serial.println("\n\n========================================");
  Serial.println("Arduino Sensor Kit - WiFi Temperature Monitor");
  Serial.println("Firmware Version: 1.0");
  Serial.print("WiFi Module Firmware: ");
  Serial.println(WiFi.firmwareVersion());
  Serial.println("========================================\n");

  httpClient.setHttpResponseTimeout(HTTP_RESPONSE_TIMEOUT);
  wifiClient.setConnectionTimeout(TLS_CONNECTION_TIMEOUT);
  
  // Setup LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize OLED display
  Serial.println("Initializing OLED display...");
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("[FAILED] SSD1306 allocation failed");
    while (1) delay(10);
  }
  Serial.println("[OK] OLED display initialized");
  
  // Flip the display 180 degrees
  display.setRotation(2);
  
  // Clear display and show startup message
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Sensor Kit Init...");
  display.display();
  
  // Initialize sensor
  Serial.println("\nInitializing AHT sensor...");
  if (!aht.begin()) {
    Serial.println("[FAILED] Could not find AHT sensor!");
    Serial.println("Check wiring and I2C connections");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("AHT Sensor Error!");
    display.println("Check wiring");
    display.display();
    while (1) delay(10);
  }
  
  Serial.println("[OK] AHT sensor found and initialized!");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("AHT Sensor Ready!");
  display.display();
  delay(1000);
  
  // Connect to WiFi
  connectToWiFi();
}

void loop() {
  unsigned long currentTime = millis();
  if (currentTime - lastDisplayUpdateTime < DISPLAY_UPDATE_INTERVAL) {
    delay(10);
    return;
  }
  lastDisplayUpdateTime = currentTime;

  // Read sensor
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  
  // Convert Celsius to Fahrenheit
  float tempF = (temp.temperature * 9.0 / 5.0) + 32.0;
  
  // Print values to Serial
  Serial.print("Temperature: ");
  Serial.print(tempF, 1);
  Serial.print("°F (");
  Serial.print(temp.temperature, 1);
  Serial.print("°C), Humidity: ");
  Serial.print(humidity.relative_humidity, 1);
  Serial.println("%");
  
  // Control LED based on temperature (75°F threshold)
  if (tempF >= 75.0) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED ON - Temperature >= 75°F");
  } else {
    digitalWrite(LED_PIN, LOW);
  }
  
  // Display on OLED
  display.clearDisplay();
  
  // Temperature in large text
  display.setTextSize(3);
  display.setCursor(0, 0);
  display.print(tempF, 1);
  display.println("F");
  
  // Humidity in smaller text
  display.setTextSize(2);
  display.setCursor(0, 30);
  display.print("Hum: ");
  display.print(humidity.relative_humidity, 1);
  display.println("%");
  
  // Temperature status
  display.setTextSize(1);
  display.setCursor(0, 55);
  if (tempF > 77) {  // 77°F = 25°C
    display.println("HOT!");
  } else {
    display.println("Normal");
  }

  drawStatusIcons();

  // Push the latest sensor reading before any potentially blocking network work.
  display.display();
  
  // Send data to API at regular intervals
  if (currentTime - lastUpdateTime >= UPDATE_INTERVAL) {
    Serial.println("\n***** Time to send API update *****");
    Serial.print("Time since last update: ");
    Serial.print((currentTime - lastUpdateTime) / 1000);
    Serial.println(" seconds");
    sendDataToAPI(tempF, temp.temperature, humidity.relative_humidity);
    lastUpdateTime = currentTime;
    Serial.println("Next update in 30 seconds\n");
  }
}
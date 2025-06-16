#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <Servo.h>

#define SERVO_PIN 13           // D7 GPIO13
#define LED_PIN LED_BUILTIN    // GPIO2

ESP8266WebServer server(80);
Servo myServo;
unsigned long lastAlive = 0;
String curtainStatus = "Unknown";

const char* ssid = "BANKDETAILS";
const char* password = "HERE";

// ────────────────────────────────────────────────────────────────
// Setup
// ────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  IPAddress local_IP(192, 168, 1, 19);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(8, 8, 8, 8); // Optional, Google DNS

  WiFi.config(local_IP, gateway, subnet, dns);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  // mDNS setup
  if (MDNS.begin("onlyfans")) {
    Serial.println("mDNS responder started");
    Serial.println("Try visiting: http://onlyfans.local");
  } else {
    Serial.println("mDNS setup failed");
  }
  

  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
    return;
  }

  // Confirm required files exist
  if (!LittleFS.exists("/index.html")) Serial.println("❌ Missing: index.html");
  if (!LittleFS.exists("/style.css"))   Serial.println("❌ Missing: style.css");
  if (!LittleFS.exists("/script.js"))   Serial.println("❌ Missing: script.js");
  if (!LittleFS.exists("/FanLogo.svg")) Serial.println("❌ Missing: FanLogo.svg");


  // File serving
  server.serveStatic("/",            LittleFS, "/index.html");
  server.serveStatic("/style.css",   LittleFS, "/style.css");
  server.serveStatic("/script.js",   LittleFS, "/script.js");
  server.serveStatic("/FanLogo.svg", LittleFS, "/FanLogo.svg");
  server.serveStatic("/curtain.png", LittleFS, "/curtain.png");  // optional if used

  // API routes
  server.on("/open", handleOpen);
  server.on("/close", handleClose);

  server.on("/restart", []() {
    server.send(200, "text/plain", "Restarting ESP...");
    delay(500);  // let the message send
    ESP.restart();
  });

  server.begin();
  Serial.println("HTTP server started");

  // Startup movement
  Serial.println("Attaching servo...");
  myServo.attach(SERVO_PIN);
  myServo.write(0);
  Serial.println("Moved to 0°");
  delay(1000);

  myServo.write(180);
  Serial.println("Moved to 180°");
  delay(10000);

  myServo.detach();
  Serial.println("PWM detached");
}

// ────────────────────────────────────────────────────────────────
// Curtain Handlers
// ────────────────────────────────────────────────────────────────
void handleOpen() {
  Serial.println("Opening curtain...");
  myServo.attach(SERVO_PIN);
  myServo.write(180);
  delay(1000);
  myServo.detach();
  curtainStatus = "Open";
  server.send(200, "text/plain", "Curtain opened.");
}

void handleClose() {
  Serial.println("Closing curtain...");
  myServo.attach(SERVO_PIN);
  myServo.write(0);
  delay(1000);
  myServo.detach();
  curtainStatus = "Closed";
  server.send(200, "text/plain", "Curtain closed.");
}

// ────────────────────────────────────────────────────────────────
// Main Loop
// ────────────────────────────────────────────────────────────────
void loop() {
  MDNS.update();
  server.handleClient();

  unsigned long now = millis();
    if (now - lastAlive >= 20UL * 60UL * 1000UL) {  // 20 minutes
      Serial.println("[DEBUG] Keep-alive blink");
      digitalWrite(LED_PIN, LOW);   // ON
      delay(100);                   // short blink
      digitalWrite(LED_PIN, HIGH);  // OFF
      lastAlive = now;
    }
}

// Window exhaust fan curtain control
// Using ESP8266 NodeMCU ESP-12
// 2025 http://DisasterOfPuppets.com

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <Servo.h>

#define SERVO_PIN 13           // D7 GPIO13 — safe for servo PWM
#define LED_PIN LED_BUILTIN    // GPIO2 — onboard blue LED
#define LOG_BUFFER_SIZE 2048
String logBuffer = "";

ESP8266WebServer server(80);
Servo myServo;
unsigned long lastAlive = 0;

//**************CHANGE ME**************
const char* ssid = "SLOWKEVIN";
const char* password = "FUKevin07";
//*************************************

void setup() {
  Serial.begin(115200);
  delay(100);
  Log("\nBooting...");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // OFF (active LOW)

  IPAddress local_IP(192, 168, 1, 19);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(8, 8, 8, 8); // Optional, Google DNS
  
  WiFi.config(local_IP, gateway, subnet, dns);

  WiFi.begin(ssid, password);
  Log("Connecting to Wi-Fi");

  // Blink LED while connecting
  while (WiFi.status() != WL_CONNECTED) {
    Log(".");
    digitalWrite(LED_PIN, LOW);
    delay(250);
    digitalWrite(LED_PIN, HIGH);
    delay(250);
  }

  Log("\nWi-Fi connected!");
  Log("IP address: ");
  Log(WiFi.localIP().toString()); // ✅ Now it's a String
  digitalWrite(LED_PIN, HIGH); // OFF

  // mDNS setup
  if (MDNS.begin("onlyfans")) {
    Log("mDNS responder started");
    Log("Try visiting: http://onlyfans.local");
  } else {
    Log("mDNS setup failed");
  }

  // LittleFS setup
  if (!LittleFS.begin()) {
    Log("❌ LittleFS mount failed!");
    return;
  }

  // Confirm required files exist
  if (!LittleFS.exists("/index.html")) Log("❌ Missing: index.html");
  if (!LittleFS.exists("/style.css"))   Log("❌ Missing: style.css");
  if (!LittleFS.exists("/script.js"))   Log("❌ Missing: script.js");
  if (!LittleFS.exists("/fanlogo.svg")) Log("❌ Missing: fanlogo.svg");
  if (!LittleFS.exists("/log.html")) Log("❌ Missing: log.html");
  if (!LittleFS.exists("/curtain.png")) Log("❌ Missing: curtain.png");

  // File routes
  server.on("/", handleRoot);
  server.serveStatic("/style.css",   LittleFS, "/style.css");
  server.serveStatic("/script.js",   LittleFS, "/script.js");
  server.serveStatic("/fanlogo.svg", LittleFS, "/fanlogo.svg");
  server.serveStatic("/log.html", LittleFS, "/log.html");
  server.serveStatic("/curtain.png", LittleFS, "/curtain.png"); // optional
  server.serveStatic("/index.html", LittleFS, "/index.html");

  // Action routes
  server.on("/open", handleOpen);
  server.on("/close", handleClose);
  server.on("/restart", handleRestart);
  server.on("/log", []() {
    server.send(200, "text/plain", logBuffer);
  });

  server.begin();
  Log("HTTP server started");

  // Startup movement
  Log("Attaching servo...");
  myServo.attach(SERVO_PIN);
  myServo.write(0);
  Log("Moved to 0°");
  delay(1000);

  myServo.write(180);
  Log("Moved to 180°");
  delay(10000);

  myServo.detach();
  Log("PWM detached");
}

//******************FUNCTIONS **********************

void Log(const String& msg) {
  Serial.println(msg); // still prints over USB if connected
  logBuffer += msg + "\n";
  if (logBuffer.length() > LOG_BUFFER_SIZE) {
    logBuffer = logBuffer.substring(logBuffer.length() - LOG_BUFFER_SIZE); // trim oldest
  }
}

// Routes
void handleRoot() {
  Log("HTTP connection detected");
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Missing index.html");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

void handleOpen() {
  Log("Opening curtain...");
  myServo.attach(SERVO_PIN);
  myServo.write(180);
  delay(1000);
  myServo.detach();
  server.send(200, "text/plain", "Curtain opened.");
}

void handleClose() {
  Log("Closing curtain...");
  myServo.attach(SERVO_PIN);
  myServo.write(0);
  delay(1000);
  myServo.detach();
  server.send(200, "text/plain", "Curtain closed.");
}

void handleRestart() {
  Log("Restarting ESP...");
  server.send(200, "text/plain", "Restarting ESP...");
  delay(500);
  ESP.restart();
}




void loop() {
  MDNS.update();
  server.handleClient();

  unsigned long now = millis();
  if (now - lastAlive >= 20UL * 60UL * 1000UL) {  // 20 minutes
    Log("[DEBUG] Keep-alive blink");
    digitalWrite(LED_PIN, LOW);   // ON
    delay(100);                   // short blink
    digitalWrite(LED_PIN, HIGH);  // OFF
    lastAlive = now;
  }
}

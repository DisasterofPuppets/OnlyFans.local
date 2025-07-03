/* --------------------------------------------------------------  
* Window exhaust fan curtain control
* This shoddily put together code creates a server on an ESP8266 via Http
* that communicates with a Nema17 Stepper using a TMC2208 driver and toggles it between two limit switches
* Using ESP8266 NodeMCU ESP-12
* 2025 http://DisasterOfPuppets.com & Codex :P
* 
* 
*WARNING - ENSURE YOU SET THE CORRECT TMC2208 VOLTAGE BEFORE PROCEEDING - SEE README GITHUBLINKHERE
-----------------------------------------------------------------*/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <AccelStepper.h>
#include <user_interface.h>
#define LOG_BUFFER_SIZE 2048

struct RTCData {
  uint32_t magic;
  bool lastWasOpen;
};

RTCData rtcData;
uint32_t RTC_MEM_START = 65;

bool skipStartupMovement = false;
String logBuffer = "";
bool isOpen = false;
bool isMoving = false;
unsigned long lastAlive = 0;

// -= WIFI SETTINGS =-
const char* ssid = "YOURBANK";
const char* password = "DETAILSHERE";

// -= STEPPER SETTINGS =-
#define STEP_PIN D1
#define DIR_PIN  D2
#define OPEN_SWITCH_PIN D5
#define CLOSED_SWITCH_PIN D6
#define LED_PIN LED_BUILTIN
constexpr bool OPEN_ON_RUN = true;
constexpr int MOVE_SPEED = 600;   // steps per second
constexpr int ACCEL = 800;        // acceleration

// -= SERVER SETTINGS =-
IPAddress local_IP(192, 168, 1, 19);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);
constexpr bool USE_DNS = false;

ESP8266WebServer server(80);
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

void handleRoot();
void handleOpen();
void handleClose();
void handleRestart();
void RunStartupMovement();
void Log(const String& msg);
void CheckWiFi();

void setup() {
  Serial.begin(115200);

  system_rtc_mem_read(RTC_MEM_START, &rtcData, sizeof(rtcData));
  if (rtcData.magic == 0xCAFEBABE) {
    skipStartupMovement = true;
    isOpen = rtcData.lastWasOpen;
    Log("User initiated restart. Skipping startup movement.");
    Log("Curtain state restored: " + String(isOpen ? "OPEN" : "CLOSED"));
    rtcData.magic = 0;
    system_rtc_mem_write(RTC_MEM_START, &rtcData, sizeof(rtcData));
  }

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  pinMode(OPEN_SWITCH_PIN, INPUT_PULLUP);
  pinMode(CLOSED_SWITCH_PIN, INPUT_PULLUP);

  stepper.setMaxSpeed(MOVE_SPEED);
  stepper.setAcceleration(ACCEL);

  if (USE_DNS) {
    WiFi.config(local_IP, gateway, subnet, dns);
  } else {
    WiFi.config(local_IP, gateway, subnet);
  }
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(LED_PIN, LOW);
    delay(250);
    digitalWrite(LED_PIN, HIGH);
    delay(250);
  }
  Log("\nWi-Fi connected!");
  Log("IP address: ");
  Log(WiFi.localIP().toString());
  digitalWrite(LED_PIN, HIGH);

  if (MDNS.begin("onlyfans")) {
    Log("mDNS responder started");
    Log("Try visiting: http://onlyfans.local");
  } else {
    Log("mDNS setup failed");
  }

  if (!LittleFS.begin()) {
    Log("❌ LittleFS mount failed!");
    return;
  }

  if (!LittleFS.exists("/index.html")) Log("❌ Missing: index.html");
  if (!LittleFS.exists("/style.css"))   Log("❌ Missing: style.css");
  if (!LittleFS.exists("/script.js"))   Log("❌ Missing: script.js");
  if (!LittleFS.exists("/fanlogo.svg")) Log("❌ Missing: fanlogo.svg");
  if (!LittleFS.exists("/log.html")) Log("❌ Missing: log.html");
  if (!LittleFS.exists("/curtain.png")) Log("❌ Missing: curtain.png");

  server.on("/", handleRoot);
  server.serveStatic("/style.css",   LittleFS, "/style.css");
  server.serveStatic("/script.js",   LittleFS, "/script.js");
  server.serveStatic("/fanlogo.svg", LittleFS, "/fanlogo.svg");
  server.serveStatic("/log.html", LittleFS, "/log.html");
  server.serveStatic("/curtain.png", LittleFS, "/curtain.png");
  server.serveStatic("/index.html", LittleFS, "/index.html");
  server.on("/curtain-status", [](){
    String state;
    if (isMoving) state = "moving";
    else if (isOpen) state = "open";
    else state = "closed";
    server.send(200, "application/json", "{\"state\":\"" + state + "\"}");
  });
  server.on("/open", handleOpen);
  server.on("/close", handleClose);
  server.on("/restart", handleRestart);
  server.on("/log", [](){ server.send(200, "text/plain", logBuffer); });

  server.begin();
  Log("HTTP server started");

  if (!skipStartupMovement) {
    Log("Running startup routines....");
    if (OPEN_ON_RUN) {
      RunStartupMovement();
    }
  } else {
    Log("User initiated server restart");
  }
  Log("Curtain current state at startup: " + String(isOpen ? "OPEN" : "CLOSED"));
}

void Log(const String& msg) {
  Serial.println(msg);
  logBuffer += msg + "\n";
  if (logBuffer.length() > LOG_BUFFER_SIZE) {
    logBuffer = logBuffer.substring(logBuffer.length() - LOG_BUFFER_SIZE);
  }
}

void CheckWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Log("[WiFi] Disconnected. Attempting reconnect...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      Log(".");
      digitalWrite(LED_PIN, LOW);
      delay(250);
      digitalWrite(LED_PIN, HIGH);
      delay(250);
    }
    Log("\nWi-Fi Reconnected!");
    Log("IP address: ");
    Log(WiFi.localIP().toString());
    digitalWrite(LED_PIN, HIGH);
  }
}

void handleRoot() {
  IPAddress clientIP = server.client().remoteIP();
  Log("HTTP connection detected from: " + clientIP.toString());
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Missing index.html");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

void stepUntil(int limitPin, bool dir) {
  unsigned long start = millis();
  digitalWrite(DIR_PIN, dir ? HIGH : LOW);
  stepper.setSpeed(MOVE_SPEED * (dir ? 1 : -1));
  while (digitalRead(limitPin) == HIGH) {
    if (millis() - start > 10000) { // 10s timeout
      Log("❌ Timeout: Limit switch not triggered.");
      break;
    }
    stepper.runSpeed();
    server.handleClient();
    MDNS.update();
    CheckWiFi();
  }
  stepper.stop();
}

void handleOpen() {
  if (isMoving) {
    server.send(200, "text/plain", "Curtain currently moving.");
    return;
  }
  if (isOpen) {
    server.send(200, "text/plain", "Curtain already open.");
    return;
  }
  Log("Curtain Open");
  isMoving = true;
  stepUntil(OPEN_SWITCH_PIN, true);
  isMoving = false;
  isOpen = true;
  server.send(200, "text/plain", "Curtain opened.");
}

void handleClose() {
  if (isMoving) {
    server.send(200, "text/plain", "Curtain currently moving.");
    return;
  }
  if (!isOpen) {
    server.send(200, "text/plain", "Curtain already closed.");
    return;
  }
  Log("Curtain Closed");
  isMoving = true;
  stepUntil(CLOSED_SWITCH_PIN, false);
  isMoving = false;
  isOpen = false;
  server.send(200, "text/plain", "Curtain closed.");
}

void handleRestart() {
  Log("Restarting Server...");
  rtcData.magic = 0xCAFEBABE;
  rtcData.lastWasOpen = isOpen;
  system_rtc_mem_write(RTC_MEM_START, &rtcData, sizeof(rtcData));
  server.send(200, "text/plain", "Restarting ESP...");
  delay(500);
  ESP.restart();
}

void RunStartupMovement() {
  Serial.println("Initialising...");
  isMoving = true;
  Log("Homing to CLOSED...");
  stepUntil(CLOSED_SWITCH_PIN, false);
  Log("Homing complete");
  delay(500);
  stepUntil(OPEN_SWITCH_PIN, true);
  delay(500);
  isOpen = true;
  isMoving = false;
  Log("Startup movement complete");
}

void loop() {
  MDNS.update();
  server.handleClient();
  CheckWiFi();
  unsigned long now = millis();
  if (now - lastAlive >= 20UL * 60UL * 1000UL) {
    Log("[DEBUG] Keep-alive blink");
    digitalWrite(LED_PIN, LOW);
    delay(100);
    digitalWrite(LED_PIN, HIGH);
    lastAlive = now;
  }
}

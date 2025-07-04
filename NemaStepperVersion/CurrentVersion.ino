// Shits broke yo, log.html works fine...index.html is slow as fuck.
//I think it has to do with the way it is trying to query the current servo state


/* --------------------------------------------------------------  
 * Window exhaust fan curtain control
 * This code creates a server on an ESP8266 via HTTP to control a Nema17 Stepper
 * using a TMC2208 driver, toggling between two limit switches.
 * Using ESP8266 NodeMCU ESP-12
 * 2025 http://DisasterOfPuppets.com & Codex :P
 * 
 * NOTE: LIMIT SWITCHES ARE LOW WHEN TRIGGERED
 * WARNING: SET CORRECT TMC2208 VOLTAGE BEFORE PROCEEDING I used 0.634V
 * Upload the Code, once done, upload the files using the LittleFS plugin
 *---------------------------------------------------------------*/

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
#define DIR_PIN D2
#define ENABLE_PIN D7  // Added enable pin for TMC2208
#define OPEN_SWITCH_PIN D5
#define CLOSED_SWITCH_PIN D6
#define LED_PIN LED_BUILTIN
constexpr bool OPEN_ON_RUN = true;
constexpr int MOVE_SPEED = 600; // Steps per second
constexpr int ACCEL = 800;     // Acceleration
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// -= SERVER SETTINGS =-
IPAddress local_IP(192, 168, 1, 19);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);
constexpr bool USE_DNS = false;

ESP8266WebServer server(80);

void handleRoot();
void handleOpen();
void handleClose();
void handleRestart();
void RunStartupMovement();
void Log(const String& msg);
void CheckWiFi();

void setup() {
  Serial.begin(115200);

  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);  // Initialize enable pin
  pinMode(OPEN_SWITCH_PIN, INPUT_PULLUP);
  pinMode(CLOSED_SWITCH_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(ENABLE_PIN, LOW);  // Enable the stepper driver (LOW = enabled)

  // Initialize stepper
  stepper.setMaxSpeed(MOVE_SPEED);
  stepper.setAcceleration(ACCEL);

  // Read RTC memory
  system_rtc_mem_read(RTC_MEM_START, &rtcData, sizeof(rtcData));
  if (rtcData.magic == 0xCAFEBABE) {
    skipStartupMovement = true;
    isOpen = rtcData.lastWasOpen;
    Log("User initiated restart. Skipping startup movement.");
    Log("Curtain state restored: " + String(isOpen ? "OPEN" : "CLOSED"));
    rtcData.magic = 0;
    system_rtc_mem_write(RTC_MEM_START, &rtcData, sizeof(rtcData));
  }

  // Initialize WiFi
  if (USE_DNS) {
    WiFi.config(local_IP, gateway, subnet, dns);
  } else {
    WiFi.config(local_IP, gateway, subnet);
  }
  WiFi.begin(ssid, password);

  Serial.print("Connecting to Wi-Fi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
    delay(50);
    yield();
  }
  if (WiFi.status() == WL_CONNECTED) {
    Log("Wi-Fi up: " + WiFi.localIP().toString());
  } else {
    Log("Wi-Fi failed or timed out");
  }

  // Initialize mDNS
  if (MDNS.begin("onlyfans")) {
    Log("mDNS responder started");
    Log("Try visiting: http://onlyfans.local");
  } else {
    Log("mDNS setup failed");
  }

  // Initialize LittleFS
  if (!LittleFS.begin()) {
    Log("LittleFS mount failed!");
    return;
  }

  // Check for required files
  if (!LittleFS.exists("/index.html")) Log("Missing: index.html");
  if (!LittleFS.exists("/style.css")) Log("Missing: style.css");
  if (!LittleFS.exists("/script.js")) Log("Missing: script.js");
  if (!LittleFS.exists("/fanlogo.svg")) Log("Missing: fanlogo.svg");
  if (!LittleFS.exists("/log.html")) Log("Missing: log.html");
  if (!LittleFS.exists("/curtain.png")) Log("Missing: curtain.png");

  // Set up server routes
  server.on("/", handleRoot);
  server.serveStatic("/style.css", LittleFS, "/style.css");
  server.serveStatic("/script.js", LittleFS, "/script.js");
  server.serveStatic("/fanlogo.svg", LittleFS, "/fanlogo.svg");
  server.serveStatic("/log.html", LittleFS, "/log.html");
  server.serveStatic("/curtain.png", LittleFS, "/curtain.png");
  server.serveStatic("/index.html", LittleFS, "/index.html");
  server.on("/curtain-status", []() {
    String state = isMoving ? "moving" : (isOpen ? "open" : "closed");
    server.send(200, "application/json", "{\"state\":\"" + state + "\"}");
  });
  server.on("/open", handleOpen);
  server.on("/close", handleClose);
  server.on("/restart", handleRestart);
  server.on("/log", []() { server.send(200, "text/plain", logBuffer); });
  
  // Add test motor endpoint for debugging
  server.on("/test-motor", []() {
    Log("=== TESTING MOTOR MOVEMENT ===");
    Log("Enabling stepper driver...");
    digitalWrite(ENABLE_PIN, LOW);
    
    Log("Moving 200 steps forward...");
    stepper.move(200);
    while (stepper.distanceToGo() != 0) {
      stepper.run();
      yield();
    }
    
    delay(1000);
    
    Log("Moving 200 steps backward...");
    stepper.move(-200);
    while (stepper.distanceToGo() != 0) {
      stepper.run();
      yield();
    }
    
    stepper.setCurrentPosition(0);
    Log("=== MOTOR TEST COMPLETE ===");
    server.send(200, "text/plain", "Motor test complete - check logs");
  });

  server.begin();
  Log("HTTP server started");

  // Run startup movement if not skipped
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
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 5000) return; // Check every 5 seconds
  lastCheck = millis();
  if (WiFi.status() != WL_CONNECTED) {
    Log("[WiFi] Disconnected. Attempting reconnect...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(100);
      yield(); // Prevent watchdog reset
    }
    if (WiFi.status() == WL_CONNECTED) {
      Log("\nWi-Fi Reconnected!");
      Log("IP address: " + WiFi.localIP().toString());
      digitalWrite(LED_PIN, HIGH);
    } else {
      Log("Wi-Fi Reconnect failed!");
    }
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

// CORRECTED stepUntil function using proper AccelStepper methods
void stepUntil(int limitPin, bool dir) {
  unsigned long start = millis();
  //Log("Moving to " + String(dir ? "OPEN" : "CLOSED") + "...");
  Log("Stepper moving");
  
  // Ensure the stepper driver is enabled
  digitalWrite(ENABLE_PIN, LOW);
  
  // Set direction and start moving with a large step count
  //switch these if your stepper goes in the wrong direction
  if (dir) {
    stepper.move(-1000);  // positive number
  } else {
    stepper.move(1000); // negative number for closing
  }
  
  // Keep moving until limit switch is triggered or timeout
  while (digitalRead(limitPin) == HIGH && stepper.distanceToGo() != 0) {
    if (millis() - start > 5000) { // 5s timeout (increased from 5s)
      Log("Timeout: Limit switch not triggered.");
      break;
    }
    stepper.run();  // This handles acceleration/deceleration automatically
    yield(); // Allow ESP8266 to handle background tasks
  }
  
  stepper.stop();
  stepper.setCurrentPosition(0); // Reset position counter
  Log("Movement complete. Limit switch: " + String(digitalRead(limitPin) == LOW ? "TRIGGERED" : "NOT TRIGGERED"));
}

void handleOpen() {
  Log("=== OPEN COMMAND RECEIVED ===");
  Log("Current state - isMoving: " + String(isMoving) + ", isOpen: " + String(isOpen));
  Log("Limit switches - Open: " + String(digitalRead(OPEN_SWITCH_PIN)) + ", Closed: " + String(digitalRead(CLOSED_SWITCH_PIN)));
  Log("Stepper enabled: " + String(digitalRead(ENABLE_PIN) == LOW ? "YES" : "NO"));
  
  if (isMoving) {
    server.send(200, "text/plain", "Curtain currently moving.");
    return;
  }
  if (isOpen) {
    server.send(200, "text/plain", "Curtain already open.");
    return;
  }
  
  Log("Opening vent door...");
  isMoving = true;
  stepUntil(OPEN_SWITCH_PIN, true);
  isMoving = false;
  isOpen = true;
  Log("=== OPEN SEQUENCE COMPLETE ===");
  server.send(200, "text/plain", "Curtain opened.");
}

void handleClose() {
  Log("=== CLOSE COMMAND RECEIVED ===");
  Log("Current state - isMoving: " + String(isMoving) + ", isOpen: " + String(isOpen));
  Log("Limit switches - Open: " + String(digitalRead(OPEN_SWITCH_PIN)) + ", Closed: " + String(digitalRead(CLOSED_SWITCH_PIN)));
  Log("Stepper enabled: " + String(digitalRead(ENABLE_PIN) == LOW ? "YES" : "NO"));
  
  if (isMoving) {
    server.send(200, "text/plain", "Vent door moving.");
    return;
  }
  if (!isOpen) {
    server.send(200, "text/plain", "Vent door already closed.");
    return;
  }
  
  Log("Closing vent door...");
  isMoving = true;
  stepUntil(CLOSED_SWITCH_PIN, false);
  isMoving = false;
  isOpen = false;
  Log("=== CLOSE SEQUENCE COMPLETE ===");
  server.send(200, "text/plain", "Vent door closed.");
}

void handleRestart() {
  Log("Restarting Server...");
  rtcData.magic = 0xCAFEBABE;
  rtcData.lastWasOpen = isOpen;
  system_rtc_mem_write(RTC_MEM_START, &rtcData, sizeof(rtcData));
  server.send(200, "text/plain", "Restarting ESP...");
  delay(100);
  ESP.restart();
}

void RunStartupMovement() {
  if (digitalRead(CLOSED_SWITCH_PIN) == LOW) {
    Log("Initialising...");
    Log("Vent door already closed.");
    Log("Homing not required");
    isOpen = false;
    isMoving = false;
  } else {
    Log("Initialising...");
    Log("Closing vent door");
    isMoving = true;
    stepUntil(CLOSED_SWITCH_PIN, false);
    isOpen = false;
    isMoving = false;
    Log("Startup sequence completed.");
  }
}

void loop() {
  server.handleClient();
  MDNS.update();
  CheckWiFi();
  unsigned long now = millis();
  if (now - lastAlive >= 20UL * 60UL * 1000UL) {
    lastAlive = now;
    Log("[DEBUG] Keep-alive blink");
    digitalWrite(LED_PIN, LOW);
    delay(100);
    digitalWrite(LED_PIN, HIGH);
    Log("Open limit switch: " + String(digitalRead(OPEN_SWITCH_PIN) == LOW ? "TRIGGERED" : "NOT TRIGGERED"));
    Log("Closed limit switch: " + String(digitalRead(CLOSED_SWITCH_PIN) == LOW ? "TRIGGERED" : "NOT TRIGGERED"));
    Log("Stepper enabled: " + String(digitalRead(ENABLE_PIN) == LOW ? "YES" : "NO"));
  }
}

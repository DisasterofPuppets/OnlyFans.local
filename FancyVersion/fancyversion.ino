/* --------------------------------------------------------------  
* Window exhaust fan curtain control
* This shoddily put together code creates a server on an ESP8266 via Http
* that communicates with a Servo and toggles it between defined angles
* Using ESP8266 NodeMCU ESP-12
* 2025 http://DisasterOfPuppets.com
-----------------------------------------------------------------*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <Servo.h>
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

//**************ADJUST THESE SETTINGS FOR YOUR SETUP **************

// -= WIFI SETTINGS =-
// Wi-Fi credentials (replace with your network details)
const char* ssid = "YOUR_BANK";
const char* password = "DETAILS_HERE";

// -= SERVO SETTINGS =-
#define SERVO_PIN 13           // D7 GPIO13 — safe for servo PWM
#define LED_PIN LED_BUILTIN    // GPIO2 — onboard blue LED
constexpr int SERVOMIN = 600;  // Min pulse width in µs. DO NOT modify unless calibrating manually.
constexpr int SERVOMAX = 2400; // Max pulse width in µs. See GitHub readme for safe tuning instructions.
constexpr int CLOSED_ANGLE = 0; // Angle of the Servo when curtain is closed
constexpr int OPEN_ANGLE = 70; // Angle of the Servo when the curtain is open, adjust as needed
constexpr bool OPEN_ON_RUN = true; // Have the Servo open the curtain on power (this won't re-run on manual software reset)

// Tuning values for smooth motion
// Reduced step size for ~20% slower movement
#define STEP_US 40          // Microsecond step size for interpolation
#define STEP_DELAY 2        // Delay in milliseconds between steps

// Function prototypes
int angleToMicros(int angle);
void moveServoSmooth(int targetAngle);

// -= SERVER SETTINGS =-
IPAddress local_IP(192, 168, 1, 19); // Change this to the IP address you want to use for the server
IPAddress gateway(192, 168, 1, 1); // Your router's gateway
IPAddress subnet(255, 255, 255, 0); 
IPAddress dns(8, 8, 8, 8); // Optional, Google DNS
constexpr bool USE_DNS = false; // true : On | false : Off

//*****************************************************************

ESP8266WebServer server(80);
Servo myServo;
int currentPulse = 0; // tracks last pulse sent to the servo

void setup() {
  
  Serial.begin(115200);

//reads the system memory states
  system_rtc_mem_read(RTC_MEM_START, &rtcData, sizeof(rtcData));
  
  if (rtcData.magic == 0xCAFEBABE) {
    skipStartupMovement = true;
    isOpen = rtcData.lastWasOpen;
    Log("User initiated restart. Skipping startup movement.");
    Log("Curtain state restored: " + String(isOpen ? "OPEN" : "CLOSED"));

    // ensure smooth moves start from the remembered position
    currentPulse = angleToMicros(isOpen ? OPEN_ANGLE : CLOSED_ANGLE);

    rtcData.magic = 0;
    system_rtc_mem_write(RTC_MEM_START, &rtcData, sizeof(rtcData));
  }
  
  delay(100);
  Log("Booting...");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // OFF (active LOW)


if (USE_DNS) {
  WiFi.config(local_IP, gateway, subnet, dns);
} else {
  WiFi.config(local_IP, gateway, subnet);
}
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");

  // Blink LED while connecting
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
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
  server.on("/curtain-status", []() {
  String state;
  if (isMoving) {
    state = "moving";
  } else if (isOpen) {
    state = "open";
  } else {
    state = "closed";
  }
  server.send(200, "application/json", "{\"state\":\"" + state + "\"}");});
  
  server.on("/open", handleOpen);
  server.on("/close", handleClose);
  server.on("/restart", handleRestart);
  server.on("/log", []() {
    server.send(200, "text/plain", logBuffer);});

  server.begin();
  Log("HTTP server started");

  // Startup movement only runs if the server is not hard reset
  if (!skipStartupMovement) {
    Log("Running servo startup routines....");
    if (OPEN_ON_RUN == true) { // Check the user defined option
      RunStartupMovement();
    }
  } else {
    Log("User initiated server restart");  
  }
  Log("Curtain current state at startup: " + String(isOpen ? "OPEN" : "CLOSED"));
}

//******************FUNCTIONS **********************

void Log(const String& msg) {
  Serial.println(msg); // still prints over USB if connected
  logBuffer += msg + "\n";
  if (logBuffer.length() > LOG_BUFFER_SIZE) {
    logBuffer = logBuffer.substring(logBuffer.length() - LOG_BUFFER_SIZE); // trim oldest
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
    digitalWrite(LED_PIN, HIGH); // OFF
  }
}

// Smoothly move the servo to a target angle using STEP_US and STEP_DELAY
void moveServoSmooth(int targetAngle) {
  int targetPulse = angleToMicros(targetAngle);
  if (targetPulse == currentPulse) return;

  int step = (targetPulse > currentPulse) ? STEP_US : -STEP_US;
  while (currentPulse != targetPulse) {
    currentPulse += step;
    if ((step > 0 && currentPulse > targetPulse) ||
        (step < 0 && currentPulse < targetPulse)) {
      currentPulse = targetPulse;
    }
    myServo.writeMicroseconds(currentPulse);
    delay(STEP_DELAY);
  }
}

// Routes
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

void handleOpen() {
  Log("Curtain Open");
  myServo.attach(SERVO_PIN, SERVOMIN, SERVOMAX);
  isMoving = true;
  moveServoSmooth(constrain(OPEN_ANGLE, 0, 180));
  Log("Moved to " + String(OPEN_ANGLE) + " degrees (" + String(currentPulse) + " µs)");
  delay(1000);
  myServo.detach();
  isMoving = false;
  isOpen = true;
  server.send(200, "text/plain", "Curtain opened.");
}

void handleClose() {
  Log("Curtain Closed");
  myServo.attach(SERVO_PIN, SERVOMIN, SERVOMAX);
  isMoving = true;
  moveServoSmooth(constrain(CLOSED_ANGLE, 0, 180));
  Log("Moved to " + String(CLOSED_ANGLE) + " degrees (" + String(currentPulse) + " µs)");
  delay(1000);
  myServo.detach();
  isMoving = false;
  isOpen = false;
  server.send(200, "text/plain", "Curtain closed.");
}

void handleRestart() {
  Log("Restarting Server...");

  rtcData.magic = 0xCAFEBABE;
  rtcData.lastWasOpen = isOpen; // ← Track actual curtain state
  system_rtc_mem_write(RTC_MEM_START, &rtcData, sizeof(rtcData));

  server.send(200, "text/plain", "Restarting ESP...");
  delay(500);
  ESP.restart();
}

void RunStartupMovement() {
  Serial.println("Initialising...");  // Display init message to serial

  Log("Attaching servo...");
  myServo.attach(SERVO_PIN, SERVOMIN, SERVOMAX);
  isMoving = true;

  currentPulse = angleToMicros(constrain(CLOSED_ANGLE, 0, 180));
  myServo.writeMicroseconds(currentPulse);
  Log("Moved to " + String(CLOSED_ANGLE) + " degrees (" + String(currentPulse) + " µs)");
  delay(1000);

  moveServoSmooth(constrain(OPEN_ANGLE, 0, 180));
  Log("Moved to " + String(OPEN_ANGLE) + " degrees (" + String(currentPulse) + " µs)");
  delay(1000);

  isOpen = true;
  isMoving = false;
  myServo.detach();
  Log("PWM detached");
}


//Converts from Angle to Microseconds
int angleToMicros(int angle) {
  return map(angle, 0, 180, SERVOMIN, SERVOMAX);
}

void loop() {
  MDNS.update();
  server.handleClient();

  CheckWiFi();

  unsigned long now = millis();
  if (now - lastAlive >= 20UL * 60UL * 1000UL) {  // 20 minutes
    Log("[DEBUG] Keep-alive blink");
    digitalWrite(LED_PIN, LOW);   // ON
    delay(100);                   // short blink
    digitalWrite(LED_PIN, HIGH);  // OFF
    lastAlive = now;
  }
}

/* --------------------------------------------------------------  
* Window exhaust fan door control
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

//Stores the restart and servo open/closed state to memory
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
int currentPulse; //to Track the servo's last position in µs
//**************ADJUST THESE SETTINGS FOR YOUR SETUP **************

// -= WIFI SETTINGS =-

const char* ssid = "YOUR_BANK";
const char* password = "DETAILS_HERE";

// -= SERVO SETTINGS =-

#define SERVO_PIN 13           // D7 GPIO13 — safe for servo PWM
#define LED_PIN LED_BUILTIN    // GPIO2 — onboard blue LED

int SERVOMIN = 600;  // Min pulse width in µs. DO NOT modify unless calibrating manually.
int SERVOMAX = 2400; // Max pulse width in µs. See GitHub readme for safe tuning instructions.
int CLOSED_ANGLE = 0; // Angle of the Servo when door is closed
int OPEN_ANGLE = 110; // Angle of the Servo when the door is open, adjust as needed
constexpr bool OPEN_ON_RUN = true; // Have the Servo open the door on power (this won't re-run on manual software reset)

bool DIRECTION_REVERSED = true; // switch from true to false if your Open and Closed directions are switched

// Tuning values for smooth motion
// How to calculate servo speed:
// The total time of movement is (Number of Steps) * (Delay per Step).
//   - Number of Steps = (Total servo travel in µs) / STEP_US.
//   - Delay per Step = STEP_DELAY in milliseconds.
//
// Example calculation for this setup (110 degrees = approx. 1100µs of travel):
//   - Fast (original): (1100µs / 50µs) * 10ms = 22 steps * 10ms = 220ms (0.22 seconds)
//   - Slow & Smooth:   (1100µs / 10µs) * 20ms = 110 steps * 20ms = 2200ms (2.2 seconds)
//
#define STEP_US 10          // Smaller number = MORE steps = SMOOTHER motion.
#define STEP_DELAY 20       // Larger number = MORE delay per step = SLOWER motion.

// -= SERVER SETTINGS =-

IPAddress local_IP(192, 168, 1, 19); // Change this to the IP address you want to use for the server
IPAddress gateway(192, 168, 1, 1); // Your router's gateway
IPAddress subnet(255, 255, 255, 0); 
IPAddress dns(8, 8, 8, 8); // Optional, Google DNS
constexpr bool USE_DNS = false; // true : On | false : Off

//*****************************************************************

ESP8266WebServer server(80);
Servo myServo;

void setup() {
  //Sanity delay
  delay(1000);
  
  // Switch servoe direction values if true
  if (DIRECTION_REVERSED) {

    int TEMP_ANGLE = CLOSED_ANGLE;
    CLOSED_ANGLE = OPEN_ANGLE;
    OPEN_ANGLE = TEMP_ANGLE;
    
  }

  //clear the log buffer to ensure a clean slate on every boot.
  logBuffer = ""; 

  Serial.begin(115200);
  currentPulse = angleToMicros(CLOSED_ANGLE); 
//reads the system memory states
  system_rtc_mem_read(RTC_MEM_START, &rtcData, sizeof(rtcData));
  
  if (rtcData.magic == 0xCAFEBABE) {
    skipStartupMovement = true;
    isOpen = rtcData.lastWasOpen;
    Log("User initiated restart. Skipping startup movement.");
    Log("door state restored: " + String(isOpen ? "OPEN" : "CLOSED"));
  
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

  // Confirm required files exist ///Just change this to the files you know are required for functionality
  if (!LittleFS.exists("/index.html")) Log("❌ Missing: index.html");


  // File routes // Don't forget to list any additional files you put in the data folder here.
  server.on("/", handleRoot);
  server.serveStatic("/index.html", LittleFS, "/index.html");
  server.serveStatic("/favicon-32x32.png", LittleFS, "/favicon-32x32.png");
  server.serveStatic("/favicon-16x16.png", LittleFS, "/favicon-16x16.png");
  server.serveStatic("/apple-touch-icon.png", LittleFS, "/apple-touch-icon.png");
  server.serveStatic("/site.webmanifest", LittleFS, "/site.webmanifest");
  server.serveStatic("/android-chrome-512x512.png", LittleFS, "/android-chrome-512x512.png");
  server.serveStatic("/android-chrome-192x192.png", LittleFS, "/android-chrome-192x192.png");  


  // Action routes
  server.on("/door-status", []() {
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
  if (logBuffer.length() == 0) {
    server.send(200, "text/plain", " "); // Send a single space if buffer is empty
  } else {
    server.send(200, "text/plain", logBuffer); // Otherwise, send the buffer
  }
  });
  server.begin();
  Log("HTTP server started");

  // Startup movement only runs if the server is not hard reset
  if (!skipStartupMovement) {
      if (OPEN_ON_RUN == true) { // Check the user defined option
        RunStartupMovement();
    }
  } else {
    Log("User initiated server restart");  
  }
  Log("Door state at startup: " + String(isOpen ? "OPEN" : "CLOSED"));
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
  Log("Request: Open Door");
  if (isOpen) {
    Log("Door is already open.");
    server.send(200, "text/plain", "Door already open.");
    return;
  }
  moveServoSmoothly(OPEN_ANGLE);
  isOpen = true;
  server.send(200, "text/plain", "Door Open.");
}

void handleClose() {
  Log("Request: Close Door");
  if (!isOpen) {
    Log("Door is already closed.");
    server.send(200, "text/plain", "Door already closed.");
    return;
  }
  moveServoSmoothly(CLOSED_ANGLE);
  isOpen = false;
  server.send(200, "text/plain", "Door Closed.");
}

void handleRestart() {
  Log("Restarting Server...");

  rtcData.magic = 0xCAFEBABE;
  rtcData.lastWasOpen = isOpen; // ← Track actual door state
  system_rtc_mem_write(RTC_MEM_START, &rtcData, sizeof(rtcData));

  server.send(200, "text/plain", "Restarting ESP...");
  delay(500);
  ESP.restart();
}

void moveServoSmoothly(int targetAngle) {
  myServo.attach(SERVO_PIN);
  isMoving = true;

  int targetPulse = angleToMicros(targetAngle);

  Log("Moving servo to " + String(targetAngle) + " degrees...");

  if (targetPulse > currentPulse) {
    // Moving from a smaller pulse to a larger one
    for (int p = currentPulse; p <= targetPulse; p += STEP_US) {
      myServo.writeMicroseconds(p);
      delay(STEP_DELAY);
      server.handleClient(); // keeps web server responsive
      yield(); // prevents Wifi dropping out due to code blocking.
    }
  } else {
    // Moving from a larger pulse to a smaller one
    for (int p = currentPulse; p >= targetPulse; p -= STEP_US) {
      myServo.writeMicroseconds(p);
      delay(STEP_DELAY);
      server.handleClient(); 
      yield(); 
    }
  }

  // Ensure it finishes at the exact target pulse
  myServo.writeMicroseconds(targetPulse);
  currentPulse = targetPulse; // IMPORTANT: Update the current position

  Log("Move complete. Position: " + String(currentPulse) + " µs");

  delay(500); // Let the servo settle before detaching
  myServo.detach();
  isMoving = false;
}

void RunStartupMovement() {
  Log("Running servo startup routine...");

  // Assume the door might be open and home it to the closed position
  currentPulse = angleToMicros(OPEN_ANGLE); 
  moveServoSmoothly(CLOSED_ANGLE);
  delay(1000); // Pause for a second

  if (OPEN_ON_RUN) {
    Log("Moving to startup open position...");
    moveServoSmoothly(OPEN_ANGLE);
    isOpen = true;
  } else {
    isOpen = false;
  }
  Log("Startup routine complete.");
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

  delay(1); 

}

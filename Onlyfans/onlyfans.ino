/* --------------------2025 http://DisasterOfPuppets.com ------------------------------------------

* Window exhaust fan door control
* This shoddily put together code creates a server on an ESP8266 via Http
* that communicates with a Servo and toggles it between defined angles
* Using ESP8266 NodeMCU ESP-12

*

* DISCLAIMER : THIS CODE IS FREE TO BE SHARED FOR PERSONAL USE.
* I ACCEPT NO LIABILITY FOR ANY DAMAGE DIRECT OR INDIRECTLY CAUSED BY USING THIS CODE.
* IF YOU DO DUMB, YOU GET DUMB
* ADDITONAL WARNING!!!!!!!! SET YOUR SERVO AND POTENTIOMETER VALUES BEFORE PHYSICALLY ATTACHING YOUR SERVO TO THE DOOR SHAFT..
* FAILURE TO DO SO MAY DAMAGE YOUR COMPONENTS, YOU HAVE BEEN WARNED.
* DO NOT SET OPEN_ON_RUN TO TRUE BEFORE CONFIG STEPS HAVE BEEN COMPLETED
---------------------------------------------------------------------------------------------------*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <Servo.h>
#include <user_interface.h>


//PIN SETTINGS
#define POT_PIN A0
#define SERVO_PIN 13           // D7 GPIO13 — safe for servo PWM
#define LED_PIN LED_BUILTIN    // GPIO2 — onboard blue LED

#define LOG_BUFFER_SIZE 2048

//Stores the restart and servo open/closed state to memory
struct RTCData {
  uint32_t magic;
  bool lastWasOpen;
  bool developerMode;
};

RTCData rtcData;
uint32_t RTC_MEM_START = 65;

String logBuffer = "";
bool isOpen = false;
bool isMoving = false;
unsigned long lastAlive = 0;
int currentPulse;
int bootPot;


//**************ADJUST THESE SETTINGS FOR YOUR SETUP **************

constexpr bool OPEN_ON_RUN = true; // DON'T SET TO TRUE BEFORE CALIBRATION STEPS
//Have the Servo open door on power on (ignored on manual software reset).

bool developerMode = true; // Developer mode off by default: set to True for more detailed Serial logging
//this can be toggled in the serial using devOn or DevOff or change the above value

// -= WIFI SETTINGS =-

const char* ssid = "YOUR_BANK";
const char* password = "DETAILS_HERE";

// WARNING!!!!!!!! SET YOUR SERVO AND POTENTIOMETER VALUES BEFORE PHYSICALLY ATTACHING YOUR SERVO TO THE DOOR SHAFT..
// FAILURE TO DO SO MAY DAMAGE YOUR STUFF, YOU HAVE BEEN WARNED.

// CONFIG STEP 1. ****************************** SET SERVO MIN AND MAX VALUES ******************************
// Obtain these according to your servo specifications, ask Dr Google

int SERVOMIN = 500;  // Min pulse width in µs.
int SERVOMAX = 2500; // Max pulse width in µs.


// CONFIG STEP 2. ****************************** USE THE TERMINAL TO TEST AND CONFIRM YOUR POTENTIOMETER VALUES WHEN DOOR IS OPEN AND CLOSED ******************************
// In the terminal, enter r to read the current potentiometer position, enter an angle to test eg a90 sets the servo to 90 Degrees and provides the equivalent reading between 0 - 1024
// Don't worry if your values seem reversed vs mine, this can happen depending on wiring and which way you face your potentiometer.
// Update the below with your variables and reupload the code. (do not attach servo to door yet)

int POT_CLOSED = 1024;         // Values obtained from Calibration  1024 maps to 180 Degrees for your Servo (Closed)  <--- CORRECTED COMMENT
int POT_OPEN = 768;            // Values obtained from Calibration  745 maps to 70 Degrees for your Servo (Open)
int POSITION_TOLERANCE = 5;   // Allowable pot reading error (leave this as it or set to 0)

// CONFIG  STEP 3. ****************************** USE THE TERMINAL TO TEST AND CONFIRM YOUR STEP_US AND STEP_DELAY VALUES ******************************
// YOU SHOULD NOW HAVE SET YOUR SERVOMIN, SERVOMAX, POT_CLOSED, POT_OPEN, AND POSITION_TOLERANCE
// Make sure you have uploaded the code again with the new values saved.
// You can set the values for SERVO_US and SERVO_DELAY which are used to slow movement of the servo.
// if you do not wish to use these values set the below variable to FALSE;
bool USE_SMOOTHING = true;

// Tuning values for smooth motion with potentiometer feedback
// Movement time varies based on distance between current and target potentiometer readings.
//
// STEP_US: Microsecond step size for servo pulse width
//   - Smaller values = MORE steps = SMOOTHER motion
//   - Range: 5-20µs (10µs is good balance)
//
// STEP_DELAY: Delay between each step in milliseconds
//   - Larger values = SLOWER motion = MORE torque
//   - Range: 10-50ms (20ms prevents servo stall)
//
// Example: Moving 500 pot units with below defaul settings: 10 / 20
//   - Estimated steps: ~110 (varies by servo position)
//   - Movement time: ~110 steps × 20ms = ~2.2 seconds
//
//
int STEP_US = 10;       // Smaller number = MORE steps = SMOOTHER motion.
int STEP_DELAY = 20;       // Larger number = MORE delay per step = SLOWER motion.

// -= SERVER SETTINGS =-

IPAddress local_IP(192, 168, 1, 19); // Change this to the IP address you want to use for the server
IPAddress gateway(192, 168, 1, 1); // Your router's gateway
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8); // Optional, Google DNS
constexpr bool USE_DNS = false; // true : On | false : Off

ESP8266WebServer server(80);
Servo myServo;

int angleToMicros(int angle) {
  // This maps 0 degrees to SERVOMIN and 180 degrees to SERVOMAX, which is a standard servo mapping.
  return map(angle, 0, 180, SERVOMIN, SERVOMAX);
}

void setup() {
  //Sanity delay
  delay(2000);

  //clear the log buffer to ensure a clean slate on every boot.
  logBuffer = "";


  Serial.begin(115200);
  delay(10);

  bootPot = analogRead(POT_PIN);
  Log("Initial boot pot reading: " + String(bootPot));

  // Correct currentPulse initialization based on your inverted pot-angle mapping
  // POT_OPEN (745) maps to 70 degrees (angleToMicros(70))
  // POT_CLOSED (1024) maps to 180 degrees (SERVOMAX)
  currentPulse = map(bootPot, POT_OPEN, POT_CLOSED, angleToMicros(70), SERVOMAX);
  currentPulse = constrain(currentPulse, SERVOMIN, SERVOMAX);
  Log("Initial currentPulse based on boot pot: " + String(currentPulse) + " µs");


//reads the system memory states
  system_rtc_mem_read(RTC_MEM_START, &rtcData, sizeof(rtcData));

  if (rtcData.magic == 0xCAFEBABE) {
      isOpen = rtcData.lastWasOpen;
      developerMode = rtcData.developerMode;
      Log("Developer mode restored: " + String(developerMode ? "ON" : "OFF"));
      Log("User initiated restart. Skipping startup movement.");
      Log("Door state restored: " + String(isOpen ? "OPEN" : "CLOSED"));

      rtcData.magic = 0; // Clear magic to indicate handled restart
      system_rtc_mem_write(RTC_MEM_START, &rtcData, sizeof(rtcData));
  } else {
    // Initial boot state determination for fresh power-on (only if not a software restart)
    if (abs(bootPot - POT_CLOSED) <= POSITION_TOLERANCE) {
      isOpen = false; // Physically closed
      Log("Boot state: CLOSED (based on pot reading " + String(bootPot) + ")");
    } else if (abs(bootPot - POT_OPEN) <= POSITION_TOLERANCE) {
      isOpen = true; // Physically open
      Log("Boot state: OPEN (based on pot reading " + String(bootPot) + ")");
    } else {
      isOpen = false; // Default to closed if unclear on power up
      Log("Boot state: UNKNOWN - defaulting to CLOSED (pot: " + String(bootPot) + ")");
    }
  }


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
    yield();
  }

  Log("\nWi-Fi connected!");
  Log("IP address: ");
  Log(WiFi.localIP().toString());
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

  // Confirm required files exist //Just change this to the files you know are required for functionality
  if (!LittleFS.exists("/index.html")) Log("❌ Missing: index.html");

  // File routes for server files // Don't forget to list any additional files you wish put in the data folder.
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

  RunStartupMovement();

  Log("Door state at startup: " + String(isOpen ? "OPEN" : "CLOSED"));


// Commands for the serial (calibration)

  server.on("/command", []() {
    String cmd = server.arg("cmd");
    cmd.trim();

// a Action
    if (cmd.startsWith("a")) {
      int angle = cmd.substring(1).toInt();
      if (angle >= 0 && angle <= 180) { // Keep angle input range 0-180 for flexibility
        Log("> " + cmd);
        server.send(200, "text/plain", "OK");
        // Correct targetPot mapping for `a<angle>` command
        // Map user's desired angle (70-180) to the corresponding pot readings (POT_OPEN-POT_CLOSED)
        int targetPot = map(angle, 70, 180, POT_OPEN, POT_CLOSED);
        // Constrain to the actual operational pot range for safety
        targetPot = constrain(targetPot, POT_OPEN, POT_CLOSED);
        Log("Calculated target pot for angle " + String(angle) + "°: " + String(targetPot));
        if (USE_SMOOTHING) {
          moveServoToPotPositionSmooth(targetPot);
        }
        else {
          moveServoToPotPositionNoSmoothing(targetPot);
        }

        int potValue = analogRead(POT_PIN);
        Log("Pot reading after move: " + String(potValue));
        return;
      } else {
        Log("> " + cmd);
        Log("Error: Angle must be 0-180");
      }
    }

// r Action (read values)
    else if (cmd == "r") {
      int currentPot = analogRead(POT_PIN);
      // Correct mappedAngle for 'r' command logging
      // Map current pot reading to your defined angle range (70-180)
      int mappedAngle = map(currentPot, POT_OPEN, POT_CLOSED, 70, 180);
      mappedAngle = constrain(mappedAngle, 70, 180); // Constrain to your operational angle range
      Log("> " + cmd);
      Log("Current pot: " + String(currentPot));
      Log("Equivalent angle: " + String(mappedAngle) + "°");

      if (!USE_SMOOTHING) {
        Log("USE_SMOOTHING is False - servo moves at normal rate");
      }
      else {
        Log("STEP_US: " + String(STEP_US));
        Log("STEP_DELAY: " + String(STEP_DELAY));
      }

    }


// devOn Action
    else if (cmd == "devOn") {
      developerMode = true;
      Log("> " + cmd);
      Log("Developer mode enabled - verbose logging active");
    }

// devOff Action
    else if (cmd == "devOff") {
      developerMode = false;
      Log("> + " + cmd);
      Log("Developer mode disabled - basic logging only");
    }

// h Action (help menu)
    else if (cmd == "h") {
      Log("> " + cmd);
      instructions();
    }

//i Action (STEP_US and STEP_DELAY Instructions)
    else if (cmd == "i") {
      stepinstructions();
    }

// m Action (Move server to angle regardless of Pot reading value)
    else if (cmd.startsWith("m")) {
      int targetAngle = cmd.substring(1).toInt();
      if (targetAngle >= 0 && targetAngle <=180) {
        Log("> " + cmd);
        server.send(200, "text/plain", "OK");
        if (USE_SMOOTHING) {
          moveServoToAngleNoPotSmooth(targetAngle);
          Log("Servo moved to " + String(targetAngle));
          return;
        }
        else {
          moveServoToAngleNoPotNoSmoothing(targetAngle);
          Log("Servo moved to " + String(targetAngle));
          return;
        }
      }
      else {
        Log("> " + cmd);
        Log("Error: Value must be between 0-180");
      }
    }


// s Action (STEP_US value)
    else if (cmd.startsWith("s")) {
      int value = cmd.substring(1).toInt();
      if (value >= 5 && value <= 20) {
        Log("> " + cmd);
        server.send(200, "text/plain", "OK");
        STEP_US = value;
        Log("New STEP_US value set to " + String(STEP_US));
        return;
      }
      else {
        Log("> " + cmd);
        Log("Error: Value must be between 5-20");
      }
    }

// d Action (STEP_DELAY value)
    else if (cmd.startsWith("d")) {
      int value = cmd.substring(1).toInt();
      if (value >= 10 && value <= 50) {
        Log("> " + cmd);
        server.send(200, "text/plain", "OK");
        STEP_DELAY = value;
        Log("New STEP_DELAY value set to " + String(STEP_DELAY));
        return;
      }
      else {
        Log("> " + cmd);
        Log("Error: Value must be between 10-50");
      }
    }

// cls Action (clear log)
    else if (cmd == "cls") {
      logBuffer = "";
      Log("> " + cmd);
      Log("Log cleared");
    }
//other command catch
    else {
      Log("> " + cmd);
      Log("Unknown command. Type 'h' for help.");
    }

    server.send(200, "text/plain", "OK");
  });
}
//********************************* SETUP END *********************************************

//****************** FUNCTIONS **********************

// Always shows (WiFi, door status, errors)
void Log(const String& msg) {
  Serial.println(msg);
  logBuffer += msg + "\n";
  if (logBuffer.length() > LOG_BUFFER_SIZE) {
    logBuffer = logBuffer.substring(logBuffer.length() - LOG_BUFFER_SIZE);
  }
}

// Only shows in developer mode (verbose debugging)
void DevLog(const String& msg) {
  if (developerMode) {
    Log(msg);
  }
}

void CheckWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Log("[WiFi] Disconnected. Attempting reconnect...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      digitalWrite(LED_PIN, LOW);
      delay(250);
      digitalWrite(LED_PIN, HIGH);
      delay(250);
      yield();  // ← Let WiFi stack try to reconnect
    }

    Log("\nWi-Fi Reconnected!");
    Log("IP address: ");
    Log(WiFi.localIP().toString());
    digitalWrite(LED_PIN, HIGH); // OFF
  }
}

void stepinstructions(){

  Log("Tuning values for smooth motion with potentiometer feedback");
  Log("Movement time varies based on distance between current and target potentiometer readings.");
  Log("");
  Log("STEP_US: Microsecond step size for servo pulse width");
  Log("- Smaller values = MORE steps = SMOOTHER motion");
  Log("- Range: 5-20µs (10µs is good balance)");
  Log("");
  Log("STEP_DELAY: Delay between each step in milliseconds  ");
  Log("- Larger values = SLOWER motion = MORE torque");
  Log("- Range: 10-50ms (20ms prevents servo stall)");
  Log("");
  Log("Example: Moving 500 pot units with below defaul settings: 10 / 20");
  Log("- Estimated steps: ~110 (varies by servo position)");
  Log("- Movement time: ~110 steps × 20ms = ~2.2 seconds");
  Log("");
}

// Calibration instructions
void instructions (){

  Log("");
  Log("*************************************");
  Log("***   OnlyFans Exhaust Fan Help   ***");
  Log("*************************************");
  Log("");
  Log(" Command             Function");
  Log("=========           ==========");
  Log("");
  Log("h                    Show this help menu");
  Log("cls                  Clears the Log");
  Log("a<angle>             Move servo to angle (0-180) e.g a90");
  Log("r                    Displays current potentiometer, STEP_US and STEP_DELAY, values");
  Log("s<value>             Set a new STEP_US value");
  Log("d<value>             Set a new STEP_DELAY value");
  Log("i                    Displays the STEP_US / STEP_DELAY help");
  Log("");
  Log("====== Servo / Pot Calibration ======");
  Log("1. Move angle to closed position using a<angle> note pot value");
  Log("2. Move to open position and note pot value");
  Log("Enter these new values in your code and re-upload");
  Log("4. OPTIONAL Slow the servo movement (USE_SMOOTHING must be set to true)");
  Log("");
  Log("Change the STEP_US value by entering s<number> between 5 and 20");
  Log("Change the STEP_DELAY value by entering d<number> between 10 - 50");
  Log("Make sure to update the STEP values in your main code and re-upload.");
  Log("-------------------------------------");
  Log("");
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
  // Double check physical state before moving to prevent unnecessary action
  if (isOpen && abs(analogRead(POT_PIN) - POT_OPEN) <= POSITION_TOLERANCE) {
    Log("Door is already open.");
    server.send(200, "text/plain", "Door already open.");
    return;
  }
  server.send(200, "text/plain", "Door Opening...");
  if (USE_SMOOTHING) {
    moveServoToPotPositionSmooth(POT_OPEN);
  }
  else {
    moveServoToPotPositionNoSmoothing(POT_OPEN);
  }
  // isOpen is set by moveServoToPotPositionSmooth/NoSmoothing after movement
}

void handleClose() {
  Log("Request: Close Door");
  // Double check physical state before moving to prevent unnecessary action
  if (!isOpen && abs(analogRead(POT_PIN) - POT_CLOSED) <= POSITION_TOLERANCE) {
    Log("Door is already closed.");
    server.send(200, "text/plain", "Door already closed.");
    return;
  }
  server.send(200, "text/plain", "Door Closing...");
  if (USE_SMOOTHING) {
    moveServoToPotPositionSmooth(POT_CLOSED);
  }
  else {
    moveServoToPotPositionNoSmoothing(POT_CLOSED);
  }
  // isOpen is set by moveServoToPotPositionSmooth/NoSmoothing after movement
}

void handleRestart() {
  Log("Restarting Server...");

  rtcData.magic = 0xCAFEBABE;
  rtcData.lastWasOpen = isOpen; // ← Track actual door state
  rtcData.developerMode = developerMode;
  system_rtc_mem_write(RTC_MEM_START, &rtcData, sizeof(rtcData));

  server.send(200, "text/plain", "Restarting ESP...");
  Log("Restarting...Please wait.......");
  delay(500);
  ESP.restart();
}


// Used when USE_SMOOTHING is True
void moveServoToPotPositionSmooth(int targetPotPos) {
  Log("Attempting to move servo to target pot: " + String(targetPotPos));

  // Correct mappedAngle for logging
  // Map target pot reading to your defined angle range (70-180) for display
  int mappedAngle = map(targetPotPos, POT_OPEN, POT_CLOSED, 70, 180);
  mappedAngle = constrain(mappedAngle, 70, 180); // Constrain to your operational angle range
  Log("Equivalent target angle: " + String(mappedAngle) + "°");

  // Read current physical potentiometer position
  int currentPotReading = analogRead(POT_PIN);

  // Check if the door is already near the target position to avoid unnecessary movement
  if (abs(currentPotReading - targetPotPos) <= POSITION_TOLERANCE) {
    Log("Door already near target pot position: " + String(currentPotReading));
    // Ensure isOpen flag is updated if movement was skipped but state might have changed
    if (abs(currentPotReading - POT_OPEN) <= POSITION_TOLERANCE) {
        isOpen = true;
    } else if (abs(currentPotReading - POT_CLOSED) <= POSITION_TOLERANCE) {
        isOpen = false;
    }
    return;
  }

  myServo.attach(SERVO_PIN);
  isMoving = true; // Indicate that the servo is in motion
  DevLog("Servo attached to pin " + String(SERVO_PIN));

  // Correct targetPulse calculation based on your inverted pot-angle mapping
  // If targetPotPos is POT_CLOSED (1024), targetPulse should be SERVOMAX (180 degrees).
  // If targetPotPos is POT_OPEN (745), targetPulse should be angleToMicros(70) (70 degrees).
  int targetPulse = map(targetPotPos, POT_OPEN, POT_CLOSED, angleToMicros(70), SERVOMAX);
  targetPulse = constrain(targetPulse, SERVOMIN, SERVOMAX); // Ensure target pulse is within defined limits
  DevLog("Target pulse: " + String(targetPulse) + " µs (from target pot: " + String(targetPotPos) + ")");
  DevLog("Starting currentPulse: " + String(currentPulse) + " µs");

  unsigned long moveStart = millis(); // Timestamp for overall movement timeout
  int lastPotReadingForStall = currentPotReading; // Keep track of pot reading for stall detection
  unsigned long lastMovementActivity = millis(); // Timestamp for last significant pot change

  // Move servo incrementally towards the target pulse width
  // The loop continues until the commanded currentPulse is very close to the targetPulse
  while (abs(currentPulse - targetPulse) > (STEP_US / 2)) {
    // Determine the direction to adjust the servo's commanded pulse width (currentPulse)
    if (currentPulse < targetPulse) {
      currentPulse += STEP_US;
      if (currentPulse > targetPulse) currentPulse = targetPulse; // Prevent overshoot
    } else if (currentPulse > targetPulse) {
      currentPulse -= STEP_US;
      if (currentPulse < targetPulse) currentPulse = targetPulse; // Prevent overshoot
    }

    currentPulse = constrain(currentPulse, SERVOMIN, SERVOMAX); // Keep pulse within servo's operational limits
    myServo.writeMicroseconds(currentPulse); // Send the updated pulse command to the servo

    delay(STEP_DELAY); // Pause for a short duration to smooth the movement
    server.handleClient(); // Allow the web server to process requests during movement
    yield(); // Allow ESP8266 background tasks to run

    // Update current physical pot reading for stall detection
    currentPotReading = analogRead(POT_PIN);

    // Stall detection: check if the potentiometer reading has changed significantly
    if (abs(currentPotReading - lastPotReadingForStall) > POSITION_TOLERANCE) {
        lastPotReadingForStall = currentPotReading;
        lastMovementActivity = millis(); // Reset activity timer if movement detected
    }

    // If no significant physical movement has occurred for a while, assume a stall
    if (millis() - lastMovementActivity > 3000) { // 3 seconds of no pot change
        DevLog("Movement stalled. Current pot: " + String(currentPotReading));
        break; // Exit loop
    }

    // Overall movement timeout to prevent endless loops
    if (millis() - moveStart > 10000) { // 10-second timeout
        Log("Movement timed out after " + String(millis() - moveStart) + "ms. Current pot: " + String(currentPotReading));
        break; // Exit loop
    }
  }

  // After the loop, ensure the servo is sent the exact final target pulse command
  myServo.writeMicroseconds(targetPulse);
  currentPulse = targetPulse; // Update global currentPulse to reflect the final commanded position
  DevLog("Final servo pulse command: " + String(currentPulse) + " µs");

  delay(200); // Small delay to allow the servo to physically settle at its final position

  myServo.detach(); // Detach the servo to save power and prevent jitter/buzzing
  isMoving = false; // Indicate that the servo has stopped moving
  DevLog("Servo detached.");

  // Final check of the physical door state based on potentiometer reading
  currentPotReading = analogRead(POT_PIN);
  if (abs(currentPotReading - POT_OPEN) <= POSITION_TOLERANCE) {
    isOpen = true;
    Log("Door successfully moved to OPEN (pot: " + String(currentPotReading) + ")");
  } else if (abs(currentPotReading - POT_CLOSED) <= POSITION_TOLERANCE) {
    isOpen = false;
    Log("Door successfully moved to CLOSED (pot: " + String(currentPotReading) + ")");
  } else {
    Log("Door moved to intermediate or unexpected position (pot: " + String(currentPotReading) + ")");
  }
}

void RunStartupMovement() {
  Log("Running servo startup routine with potentiometer feedback...");

  // Step 1: Read actual door position
  int currentPot = analogRead(POT_PIN);
  Log("Current pot reading: " + String(currentPot));

  // Only perform startup movement if not a software restart or if OPEN_ON_RUN is true
  if (rtcData.magic != 0xCAFEBABE || OPEN_ON_RUN) {
    if (OPEN_ON_RUN) {
      Log("OPEN_ON_RUN is true. Moving to open position...");
      // Always use smooth movement during startup for safety and speed adherence
      moveServoToPotPositionSmooth(POT_OPEN);
      delay(1000); // Allow time for movement and settling
      // isOpen is updated by moveServoToPotPositionSmooth()
    } else {
      Log("OPEN_ON_RUN is false. Door returning to closed.");
      // Always use smooth movement during startup for safety and speed adherence
      moveServoToPotPositionSmooth(POT_CLOSED);
      delay(1000); // Allow time for movement and settling
      // isOpen is updated by moveServoToPotPositionSmooth()
    }
  } else {
    Log("Software restart detected and OPEN_ON_RUN is false. Skipping startup movement.");
  }
  Log("Startup routine complete. Final state: " + String(isOpen ? "OPEN" : "CLOSED"));
}

// Used when USE_SMOOTHING is false
void moveServoToPotPositionNoSmoothing(int targetPotPos) {
  Log("Moving servo directly to: " + String(targetPotPos));

  // Correct mappedAngle for logging
  int mappedAngle = map(targetPotPos, POT_OPEN, POT_CLOSED, 70, 180);
  mappedAngle = constrain(mappedAngle, 70, 180); // Constrain to your operational angle range
  Log("Equivalent target angle: " + String(mappedAngle) + "°");

  // Read current physical potentiometer position
  int currentPotReading = analogRead(POT_PIN);

  // Check if already at target
  if (abs(currentPotReading - targetPotPos) <= POSITION_TOLERANCE) {
    Log("Door already at target position: " + String(currentPotReading));
    if (abs(currentPotReading - POT_OPEN) <= POSITION_TOLERANCE) {
        isOpen = true;
    } else if (abs(currentPotReading - POT_CLOSED) <= POSITION_TOLERANCE) {
        isOpen = false;
    }
    return;
  }

  myServo.attach(SERVO_PIN);
  isMoving = true;
  DevLog("Servo attached to pin " + String(SERVO_PIN));

  // Correct targetPulse calculation
  int targetPulse = map(targetPotPos, POT_OPEN, POT_CLOSED, angleToMicros(70), SERVOMAX);
  targetPulse = constrain(targetPulse, SERVOMIN, SERVOMAX);

  DevLog("Moving directly to pulse: " + String(targetPulse) + " µs");
  myServo.writeMicroseconds(targetPulse);
  currentPulse = targetPulse;

  // Wait for movement completion with timeout
  unsigned long moveStart = millis();
  while (millis() - moveStart < 5000) { // 5 second timeout
    server.handleClient();
    yield();
    delay(100);

    currentPotReading = analogRead(POT_PIN);
    if (abs(currentPotReading - targetPotPos) <= POSITION_TOLERANCE) {
      break; // Reached target
    }
  }

  delay(200); // Let servo settle
  myServo.detach();
  isMoving = false;
  DevLog("Servo detached.");

  // Final position check
  currentPotReading = analogRead(POT_PIN);
  if (abs(currentPotReading - POT_OPEN) <= POSITION_TOLERANCE) {
    isOpen = true;
    Log("Door moved to OPEN (pot: " + String(currentPotReading) + ")");
  } else if (abs(currentPotReading - POT_CLOSED) <= POSITION_TOLERANCE) {
    isOpen = false;
    Log("Door moved to CLOSED (pot: " + String(currentPotReading) + ")");
  } else {
    Log("Door at position (pot: " + String(currentPotReading) + ")");
  }
}

//calibration smoothly move servo to angle ignore potentiometer
void moveServoToAngleNoPotSmooth(int targetAngle) {
  targetAngle = constrain(targetAngle, 0, 180);
  Log("Moving servo to angle: " + String(targetAngle) + "°");

  int targetPulse = angleToMicros(targetAngle);
  DevLog("Target pulse: " + String(targetPulse) + " µs");
  DevLog("Starting currentPulse: " + String(currentPulse) + " µs");

  // Check if already at target
  if (abs(currentPulse - targetPulse) <= (STEP_US / 2)) {
    Log("Servo already at target angle");
    return;
  }

  myServo.attach(SERVO_PIN);
  isMoving = true;
  DevLog("Servo attached to pin " + String(SERVO_PIN));

  unsigned long moveStart = millis();

  // Move servo incrementally towards target pulse
  while (abs(currentPulse - targetPulse) > (STEP_US / 2)) {
    if (currentPulse < targetPulse) {
      currentPulse += STEP_US;
      if (currentPulse > targetPulse) currentPulse = targetPulse;
    } else if (currentPulse > targetPulse) {
      currentPulse -= STEP_US;
      if (currentPulse < targetPulse) currentPulse = targetPulse;
    }

    currentPulse = constrain(currentPulse, SERVOMIN, SERVOMAX);
    myServo.writeMicroseconds(currentPulse);

    delay(STEP_DELAY);
    server.handleClient();
    yield();

    // Timeout protection
    if (millis() - moveStart > 10000) {
      Log("Servo movement timed out");
      break;
    }
  }

  // Ensure final position
  myServo.writeMicroseconds(targetPulse);
  currentPulse = targetPulse;
  DevLog("Final servo pulse: " + String(currentPulse) + " µs");

  delay(200);
  myServo.detach();
  isMoving = false;
  DevLog("Servo detached");

  Log("Servo moved to " + String(targetAngle) + "° (" + String(targetPulse) + " µs)");
}

//calibration move servo to angle ignore potentiometer
void moveServoToAngleNoPotNoSmoothing(int targetAngle) {
  targetAngle = constrain(targetAngle, 0, 180);
  Log("Moving servo directly to angle: " + String(targetAngle) + "°");

  int targetPulse = angleToMicros(targetAngle);
  DevLog("Target pulse: " + String(targetPulse) + " µs");

  // Check if already at target
  if (abs(currentPulse - targetPulse) <= 10) {
    Log("Servo already at target angle");
    return;
  }

  myServo.attach(SERVO_PIN);
  isMoving = true;
  DevLog("Servo attached to pin " + String(SERVO_PIN));

  // Move directly to target
  myServo.writeMicroseconds(targetPulse);
  currentPulse = targetPulse;
  DevLog("Moving directly to pulse: " + String(targetPulse) + " µs");

  // Wait for movement completion
  unsigned long moveStart = millis();
  while (millis() - moveStart < 3000) { // 3 second timeout
    server.handleClient();
    yield();
    delay(100);
  }

  delay(200);
  myServo.detach();
  isMoving = false;
  DevLog("Servo detached");

  Log("Servo moved to " + String(targetAngle) + "° (" + String(targetPulse) + " µs)");
}


void loop() {
  MDNS.update();
  server.handleClient();
  CheckWiFi();

  // Update door state based on pot reading, ONLY IF THE SERVO IS NOT CURRENTLY MOVING
  // This prevents loop() from interfering with the isOpen state while a move command is active
  if (!isMoving) {
    int potReading = analogRead(POT_PIN);
    if (abs(potReading - POT_CLOSED) <= POSITION_TOLERANCE) {
      isOpen = false;
    } else if (abs(potReading - POT_OPEN) <= POSITION_TOLERANCE) {
      isOpen = true;
    }
  }

  unsigned long now = millis();
  if (now - lastAlive >= 20UL * 60UL * 1000UL) {
  Log("[DEBUG] Keep-alive blink");
  digitalWrite(LED_PIN, LOW);   // Brief ON
  delayMicroseconds(50000);      // 50ms, shorter
  digitalWrite(LED_PIN, HIGH);    // Back OFF
  lastAlive = now;
}

  delay(10);

}

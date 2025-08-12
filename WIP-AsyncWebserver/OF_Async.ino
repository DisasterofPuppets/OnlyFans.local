/* --------------------------------------------------------------  
WIP - Zapping bugs Don't forget to change the 'Erase Flash' setting to All Flash Contents' otherwise you will still see old files

To Do
Convert to allow HTML upload instead of using LittleFS upload function (doesn't work with new IDE, or at least I couldn't get it to)
MDNS - nah shes fucked. use hosts method instead
FETCH CURTAIN STATES / RTC - DONE
SERIAL MONITOR - DONE
SERVO
REMOVE SHADOW ON SVG LOGO (may be in the Style.css)


* Window exhaust fan curtain control
* This shoddily put together code creates a server on an ESP8266 via Http
* that communicates with a Servo and toggles it between defined angles
* Using ESP8266 NodeMCU ESP-12
* 2025 http://DisasterOfPuppets.com


Hardware list
1 x 1N5822 Schottky diode
1 x ESP8266 Microcontroller
1 x DC Buck Converter (12v to 6V)
2 x 1000 ohm Capacitors
1 x Servo (DS3240 is my recommendation as it has some good torque)
1 x 12V 3 Amp power source
1 x 5V Power source (or  simply power via USB connection to the ESP8266)
-----------------------------------------------------------------*/

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <Servo.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <user_interface.h>
#define LOG_BUFFER_SIZE 2048

// Structure stored in RTC memory to remember the curtain state across resets
struct RTCData {
  uint32_t magic;
  bool lastWasOpen;
};

RTCData rtcData;
uint32_t RTC_MEM_START = 65;

// Runtime flags and state
bool skipStartupMovement = false;
String logBuffer = "";
bool isOpen = false;
bool isMoving = false;
unsigned long lastAlive = 0;

int currentPulse = 0; // tracks last pulse sent to the servo

//**************ADJUST THESE SETTINGS FOR YOUR SETUP **************

// -= WIFI SETTINGS =-

// Wi-Fi credentials (replace with your network details)

const char* ssid = "YourBank";
const char* password = "DetailsHere";

// -= SERVO SETTINGS =-  (adjust to match your hardware)

#define SERVO_PIN 13           // D7 GPIO13 — safe for servo PWM
#define LED_PIN LED_BUILTIN    // GPIO2 — onboard blue LED
constexpr int SERVOMIN = 600;  // Min pulse width in µs. DO NOT modify unless calibrating manually.
constexpr int SERVOMAX = 2400; // Max pulse width in µs. See GitHub readme for safe tuning instructions.
constexpr int CLOSED_ANGLE = 0; // Angle of the Servo when curtain is closed (With our 3 gears turns the Door Anticlockwise)
constexpr int OPEN_ANGLE = 110; // Angle of the Servo when the curtain is open, adjust as needed (Gear turns Clockwise)
constexpr bool OPEN_ON_RUN = true; // Have the Servo open the curtain on power (this won't re-run on manual software reset)
//Servo with the horn mounted closer to the left side - 0 goes clockwise, 180 is anticlockwise

// Tuning values for smooth motion and slowing servo down.
#define SERVO_SPEED_DELAY 15       // [Was 2] Delay (in milliseconds) between each step. Higher = slower movement.
#define SERVO_STEP_SIZE 5          // [Was 40 wayy too fast] Microseconds per step. Smaller = smoother and slower movement.
// If your servo starts stuttering or twitching: try increasing SERVO_STEP_SIZE or reducing SERVO_SPEED_DELAY.
// Stuttering typically means the servo isn't receiving fast enough or large enough signal changes to move cleanly.


// -= SERVER SETTINGS =-  (network configuration for the HTTP server)

IPAddress local_IP(192, 168, 1, 19); // Change this to the IP address you want to use for the server
IPAddress gateway(192, 168, 1, 1); // Your router's gateway
IPAddress subnet(255, 255, 255, 0); 
IPAddress dns(8, 8, 8, 8); // Optional, Google DNS
constexpr bool USE_GOOGLE_DNS = false; // true : On | false : Off

//*****************************************************************


//****************** WEB SERVER & FILE UPLOAD FUNCTIONS **********************
// Create Server
static AsyncWebServer server(80);

// These use Serial print as they output and interact with the Serial monitor for file management

void listFiles() {
    Dir dir = LittleFS.openDir("/");
    while (dir.next()) {
        Serial.print("FILE: ");
        Serial.println(dir.fileName());
    }
}

void deleteFile(const char* path) {
    if (LittleFS.remove(path)) {
        Serial.printf("File %s deleted successfully\n", path);
    } else {
        Serial.printf("Failed to delete file %s\n", path);
    }
}

void formatFS() {
    if (LittleFS.format()) {
        Serial.println("Filesystem formatted successfully");
    } else {
        Serial.println("Failed to format filesystem");
    }
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

//handles the restart command typed into serial
void CMDRestart() {
  Log("Restarting Server");
  ESP.restart();
}

Servo myServo; //initialize Servo

//****************** HANDLER FUNCTIONS **********************

// Serve the main interface page
void handleRoot(AsyncWebServerRequest *request) {
  IPAddress clientIP = request->client()->remoteIP();
  Log("HTTP connection detected from: " + clientIP.toString());
  
  // Check if files have been uploaded
  if (LittleFS.exists("/index.html")) {
    // Serve the uploaded index.html
    request->send(LittleFS, "/index.html", "text/html");
  } else {
    // No files uploaded yet, serve the upload page
    request->send(200, "text/html",
    R"rawliteral(
    <!DOCTYPE html>
    <html>
      <head>
        <title>Upload Files</title>
        <style>
          body { font-family: Arial; padding: 20px; }
          #status { margin-top: 1em; color: green; }
          #error { margin-top: 1em; color: red; }
        </style>
      </head>
      <body>
        <h2>Upload Files</h2>
        <form id="uploadForm" enctype="multipart/form-data">
          <input type="file" id="fileInput" name="file" multiple>
          <input type="submit" value="Upload">
        </form>
        <div id="status"></div>
        <div id="error"></div>

        <script>
          const form = document.getElementById('uploadForm');
          const fileInput = document.getElementById('fileInput');
          const statusDiv = document.getElementById('status');
          const errorDiv = document.getElementById('error');

          form.addEventListener('submit', function(event) {
            event.preventDefault(); // prevent default form submission

            if (!fileInput.files.length) {
              errorDiv.textContent = "Please select files before uploading.";
              statusDiv.textContent = "";
              return;
            }

            errorDiv.textContent = "";
            statusDiv.textContent = "Please wait, uploading files...";

            const formData = new FormData();
            for (let i = 0; i < fileInput.files.length; i++) {
              formData.append("file", fileInput.files[i]);
            }

            const xhr = new XMLHttpRequest();
            xhr.open("POST", "/upload", true);

            xhr.onload = function() {
              if (xhr.status === 200) {
                statusDiv.innerHTML = `
                <img src="/restart" style="display:none" onerror=""/>
                <br>Restarting Server <BR><BR><a href="/">Click here to reload page and see your new index.html</a>.`;                 
                fileInput.value = ""; // clear selected files
              } else {
                errorDiv.textContent = "Upload failed. Try again.";
              }
            };

            xhr.onerror = function() {
              errorDiv.textContent = "Upload error. Check connection.";
            };
            
            fetch("/uploading"); // Inform ESP we're starting upload
            console.log("Uploading files..."); // Debug to browser console
            xhr.send(formData);
          });
        </script>
      </body>
    </html>
    )rawliteral");
  }
}

// Handles any restart requests from the server
void handleRestart(AsyncWebServerRequest *request) {
  
  rtcData.magic = 0xCAFEBABE;
  rtcData.lastWasOpen = isOpen; // ← Track actual curtain state
  system_rtc_mem_write(RTC_MEM_START, &rtcData, sizeof(rtcData));
  
  Log("Restarting Server...");
  request->send(200, "text/plain", "Restarting...");
  delay(500);
  ESP.restart();
}



// Move the curtain to the open position
void handleOpen(AsyncWebServerRequest *request) {
  Log("Curtain Open");
//  myServo.attach(SERVO_PIN, SERVOMIN, SERVOMAX);
//  isMoving = true;
//  moveServoSmooth(constrain(OPEN_ANGLE, 0, 180));
  Log("Moved to " + String(OPEN_ANGLE) + " degrees (" + String(currentPulse) + " µs)");
//  delay(1000);
//  myServo.detach();
  isMoving = false;
  isOpen = true;
  request->send(200, "text/plain", "Curtain opened.");
}

// Move the curtain to the closed position
void handleClose(AsyncWebServerRequest *request) {
  Log("Curtain Closed");
 // myServo.attach(SERVO_PIN, SERVOMIN, SERVOMAX);
  //isMoving = true;
  //moveServoSmooth(constrain(CLOSED_ANGLE, 0, 180));
  Log("Moved to " + String(CLOSED_ANGLE) + " degrees (" + String(currentPulse) + " µs)");
//  delay(1000);
//  myServo.detach();
  isMoving = false;
  isOpen = false;
  request->send(200, "text/html", "Curtain closed.");
}

// Initial servo homing cycle executed on power-up
void RunStartupMovement() {
  Serial.println("Initialising...");
  
  Log("Attaching servo...");
  myServo.attach(SERVO_PIN, SERVOMIN, SERVOMAX);
  isMoving = true;

  // Slowly home to closed position
  Log("Homing to CLOSED...");
  moveServoSmooth(constrain(CLOSED_ANGLE, 0, 180));
  Log("Homed to " + String(CLOSED_ANGLE) + " degrees (" + String(currentPulse) + " µs)");
  delay(500);

  // Then open smoothly
  moveServoSmooth(constrain(OPEN_ANGLE, 0, 180));
  Log("Moved to " + String(OPEN_ANGLE) + " degrees (" + String(currentPulse) + " µs)");
  delay(500);

  isOpen = true;
  isMoving = false;
  myServo.detach();
  Log("PWM detached");
}

//****************** RECONNECT WIFI FUNCTION **********************

// Reconnect if Wi-Fi drops
void CheckWifi() {
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
      Log("Wi-Fi Reconnected!");
      Log("IP address: " + WiFi.localIP().toString());
      digitalWrite(LED_PIN, HIGH);
    } else {
      Log("Wi-Fi Reconnect failed!");
    }
  }
}

// Helper to convert an angle (0-180°) into the corresponding pulse width
inline int angleToMicros(int angle) {
  return map(angle, 0, 180, SERVOMIN, SERVOMAX);
}

//================================================================= SETUP ================================================================

void setup() {

  Serial.begin(115200);
  delay(1000);  // Give USB serial time to come online
  Serial.println();
  Serial.println(F("=== Booting ESP8266 Curtain Controller ==="));


//Initialize pins
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // OFF (active LOW)

// Was the previous reset triggered by our restart handler?

  system_rtc_mem_read(RTC_MEM_START, &rtcData, sizeof(rtcData));
  if (rtcData.magic == 0xCAFEBABE) {
    skipStartupMovement = true;
    isOpen = rtcData.lastWasOpen;
    Log("User initiated restart. Skipping startup movement.");
    Log("Curtain state restored: " + String(isOpen ? "OPEN" : "CLOSED"));
    // ensure smooth moves start from the remembered position
    //currentPulse = angleToMicros(isOpen ? OPEN_ANGLE : CLOSED_ANGLE);
    rtcData.magic = 0;
    system_rtc_mem_write(RTC_MEM_START, &rtcData, sizeof(rtcData));
  }

// Initialize currentPulse based on current/restored state
currentPulse = angleToMicros(isOpen ? OPEN_ANGLE : CLOSED_ANGLE);

  Log("Booting...");


//Initialize WiFi

  if (USE_GOOGLE_DNS) {
    WiFi.config(local_IP, gateway, subnet, dns);
  } else {
    WiFi.config(local_IP, gateway, subnet);
  }
    
  WiFi.begin(ssid, password);
  Serial.println(F("[WiFi] Connecting to SSID: ") + String(ssid));
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
    delay(50);
    yield();
  }
  if (WiFi.status() == WL_CONNECTED) {
    Log("[WiFi] Connected. IP: " + WiFi.localIP().toString());
  } else {
    Log("[WiFi] Failed or timed out");
  }

  // Initialize mDNS
  if (MDNS.begin("onlyfans")) {
    Log("mDNS responder started");
    Log("Try visiting: http://onlyfans.local");
  } else {
    Log("mDNS setup failed");
  }


// Initialize File System
  bool fsMounted = LittleFS.begin();
  if (!fsMounted) {
    Log("[FS] LittleFS Mount Failed");
  } else {
    Log("[FS] LittleFS Mount Successful");
  }
        
  server.on("/uploading", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[UPLOAD] Upload started");
    request->send(200, "text/plain", "OK");
  });

  // Always use handleRoot for the main page
  server.on("/", HTTP_GET, handleRoot);

  // Handle file upload
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200); // Return blank success for AJAX to catch
  }, [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
      request->_tempFile = LittleFS.open("/" + filename, "w");
    }
    if (len) {
      request->_tempFile.write(data, len);
    }
    if (final) {
      request->_tempFile.close();
      LittleFS.open("/uploaded.flag", "w").close();
      Serial.println("File uploaded: " + filename);
    }
  });

  // Serve other static files
  server.serveStatic("/", LittleFS, "/");

  server.onNotFound(notFound);

// Endpoints that control the curtain and expose status/logs
  server.on("/curtain-status", HTTP_GET, [](AsyncWebServerRequest *request){
    String state = isMoving ? "moving" : (isOpen ? "open" : "closed");
    request->send(200, "application/json", "{\"state\":\"" + state + "\"}");
  });
  server.on("/open", handleOpen);
  server.on("/close", handleClose);
  server.on("/restart", handleRestart);
  server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request){
    if (logBuffer.length() == 0) {
      request->send(200, "text/plain", "No log data available yet...\n");
    } else {
      request->send(200, "text/plain", logBuffer);
    }
  });

// Start listening for HTTP requests
  server.begin();
  Serial.println("[HTTP] Server running!!");
  Serial.println("Open a Browser to http://" + WiFi.localIP().toString());

}


//=============================================== END SETUP ================================================================



//**************************************************** FUNCTIONS ***********************************************************

// Append a message to the log and keep the buffer within limits

void Log(const String& msg) {
  if (msg.length() == 0) return; // ignore empty logs
  Serial.println(msg); 
  logBuffer += msg + "\n";
  if (logBuffer.length() > LOG_BUFFER_SIZE) {
    logBuffer = logBuffer.substring(logBuffer.length() - LOG_BUFFER_SIZE); // trim oldest
  }
}

// Reconnect if Wi-Fi drops
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

// Interpolate servo position in microsecond steps for smooth motion
void moveServoSmooth(int targetAngle) {
  int targetPulse = angleToMicros(targetAngle);
  if (targetPulse == currentPulse) return;

  int step = (targetPulse > currentPulse) ? SERVO_STEP_SIZE : -SERVO_STEP_SIZE;
  while (currentPulse != targetPulse) {
    currentPulse += step;
    if ((step > 0 && currentPulse > targetPulse) ||
        (step < 0 && currentPulse < targetPulse)) {
      currentPulse = targetPulse;
    }
    myServo.writeMicroseconds(currentPulse);
    delay(SERVO_SPEED_DELAY);
  }
}

//**************************************************** END FUNCTIONS ***********************************************************


//--------------------------------------------------------------------MAIN LOOP ---------------------------------------------------------------------------
void loop() {
  MDNS.update();
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
//-------------------------------------------------------------------END MAIN LOOP -------------------------------------------------------------------------

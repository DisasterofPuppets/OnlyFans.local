// Window exhaust fan curtain control
// Using ESP8266 NodeMCU ESP-12
// 2025 http://DisasterOfPuppets.com

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Servo.h>
#include <LittleFS.h> //For the Logo File


#define SERVO_PIN 13           // D7 GPIO13 — safe for servo PWM
#define LED_PIN LED_BUILTIN    // GPIO2 — onboard blue LED

ESP8266WebServer server(80);
Servo myServo;
unsigned long lastAlive = 0;

const char* ssid = "CHANGEME";
const char* password = "CHANGEME";

String curtainStatus = "Unknown";

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nBooting...");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // OFF (active LOW)

  IPAddress local_IP(192, 168, 1, 19);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(8, 8, 8, 8); // Optional, Google DNS
  
  WiFi.config(local_IP, gateway, subnet, dns);

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

  Serial.println("\nWi-Fi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_PIN, HIGH); // OFF

  // mDNS setup
  if (MDNS.begin("onlyfans")) {
    Serial.println("mDNS responder started");
    Serial.println("Try visiting: http://onlyfans.local");
  } else {
    Serial.println("mDNS setup failed");
  }

//Confirm Logo exists on the board (serial monitor troubleshooting)
if (LittleFS.exists("/FanLogo.png")) {
  Serial.println("✅ FanLogo.png found on LittleFS.");
} else {
  Serial.println("❌ FanLogo.png NOT found on LittleFS.");
}


// Mount LittleFS and serve files like /FanLogo.png from internal storage
//Anything requested from /, checks if that file exists on the flash storage (LittleFS)

  if (!LittleFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
  // uploads the Image for the server (saved in /data folder
  server.serveStatic("/FanLogo.png", LittleFS, "/FanLogo.png");

  // Route definitions
  server.on("/", displayHTML);
  server.on("/open", handleOpen);
  server.on("/close", handleClose);
  server.begin();
  Serial.println("HTTP server started");

  // Startup movement
  Serial.println("Attaching servo...");
  myServo.attach(SERVO_PIN);
  myServo.write(0);
  Serial.println("Moved to 0°");
  delay(1000);

  myServo.write(180);
  curtainStatus = "Open";
  Serial.println("Moved to 180°");
  delay(10000);

  myServo.detach();
  Serial.println("PWM detached");
}

//-------------------Opens Curtain
void handleOpen() {
  Serial.println("Opening curtain...");
  myServo.attach(SERVO_PIN);
  myServo.write(180);
  delay(1000);
  myServo.detach();
  curtainStatus = "Open";
  server.send(200, "text/plain", "Curtain opened.");
}

//-------------------Closes Curtain
void handleClose() {
  Serial.println("Closing curtain...");
  myServo.attach(SERVO_PIN);
  myServo.write(0);
  delay(1000);
  myServo.detach();
  curtainStatus = "Closed";
  server.send(200, "text/plain", "Curtain closed.");
}
//-------------------Main Loop
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


//-------------------HTML
void displayHTML() {
  Serial.println("HTTP connection detected");

  String statusColor = (WiFi.status() == WL_CONNECTED) ? "green" : "red";
  String statusText = (WiFi.status() == WL_CONNECTED) ? "Good" : "ShitsBrokeYo";

  String fullPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Onlyfans.Local</title>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;700&display=swap" rel="stylesheet">
  <style>
    html, body {
      height: 100%;
      margin: 0;
      display: flex;
      flex-direction: column;
      background-color: #fff;
      font-family: 'Inter', sans-serif;
      text-align: center;
    }

    .logo-container {
      display: flex;
      margin-top: 50px;
      align-items: center;
      justify-content: center;
      gap: 30px;
      margin-bottom: 15px;
    }

    .fan-icon {
      width: 120px;
      height: 120px;
    }

    .logo-text {
      text-align: left;
    }

    .site-name {
      font-size: 2.8em;
      font-weight: bold;
      color: #0096FF;
    }

    .tagline {
      font-size: 0.85em;
      font-style: italic;
      color: #555;
      margin-top: -2px;
      text-align: right;
      padding-right: 10px;
    }

    .status {
      font-size: 1.5em;
      margin-top: 30px;
    }

    .button-row {
      display: flex;
      justify-content: center;
      gap: 20px;
      margin-top: 20px;
    }

    button {
      flex: 1;
      height: 30vh;
      font-size: 2em;
      border: none;
      border-radius: 10px;
      max-width: 200px;
      cursor: pointer;
    }

    .open {
      background-color: #4CAF50;
      color: white;
    }

    .close {
      background-color: #f44336;
      color: white;
    }

    button:hover {
      opacity: 0.9;
      transform: scale(1.02);
      transition: all 0.15s ease-in-out;
    }

    .footer {
      margin-top: 40px;
      text-align: center;
      font-weight: bold;
      font-size: 1.2em;
    }

    .bottom-footer {
      margin-top: auto;
      background-color: #0096FF;
      color: white;
      padding: 15px 0;
      font-size: 0.95em;
      text-align: center;
    }

    .bottom-footer a {
      color: white;
      text-decoration: underline;
      margin: 0 8px;
    }
  </style>
</head>
<body>
  <!-- Fan logo + site title -->
  <div class="logo-container">
    <img src="/FanLogo.png" alt="Fan Logo" class="fan-icon">
    <div class="logo-text">
      <div class="site-name">Onlyfans.Local</div>
      <div class="tagline">When you just wanna play with yourself</div>
    </div>
  </div>

  <p class="status">Curtain Status: )rawliteral" + curtainStatus + R"rawliteral(</p>

  <div class="button-row">
    <button class="open" onclick="fetch('/open').then(() => location.reload())">Open Curtain</button>
    <button class="close" onclick="fetch('/close').then(() => location.reload())">Close Curtain</button>
  </div>

  <div class="footer">
    Server Health: <span style="color: )rawliteral" + statusColor + R"rawliteral(; font-weight: bold;">)rawliteral" + statusText + R"rawliteral(</span>
  </div>

  <div class="bottom-footer">
    <a href="http://disasterofpuppets.com" target="_blank">Disaster's YouTube Channel</a> |
    <a href="https://github.com/DisasterofPuppets/OnlyFans.local" target="_blank">Disaster's GitHub</a>
  </div>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", fullPage);
}

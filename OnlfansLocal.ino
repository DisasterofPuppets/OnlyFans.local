// Window exhaust fan curtain control
// Using ESP8266 NodeMCU ESP-12
// 2025 http://DisasterOfPuppets.com

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Servo.h>

#define SERVO_PIN 13           // D7 GPIO13 — safe for servo PWM
#define LED_PIN LED_BUILTIN    // GPIO2 — onboard blue LED

ESP8266WebServer server(80);
Servo myServo;
unsigned long lastAlive = 0;

//**************CHANGE ME**************
const char* ssid = "BANKACCOUNT";
const char* password = "DETAILS";
//*************************************


// HTML page with big buttons
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Onlyfans.Local</title>
 
  <style>
    body { font-family: sans-serif; text-align: center; margin-top: 50px; }
    button {
      width: 80%%;
      height: 30vh;
      font-size: 2em;
      margin: 10px;
      border: none;
      border-radius: 10px;
    }
    .open { background-color: #4CAF50; color: white; }
    .close { background-color: #f44336; color: white; }
  </style>
</head>
<body>
  <h1>Onlyfans.Local</h1>
  <p style="font-style: italic; color: #666; margin-top: -10px;">When you just wanna play with yourself</p>
  <button class="open" onclick="fetch('/open')">Open Curtain</button>
  <button class="close" onclick="fetch('/close')">Close Curtain</button>
</body>
</html>
)rawliteral";

// Routes
void handleRoot() {
  Serial.println("HTTP connection detected");
  server.send(200, "text/html", htmlPage);
}

void handleOpen() {
  Serial.println("Opening curtain...");
  myServo.attach(SERVO_PIN);
  myServo.write(180);
  delay(1000);
  myServo.detach();
  server.send(200, "text/plain", "Curtain opened.");
}

void handleClose() {
  Serial.println("Closing curtain...");
  myServo.attach(SERVO_PIN);
  myServo.write(0);
  delay(1000);
  myServo.detach();
  server.send(200, "text/plain", "Curtain closed.");
}

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

  // Route definitions
  server.on("/", handleRoot);
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
  Serial.println("Moved to 180°");
  delay(10000);

  myServo.detach();
  Serial.println("PWM detached");
}

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

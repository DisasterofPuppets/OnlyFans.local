# OnlyFans.local

**ESP8266 WiFi-controlled Servo Curtain Opener**  
2025 · [http://DisasterOfPuppets.com](http://DisasterOfPuppets.com)

> *"When you just wanna play with yourself."*

This project hosts a simple HTTP interface on an ESP8266 that controls a servo motor to open or close a curtain — designed to allow airflow for an exhaust fan. The web UI includes big friendly buttons and is fully LAN-local.

## Features

- Hosted webpage at `http://onlyfans.local` *(or via static IP)*
- Servo control via `/open` and `/close` endpoints
- LED heartbeat every 20 minutes to indicate activity
- Startup motion test to verify servo operation
- Designed for easy drop-in use with Google Home or voice triggers

---

## Hardware Required

| Item                              | Notes                                            |
|-----------------------------------|--------------------------------------------------|
| 1 × ESP8266 (NodeMCU or similar) | Hosts the web page and controls the servo       |
| 1 × 100µF Capacitor               | Power smoothing for the servo                   |
| 1 × 2-Pin Female Connector        | *Optional:* external power for the servo        |
| 1 × 3-Pin Male Header             | *Optional:* clean servo connection              |
| 2 × 15-Pin Female Headers         | *Optional:* socket the ESP instead of soldering |
| 1 × Servo (e.g. MG996R)           | High-torque recommended for curtain pulling     |
| Misc wiring & solder              | You know the drill                              |

---

## Setup

1. Open `OnlyFansLocal.ino` in the Arduino IDE
2. Update the following lines with your own Wi-Fi credentials:
   ```cpp
   const char* ssid = "YourSSID";
   const char* password = "YourPassword";

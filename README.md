# OnlyFans.local
![Certified Kevin-Proof](https://img.shields.io/badge/Certified-Kevin--Proof-brightgreen)
![Curtain Status](https://img.shields.io/badge/Curtain-Open%20%2F%20Closed-lightgrey)
![Servo Mood](https://img.shields.io/badge/Servo-Ready-blue)
![OnlyFans.local](https://img.shields.io/badge/Access-Controlled-important)

**ESP8266 WiFi-controlled Servo Curtain Opener**  
2025 · [http://DisasterOfPuppets.com](http://DisasterOfPuppets.com)

> *"When you just wanna play with yourself."*

This project hosts a simple HTTP interface on an ESP8266 that controls a servo motor to open or close a curtain — designed to allow airflow for an exhaust fan. The web UI includes big friendly buttons and is fully LAN-local.

![Html_Preview](https://github.com/DisasterofPuppets/OnlyFans.local/blob/main/html.png)

## Table of Contents

- [Features](#features)
- [Hardware Required](#hardware-required)
- [Setup](#setup)
- [Wiring](#wiring)
- [Behavior Summary](#behavior-summary)
- [mDNS and Static Host Access](#mdns-and-static-host-access-refer-from-step-4)
  - [Option A: Bonjour](#option-a-install-bonjour-apples-mdns-service)
  - [Option B: Hosts File](#option-b-use-a-static-ip-and-edit-your-hosts-file)
- [Known Limitations](#known-limitations)
- [TODO (Future Ideas)](#todo-future-ideas)
- [License](#license)
- [Disclaimer](#disclaimer)

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
3. Upload the code to your ESP8266 board using the Arduino IDE
4. Access the web UI:
   - Open a browser and visit: `http://onlyfans.local`  
     *(if your system supports mDNS — refer below for Windows caveats)*  
   - Or go to the static IP: `http://192.168.1.19`  
     *(set in the sketch — feel free to change it if Kevin steals that IP)*
5. Use the big beautiful buttons to open and close your curtain
6. Want to adjust angles, timing, or add more controls?  
   Crack open `handleOpen()` and `handleClose()` and go nuts
7. Want to call it something *other* than `onlyfans.local`?  
   Like you would... but hey, if you insist, just update this line in `setup()`:
   ```cpp
   if (MDNS.begin("onlyfans")) {
   ```
   Replace `"onlyfans"` with any lowercase name you want — for example, `"ventlord"`, `"curtainmaster"`, or `"definitelynotNSFW"`.  
   Then you can access it via `http://yourname.local` on systems that support mDNS (or with the `hosts` file workaround below).

---

## mDNS and Static Host Access (Refer from Step 4)

### Using mDNS (`.local` names)

mDNS lets you access devices on your LAN by name, e.g. `onlyfans.local`.  
The ESP8266 handles this internally via:

```cpp
MDNS.begin("onlyfans");
```

However, **Windows does NOT support mDNS out of the box**. You have two options:

---

### Option A: Use a static IP and edit your `hosts` file

This avoids mDNS entirely and gives you reliable local access by name, without the need to install 
*'Bonjourrrrrrrrr Ya Cheese Eating Surrender Monkeys.' - Groundskeeper Willie*

1. Find and open this file as Administrator:
   ```
   C:\Windows\System32\drivers\etc\hosts
   ```
2. At the bottom, add a line like this:
   ```
   192.168.1.19    onlyfans.local    # Exhaust Fan Controller
   ```
3. Save the file
4. You can now access your ESP8266 via `http://onlyfans.local` on that machine

### Option B: Install Bonjour (Apple's mDNS service)

1. Download: [Bonjour Print Services for Windows](https://support.apple.com/kb/DL999)
2. Install it
3. Reboot (or restart your network stack)
4. Then you should be able to open:  
   `http://onlyfans.local` in your browser


## Wiring

![Wiring Diagram](https://github.com/DisasterofPuppets/OnlyFans.local/blob/main/Wiring%20Diagram.png)

| Connection       | Pin on ESP8266 | Notes                        |
|------------------|----------------|------------------------------|
| Servo Signal     | GPIO13 (D7)    | PWM pin for servo control    |
| Servo Power      | External 5V    | Don't use ESP’s 3.3V rail    |
| Servo GND        | GND            | Must be shared with ESP GND  |
| Capacitor (100µF)| Across 5V + GND| Helps smooth inrush current  |

Make sure:
- The servo is powered directly from a **stable 5V supply**
- **Ground is shared** between the ESP and servo supply
- The **100µF cap** is as close as possible to the servo's power pins


## License

MIT-ish. Feel free to use, remix, laugh at, or deploy in your suspiciously well-ventilated server room.

No warranty, no support hotline, and definitely no refunds if you short out your servo or emotionally scar your network admin.

---

## Disclaimer

Use at your own risk.

This project assumes you know what you're doing with power, servos, and microcontrollers.  
We accept no responsibility for:

- Fried boards  
- Bent curtains  
- Kevin’s unauthorized use of the curtain  
- Network policies violated by naming your device `onlyfans.local` in a shared workplace

You've been warned. And honestly? You knew what this was.

## Currently working on

Better looking html page, fancy and all that

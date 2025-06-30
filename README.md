# OnlyFans.local
![Certified Kevin-Proof](https://img.shields.io/badge/Certified-Kevin--Proof-brightgreen)
![Curtain Status](https://img.shields.io/badge/Curtain-Open%20%2F%20Closed-lightgrey)
![Stepper Mood](https://img.shields.io/badge/Stepper-Ready-blue)
![OnlyFans.local](https://img.shields.io/badge/Access-Controlled-important)

**ESP8266 WiFi-controlled Stepper Curtain Opener**  
2025 · [http://DisasterOfPuppets.com](http://DisasterOfPuppets.com)

> *"When you just wanna play with yourself."*

This project hosts a simple HTTP interface on an ESP8266 that controls a Nema17 stepper motor to open or close a curtain — designed to allow airflow for an exhaust fan. The web UI includes big friendly buttons and is fully LAN-local.

<div>
  <img src="https://raw.githubusercontent.com/DisasterofPuppets/OnlyFans.local/main/ReadmeImages/html.png" alt="HTML Preview" width="400"/>
</div>

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
- [Currently working on](#Currently-working-on)

## Features

- Hosted webpage at `http://onlyfans.local` *(or via static IP)*
- Stepper control via `/open` and `/close` endpoints
- LED heartbeat every 20 minutes to indicate activity
- Startup motion test to verify stepper operation
- Designed for easy drop-in use with Google Home or voice triggers

---

## Hardware Required

| Item                              | Notes                                                               |
|-----------------------------------|---------------------------------------------------------------------|
| 1 × ESP8266 (NodeMCU or similar)  | Hosts the web interface and sends STEP/DIR signals to the driver   |
| 1 × NEMA17 Stepper Motor (Pancake)| For curtain movement — low current version ideal for TMC2208       |
| 1 × TMC2208 Stepper Driver        | Controls the stepper via STEP/DIR inputs                           |
| 1 × 12V 3A Power Supply           | Powers the TMC2208 and motor — 4A preferred for extra headroom     |
| 1 × 5V Power Supply               | Powers the ESP8266 (via VIN or buck converter)                     |
| 1 × 100µF Capacitor               | Power smoothing across VM and GND near the TMC2208                 |
| 1 × SMAJ12A (TVS) Diode           | Surge protection across 12V input to TMC2208                       |
| 2 × Limit Switches (NO type)      | Detect curtain fully open/closed positions                         |
| 4 × 2-Pin Female Connectors       | *Optional:* Clean plug-in connections for power and switches       |
| 2 × 8-Pin Male Headers            | *Optional:* Socket for the TMC2208 module                          |
| 2 × 15-Pin Female Headers         | *Optional:* Socket for the ESP8266                                 |
| Misc wiring & solder              | You know the drill                                                 |

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

## ⚠️ TMC2208 Current Limit Setup (DO THIS FIRST), FAILURE TO DO SO MAY RESULT IN HARDWARE DAMAGE

Before connecting the ESP, stepper motor, or powering anything — **you MUST configure the TMC2208 driver’s voltage** (Vref) to set the motor current. If this step is skipped or incorrectly done, you risk damaging the driver or motor.

> This is **critical**. Set the Vref on your TMC2208 *before attaching the stepper or ESP8266.*

### How to Set Vref on the TMC2208

## 🧭 TMC2208 Pinout (To match our wiring physical orientation)

<div align="center">
  <img src="https://raw.githubusercontent.com/DisasterofPuppets/OnlyFans.local/main/ReadmeImages/TMC2208pins.png" alt="TMC2208 Pin Reference" width="400"/>
</div>

Position the board with the **label side facing up** (text upright and readable), just like shown above.  
This is the official orientation used by FYSETC and most major TMC2208-compatible breakout boards.

- The **left column** contains motor and power pins (GND → VM)
- The **right column** contains logic and control pins (DIR → EN)

| Left Side (Top → Bottom) | Name | Description                  |
|--------------------------|------|------------------------------|
| Top-left                 | GND  | Ground                       |
|                          | VIO  | Logic IO voltage (3.3V–5V)   |
|                          | M2B  | Motor coil B output          |
|                          | M2A  | Motor coil B output          |
|                          | M1A  | Motor coil A output          |
|                          | M1B  | Motor coil A output          |
|                          | GND  | Ground                       |
| Bottom-left              | VM   | **Motor voltage input (12V)**|

| Right Side (Top → Bottom)| Name | Description                  |
|--------------------------|------|------------------------------|
| Top-right                | DIR  | Direction input              |
|                          | STEP | Step input                   |
|                          | CLK  | Clock (optional)             |
|                          | PDN  | UART / Power-down            |
|                          | NC   | Not connected                |
|                          | MS2  | Microstep setting 2          |
|                          | MS1  | Microstep setting 1          |
| Bottom-right             | EN   | Enable                       |


> ✅ **Power Input Pins (for Vref setup):**  
> - Connect your **+12V** supply to **VM (Pin 16)** - bottom left
> - Connect **GND** to **Pin 9 - top left
> These are the only pins you need powered during Vref calibration.

> ⚠️ **Do NOT connect the stepper motor, ESP, or logic lines (STEP, DIR, EN, etc.) while setting Vref**  
> Power the module standalone to safely measure and adjust the reference voltage.

2. **Locate the Vref pad** on the TMC2208 module.  
   It’s typically near the trimpot and often labeled `VREF`. On most boards, it’s one of the **three small gold pads near the `EN` pin**.  
   → Use the **bottom-right pad** if unsure — that’s the Vref test point on most FYSETC boards.

3. **Set your multimeter to DC volts** (20V range or auto-range mode if available).

4. **Attach your multimeter probes:**  
   - **Black probe = GND**, such as **Pin 15** (second up on the left side)  
   - **Red probe = Vref test pad** (bottom-right pad in the trio near the EN corner)

5. **Adjust the trimpot carefully:**  
   - Use a small ceramic or plastic screwdriver if possible  
   - **Turn clockwise** to increase Vref  
   - **Turn counter-clockwise** to decrease Vref  
   - Go slowly — small turns cause noticeable changes

6. **Set a safe starting Vref based on your stepper’s rated current.**  

   Use this formula:  
	 Motor Current (A) = Vref × 1.77
	 → Vref = Desired Current ÷ 1.77
		
   Example:  
	 Target = 0.9A → Vref = 0.9 ÷ 1.77 ≈ 0.51V

### 🔧 Typical NEMA17 Models and Recommended Vref (for TMC2208)

| NEMA17 Type         | Body Depth | Rated Current | Recommended Vref | Notes                                 |
|---------------------|------------|----------------|------------------|----------------------------------------|
| Pancake             | 20–28mm    | 0.4A–0.6A      | 0.23V–0.34V      | Quiet and cool. Ideal for light loads. |
| Standard Mid-Torque | 38–48mm    | 0.8A–1.0A      | 0.45V–0.56V      | Most common general-purpose size.      |
| High Torque         | 48mm+      | 1.2A–1.5A+     | **Not safe on TMC2208** | Use TMC2209 or TB6600 instead.         |

> ⚠️ The TMC2208 is not designed to drive motors over ~1.2A.  
> If your motor is rated higher, use a stronger driver like the **TMC2209**, **TB6600**, or **DM542**.

> ✅ For most 3D printer or automation builds (including exhaust fans, sliders, light-duty actuators), a **0.45V Vref** is a good all-around starting point.


<pre style="white-space: pre-wrap;">
I used a Pancake and set it to 0.30V (using a 12mm GT2 Pulley on the Stepper and 38mm Pulley on the door hinge)

Chat GPT came up with this to get those figures (in case it's wrong...which it likely is).

I provided the weight of the lid under 300grams, and the pull force to open lid less than 1kg. 
I got these figures using a luggage scale.

You have 3 GT2 pulleys:

12mm diameter (smallest — use on motor)

24mm diameter (medium)

38mm diameter (largest — use on lid)

Assuming you’re doing simple rotary torque transfer via belt, the torque multiplication ratio is based on:

Ratio = Output Pulley Diameter ÷ Input Pulley Diameter

Option A: 12mm (motor) → 38mm (lid)

 Ratio = 38 / 12 ≈ 3.17 : 1
→ Torque is multiplied by 3.17
→ Speed is divided by 3.17  

With the 12:38 setup, your 0.25 N·m stepper becomes:
0.25 N·m × 3.17 = 0.79 N·m

That's more than half the 1.47 N·m worst-case torque you estimated — and probably good enough in real-world testing, especially if:

Lid weight was overestimated

You use slightly higher Vref (~0.30–0.32V) to push the driver closer to 0.28 N·m

You minimize friction and keep belt tension correct

Use 12mm on the motor, 38mm on the lid

This gives you a ~3.17× torque multiplier

Real-world result: you’re probably in the green — test before upgrading motor

“If I use that pulley reduction, how should I adjust Vref (voltage)?”

Using your 3.17:1 pulley setup multiplies torque, so:

You don't need to increase current (Vref) to brute-force more torque

Instead, you can run at a lower Vref, reducing:

Heat on the stepper and TMC2208

Power draw

Driver stress

✅ Recommended Vref with 3.17:1 Reduction
Assuming your NEMA17 pancake stepper is rated for 0.6A max:

Vref	Current (A)	Raw Torque (est.)	Torque @ 3.17:1 Pulley
0.28V	~0.5A	~0.21 N·m	~0.66 N·m
0.30V	~0.53A	~0.23 N·m	~0.73 N·m
0.34V	~0.60A (max)	~0.28 N·m	~0.89 N·m

Set Vref to: 0.30V to 0.32V
</pre>
</div>

   
Don't worry these images are WIP, they'll be much easier to follow when I am done. :)

<div align="center" Padding="40">
  <img src="https://raw.githubusercontent.com/DisasterofPuppets/OnlyFans.local/main/ReadmeImages/Basic Schematic.png" alt="ESP8266 Basic Wiring diagram" width="300"/>
  <img src="https://raw.githubusercontent.com/DisasterofPuppets/OnlyFans.local/main/ReadmeImages/Stepper Wiring Boards in place.png" alt="PCB Wiring" width="300"/>
</div>

| Connection           | Pin on ESP8266 | Notes                                                              |
|----------------------|----------------|--------------------------------------------------------------------|
| STEP Signal          | GPIO5 (D1)     | Connect to TMC2208 STEP pin                                       |
| DIR Signal           | GPIO4 (D2)     | Connect to TMC2208 DIR pin                                        |
| Open Limit Switch    | GPIO14 (D5)    | Connect to one side of OPEN switch (normally open)                |
| Closed Limit Switch  | GPIO12 (D6)    | Connect to one side of CLOSED switch (normally open)              |
| Limit Switch Ground  | GND            | All switches share common ground with ESP                         |
| Stepper Power +12V   | VM on TMC2208  | Connect from 12V power supply (NOT from ESP)                      |
| Stepper GND          | GND on TMC2208 | Must be shared with ESP GND (via power supply or common ground)   |
| Capacitor (100µF+)   | Across VM + GND| Optional: stabilizes voltage to TMC2208, mount close to driver     |


> ⚠️ **Important:**  
> Make sure:  
> - The stepper is powered directly from a **stable 12V supply** (not from the ESP8266)  
> - **Ground is shared** between the ESP8266 and the stepper driver’s 12V power supply  
>   (this ensures consistent logic signal reference between ESP and TMC2208)

## License

MIT-ish. Feel free to use, remix, laugh at, or deploy in your suspiciously well-ventilated server room.

No warranty, no support hotline, and definitely no refunds if you short out your stepper or emotionally scar your network admin.

---

## Stepper Speed / Range Tuning

Stepper motor movement is defined by **step pulses**, with speed and acceleration controlling how smoothly or quickly it moves.

### Key Parameters:

- **`MOVE_SPEED`** – Maximum speed in steps per second  
- **`ACCEL`** – Acceleration rate in steps per second squared  

You can tune these to match your curtain mechanism’s weight and inertia.  
Higher values = faster movement, but may cause missed steps if torque is too low.

If you're using **limit switches**, the stepper moves until the switch is triggered —  
so you don’t need to define a fixed number of steps. This makes tuning simpler and more flexible.

> 💡 For heavy lids or longer arms, increase `ACCEL` slowly to avoid jerky starts.


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

Switching this from Servo to Stepper

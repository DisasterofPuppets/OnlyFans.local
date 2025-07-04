#include <SoftwareSerial.h>      // ESPSoftwareSerial v8.1.0 by Dirk Kaar and Peter Lerup
#include <TMCStepper.h>
#include <ESP8266WiFi.h>         // Ensure ESP8266 compatibility

/* Stepper test for ESP8266 using TMC2208 driver in UART mode and Nema Stepper

This code allows you to use two limit switches to act as clockwise and anticlockwise controls.
While a switch is triggered, the stepper will move in the nominated direction.
When the switch is released, the stepper will stop.

TMC2208 is configured in UART mode for advanced features like current control,
stealthChop, and stallGuard detection.

2025 Disaster of Puppets

===============================================================================
                              WIRING TABLE
===============================================================================

ESP8266 (NodeMCU)    | TMC2208 Driver    | NEMA17 Stepper    | Switches/Power
---------------------|-------------------|-------------------|------------------
D3 (GPIO0)           | STEP              | -                 | -
D4 (GPIO2)           | DIR               | -                 | -
D7 (GPIO13)          | EN                | -                 | -
D1 (GPIO5)           | PDN_UART (RX)     | -                 | -
D2 (GPIO4)           | PDN (TX)          | -                 | - via 1k resistor
D5 (GPIO14)          | -                 | -                 | Open Switch (Pin 1)
D6 (GPIO12)          | -                 | -                 | Closed Switch (Pin 1)
3V3                  | VDD               | -                 | -
GND                  | GND               | -                 | Open Switch (Pin 2)
GND                  | -                 | -                 | Closed Switch (Pin 2)
-                    | VM                | -                 | 12-24V Power Supply (+)
-                    | GND               | -                 | 12-24V Power Supply (-)

TMC2208 Driver      | NEMA17 Stepper    | Wire Color        | Description
--------------------|-------------------|-------------------|------------------
1B                  | Coil 1 End        | Black             | Phase A-
1A                  | Coil 1 Start      | Green             | Phase A+
2A                  | Coil 2 Start      | Red               | Phase B+
2B                  | Coil 2 End        | Blue              | Phase B-

Additional TMC2208 Connections:
- MS1: Leave floating (internal pulldown for UART mode)
- MS2: Leave floating (internal pulldown for UART mode)
- SPREAD: Leave floating (controlled via UART)
- VREF: Leave floating (current set via UART)
- SENSE: Connect 0.11Ω sense resistor between SENSE and GND (usually on-board)

Power Supply Requirements:
- ESP8266: 3.3V (from USB or external regulator)
- TMC2208 Logic: 3.3V (VDD pin)
- TMC2208 Motor: 12-24V (VM pin) - Match your NEMA17 voltage rating
- Current: Minimum 2A for typical NEMA17 motors

Switch Connections:
- Use normally open (NO) switches
- One terminal to ESP8266 pin, other terminal to GND
- Internal pullup resistors are enabled in code

UART Communication:
- PDN_UART pin on TMC2208 handles both TX and RX
- Connect to both D1 and D2 on ESP8266 for bidirectional communication
- **TX (D2 / GPIO4)** must be connected **via a 1kΩ resistor** to protect against bus contention
- Ensure 3.3V logic levels (TMC2208 is 3.3V tolerant)

===============================================================================

*/

// ====================== Pin Definitions =========================
#define STEP_PIN D3           // Step signal pin GPIO0 //May prevent boot if pulled low
#define DIR_PIN D4            // Direction control pin GPIO2 //May prevent boot if pulled low
#define ENABLE_PIN D7         // Enable pin (active LOW) GPIO13	
#define OPEN_SWITCH_PIN D5    // Open/anticlockwise switch GPIO14
#define CLOSED_SWITCH_PIN D6  // Closed/clockwise switch 	GPIO12	

#define RX_PIN D1            // Software UART RX (connect to PDN_UART)  GPIO5	 
#define TX_PIN D2             // Software UART TX (connect to PDN) GPIO4


// ====================== TMC2208 Configuration ===================
#define R_SENSE 0.11f         // External sense resistor (Ω)
#define DRIVER_ADDRESS 0b00   // TMC2208 default address

// Create TMC2208Stepper using SoftwareSerial via RX/TX
SoftwareSerial tmc_serial(RX_PIN, TX_PIN);
TMC2208Stepper driver(&tmc_serial, R_SENSE);

// ====================== Movement Parameters =====================
const uint16_t STEPS_PER_REV = 200;     // Standard NEMA17 steps/rev
const uint16_t MICROSTEPS = 1;          // 1 = Full step, 16 = 1/16 step
const uint16_t STEP_DELAY = 1000;       // Microseconds between steps
const uint16_t RMS_CURRENT = 1200;       // Desired motor current (mA) 1.2 A RMS // make sure to use a heatsink over 800

void setup() {
  Serial.begin(115200);                 // Debug via hardware UART
  delay(50);                            // Stabilization delay

  Serial.println("\nStepper UART control starting...");

  // Initialize virtual UART for TMC2208 communication
  driver.begin();                       // Automatically initializes SoftwareSerial

  // Set up control and switch pins
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(OPEN_SWITCH_PIN, INPUT_PULLUP);
  pinMode(CLOSED_SWITCH_PIN, INPUT_PULLUP);

  // Initial pin states
  digitalWrite(ENABLE_PIN, HIGH);       // Disable motor on boot
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);

  delay(100);                           // Allow TMC2208 to wake up

  configureTMC2208();                   // Driver setup
  digitalWrite(ENABLE_PIN, LOW);       // Motor ready

  Serial.println("TMC2208 configuration complete.");
}

void configureTMC2208() {
  Serial.println("Configuring TMC2208...");

  // Set RMS current - this determines the motor torque
  // Value in mA, typically 60–90% of motor's rated current
  // Too low = missed steps / weak torque
  // Too high = overheating and loss of efficiency
  driver.rms_current(RMS_CURRENT);

  // Enable stealthChop for quiet operation at low speeds
  // stealthChop provides nearly silent operation but less torque at high speeds
  // spreadCycle is better for torque but noisier
  driver.en_spreadCycle(true);  // Enable SpreadCycle for more torque (disabling sets it back to StealthChop)

  // Set microstepping resolution
  // Higher values give smoother movement but require more steps and processing time
  // Lower values give more torque but cause more vibration
  driver.microsteps(MICROSTEPS);

  // Enable automatic current scaling
  // Dynamically adjusts motor current based on mechanical load
  // Improves energy efficiency and reduces heat
  driver.pwm_autoscale(true);
  
  // Set PWM frequency for stealthChop
  // Higher frequencies reduce audible noise but can increase driver temperature
  // Lower frequencies increase torque ripple
  driver.pwm_freq(0);  // 0=2/1024 fclk, 1=2/683 fclk, 2=2/512 fclk, 3=2/410 fclk

  // Enable automatic gradient adaptation
  // Automatically adjusts PWM gradient based on current scaling
  driver.pwm_autograd(false);

  // Enable interpolation to 256 microsteps
  // Smooths movement by faking 256 microsteps – active only if microsteps > 1
  driver.intpol(false);

  // Set hold current to 50% of run current
  // Saves power and reduces heat when motor is idle
  // Value from 0–31, where 16 ≈ 50%
  driver.ihold(16);

  // Set run current scale
  // 0–31 scale, 31 = 100% of rms_current setting
  // Use lower values if torque is too high or driver overheats
  driver.irun(31);

  // Set power down delay after inactivity
  // After this delay, motor switches to hold current level
  driver.iholddelay(10);

  // Enable UART mode and set driver to use digital current control
  // Analog mode (VREF pin) is disabled
  driver.I_scale_analog(false);

  // Tell driver to use external sense resistor instead of internal reference
  driver.internal_Rsense(false);

//----------------------------PRINTOUT OF SETTINGS TO TERMINAL-------------------------
  Serial.print("RMS Current set to: ");
  Serial.print(driver.rms_current());
  Serial.println(" mA");

  Serial.print("Microstepping set to: ");
  Serial.println(driver.microsteps());

  if (driver.pwm_autoscale()) {
    Serial.println("Automatic current scaling enabled");
  } else {
    Serial.println("Automatic current scaling disabled");
  }

  Serial.print("PWM frequency set to: ");
  Serial.println(driver.pwm_freq());

  if (driver.pwm_autograd()) {
    Serial.println("Automatic PWM gradient adaptation enabled");
  } else {
    Serial.println("Automatic PWM gradient adaptation disabled");
  }

  if (driver.intpol()) {
    Serial.println("256 microstep interpolation enabled");
  } else {
    Serial.println("256 microstep interpolation disabled");
  }

  Serial.print("Run current scale (irun): ");
  Serial.println(driver.irun());

  Serial.print("Hold current scale (ihold): ");
  Serial.println(driver.ihold());

  Serial.print("Hold delay: ");
  Serial.println(driver.iholddelay());

  if (driver.I_scale_analog()) {
    Serial.println("Analog current scaling enabled");
  } else {
    Serial.println("Analog current scaling disabled (using UART)");
  }

  if (driver.internal_Rsense()) {
    Serial.println("Internal Rsense enabled");
  } else {
    Serial.println("External Rsense enabled");
  }

//-----------------------END OF PRINTOUT OF SETTINGS TO TERMINAL-------------------------

  // Test UART communication link with the driver
  // If this fails, check PDN_UART wiring and logic voltage levels
  if (driver.test_connection()) {
    Serial.println("TMC2208 UART communication: OK");
  } else {
    Serial.println("TMC2208 UART communication: FAILED");
    Serial.println("→ Check wiring, logic levels, and PDN_UART pin continuity");
  }
}


void loop() {
  bool openSwitch = digitalRead(OPEN_SWITCH_PIN) == LOW;
  bool closedSwitch = digitalRead(CLOSED_SWITCH_PIN) == LOW;

  if (openSwitch && !closedSwitch) {
    digitalWrite(DIR_PIN, LOW);        // Anticlockwise
    stepMotor();
    Serial.println("↺ Moving anticlockwise...");
  } 
  else if (closedSwitch && !openSwitch) {
    digitalWrite(DIR_PIN, HIGH);       // Clockwise
    stepMotor();
    Serial.println("↻ Moving clockwise...");
  } 
  else {
    static unsigned long lastMessage = 0;
    if (millis() - lastMessage > 1000) {
      Serial.println("⏸ Motor idle – waiting for input");
      lastMessage = millis();
    }
    delay(10);                         // Prevent CPU thrashing
  }
}

void stepMotor() {
  digitalWrite(ENABLE_PIN, LOW);       // Ensure motor is enabled

  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(STEP_DELAY / 2);
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(STEP_DELAY / 2);
}

// Optional diagnostics
void printDriverStatus() {
  Serial.print("Driver Status: ");
  if (driver.ot())     Serial.print("Overtemperature ");
  if (driver.otpw())   Serial.print("Warning ");
  if (driver.s2ga())   Serial.print("Short to GND A ");
  if (driver.s2gb())   Serial.print("Short to GND B ");
  if (driver.s2vsa())  Serial.print("Short to VS A ");
  if (driver.s2vsb())  Serial.print("Short to VS B ");
  if (driver.ola())    Serial.print("Open Load A ");
  if (driver.olb())    Serial.print("Open Load B ");
  Serial.println();
}

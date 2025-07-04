/* basic test for Esp8266 using a TMC2208 driver with a Nema17
* This will send your stepper one way, then back the other.
*
* ENSURE you have manually set the Vref on your TMC2208 before connecting as you may blow things up.
* I used a vRef of 0.634V but I don't know what I'm doing most of the time so take that
* as you will.
*
* 2025 Disaster of Puppets and the voices from the internets
*
Wiring Table 

From ESP8266 to the TMC2208

*Important, use the ground beside each Power input, do not use them as interchangable...ask me why.

=================================
| ESP8266 Pin  |   TMC2208 PIN  |   
=================================
| D1 (GPIO5)   |    STEP        |
---------------------------------
| D2 (GPIO4)   |    DIR         |
---------------------------------
| D7 (GPIO13)  |    EN          |
---------------------------------
| 3.3V         |    VIO         |
---------------------------------
| GND          |    GND         |
---------------------------------

12V External Power Supply (3A minimium) to the TMC2208

===========================
| 12 V   |   TMC2208 PIN  |   
===========================
| +12V   |       VM       |
---------------------------
| Ground |       GND      |
---------------------------

****Additional, put a 100 - 1000uf capacitor across the TMC2208 VM and Ground (either solder direct to the pins, or make sure its after the 12V / Ground Input)

*/
#define STEP_PIN D1
#define DIR_PIN  D2
#define EN_PIN   D7  // GPIO13

const int stepsPerCycle = 2000;
const unsigned long stepDelay = 2000; // microseconds

void stepMotor(int direction) {
  digitalWrite(DIR_PIN, direction);
  Serial.println(direction ? "Stepping forward..." : "Reversing...");

  for (int i = 0; i < stepsPerCycle; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(2);  // short HIGH pulse
    digitalWrite(STEP_PIN, LOW);

    unsigned long tStart = micros();
    while (micros() - tStart < stepDelay) {
      yield();  // Allow background tasks
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Stepper enable pin test");

  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW);  // TMC2208 ENABLE = LOW
}

void loop() {
  stepMotor(HIGH);  // Clockwise
  delay(1000);      // Pause

  stepMotor(LOW);   // Anticlockwise
  delay(1000);      // Pause

  Serial.println("Sleeping before next run...");
  unsigned long sleepStart = millis();
  while (millis() - sleepStart < 30000) {
    yield();
  }
}

#define STEP_PIN D1
#define DIR_PIN D2
#define ENABLE_PIN D7
#define OPEN_SWITCH_PIN D5
#define CLOSED_SWITCH_PIN D6

void setup() {
  Serial.begin(115200);
  Serial.println("Stepper control with switches");

  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(OPEN_SWITCH_PIN, INPUT_PULLUP);
  pinMode(CLOSED_SWITCH_PIN, INPUT_PULLUP);

  digitalWrite(ENABLE_PIN, LOW);  // Enable TMC2208
  digitalWrite(STEP_PIN, LOW);    // Initialize step pin
  digitalWrite(DIR_PIN, LOW);     // Initialize direction
}

void loop() {
  bool openSwitch = digitalRead(OPEN_SWITCH_PIN) == LOW;
  bool closedSwitch = digitalRead(CLOSED_SWITCH_PIN) == LOW;

  if (openSwitch && !closedSwitch) {
    // Move anticlockwise when open switch is LOW
    digitalWrite(DIR_PIN, LOW);
    stepMotor();
    Serial.println("Moving anticlockwise...");
  } else if (closedSwitch && !openSwitch) {
    // Move clockwise when closed switch is LOW
    digitalWrite(DIR_PIN, HIGH);
    stepMotor();
    Serial.println("Moving clockwise...");
  } else {
    // Stop when both switches are HIGH or both are LOW
    digitalWrite(ENABLE_PIN, HIGH);  // Disable motor
    Serial.println("Motor stopped");
    delay(100);  // Small delay to reduce serial spam
  }
}

void stepMotor() {
  digitalWrite(ENABLE_PIN, LOW);  // Ensure motor is enabled
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(500);  // Adjust for speed
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(500);
}

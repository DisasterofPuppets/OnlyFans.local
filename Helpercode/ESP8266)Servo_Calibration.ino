#include <Servo.h>

// Pin definitions - match your hardware
#define SERVO_PIN 13          // D7 
#define POT_PIN A0            // Analog pin for potentiometer

// Servo settings (copy from your main code)
int SERVOMIN = 600;
int SERVOMAX = 2400;

Servo myServo;

int angleToMicros(int angle) {
  return map(angle, 0, 180, SERVOMIN, SERVOMAX);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("=== SERVO POTENTIOMETER CALIBRATION ===");
  Serial.println("Commands:");
  Serial.println("  a<angle>  - Move servo to angle (0-180)");
  Serial.println("  r         - Read current potentiometer value");
  Serial.println("  h         - Show this help");
  Serial.println("Example: a0 (closed), a110 (open)");
  Serial.println("=======================================");
  Serial.println("1. Move to closed position and note pot value");
  Serial.println("2. Move to open position and note pot value");
  Serial.println("3. Use these values in your OnlyFans code");
  Serial.println();
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.startsWith("a")) {
      // Move servo to angle
      int angle = input.substring(1).toInt();
      if (angle >= 0 && angle <= 180) {
        myServo.attach(SERVO_PIN);
        int pulse = angleToMicros(angle);
        myServo.writeMicroseconds(pulse);
        Serial.println("Moving to " + String(angle) + "° (" + String(pulse) + "µs)");
        delay(2000); // Let servo reach position
        myServo.detach();
        
        int potValue = analogRead(POT_PIN);
        Serial.println("Potentiometer reading: " + String(potValue));
        Serial.println("Use this value in your OnlyFans code!");
        Serial.println();
      } else {
        Serial.println("Error: Angle must be 0-180");
      }
    }
    else if (input == "r") {
      // Read potentiometer
      int potValue = analogRead(POT_PIN);
      Serial.println("Current potentiometer: " + String(potValue));
    }
    else if (input == "h") {
      // Help
      Serial.println("Commands:");
      Serial.println("  a<angle> - Move servo");
      Serial.println("  r        - Read pot value");
      Serial.println("  h        - Help");
    }
    else {
      Serial.println("Unknown command. Type 'h' for help.");
    }
  }
  
  delay(100);
}

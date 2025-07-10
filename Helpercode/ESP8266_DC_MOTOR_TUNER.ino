#include <ESP8266WiFi.h>
/*
 * ESP8266 + L298N Door Control
 * Simple open/close via serial commands
 * 
 * Serial Commands:
 * "open" - opens the door
 * "close" - closes the door  
 * "stop" - stops the motor
 * "speed" - enter new speed for the motor
 * "time" - enter a new duration for the motor
 * "status" - displays limit switch values along with motor time and speed
 * "kill" - immediately stops everything and reboots

 */


/*
 * WIRING TABLE:
 * 
 * ESP8266 NodeMCU → L298N
 * ==========================================
 * D1 (GPIO5)      → ENA   (Speed control PWM)
 * D2 (GPIO4)      → IN1   (Direction control)
 * D5 (GPIO14)     → IN2   (Direction control)  
 * GND             → GND   (Common ground)
 * 
 * ESP8266 → Endstops
 * ==========================================
 * D6 (GPIO12)     → Open endstop switch
 * D7 (GPIO13)     → Close endstop switch
 * 3.3V            → Common terminal of both switches
 * 
 * Endstop wiring: Normally Open switches
 * When switch is NOT pressed: pin reads HIGH (pullup)
 * When switch IS pressed: pin reads LOW (connected to ground through switch)
 * Code looks for LOW state to detect endstop trigger
 * 
 * L298N → 12V Motor
 * ==========================================
 * OUT1            → Motor Red wire
 * OUT2            → Motor Black wire
 * 
 * Power Connections:
 * ==========================================
 * L298N +12V      → 12V Power Supply +
 * L298N GND       → 12V Power Supply -
 * L298N +5V       → 5V (for L298N logic - can use ESP 3.3V if no 5V available)
 * 
 * ESP8266 Power:
 * ==========================================
 * ESP8266 Vin     → 5V or USB power
 * ESP8266 GND     → Common ground with everything
 * 
 * IMPORTANT NOTES:
 * - Start with MOTOR_SPEED = 180 (about 70%) - this motor will be POWERFUL
 * - Adjust MOVE_TIME based on how long it takes your door to open/close
 * - If motor spins wrong direction, swap IN1/IN2 connections or swap motor wires
 * - Make sure all grounds are connected together
 * - The L298N can get hot - ensure good ventilation
 */


// Motor control pins
#define ENA D1    // PWM speed control pin (GPIO5)
#define IN1 D2    // Direction pin 1 (GPIO4)
#define IN2 D5    // Direction pin 2 (GPIO14)

// Endstop pins for position feedback
#define ENDSTOP_OPEN  D6   // Endstop for fully open position (GPIO12)
#define ENDSTOP_CLOSE D7   // Endstop for fully closed position (GPIO13)
#define SWAP_DIRECTION true // Because I wired the motor output with the convention of Ground is left, +V is right.

enum MotorState {
  IDLE,
  MOVING_OPEN,  // Anti-clockwise
  MOVING_CLOSE  // Clockwise
};
MotorState currentState = IDLE;

int MOTOR_SPEED = 100;    // PWM value 0-255 (start conservative, you can use speed to change this in the code)
int MOVE_TIME = 3000;     // How long to run motor (milliseconds)
bool KICKSTART = true;    // Set to true if your motor needs a quick burst to spin up
int KICKSTART_POWER = 255;  // PWM value for kickstart (0-255)
int KICKSTART_TIME = 100;   // How long to kickstart in milliseconds

//------------============================== | SETUP | ==============================------------**

void setup() {
 
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("ESP8266 Door Control Starting...");
 
  // Set motor control pins as outputs
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  
  // Set endstop pins as inputs with pullups
  pinMode(ENDSTOP_OPEN, INPUT_PULLUP);
  pinMode(ENDSTOP_CLOSE, INPUT_PULLUP);
  
  // Make sure motor starts stopped
  stopMotor();
  
  //Serial.println("Ready! Send 'open', 'close', 'stop', or 'status'");
  unsigned long lastPromptTime = 0;
  const unsigned long PROMPT_INTERVAL_MS = 5000; // 5 seconds

  Serial.println("\n\nWaiting for User, Press Enter to start..");
  lastPromptTime = millis(); 

  while (Serial.available() == 0) {
    if (millis() - lastPromptTime >= PROMPT_INTERVAL_MS) {
      Serial.println("Waiting for User, Press Enter to start..");
      lastPromptTime = millis();
    }
    yield();
  }
  
  // Clear the serial buffer to consume the "Enter" keypress
  while(Serial.available() > 0) {
    Serial.read();
  }
  delay(100); // Give time for any remaining serial data
  // Clear again to be sure
  while(Serial.available() > 0) {
    Serial.read();
  }
  

  Serial.println("-------------------------------------------------");
  Serial.println("| L298N Stepper controller for 12V Drill motor. |");
  Serial.println("-------------------------------------------------");
  Serial.println("Enter 'Open', 'Close', 'Stop, 'Status', 'Speed', 'Time', 'Kill', or Enter '?' for help.");
  Serial.println("Enter 'KILL' to Stop all function Immediately (You will need to manually reboot after)");
  Serial.println("-------------------------------------------------\n");
  
  delay(100);

  Serial.println("\nSystem ready. Motor is idle, waiting for command entry.");
  Serial.println("_");

}
//--------------------------------------- | SETUP END | ----------------------------------------------**



//------------============================== | FUNCTIONS | ==============================------------**


//--------------============== | Stop Motor Function | ==============--------------
void stopMotor() {
  // Turn off both direction pins
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  
  // Turn off PWM (speed = 0)
  analogWrite(ENA, 0);
  
  Serial.println("Motor stopped.");
}

//--------------------------------------------------------- Stop Motor END

//--------------============== | Turn Clockwise Function| ==============--------------
void turnClockwise() {
 // Check if already at open position
 if (digitalRead(ENDSTOP_OPEN) == LOW) {
   Serial.println("But, the Door is already open!");
   return;
 }
 
 // Set direction for opening (you may need to reverse these)
 digitalWrite(IN1, HIGH);  // Forward direction
 digitalWrite(IN2, LOW);
 
  // Start motor with optional kickstart
  if (KICKSTART) {
    analogWrite(ENA, KICKSTART_POWER);  // Full power burst
    delay(KICKSTART_TIME);
  }
  analogWrite(ENA, MOTOR_SPEED);  // Normal operating speed
 
 // Run until endstop is triggered, timed out or kill command issued
 unsigned long startTime = millis();
 while (digitalRead(ENDSTOP_OPEN) == HIGH && (millis() - startTime) < MOVE_TIME) {
   // Check for emergency commands
   if (Serial.available() > 0) {
     String command = Serial.readStringUntil('\n');
     command.trim();
     command.toLowerCase();
     if (command == "kill" || command == "stop") {
       Serial.println("Emergency stop received!");
       stopMotor();
       if (command == "kill") {
         Serial.println("System halting. Manual reboot required.");
         while (true) { yield(); }
       }
       return;
     }
   }
   delay(10);
 }
 
 stopMotor();
 
 if (digitalRead(ENDSTOP_OPEN) == LOW) {
   Serial.println("Door opened! (Endstop triggered)");
 } else {
   Serial.println("Door open timeout - check endstop or increase MOVE_TIME");
 }
}
//--------------------------------------------------------- Turn Clockwise END

//--------------============== | Turn Anticlockwise Function| ==============--------------
void turnAnticlockwise() {
 // Check if already at close position
 if (digitalRead(ENDSTOP_CLOSE) == LOW) {
   Serial.println("But, the Door is already closed!");
   return;
 }
 
 // Set direction for closing (opposite of opening)
 digitalWrite(IN1, LOW);   // Reverse direction
 digitalWrite(IN2, HIGH);
 
  // Start motor with optional kickstart
  if (KICKSTART) {
    analogWrite(ENA, KICKSTART_POWER);  // Full power burst
    delay(KICKSTART_TIME);
  }
  analogWrite(ENA, MOTOR_SPEED);  // Normal operating speed
 
 // Run until endstop is triggered, timed out or kill command issued
 unsigned long startTime = millis();
 while (digitalRead(ENDSTOP_CLOSE) == HIGH && (millis() - startTime) < MOVE_TIME) {
   // Check for emergency commands
   if (Serial.available() > 0) {
     String command = Serial.readStringUntil('\n');
     command.trim();
     command.toLowerCase();
     if (command == "kill" || command == "stop") {
       Serial.println("Emergency stop received!");
       stopMotor();
       if (command == "kill") {
         Serial.println("System halting. Manual reboot required.");
         while (true) { yield(); }
       }
       return;
     }
   }
   delay(10);
 }
 
 stopMotor();
 
 if (digitalRead(ENDSTOP_CLOSE) == LOW) {
   Serial.println("Door closed! (Endstop triggered)");
 } else {
   Serial.println("Door close timeout - check endstop or increase MOVE_TIME");
 }
}
//--------------------------------------------------------- Turn Anticlockwise END


//------------============= | Handle Serial Input Function | =============------------
void handleSerialCommands() {
  // Check if data is available on serial
  if (Serial.available() > 0) {
    // Read the incoming string
    String command = Serial.readStringUntil('\n');
    command.trim(); // Remove any whitespace
    command.toLowerCase(); // Make case insensitive

    // The 'Kill' command is a special case and must be processed immediately,
    // regardless of the current motor state.
if (command == "kill") {
  Serial.println("Killswitch Applied !!!! System Rebooting !!!!");
  stopMotor();
  delay(200);
  ESP.reset(); // Clean restart instead of infinite loop
}

  //non blocking stop command
if (command == "stop") {
  Serial.println("Stopping motor...");
  stopMotor();
  currentState = IDLE;
  Serial.println("_");
  return;
}

  // If the motor is currently moving, reject any other command.
  if (currentState != IDLE) {
    Serial.println("Sorry, you must wait for the current command to complete before inputting another. Your command has not been queued.");
    Serial.println("_");
    return; // Ignore the command and exit the function
  }

  // If we reach this point, the motor is IDLE and the command was not 'Kill' or 'Stop'.
  // We can now process the other commands.

  if (command == "open") {
    Serial.println("Opening door...(Clockwise)");
    currentState = MOVING_OPEN;
    if (SWAP_DIRECTION) {
      turnAnticlockwise();
    }
    else {
      turnClockwise();
    }
    currentState = IDLE;
  }

  else if (command.startsWith("time")) {
    // Check if command has a value after "time"
    if (command.length() > 4) {  // "time" is 4 characters
      String timeStr = command.substring(5);  // Get everything after "time "
      int newTime = timeStr.toInt();
      
      if (newTime > 0) {
        MOVE_TIME = newTime;
        Serial.print("Motor timeout updated to: ");
        Serial.print(MOVE_TIME);
        Serial.println(" ms");
      } else {
        Serial.println("Invalid time. Must be greater than 0");
      }
    } else {
      // No value provided, prompt for input
      Serial.print("Current motor timeout: ");
      Serial.print(MOVE_TIME);
      Serial.println(" ms");
      Serial.println("Enter new timeout (milliseconds): ");
      
      while (Serial.available() == 0) {
        yield();
      }
      
      String timeInput = Serial.readStringUntil('\n');
      timeInput.trim();
      int newTime = timeInput.toInt();
      
      if (newTime > 0) {
        MOVE_TIME = newTime;
        Serial.print("Motor timeout updated to: ");
        Serial.print(MOVE_TIME);
        Serial.println(" ms");
      } else {
        Serial.println("Invalid time. Must be greater than 0");
      }
    }
    Serial.println("_");
  }

  else if (command.startsWith("speed")) {
    // Check if command has a value after "speed"
    if (command.length() > 5) {  // "speed" is 5 characters
      String speedStr = command.substring(6);  // Get everything after "speed "
      int newSpeed = speedStr.toInt();
      
      if (newSpeed >= 0 && newSpeed <= 255) {
        MOTOR_SPEED = newSpeed;
        Serial.print("Motor speed updated to: ");
        Serial.println(MOTOR_SPEED);
      } else {
        Serial.println("Invalid speed. Must be 0-255");
      }
    } else {
      // No value provided, prompt for input
      Serial.print("Current motor speed: ");
      Serial.println(MOTOR_SPEED);
      Serial.println("Note: Some motors won't turn until a certain PWM minimum is hit, so slowly increase and retest.");
      Serial.println("Enter new speed (0-255): ");
      
      while (Serial.available() == 0) {
        yield();
      }
      
      String speedInput = Serial.readStringUntil('\n');
      speedInput.trim();
      int newSpeed = speedInput.toInt();
      
      if (newSpeed >= 0 && newSpeed <= 255) {
        MOTOR_SPEED = newSpeed;
        Serial.print("Motor speed updated to: ");
        Serial.println(MOTOR_SPEED);
      } else {
        Serial.println("Invalid speed. Must be 0-255");
      }
    }
    Serial.println("_");
  }
  
  else if (command == "close") {
    Serial.println("Closing door...(Anti-Clockwise)");
    currentState = MOVING_CLOSE;
    if (SWAP_DIRECTION) {
      turnClockwise();
    }

    else {
      turnAnticlockwise();
    } 
    currentState = IDLE;
  }

  else if (command.equalsIgnoreCase("?")) {
     Serial.println("**************************************************************************************************************");
     Serial.println("                                                HELP                                                          ");
     Serial.println("**************************************************************************************************************");
     Serial.println("The available commands you can enter are 'Open', 'Close', 'Stop, 'Status', 'Speed', 'Time', 'Kill', or Enter '?' for help.");
     Serial.println("They are not case sensitive");
     Serial.println("");
     Serial.println("OPEN - Runs the stepper motor Anti-clockwise and stops when the Open switch (Connected to D5) is pulled LOW");
     Serial.println("CLOSE - Runs the stepper motor Clockwise and stops when the Closed switch (Connected to D6) is pulled LOW");
     Serial.println("Note: The Motor will automatically stop if a limit switch is not triggered within your defined MOVE_TIME variable seconds");
     Serial.println("STOP - Stops the motor");
     Serial.println("STATUS - Reports the End Stop values to determine door position along with the current motor speed and duration settings.");
     Serial.println("SPEED - Change the motor speed variable");
     Serial.println("TIME - Change the motor movement duration");
     Serial.println("KILL - Immediately stops all actions and reboots the ESP");
     Serial.println("? - Displays this text");
     Serial.println("**************************************************************************************************************");
     Serial.println("_");
  }

  else if (command == "status") {
      Serial.println("=== MOTOR SETTINGS ===");
      Serial.print("MOTOR_SPEED [0-255]: ");
      Serial.println(MOTOR_SPEED);
      Serial.print("MOTOR_DURATION [0-255]: ");
      Serial.println(MOVE_TIME);
      Serial.println("=====================");

    Serial.println("=== Door Status ===");
    Serial.print("Open endstop: ");
    Serial.println(digitalRead(ENDSTOP_OPEN) == LOW ? "TRIGGERED" : "Open");
    Serial.print("Close endstop: ");
    Serial.println(digitalRead(ENDSTOP_CLOSE) == LOW ? "TRIGGERED" : "Open");
    if (digitalRead(ENDSTOP_OPEN) == LOW) {
      Serial.println("Door is open (or at least it should be)");
    } 
    else if (digitalRead(ENDSTOP_CLOSE) == LOW) {
      Serial.println("Door is closed (or at least it should be)");
    } 
    else {
      Serial.println("Door is between positions (....maybe, who knows?)");
    }
    Serial.println("=====================");
  }
  else {
    Serial.println("Unknown command. Valid Commands are 'Open', 'Close', 'Stop', 'Status, 'Speed', 'Time','Kill', or Enter '?' for help ");
  }

  }

}

//--------------------------------------------------------- HANDLING OF SERIAL END 

//--------------------------------------- | FUNCTIONS END | ----------------------------------------------**

//------------============================== | MAIN LOOP | ==============================------------**

void loop() {
  handleSerialCommands();
}

//--------------------------------------- | MAIN LOOP END | ----------------------------------------------**

// ******************************************************
//  Written by Luke Pan z5420133
//  Program to test H-bridge and moonitor PID controller 
//  - Atomcraft
//
// ******************************************************

// PIN Constants and Definitions
constexpr int TRIGGER = 2;
constexpr int PWMPIN_1 = 5;
constexpr int PWMPIN_2 = 11;
constexpr float MAX_CURRENT = 8.0;
constexpr int HALL_SENSOR_PIN = A0;

// Hall sensor variables
int sensorOffset =  0;

// Set duty cycle for the TF coil
float dutyCycle = 0;

// Declare PID variables
float error = 0;
float errorSum = 0;
float errorDif = 0;
float oldError = 0;

float rawVal = 0;
float currentVal = 0;
float currentValDamped = 0;
float refVal = 0;

float kp = 1;
float ki = 0.00035;
float kd = 0;

float sampTimeBegin = 0;
float sampTimeEnd = 0;
float sampPeriod = 0;

// Pulse tracking variables
int executePulse = 0;
int pulseStarted = 0;
int pulseTimeStart = 0;
int pulseTime = 0;

int timeStep = 0;
int oldTimeStep = 0;

// Declare data logging array
constexpr unsigned int MAX_ARRAY_SIZE = 200;
short numControlCycles = 0;
short period = 20;
short periodCount = 0;
unsigned short timeStamp[MAX_ARRAY_SIZE];
float currentData[MAX_ARRAY_SIZE];
unsigned short PWMData[MAX_ARRAY_SIZE];


// ******************************************************
// Setup
// ******************************************************
void setup() {
  Serial.begin(115200);

  // Set pin modes
  pinMode (TRIGGER, INPUT);      // Button to start pulse
  pinMode (PWMPIN_1, OUTPUT);    // Timer 2 "B" output: OC2B
  pinMode (PWMPIN_2, OUTPUT);    // Timer 2 "A" output: OC2A
  
  // Set OC2A on Compare Match when up-counting, clear OC2B on Compare Match when up-counting.
  // Some bit-selecting to configure the off phase PWM signals on the arduino (need to use two timers)
  TCCR2A = bit (WGM20) | bit (COM2B1) | bit (COM2A1) | bit (COM2A0);
  TCCR2B = bit (CS10);         // phase correct PWM, prescaler of 8

  // Set OC0A on Compare Match when up-counting, clear OC0B on Compare Match when up-counting.
  TCCR0A = bit (WGM20) | bit (COM2B1) | bit (COM2A1) | bit (COM2A0);       
  TCCR0B = bit (CS10);         // phase correct PWM, prescaler of 8
  // Inverts signal
  TCCR0A = 0b10110000 | (TCCR0A & 0b00001111);

  // Calibrate sensor
  sensorOffset = calibrateHallSensor();
  Serial.println("Running...");
}


// ******************************************************
// Main, enable PWM output for the TF coil
// ******************************************************
void loop() {
  // Get current measurement and convert
  rawVal = (analogRead(HALL_SENSOR_PIN) - sensorOffset);
  //currentVal = -(((rawVal - 512) / 1024) * 5.0 / 0.186);
  currentVal = -(((rawVal - 380) / 760) * 3.3 / 0.0624); // Gains for the On-Board H-bridge current sensor
  currentValDamped =  currentVal;

  // Set reference current
  executePulse = digitalRead(TRIGGER);
  if (executePulse == HIGH & pulseStarted == LOW) {
    pulseTimeStart = millis() / 32; // We modified timer so need to adjust millis
    pulseStarted = HIGH;
    numControlCycles = 0;
    periodCount = 0;
  }
  if (pulseStarted == HIGH) {
    //Save data points to array
    pulseTime = (millis() / 32 - pulseTimeStart);
    if (numControlCycles < MAX_ARRAY_SIZE) {
      timeStamp[numControlCycles] = millis() / 32 - pulseTimeStart;
      currentData[numControlCycles] = currentValDamped;
      PWMData[numControlCycles] = dutyCycle;
      periodCount++;
      if (periodCount == period) {
        numControlCycles++;
        periodCount = 0;
      }
    } else {
      Serial.println("WARNING: Data buffer full!");
    }
  }

  //Reference current shape   
  if ((pulseTime < 250) && (pulseStarted == HIGH)) {
    refVal = pulseTime * MAX_CURRENT/250;
  } else if ((pulseTime < 750) && (pulseStarted == HIGH)) {
    refVal = MAX_CURRENT;
  } else if ((pulseTime < 1000) && (pulseStarted == HIGH)) {
    refVal = MAX_CURRENT - (pulseTime - 750) * MAX_CURRENT/250;
  } else if ((pulseTime >= 1000) && (pulseStarted == HIGH)) {
    //Prints data
    pulseStarted = LOW;
    refVal = 0;
    for (int i = 0; i < MAX_ARRAY_SIZE; i++) {
      Serial.print(timeStamp[i]);
      Serial.print(", ");
      if ((i < 25)) {
        Serial.print(i * MAX_CURRENT/25);
      } else if (i < 75) {
        Serial.print(MAX_CURRENT);
      } else if (i < 100) {
        Serial.print(MAX_CURRENT - (i - 75) * MAX_CURRENT/25);
      } else if (i >= 100) {
        Serial.print(0);
      }
      Serial.print(", ");
      Serial.print(currentData[i]);
      Serial.print(", ");
      Serial.println(PWMData[i]);
    }
    numControlCycles = 0;
  } else {
    refVal = 0;
  }

  sampTimeEnd = micros();
  sampPeriod = sampTimeEnd - sampTimeBegin;
  sampTimeBegin = micros(); // Do not leave for over 8mins

  // Calculate errors
  error = refVal - currentValDamped;
  if (pulseStarted == HIGH) {
    errorSum += error;
    errorSum = constrain(errorSum, -100, 100);
  } else {
    errorSum = 0;
  }

  errorDif = error - oldError;

  // Calculate gains and output
  dutyCycle = (kp * error) + (ki * errorSum * sampPeriod) + (kd * errorDif / sampPeriod);
  dutyCycle = constrain(dutyCycle, 0, 127);

  // Send output
  //dutyCycle = 127;
  if (refVal == 0) {
    OCR2A = byte(0);
    OCR2B = byte(0);

    OCR0A = byte(0);
    OCR0B = byte(0);     // keep off
  } else {
    OCR2A = byte(dutyCycle + 128);           // duty cycle out of 255 
    OCR2B = 255 - byte(dutyCycle + 128);     // duty cycle out of 255

    OCR0A = byte(dutyCycle + 128);           // duty cycle out of 255 
    OCR0B = 255 - byte(dutyCycle + 128);     // duty cycle out of 255
  }

  // Calculate next PID step values
  oldError = error;

  // Debugging
  // Serial.println(count);
  // Serial.print(currentValDamped, 8);
  // Serial.print(", ");
  // Serial.print(refVal, 8);
  // Serial.print(", ");
  // Serial.print(kp * error, 8);
  // Serial.print(", ");
  // Serial.print(ki * errorSum * sampPeriod, 8);
  // Serial.print(", ");
  // Serial.print(kd * errorDif / sampPeriod, 8);
  // Serial.print(", ");
  // Serial.print(count);
  // Serial.print(", ");
  // Serial.print(refVal, 8);
  // Serial.print(", ");
  // Serial.println(dutyCycle);
}

double calibrateHallSensor() {
  double sensorOffset = 0;
  short numSamples = 30;
  for (int i = 0; i < numSamples; i++) {
    //sensorOffset = sensorOffset + analogRead(HALL_SENSOR_PIN) - 512;
    sensorOffset = sensorOffset + analogRead(HALL_SENSOR_PIN) - 380; // Minus 380 to zero the sensor from the halfway setpoint
    delay(100);
    Serial.println("Calibrating...");
  } 
  return sensorOffset / numSamples;
}

#include <Arduino.h>

// --- Pin Definitions (Common Anode 3.3V) ---
const uint8_t PIN_PUL = 12; // GPIO12 -> DM556 PUL-
const uint8_t PIN_DIR = 13; // GPIO13 -> DM556 DIR-
const uint8_t PIN_ENA = 15; // GPIO15 -> DM556 ENA- (or leave disconnected)

const uint8_t BTN_SPEED = 4; // GPIO4 -> Speed Button
const uint8_t BTN_DIR   = 5; // GPIO5 -> Direction Button

// --- Motor & Microstepping Configuration ---
const uint16_t STEPS_PER_REV = 1600; // Match DM556 DIP switch setting (e.g., 1/8 microstep)

// --- Speed Presets (RPM) ---
const uint16_t SPEED_PRESETS[] = {0, 50, 100, 200};
const uint8_t NUM_SPEEDS = sizeof(SPEED_PRESETS) / sizeof(SPEED_PRESETS[0]);
uint8_t currentSpeedIndex = 0;

// --- Speed Ramping Variables ---
float targetRPM = 0;
float currentRPM = 0;
const float ACCEL_RATE = 250.0; // Acceleration in RPM per second

// --- Pulse Timing Variables ---
unsigned long stepIntervalUs = 0;
unsigned long lastStepTimeUs = 0;
unsigned long lastRampTimeUs = 0;

// --- State & Debounce ---
bool motorDirection = true;
unsigned long lastDebounceSpeed = 0;
unsigned long lastDebounceDir = 0;
const unsigned long DEBOUNCE_DELAY = 200;

void updateRamp();
void checkButtons();

void setup() {
  Serial.begin(115200);
  delay(100);

  digitalWrite(PIN_PUL, HIGH);
  digitalWrite(PIN_DIR, motorDirection ? LOW : HIGH);
  digitalWrite(PIN_ENA, HIGH); // Driver enabled

  pinMode(PIN_PUL, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ENA, OUTPUT);

  pinMode(BTN_SPEED, INPUT_PULLUP);
  pinMode(BTN_DIR, INPUT_PULLUP);

  Serial.println(F("\n--- DM556 with Acceleration Ramping ---"));
}

void loop() {
  checkButtons();
  updateRamp();

  // Generate pulse train if motor is moving
  if (currentRPM > 1.0) {
    unsigned long currentUs = micros();
    if (currentUs - lastStepTimeUs >= stepIntervalUs) {
      lastStepTimeUs = currentUs;

      // Negative pulse trigger (3.3V Common Anode)
      digitalWrite(PIN_PUL, LOW);
      delayMicroseconds(5); 
      digitalWrite(PIN_PUL, HIGH);
    }
  }
}

// Smoothly adjusts currentRPM toward targetRPM
void updateRamp() {
  unsigned long currentUs = micros();
  float dt = (currentUs - lastRampTimeUs) / 1000000.0; // Time delta in seconds
  
  if (dt < 0.002) return; // Update ramp every 2ms for efficiency
  lastRampTimeUs = currentUs;

  if (abs(currentRPM - targetRPM) > 0.5) {
    if (currentRPM < targetRPM) {
      currentRPM += ACCEL_RATE * dt;
      if (currentRPM > targetRPM) currentRPM = targetRPM;
    } else if (currentRPM > targetRPM) {
      currentRPM -= ACCEL_RATE * dt;
      if (currentRPM < targetRPM) currentRPM = targetRPM;
    }

    // Calculate pulse interval for current RPM
    float stepsPerSec = (currentRPM * STEPS_PER_REV) / 60.0;
    if (stepsPerSec > 0) {
      stepIntervalUs = (unsigned long)(1000000.0 / stepsPerSec);
    }
  } else {
    currentRPM = targetRPM;
  }
}

void checkButtons() {
  unsigned long now = millis();

  // Speed Button (GPIO4)
  if (digitalRead(BTN_SPEED) == LOW && (now - lastDebounceSpeed > DEBOUNCE_DELAY)) {
    lastDebounceSpeed = now;
    currentSpeedIndex = (currentSpeedIndex + 1) % NUM_SPEEDS;
    targetRPM = SPEED_PRESETS[currentSpeedIndex];

    Serial.print(F("Target Speed set to: "));
    Serial.print(targetRPM);
    Serial.println(F(" RPM"));
  }

  // Direction Button (GPIO5) - Change only when target is 0 or low speed for safety
  if (digitalRead(BTN_DIR) == LOW && (now - lastDebounceDir > DEBOUNCE_DELAY)) {
    lastDebounceDir = now;
    motorDirection = !motorDirection;

    digitalWrite(PIN_DIR, motorDirection ? LOW : HIGH);

    Serial.print(F("Direction toggled: "));
    Serial.println(motorDirection ? F("CLOCKWISE") : F("COUNTER-CLOCKWISE"));
  }
}
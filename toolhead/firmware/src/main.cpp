#include <Wire.h>

// ── TMC2209 Pins ──────────────────────────────────────────────────────────────
#define M0_STEP_PIN     PA3
#define M0_DIR_PIN      PA4
#define M0_EN_PIN       PA5

// ── Motor Constants ───────────────────────────────────────────────────────────
#define STEPS_PER_REV   200
#define MICROSTEPS        8
#define STEP_DELAY_US   500
#define DIR_SETUP_US     20

// ── I2C ───────────────────────────────────────────────────────────────────────
#define I2C_ADDRESS     0x15

// ── Absolute position tracking ────────────────────────────────────────────────
volatile bool    pendingMove      = false;
volatile int16_t pendingTargetDeg = 0;
volatile bool    busy             = false;

int16_t currentPosDeg = 0;   // non-volatile: only touched in loop()

// ─────────────────────────────────────────────────────────────────────────────
// I2C Frame layout:
//   1 byte  → uint8_t  → target absolute position in degrees (0 to 255)
//   2 bytes → int16_t  → target absolute position in degrees (–32768 to +32767)
//             big-endian: byte[0] = high byte, byte[1] = low byte
// ─────────────────────────────────────────────────────────────────────────────
void onReceive(int numBytes) {
  if (numBytes < 1) {
    while (Wire.available()) Wire.read();
    return;
  }

  int16_t targetDeg = 0;

  if (numBytes >= 2) {
    // Two bytes: int16_t, big-endian
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    targetDeg = (int16_t)((uint16_t)hi << 8 | lo);
  } else {
    // One byte: uint8_t — zero-extend into int16_t (0–255)
    targetDeg = (int16_t)(uint8_t)Wire.read();
  }

  // Drain any unexpected extra bytes
  while (Wire.available()) Wire.read();

  if (!busy) {
    pendingTargetDeg = targetDeg;
    pendingMove      = true;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
void rotateDegrees(float degrees, unsigned int speed_us = STEP_DELAY_US) {
  if (degrees == 0) return;

  digitalWrite(M0_DIR_PIN, degrees > 0 ? LOW : HIGH);
  delayMicroseconds(DIR_SETUP_US);

  if (degrees < 0) degrees = -degrees;

  long steps = lround((degrees / 360.0f) * STEPS_PER_REV * MICROSTEPS);
  for (long i = 0; i < steps; i++) {
    digitalWrite(M0_STEP_PIN, HIGH);
    delayMicroseconds(speed_us);
    digitalWrite(M0_STEP_PIN, LOW);
    delayMicroseconds(speed_us);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  pinMode(M0_STEP_PIN, OUTPUT);
  pinMode(M0_DIR_PIN,  OUTPUT);
  pinMode(M0_EN_PIN,   OUTPUT);
  digitalWrite(M0_STEP_PIN, LOW);
  digitalWrite(M0_DIR_PIN,  LOW);
  digitalWrite(M0_EN_PIN,   LOW);   // enable TMC2209

  currentPosDeg = 0;                // absolute origin

  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(onReceive);
  Wire.onRequest([]() {
    // Return current position as int16_t big-endian
    Wire.write((uint8_t)(currentPosDeg >> 8));
    Wire.write((uint8_t)(currentPosDeg & 0xFF));
  });
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  if (pendingMove) {
    pendingMove = false;
    busy        = true;

    int16_t target = pendingTargetDeg;
    int16_t delta  = target - currentPosDeg;

    rotateDegrees((float)delta);
    currentPosDeg = target;

    busy = false;
  }
}

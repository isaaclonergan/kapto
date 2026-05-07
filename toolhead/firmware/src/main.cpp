#include <Wire.h>

// ── TMC2209 Pins ───────────────────────────────────────────────────────────────
#define M0_STEP_PIN   PA3
#define M0_DIR_PIN    PA4
#define M0_EN_PIN     PA5

// ── Motor Constants ────────────────────────────────────────────────────────────
#define STEPS_PER_REV   200
#define MICROSTEPS        8
#define STEP_DELAY_US   500
#define DIR_SETUP_US    20    // TMC2209 DIR setup time before first STEP

// ── I2C ───────────────────────────────────────────────────────────────────────
#define I2C_ADDRESS      0x15

volatile bool    pendingMove    = false;
volatile float   pendingDegrees = 0;
volatile bool    busy           = false;  // guard against re-entrancy

// ─────────────────────────────────────────────────────────────────────────────
// Frame layout (2 bytes):
//   Byte 0 – vacuum control
//     bit 0 (0x01): turn vacuum ON
//     bit 1 (0x02): turn vacuum OFF
//   Byte 1 – motor control (unchanged)
//     bit 7:    direction (1 = CW, 0 = CCW)
//     bits 6-0: degrees (0–127)
// ─────────────────────────────────────────────────────────────────────────────
void onReceive(int numBytes) {
  if (numBytes < 1) {
		while (Wire.available()) Wire.read();
		return;
	}

	if (Wire.available() < 1) return;

  uint8_t motorFrame = Wire.read();   // byte 1: motor control

	// ── Motor bits ──
  bool    dirCW   = (motorFrame & 0x80) != 0;
  uint8_t degrees = (motorFrame & 0x7F);

  if (!busy) {
    pendingDegrees = dirCW ? (float)degrees : -(float)degrees;
    pendingMove    = true;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
void rotateDegrees(float degrees, unsigned int speed_us = STEP_DELAY_US) {
  if (degrees == 0) return;
  digitalWrite(M0_DIR_PIN, degrees > 0 ? HIGH : LOW);
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
  // Stepper
  pinMode(M0_STEP_PIN, OUTPUT);
  pinMode(M0_DIR_PIN,  OUTPUT);
  pinMode(M0_EN_PIN,   OUTPUT);
  digitalWrite(M0_STEP_PIN, LOW);
  digitalWrite(M0_DIR_PIN,  LOW);
  digitalWrite(M0_EN_PIN,   LOW);   // enable TMC2209

  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(onReceive);
  Wire.onRequest([]() { Wire.write(0x00); });
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  // Handle motor move
  if (pendingMove) {
    pendingMove = false;
    float deg = pendingDegrees;
    busy = true;
    rotateDegrees(deg);
    busy = false;
  }
}

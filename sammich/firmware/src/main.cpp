#include <Wire.h>

#define SCL2 PB10
#define SDA2 PB11

#define I2C_ADDR  11

static byte x = 1;

TwoWire Wire1 = TwoWire(SDA2, SCL2);

void setup()
{
  Wire.begin();		// first i2c peripheral
	Wire1.begin();	// second i2c peripheral
}

void loop()
{
  Wire.requestFrom(I2C_ADDR, 1);

  while(Wire.available())
  {
    Wire.read();
  }

  delay(10);

  Wire.beginTransmission(I2C_ADDR);
  Wire.write(x);
  Wire.endTransmission();
  x++;

	Wire1.requestFrom(I2C_ADDR, 1);

	while (Wire1.available())
	{
		Wire1.read();
	}

	delay(10);

	Wire1.beginTransmission(I2C_ADDR);
	Wire1.write(x);
	Wire1.endTransmission();
	x++;

  delay(500);
}

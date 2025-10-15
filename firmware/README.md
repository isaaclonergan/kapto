# Basic instructions
```
python -m venv venv
source venv/bin/activate
pip install platformio
./compile.sh
```

After compilation, find .bin file and flash

# Sending I2C messages

I2C messages can be sent from Marlin with the following steps
```
M260 A<I2C address in DECIMAL>
M260 B<First byte of data>
M260 B<Second byte of data>
...
M260 B<Last byte of data>
M260 S
```

The first line of this block is the I2C address to send data to in its decimal representation. The following lines are adding bytes to the I2C buffer that will be sent. The final line sends the whole buffer to the target address.

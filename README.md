# Measure pressure and temperature by using LPS331AP via I2C bus.

## Usage

$ make

$ ./sense_lps331ap I2C_BUS DEVICE_ADDR_MODE
PRESS: 1011.02

TEMP: 36.50

if you use BagleBone Black and use P9_19-20, I2C_BUS is 1.

if you set SA0 to GND, DEVICE_ADDR_MODE is 0, otherwise 1.


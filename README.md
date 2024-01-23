# M5Stack PaHUB/PaHUB2 Unit ESP-IDF Component

This is a component library for use with the M5Stack PaHub or PaHUB2 unit on the Core2 for AWS IoT Kit.

The PaHUB uses a TCA9548A and the PaHUB2 uses a PCA9548APW low-voltage i2c multiplexer for switching. There are 6 available ports (channels) to use multiple of the same or different i2c peripherals on the hub.

## Usage
For example, to make a request on a i2c peripheral connected to channel 1 on the hub that has a i2c address of 0x40, you'd:
1. Switch to channel 1 using `unit_pahub_channel_set( UNIT_PAHUB_CHANNEL_1 );`
2. Make a i2c request as you would normally using either the Core2 for AWS library abstraction or native to ESP-IDF. To use the simplified Core for AWS library to perform a 1 byte read into a uint8_t variable named "data" in the peripheral's 0x01 register, call `core2foraws_expports_i2c_read( 0x40, 0x01, &data, 1 );` 
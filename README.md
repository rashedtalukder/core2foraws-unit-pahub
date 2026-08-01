# M5Stack Unit PaHub v2.0 ESP-IDF Component

Driver for the six-port [M5Stack Unit PaHub v2.0](https://docs.m5stack.com/en/unit/pahub2) (U040-B). The board uses a PCA9548AP-compatible I2C switch at `0x70`; only channels `0..5` are routed to connectors.

The driver serializes channel selection and downstream transactions. Use `unit_pahub_i2c_read()` and `unit_pahub_i2c_write()` when multiple tasks or devices share the hub.

## Usage

```c
#include "core2foraws.h"
#include "unit_pahub.h"

i2c_master_dev_handle_t sensor = NULL;

ESP_ERROR_CHECK( core2foraws_expports_i2c_device_add(
    0x40, 100000, &sensor ) );
ESP_ERROR_CHECK( unit_pahub_init() );

uint8_t data = 0;
ESP_ERROR_CHECK( unit_pahub_i2c_read(
    UNIT_PAHUB_CHANNEL_1, sensor, 0x01, &data, 1 ) );
```

Call `unit_pahub_deinit()` only after all downstream users have stopped. The driver may block for up to one second while waiting for its mutex.

## API

- `unit_pahub_init()` / `unit_pahub_deinit()` manage the mux device and mutex.
- `unit_pahub_channel_set()` selects one channel directly.
- `unit_pahub_channel_get()` reads the hardware mask and returns `0xFF` unless exactly one board channel is active.
- `unit_pahub_i2c_read()` / `unit_pahub_i2c_write()` atomically select a channel and transfer data.

See [datasheet/pahub.md](datasheet/pahub.md) for board behavior and [datasheet/pca9548a.md](datasheet/pca9548a.md) for the switch protocol.

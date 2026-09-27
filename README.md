# M5Stack Unit PaHub v2.0 ESP-IDF Component

Driver for the six-port [M5Stack Unit PaHub v2.0](https://docs.m5stack.com/en/unit/pahub2) (U040-B) on the Core2 for AWS IoT Kit. The unit plugs into Port A (external I2C: GPIO 32 SDA, GPIO 33 SCL) and carries a PCA9548A-compatible switch. Only channels `0..5` are routed to connectors J1-J6.

## Features

- Port A bring-up, hub presence check and all-ports-off at init.
- Reference-counted `init`/`deinit`, so every unit driver behind the hub can own it independently.
- Channel select plus transfer is atomic against **every** Port A user, because the driver serialises on the BSP's recursive bus lock rather than a private mutex.
- Cached selection: the control byte is written only when the port changes.
- Self-healing: if a transfer on an already-selected port is NACKed and a read-back shows the hub lost its selection (power glitch, hot-plug), the port is reselected and the transfer retried once.
- Bounded failure cost: control writes are retried only after a NACK, never after a timeout, so a dead bus is not held for several transfer timeouts.
- Scoped exclusive access with `unit_pahub_lock()` / `unit_pahub_unlock()` for multi-transfer sequences.
- Downstream probing and a per-port scan to locate identical sensors.
- Automatic bus recovery (`CONFIG_PAHUB_AUTO_RECOVER`): a timeout clocks out a stuck SDA line; a timed-out control write is then retried once. `unit_pahub_recover()` does the same on demand and resynchronises the hub.
- Optional deselect-when-idle for trees where a device on Port A shares an address with one behind the hub.

## Usage

```c
#include "core2foraws.h"
#include "unit_pahub.h"

i2c_master_dev_handle_t sensor = NULL;

ESP_ERROR_CHECK( unit_pahub_init() );
ESP_ERROR_CHECK( core2foraws_expports_i2c_device_add( 0x44, 100000, &sensor ) );

uint8_t data = 0;
ESP_ERROR_CHECK( unit_pahub_i2c_read( UNIT_PAHUB_CHANNEL_1, sensor, 0x01, &data, 1 ) );
```

Locate a sensor without hard-coding its port:

```c
uint8_t ports = 0;
if( unit_pahub_scan( 0x44, &ports ) == ESP_OK && ports != 0 )
{
    uint8_t channel = ( uint8_t )__builtin_ctz( ports );
    unit_pahub_i2c_read( channel, sensor, 0x01, &data, 1 );
}
```

Hold a port for a sequence that must not be interleaved:

```c
if( unit_pahub_lock( UNIT_PAHUB_CHANNEL_2 ) == ESP_OK )
{
    unit_pahub_i2c_write( UNIT_PAHUB_CHANNEL_2, dev, 0x10, cmd, sizeof( cmd ) );
    unit_pahub_i2c_read( UNIT_PAHUB_CHANNEL_2, dev, 0x20, buf, sizeof( buf ) );
    unit_pahub_unlock();
}
```

While holding the lock, use only the `unit_pahub_*` transfer and probe functions. The `core2foraws_expports_i2c_*` functions take a BSP lock in the opposite order and can time out. Every other Port A user waits while the lock is held, so keep holds short.

## API

| Function | Purpose |
|---|---|
| `unit_pahub_init()` / `unit_pahub_deinit()` | Acquire/release a reference. The first init opens Port A, turns every port off and verifies the hub answers. The last deinit turns ports off (best effort) and releases the device. |
| `unit_pahub_i2c_read()` / `unit_pahub_i2c_write()` | Select a port if needed and transfer, atomically. |
| `unit_pahub_lock()` / `unit_pahub_unlock()` | Exclusive Port A access with a port selected. Nestable per task. |
| `unit_pahub_channel_set()` / `unit_pahub_channel_get()` | Route one port / read which single port is routed (`UNIT_PAHUB_CHANNEL_NONE` otherwise). |
| `unit_pahub_mask_set()` / `unit_pahub_mask_get()` | Write/read the raw control register (bits 0-5). |
| `unit_pahub_disable_all()` | Turn every port off. |
| `unit_pahub_probe()` | Check for an address ACK on one port. |
| `unit_pahub_scan()` | Bitmask of ports where an address answers. |
| `unit_pahub_recover()` | Clear a hung bus and resynchronise the hub. |

Functions wait up to one second for Port A before returning `ESP_ERR_TIMEOUT`.

## Configuration

| Kconfig | Default | Notes |
|---|---|---|
| `CONFIG_PAHUB_ADDRESS` | `0x70` | Match the A0-A2 solder straps (`0x70..0x77`). `0x70` clashes with the ENV III QMP6988. |
| `CONFIG_PAHUB_I2C_SPEED_HZ` | `100000` | Control-register clock, up to 400 kHz if the whole Port A tree meets Fast-mode rise times. |
| `CONFIG_PAHUB_DESELECT_WHEN_IDLE` | `n` | Turn every port off after each transfer or lock hold. Costs one control write per transfer. |
| `CONFIG_PAHUB_AUTO_RECOVER` | `y` | Clear a hung bus after a timeout. Disable only if another I2C master shares Port A. |

## Migrating from pre-1.0 releases

- `unit_pahub_init()` now fails with `ESP_ERR_NOT_FOUND` or `ESP_ERR_INVALID_RESPONSE` when no PCA9548A answers, instead of only logging a warning.
- `unit_pahub_init()` opens Port A itself; calling `core2foraws_expports_i2c_begin()` first is no longer required.
- Init and deinit are reference counted. Balance every successful `unit_pahub_init()` with one `unit_pahub_deinit()`.
- `unit_pahub_deinit()` no longer fails when the hub cannot be reached; it fails only if the device cannot be released.

See [datasheet/pahub.md](datasheet/pahub.md) for board behavior and [datasheet/pca9548a.md](datasheet/pca9548a.md) for the switch protocol.

## License

Copyright 2024-2026 Rashed Talukder. Licensed under the [Apache License 2.0](LICENSE).

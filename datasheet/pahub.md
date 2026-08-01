# Unit PaHub v2.0 (SKU: U040-B) Firmware Build Specification

## Source Evidence and Precedence

Verified on 2026-08-01 against the official [M5Stack Unit PaHub v2.0 page](https://docs.m5stack.com/en/unit/pahub2), official board imagery, and checked-in TI `pca9548a.pdf` revision G (SHA-256 `fb203781fbac2d5de118ad64b15f6a6a7c93eb1121a2d00af3e6daa7f555d5b7`). M5Stack controls the six exposed ports, fitted PCA9548AP, and default address; TI controls switch protocol, timing, and electrical limits. See `schema.yml`.

## Purpose

This file is a board-level firmware implementation reference for **M5Stack Unit PaHub v2.0**, which uses a **Texas Instruments PCA9548A / PCA9548AP 8-channel I2C switch** to expand one upstream HY2.0-4P I2C port into **six populated downstream HY2.0-4P I2C ports**.

It is intended to be sufficient for implementing a correct driver and board-support integration for this specific module without needing to re-open the product page, board imagery, or chip datasheet.

---

## 1. Board Summary

### 1.1 Function

Unit PaHub v2.0 is an I2C fan-out / same-address coexistence module. Firmware selects one downstream channel at a time so multiple identical I2C slave devices that share the same slave address can coexist on separate channels.

### 1.2 Core IC

- Device: **PCA9548A / PCA9548AP**
- Function: **8-channel bidirectional translating I2C switch**
- Upstream bus: `SCL`, `SDA`
- Downstream switch channels on IC: `SC0/SD0` through `SC7/SD7`
- Board population: **channels 0 through 5 only are routed to external connectors**
- `SC6/SD6` and `SC7/SD7` are not exposed on this board revision

### 1.3 Product-Level Facts

- Module name: **Unit PaHub v2.0**
- SKU: **U040-B**
- Upstream connector type: **HY2.0-4P / Grove compatible**
- Downstream connectors: **6 × HY2.0-4P / Grove compatible**
- Intended use: I2C expansion and same-address device coexistence by channel polling
- Default chip address on module: **0x70**
- Address is hardware-modifiable through resistor options on `A0`, `A1`, `A2`

---

## 2. Physical / Connector Mapping

## 2.1 Upstream Connector Pinout

Product pin map for the HY2.0-4P connector:

| Signal Group | Black | Red | Yellow | White |
|---|---:|---:|---:|---:|
| PORT.A | GND | 5V | SDA | SCL |

### 2.2 Upstream Board Connector

The upstream connector is the board input from the host MCU.

| Connector | Pin | Signal |
|---|---:|---|
| J7 | 1 | IIC_SCL |
| J7 | 2 | IIC_SDA |
| J7 | 3 | VCC |
| J7 | 4 | GND |

### 2.3 Downstream Connector Mapping

The product page and board imagery show six downstream connectors.

| External Port | Connector | PCA9548A channel | Clock net | Data net |
|---|---|---:|---|---|
| Port 0 | J1 | 0 | SC0 | SD0 |
| Port 1 | J2 | 1 | SC1 | SD1 |
| Port 2 | J3 | 2 | SC2 | SD2 |
| Port 3 | J4 | 3 | SC3 | SD3 |
| Port 4 | J5 | 4 | SC4 | SD4 |
| Port 5 | J6 | 5 | SC5 | SD5 |

### 2.4 Unused IC Channels on This Module

The PCA9548A supports 8 channels total, but PaHub v2.0 exposes only 6 connectors.

| PCA9548A channel | Board status |
|---:|---|
| 6 | not routed to external connector |
| 7 | not routed to external connector |

Firmware should still treat the underlying control register as 8 bits wide, but board-level APIs should normally only expose channels `0..5`.

---

## 3. Board Hardware Topology

## 3.1 Address-Strap Network

The product page states that `A0`, `A1`, and `A2` are resistor-modifiable; board imagery shows pull-down and link positions:

- Each address pin has a **10 kΩ pull-down to GND**
- Each address pin also has an optional **0 Ω link to VCC**
- Without the 0 Ω strap installed, the pin remains logic low
- Installing the 0 Ω strap forces that address pin high

### Default board state

Because the default board uses pull-downs and no high straps, the default module address is:

- `A2 = 0`
- `A1 = 0`
- `A0 = 0`
- **7-bit I2C address = 0x70**

## 3.2 Reset Wiring

The PCA9548A `RESET` pin is pulled up on the board and is not exposed as a separate host control signal on the Grove connector.

Implications:

- Normal recovery is via I2C writes or host-side power cycle of the module supply
- If the board enters a bus-fault condition, firmware usually cannot directly toggle chip reset unless the system can cycle the module power rail

## 3.3 Pull-Ups and Decoupling

From the populated board imagery:

- Upstream `SCL`, `SDA`, and `RESET` have pull-up resistors
- Downstream channel lines are pulled up to board VCC through resistor networks
- Local **100 nF decoupling capacitors** are placed on the board supply rails near connectors / device power distribution

Do not add strong redundant pull-ups in firmware assumptions. Actual bus behavior depends on the host board, cable length, and attached units.

---

## 4. PCA9548A Device Facts Used By Firmware

## 4.1 Supply and Interface Limits

- Supply voltage range: **2.3 V to 5.5 V**
- I2C clock frequency: **0 kHz to 400 kHz**
- Supported modes: **Standard Mode (100 kHz)** and **Fast Mode (400 kHz)**
- Power-up / reset default: **all channels deselected**

## 4.2 Important Behavioral Rules

1. The PCA9548A uses **one 8-bit control register**.
2. Each control-register bit enables the corresponding channel.
3. **Multiple channels may be enabled simultaneously** at the silicon level.
4. On this board, for same-address coexistence, firmware should normally enable **exactly one channel at a time**.
5. A channel becomes active **after a STOP condition** following the control-register write.
6. After reset or power-on reset, **all channels are disabled**.

---

## 5. I2C Addressing

## 5.1 7-bit Address Encoding

The PCA9548A fixed upper nibble is `1110`, with hardware-selectable `A2/A1/A0` bits.

Valid addresses:

| A2 | A1 | A0 | 7-bit address |
|---:|---:|---:|---:|
| 0 | 0 | 0 | 0x70 |
| 0 | 0 | 1 | 0x71 |
| 0 | 1 | 0 | 0x72 |
| 0 | 1 | 1 | 0x73 |
| 1 | 0 | 0 | 0x74 |
| 1 | 0 | 1 | 0x75 |
| 1 | 1 | 0 | 0x76 |
| 1 | 1 | 1 | 0x77 |

### Recommended board default constant

```c
#define PAHUB_I2C_ADDR_DEFAULT 0x70
```

---

## 6. Control Register

## 6.1 Register Width

- Width: **8 bits**
- Access: **read/write**
- Reset value: **0x00**

## 6.2 Bit Mapping

| Bit | Value when 1 | Board relevance |
|---:|---|---|
| B0 | enable channel 0 | routed to J1 |
| B1 | enable channel 1 | routed to J2 |
| B2 | enable channel 2 | routed to J3 |
| B3 | enable channel 3 | routed to J4 |
| B4 | enable channel 4 | routed to J5 |
| B5 | enable channel 5 | routed to J6 |
| B6 | enable channel 6 | IC only, not routed on board |
| B7 | enable channel 7 | IC only, not routed on board |

### Control-byte examples

| Meaning | Control byte |
|---|---:|
| all channels off | `0x00` |
| channel 0 only | `0x01` |
| channel 1 only | `0x02` |
| channel 2 only | `0x04` |
| channel 3 only | `0x08` |
| channel 4 only | `0x10` |
| channel 5 only | `0x20` |
| channels 0 and 1 both on | `0x03` |

### Recommended board policy

Even though the chip supports multiple simultaneous channels, the module’s intended use is same-address isolation. Preferred policy:

- write `0x00` to disconnect all channels
- write exactly one of `0x01, 0x02, 0x04, 0x08, 0x10, 0x20` to select a single external port

---

## 7. I2C Transactions Required By Firmware

## 7.1 Select a Channel

Write a single control byte to the device.

Transaction sequence:

1. START
2. send slave address + write bit
3. ACK from PCA9548A
4. send control byte
5. ACK from PCA9548A
6. STOP

### Example: select channel 3 on default address 0x70

- target address: `0x70`
- control byte: `0x08`

## 7.2 Disable All Channels

Same transaction, but control byte is `0x00`.

## 7.3 Read Back Current Channel Mask

The control register is readable. A read operation returns the current 8-bit channel mask.

Board-level expectation:

- bits 0..5 may be used by firmware
- bits 6..7 should normally remain `0` on this module

---

## 8. Timing Requirements Relevant to Driver Code

## 8.1 Supported I2C Clock Rates

- Standard Mode: `0 .. 100 kHz`
- Fast Mode: `0 .. 400 kHz`

## 8.2 Standard Mode Timing

| Parameter | Min |
|---|---:|
| SCL frequency | 0 to 100 kHz |
| SCL high time | 4 µs |
| SCL low time | 4.7 µs |
| data setup time | 250 ns |
| input rise time | 1000 ns |
| input fall time | 300 ns |
| bus free time between STOP and START | 4.7 µs |
| START setup time | 4.7 µs |
| START hold time | 4 µs |
| STOP setup time | 4 µs |
| max bus capacitance | 400 pF |

## 8.3 Fast Mode Timing

| Parameter | Min / Max |
|---|---:|
| SCL frequency | 0 to 400 kHz |
| SCL high time | 0.6 µs min |
| SCL low time | 1.3 µs min |
| data setup time | 100 ns min |
| input rise time | `20 + 0.1 * Cb` ns to 300 ns max |
| input fall time | `20 + 0.1 * Cb` ns to 300 ns max |
| output fall time | `20 + 0.1 * Cb` ns to 300 ns max |
| bus free time between STOP and START | 1.3 µs min |
| START setup time | 0.6 µs min |
| START hold time | 0.6 µs min |
| STOP setup time | 0.6 µs min |
| max bus capacitance | 400 pF |

## 8.4 Reset Timing

Chip-level reset timing:

- `RESET low pulse width >= 6 ns`
- `RESET recovery time to START >= 0 ns`
- `RESET to SDA clear typ/max behavior reported as 500 ns`

Board note: this pin is not normally MCU-controlled on this module.

---

## 9. Electrical / Integration Constraints

## 9.1 Supply

- PCA9548A operating range: **2.3 V to 5.5 V**
- Product connector provides **5 V** on the red wire according to the module pin map
- The chip is 5 V tolerant on I/O according to the datasheet summary

## 9.2 Bus Capacitance

Maximum I2C bus capacitance is **400 pF**.

This matters more when:

- long Grove cables are used
- many downstream modules are attached
- more than one channel is enabled at the same time

## 9.3 Pull-Up Equations

When sizing external pull-ups in a custom system using the raw PCA9548A design rules:

- Minimum pull-up resistance:

```text
Rp(min) = (VDPUX - VOL(max)) / IOL
```

- Maximum pull-up resistance:

```text
Rp(max) = tr / (0.8473 * Cb)
```

## 9.4 Multi-Channel Caveat

If multiple switch channels are enabled simultaneously, the total current on the upstream side is the sum of the currents through all active pull-ups, and total visible capacitance increases.

For the PaHub use case with identical same-address sensors, **single-channel selection is the safe default**.

---

## 10. Recommended Firmware Model

## 10.1 Driver Responsibilities

A correct PaHub firmware driver should provide:

1. board address configuration
2. channel select by index `0..5`
3. all-channels-off operation
4. read-back of the channel mask
5. optional scoped / RAII selection helper if using C++ or Rust
6. input validation that rejects board-invalid channels `6` and `7`

## 10.2 Internal Driver State

Recommended software state:

```c
struct pahub_state {
    uint8_t i2c_addr;          // 0x70..0x77
    uint8_t selected_mask;     // last written control byte
};
```

Optional cached values are acceptable, but software must tolerate desynchronization after power cycle.

## 10.3 Canonical Constants

```c
#define PAHUB_CHANNEL_0 0x01
#define PAHUB_CHANNEL_1 0x02
#define PAHUB_CHANNEL_2 0x04
#define PAHUB_CHANNEL_3 0x08
#define PAHUB_CHANNEL_4 0x10
#define PAHUB_CHANNEL_5 0x20
#define PAHUB_CHANNEL_6 0x40   /* chip only, not board-routed */
#define PAHUB_CHANNEL_7 0x80   /* chip only, not board-routed */
#define PAHUB_CHANNEL_NONE 0x00
```

---

## 11. Recommended API Surface

## 11.1 Minimal C API

```c
int pahub_init(struct pahub_state *dev, uint8_t i2c_addr);
int pahub_select_channel(struct pahub_state *dev, uint8_t channel_index);
int pahub_select_mask(struct pahub_state *dev, uint8_t mask);
int pahub_disable_all(struct pahub_state *dev);
int pahub_read_mask(struct pahub_state *dev, uint8_t *mask_out);
```

## 11.2 API Rules

- `pahub_init()` should default to disabling all channels.
- `pahub_select_channel()` should only accept `0..5` for this board.
- `pahub_select_mask()` may exist for generic PCA9548A compatibility, but board-level callers should not set bits 6 or 7.
- `pahub_disable_all()` should write `0x00`.

---

## 12. Reference Implementation Logic

## 12.1 Initialize

```text
1. set device address (default 0x70 unless overridden)
2. optionally probe the device with an address ACK
3. write 0x00 to ensure all channels are deselected
4. cache selected_mask = 0x00
```

## 12.2 Select One External Port

```text
input: channel_index in 0..5
mask = 1 << channel_index
write mask to control register
issue STOP
update cached selected_mask
```

## 12.3 Scan a Same-Address Device on Every Port

```text
for ch in 0..5:
    select ch
    delay only if downstream target requires it
    probe slave target address on downstream bus
    record whether device responds
```

## 12.4 Read Multiple Identical Sensors Safely

```text
for each channel used:
    select channel
    perform sensor transaction(s)
    optionally deselect all channels before moving on
```

---

## 13. Example Pseudocode

```c
int pahub_select_channel(struct pahub_state *dev, uint8_t channel_index)
{
    uint8_t mask;

    if (dev == NULL) {
        return -1;
    }

    if (channel_index > 5) {
        return -2; /* board-invalid channel */
    }

    mask = (uint8_t)(1u << channel_index);

    if (i2c_write(dev->i2c_addr, &mask, 1) != 0) {
        return -3;
    }

    dev->selected_mask = mask;
    return 0;
}
```

### Disable all

```c
int pahub_disable_all(struct pahub_state *dev)
{
    uint8_t value = 0x00;

    if (dev == NULL) {
        return -1;
    }

    if (i2c_write(dev->i2c_addr, &value, 1) != 0) {
        return -2;
    }

    dev->selected_mask = 0x00;
    return 0;
}
```

---

## 14. Failure Modes and Recovery

## 14.1 Common Runtime Problems

1. Wrong hardware address after resistor modification
2. More than one channel enabled while talking to same-address devices
3. Host assumes channel switch is active before STOP is sent
4. Bus lockup caused by a downstream device holding SDA low
5. Host scans the sensor before selecting the intended channel
6. Host forgets that only six external ports exist on this module

## 14.2 Recovery Recommendations

1. write `0x00` to disable all channels
2. retry single-channel selection
3. if module power can be switched, perform a power cycle
4. re-probe address `0x70..0x77` if address straps may have changed

---

## 15. Safety and Correctness Rules for Code Generation

1. Treat this board as **six-port**, not generic eight-port, unless the API explicitly targets the raw PCA9548A.
2. Default board address is **0x70**.
3. Do not assume channels 6 and 7 are usable external ports.
4. Always send a **STOP** after writing the control byte; activation occurs only after STOP.
5. Reset / power-on reset returns the control register to **0x00**.
6. Do not leave same-address devices on multiple channels enabled simultaneously unless that is explicitly intended.
7. Keep bus speed at or below **400 kHz**.
8. Respect the **400 pF** total bus capacitance limit.
9. If the board was reworked with address straps, use the new address from the `A2/A1/A0` combination.
10. Because board reset is not normally host-controlled, firmware recovery strategy should rely on I2C re-selection and system-level power cycling.

---

## 16. Board-Level Quick Reference

### External port to control-byte map

| Port | Mask | Hex |
|---:|---:|---:|
| 0 | `0000 0001` | `0x01` |
| 1 | `0000 0010` | `0x02` |
| 2 | `0000 0100` | `0x04` |
| 3 | `0000 1000` | `0x08` |
| 4 | `0001 0000` | `0x10` |
| 5 | `0010 0000` | `0x20` |

### Address quick table

| A2 A1 A0 | Address |
|---|---:|
| 000 | 0x70 |
| 001 | 0x71 |
| 010 | 0x72 |
| 011 | 0x73 |
| 100 | 0x74 |
| 101 | 0x75 |
| 110 | 0x76 |
| 111 | 0x77 |

---

## 17. Final Implementation Intent

A generated firmware driver for Unit PaHub v2.0 should behave like a small, deterministic board-support layer over the PCA9548A:

- initialize at the configured module address
- default to all ports disconnected
- switch one external port on demand
- expose six valid external channels
- preserve simple, explicit control flow so that downstream same-address device access is unambiguous

The most common mistake to avoid is treating the board like a passive hub. It is an **active channel switch** and firmware must deliberately select the target port before every downstream transaction sequence.
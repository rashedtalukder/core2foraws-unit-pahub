/*!
 * @file unit_pahub.h
 * @brief Driver for the M5Stack Unit PaHub v2.0 (U040-B) on the Core2 for AWS
 * IoT Kit
 *
 * @copyright Copyright 2024-2026 Rashed Talukder (https://rashedtalukder.com)
 *
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @see [Unit PaHub v2.0](https://docs.m5stack.com/en/unit/pahub2)
 * @version 1.0.0
 * @date 2026-09-26
 */

#ifndef UNIT_PAHUB_H
#define UNIT_PAHUB_H

#include <driver/i2c_master.h>
#include <esp_err.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 7-bit address of the hub on Port A.
 *
 * Set with the A0-A2 solder straps (0x70-0x77) and CONFIG_PAHUB_ADDRESS.
 * A downstream device must not use this address.
 */
#define UNIT_PAHUB_ADDR CONFIG_PAHUB_ADDRESS

/** @brief Number of downstream ports routed to connectors (J1-J6). */
#define UNIT_PAHUB_CHANNELS_NUM 6

/** @brief Control-register bits for every routed port. */
#define UNIT_PAHUB_CHANNEL_MASK_ALL 0x3F

/** @brief Returned by unit_pahub_channel_get() unless exactly one port is on. */
#define UNIT_PAHUB_CHANNEL_NONE 0xFF

/** @brief Downstream port indexes, matching the connector labels minus one. */
#define UNIT_PAHUB_CHANNEL_0 0
#define UNIT_PAHUB_CHANNEL_1 1
#define UNIT_PAHUB_CHANNEL_2 2
#define UNIT_PAHUB_CHANNEL_3 3
#define UNIT_PAHUB_CHANNEL_4 4
#define UNIT_PAHUB_CHANNEL_5 5

/**
 * @brief Open Port A, attach the hub and turn every port off.
 *
 * Reference counted: every driver that uses the hub may call it, and each
 * successful call must be balanced by unit_pahub_deinit(). The first call
 * writes 0x00 and reads it back, so a missing hub or a different device at
 * UNIT_PAHUB_ADDR fails here instead of on the first transfer.
 *
 * @return
 *  - ESP_OK on success.
 *  - ESP_ERR_NOT_FOUND if nothing acknowledges UNIT_PAHUB_ADDR.
 *  - ESP_ERR_INVALID_RESPONSE if the device there does not behave like a
 *    PCA9548A.
 *  - Another error from opening Port A or registering the device.
 */
esp_err_t unit_pahub_init( void );

/**
 * @brief Release one unit_pahub_init() reference.
 *
 * The last reference turns every port off (best effort, so an unplugged hub
 * does not block shutdown) and releases the device. On a release failure the
 * reference is kept and the call can be retried. A no-op when not initialized.
 *
 * @return
 *  - ESP_OK on success or when not initialized.
 *  - ESP_ERR_INVALID_STATE if the calling task still holds unit_pahub_lock().
 *  - Another error from releasing the device.
 */
esp_err_t unit_pahub_deinit( void );

/**
 * @brief Read from a device on a downstream port.
 *
 * Selects @p channel if the hub is not already routing it, then reads. The
 * selection and the transfer are atomic against every other Port A user. If
 * a transfer on an already-selected port is NACKed and a read-back shows the
 * hub lost its selection (for example after a power glitch), the port is
 * reselected and the transfer is retried once. With CONFIG_PAHUB_AUTO_RECOVER
 * a timeout clears the bus so the next call can succeed.
 *
 * @param[in]  channel          Port 0-5.
 * @param[in]  dev_handle       Handle from core2foraws_expports_i2c_device_add().
 * @param[in]  register_address Register, or CORE2FORAWS_I2C_NO_REG and
 *                              CORE2FORAWS_I2C_REG_16 flags as in the BSP.
 * @param[out] data             Destination buffer.
 * @param[in]  length           Bytes to read; must be non-zero.
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE before init,
 *         ESP_ERR_TIMEOUT if Port A stayed busy, or the transfer error.
 */
esp_err_t unit_pahub_i2c_read( uint8_t channel,
                               i2c_master_dev_handle_t dev_handle,
                               uint32_t register_address, uint8_t *data,
                               uint16_t length );

/**
 * @brief Write to a device on a downstream port.
 *
 * Same selection, atomicity and retry behaviour as unit_pahub_i2c_read().
 *
 * @param[in] channel          Port 0-5.
 * @param[in] dev_handle       Handle from core2foraws_expports_i2c_device_add().
 * @param[in] register_address Register, or CORE2FORAWS_I2C_NO_REG and
 *                             CORE2FORAWS_I2C_REG_16 flags as in the BSP.
 * @param[in] data             Bytes to write.
 * @param[in] length           Byte count; must be non-zero.
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE before init,
 *         ESP_ERR_TIMEOUT if Port A stayed busy, or the transfer error.
 */
esp_err_t unit_pahub_i2c_write( uint8_t channel,
                                i2c_master_dev_handle_t dev_handle,
                                uint32_t register_address, const uint8_t *data,
                                uint16_t length );

/**
 * @brief Take exclusive use of Port A with @p channel selected.
 *
 * Use it for multi-transfer sequences that must not be interleaved with
 * other Port A traffic. While held, issue transfers only through
 * unit_pahub_i2c_read(), unit_pahub_i2c_write() and unit_pahub_probe(), not
 * the core2foraws_expports_i2c_* functions, which take a BSP lock in the
 * opposite order. Keep the hold short: it blocks every other Port A user.
 * Nestable on the same task; each call needs a matching unit_pahub_unlock().
 *
 * @param[in] channel Port 0-5.
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE before init,
 *         ESP_ERR_TIMEOUT if Port A stayed busy, or the selection error (the
 *         lock is not held on failure).
 */
esp_err_t unit_pahub_lock( uint8_t channel );

/**
 * @brief Release one unit_pahub_lock() hold.
 *
 * @return ESP_OK, or ESP_ERR_INVALID_STATE if the calling task holds no lock.
 */
esp_err_t unit_pahub_unlock( void );

/**
 * @brief Route exactly one port.
 *
 * Always writes the control register, so it also resynchronises a hub that
 * was power-cycled. Another Port A user may change the selection right after
 * this returns; use unit_pahub_lock() or the transfer functions when the
 * selection must still hold for the next transfer.
 *
 * @param[in] channel Port 0-5.
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE before init,
 *         ESP_ERR_TIMEOUT, or the I2C error.
 */
esp_err_t unit_pahub_channel_set( uint8_t channel );

/**
 * @brief Read which single port is routed.
 *
 * @param[out] channel Port 0-5, or UNIT_PAHUB_CHANNEL_NONE when no port or
 *                     more than one port is on.
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE before init,
 *         ESP_ERR_TIMEOUT, or the I2C error.
 */
esp_err_t unit_pahub_channel_get( uint8_t *channel );

/**
 * @brief Write the control register directly.
 *
 * Bit n routes port n. Several ports may be on at once only if their devices
 * use different addresses. Always writes, like unit_pahub_channel_set().
 *
 * @param[in] mask Any combination of bits in UNIT_PAHUB_CHANNEL_MASK_ALL.
 * @return ESP_OK, ESP_ERR_INVALID_ARG for bits 6-7 (not wired),
 *         ESP_ERR_INVALID_STATE before init, ESP_ERR_TIMEOUT, or the I2C error.
 */
esp_err_t unit_pahub_mask_set( uint8_t mask );

/**
 * @brief Read the control register.
 *
 * @param[out] mask Raw register value. Bits 6-7 are reported as read.
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE before init,
 *         ESP_ERR_TIMEOUT, or the I2C error.
 */
esp_err_t unit_pahub_mask_get( uint8_t *mask );

/**
 * @brief Turn every port off. Equivalent to unit_pahub_mask_set( 0 ).
 *
 * @return ESP_OK, ESP_ERR_INVALID_STATE before init, ESP_ERR_TIMEOUT, or the
 *         I2C error.
 */
esp_err_t unit_pahub_disable_all( void );

/**
 * @brief Check whether a device acknowledges @p address on one port.
 *
 * @param[in] channel Port 0-5.
 * @param[in] address 7-bit address 0x08-0x77 other than UNIT_PAHUB_ADDR.
 * @return
 *  - ESP_OK if the device acknowledged.
 *  - ESP_ERR_NOT_FOUND if nothing acknowledged.
 *  - ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE before init, ESP_ERR_TIMEOUT,
 *    or another I2C error.
 */
esp_err_t unit_pahub_probe( uint8_t channel, uint16_t address );

/**
 * @brief Find the ports on which a device at @p address answers.
 *
 * Probes with every port off, then on each port alone, and leaves every port
 * off. Use it to locate identical sensors or to check the wiring at start-up.
 *
 * @param[in]  address      7-bit address 0x08-0x77 other than UNIT_PAHUB_ADDR.
 * @param[out] channel_mask Bit n set when the device answered on port n.
 * @return
 *  - ESP_OK when the scan completed, even if @p channel_mask is 0.
 *  - ESP_ERR_INVALID_STATE if a device answers with every port off, because
 *    it sits on Port A itself and would answer on every port. Also returned
 *    before init.
 *  - ESP_ERR_INVALID_ARG, ESP_ERR_TIMEOUT, or another I2C error.
 */
esp_err_t unit_pahub_scan( uint16_t address, uint8_t *channel_mask );

/**
 * @brief Recover a hung Port A bus and resynchronise the hub.
 *
 * Clocks the bus free (releasing a downstream device that holds SDA low), then
 * turns every port off and verifies the register. Call it after repeated
 * ESP_ERR_TIMEOUT or unexpected failures. Every port is off afterwards; the
 * transfer functions reselect on demand.
 *
 * @return ESP_OK, ESP_ERR_INVALID_STATE before init, ESP_ERR_TIMEOUT,
 *         ESP_ERR_NOT_FOUND if the hub does not acknowledge,
 *         ESP_ERR_INVALID_RESPONSE if the register did not read back as 0x00,
 *         or the I2C error.
 */
esp_err_t unit_pahub_recover( void );

#ifdef __cplusplus
}
#endif

#endif /* UNIT_PAHUB_H */

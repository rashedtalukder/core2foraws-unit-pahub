/*!
 * @brief Library for M5Stack PaHUB-compatible six-channel I2C switch units on
 * the Core2 for AWS
 *
 * @copyright Copyright (c) 2024 by Rashed Talukder[https://rashedtalukder.com]
 *
 * @license SPDX-License-Identifier: Apache 2.0
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
 * @Links [PAHUB](https://docs.m5stack.com/en/unit/pahub)
 * @Links [PAHUB2](https://docs.m5stack.com/en/unit/pahub2)
 *
 * @version  V0.0.2
 * @date  2026-06-04
 */

#ifndef _UNIT_PAHUB_H_
#define _UNIT_PAHUB_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <esp_err.h>
#include <stdint.h>
#include <driver/i2c_master.h>

/** @brief 7-bit I2C address of the PaHUB, from Kconfig (default 0x70). */
#define UNIT_PAHUB_ADDR         CONFIG_PAHUB_ADDRESS
/** @brief Number of downstream channels exposed by the PaHUB. */
#define UNIT_PAHUB_CHANNELS_NUM 6 // Human number of channels
#define UNIT_PAHUB_CHANNEL_0    0 /*!< Downstream channel 0 */
#define UNIT_PAHUB_CHANNEL_1    1 /*!< Downstream channel 1 */
#define UNIT_PAHUB_CHANNEL_2    2 /*!< Downstream channel 2 */
#define UNIT_PAHUB_CHANNEL_3    3 /*!< Downstream channel 3 */
#define UNIT_PAHUB_CHANNEL_4    4 /*!< Downstream channel 4 */
#define UNIT_PAHUB_CHANNEL_5    5 /*!< Downstream channel 5 */

  /**
   * @brief Set the channel for connected IIC peripheral.
   *
   * This value needs to be set to the channel the device is plugged in to.
   *
   * @param channel Channel number of IIC peripheral (0-5).
   * @return
   * [esp_err_t](https://docs.espressif.com/projects/esp-idf/en/release-v4.3/esp32/api-reference/system/esp_err.html#macros).
   *  - ESP_OK                : Success
   *  - ESP_ERR_INVALID_ARG	: Driver parameter error or invalid channel
  *  - ESP_ERR_INVALID_STATE : unit_pahub_init() has not been called
  *  - ESP_ERR_TIMEOUT       : Internal mutex was not acquired within 1 second
   */
  esp_err_t unit_pahub_channel_set( uint8_t channel );

  /**
   * @brief Reads the active channel from the PaHUB hardware.
   *
   * The PaHUB chip stores the selected channel as a bitmask in its control
   * register. This function reads that register and converts the bitmask back
   * into a channel number (0-5). If no channel is active the output is set to
   * 0xFF.
   *
   * @param channel Output: channel number (0-5), or 0xFF if no channel is
   *                active. Must not be NULL.
   * @return
   * [esp_err_t](https://docs.espressif.com/projects/esp-idf/en/release-v4.3/esp32/api-reference/system/esp_err.html#macros).
   *  - ESP_OK                : Success
   *  - ESP_ERR_INVALID_ARG   : channel pointer is NULL
   *  - ESP_ERR_INVALID_STATE : unit_pahub_init() has not been called
   *  - ESP_ERR_TIMEOUT       : Failed to acquire the internal mutex
   */
  esp_err_t unit_pahub_channel_get( uint8_t *channel );

  esp_err_t unit_pahub_mask_set( uint8_t mask );
  esp_err_t unit_pahub_mask_get( uint8_t *mask );

  /**
   * @brief Initialize the PaHUB mutex for thread-safe operations.
   *
   * This function must be called once before using any thread-safe PaHUB
   * operations. It initializes the internal mutex used to protect against race
   * conditions.
   *
   * @return
   * [esp_err_t](https://docs.espressif.com/projects/esp-idf/en/release-v4.3/esp32/api-reference/system/esp_err.html#macros).
   *  - ESP_OK                : Success
   *  - ESP_FAIL              : Failed to create mutex
   */
  esp_err_t unit_pahub_init( void );

  /**
   * @brief Thread-safe I2C read operation with automatic channel switching.
   *
   * This function atomically switches to the specified channel and performs
   * an I2C read operation. It ensures no other task can interfere with the
   * channel setting during the operation.
   *
   * @param channel Channel number of IIC peripheral (0-5).
   * @param dev_handle The I2C device handle for the target device.
   * @param register_address The data register address.
   * @param data Pointer to the data read from the I2C peripheral.
   * @param length The number of bytes to read.
   * @return
   * [esp_err_t](https://docs.espressif.com/projects/esp-idf/en/release-v4.3/esp32/api-reference/system/esp_err.html#macros).
   *  - ESP_OK                : Success
   *  - ESP_ERR_INVALID_ARG   : Invalid channel or parameter error
   *  - ESP_ERR_TIMEOUT       : Failed to acquire mutex or I2C timeout
   */
  esp_err_t unit_pahub_i2c_read( uint8_t channel, i2c_master_dev_handle_t dev_handle,
                                 uint32_t register_address, uint8_t *data,
                                 uint16_t length );

  /**
   * @brief Thread-safe I2C write operation with automatic channel switching.
   *
   * This function atomically switches to the specified channel and performs
   * an I2C write operation. It ensures no other task can interfere with the
   * channel setting during the operation.
   *
   * @param channel Channel number of IIC peripheral (0-5).
   * @param dev_handle The I2C device handle for the target device.
   * @param register_address The data register address.
   * @param data Pointer to the data to write to the I2C peripheral.
   * @param length The number of bytes to write.
   * @return
   * [esp_err_t](https://docs.espressif.com/projects/esp-idf/en/release-v4.3/esp32/api-reference/system/esp_err.html#macros).
   *  - ESP_OK                : Success
   *  - ESP_ERR_INVALID_ARG   : Invalid channel or parameter error
   *  - ESP_ERR_TIMEOUT       : Failed to acquire mutex or I2C timeout
   */
  esp_err_t unit_pahub_i2c_write( uint8_t channel, i2c_master_dev_handle_t dev_handle,
                                  uint32_t register_address,
                                  const uint8_t *data, uint16_t length );

  /**
   * @brief Deinitialize the PaHUB and free resources.
   *
   * This function should be called when done using the PaHUB to free
   * the mutex resources.
   *
   * @return
   * [esp_err_t](https://docs.espressif.com/projects/esp-idf/en/release-v4.3/esp32/api-reference/system/esp_err.html#macros).
   *  - ESP_OK                : Success
   */
  esp_err_t unit_pahub_deinit( void );

#ifdef __cplusplus
}
#endif
#endif

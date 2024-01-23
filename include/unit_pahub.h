/*!
 * @brief Library for the PaHUB (TCA9548A) or PaHUB2 (PCA9548APW) unit by 
 * M5Stack on the Core2 for AWS
 * 
 * @copyright Copyright (c) 2024 by Rashed Talukder[https://rashedtalukder.com]
 * 
 * @license SPDX-License-Identifier: Apache 2.0
 * 
 * @Links [PAHUB](https://docs.m5stack.com/en/unit/pahub)
 * @Links [PAHUB2](https://docs.m5stack.com/en/unit/pahub2)
 * 
 * @version  V0.0.1
 * @date  2024-01-23
 */

#ifndef _UNIT_PAHUB_H_
#define _UNIT_PAHUB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <esp_err.h>

#define UNIT_PAHUB_ADDR             CONFIG_PAHUB_ADDRESS
#define UNIT_PAHUB_CHANNELS_NUM     6 // Human number of channels
#define UNIT_PAHUB_CHANNEL_0        0
#define UNIT_PAHUB_CHANNEL_1        1
#define UNIT_PAHUB_CHANNEL_2        2
#define UNIT_PAHUB_CHANNEL_3        3 
#define UNIT_PAHUB_CHANNEL_4        4
#define UNIT_PAHUB_CHANNEL_5        5

/** 
 * @brief Set the channel for connected IIC peripheral.
 * 
 * This value needs to be set to the channel the device is plugged in to.
 * 
 * @param channel Channel number of IIC peripheral (0-5).
 * @return [esp_err_t](https://docs.espressif.com/projects/esp-idf/en/release-v4.3/esp32/api-reference/system/esp_err.html#macros).
 *  - ESP_OK                : Success
 *  - ESP_ERR_INVALID_ARG	: Driver parameter error or invalid channel
 */
esp_err_t unit_pahub_channel_set( uint8_t channel );

/** 
 * @brief Retrieves the channel the PaHUB is set to.
 * 
 * @param channel Channel number the PaHUB is set to in this application (0-5).
 * @return [esp_err_t](https://docs.espressif.com/projects/esp-idf/en/release-v4.3/esp32/api-reference/system/esp_err.html#macros).
 *  - ESP_OK                : Success
 */
esp_err_t unit_pahub_channel_get( uint8_t *channel );

#ifdef __cplusplus
}
#endif
#endif

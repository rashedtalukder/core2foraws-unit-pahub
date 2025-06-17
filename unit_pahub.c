/*!
 * @brief Library for the PaHUB (TCA9548A) or PaHUB2 (PCA9548APW) unit by
 * M5Stack on the Core2 for AWS
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
 * @version  V0.0.1
 * @date  2024-01-23
 */

#include "unit_pahub.h"
#include "core2foraws.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>

static const char *TAG = "UNIT_PAHUB";
static SemaphoreHandle_t pahub_mutex = NULL;
static uint8_t current_channel = 0xFF; // Invalid channel to force initial set

esp_err_t unit_pahub_init( void )
{
  if( pahub_mutex == NULL )
  {
    pahub_mutex = xSemaphoreCreateMutex();
    if( pahub_mutex == NULL )
    {
      ESP_LOGE( TAG, "Failed to create PaHUB mutex" );
      return ESP_FAIL;
    }
    current_channel = 0xFF; // Force channel set on first use
    ESP_LOGI( TAG, "PaHUB initialized successfully" );
  }
  return ESP_OK;
}

esp_err_t unit_pahub_channel_set( uint8_t channel )
{
  esp_err_t err = ESP_ERR_INVALID_ARG;

  if( channel < UNIT_PAHUB_CHANNELS_NUM )
  {
    const uint8_t l_shift_channel = (uint8_t)1 << channel;
    err = core2foraws_expports_i2c_write( UNIT_PAHUB_ADDR, I2C_NO_REG,
                                          &l_shift_channel, 1 );
    if( err == ESP_OK )
    {
      current_channel = channel;
    }
  }

  return err;
}

esp_err_t unit_pahub_channel_get( uint8_t *channel )
{
  esp_err_t err =
      core2foraws_expports_i2c_read( UNIT_PAHUB_ADDR, I2C_NO_REG, channel, 1 );
  *channel = log2( *channel >> 1 ) + 0x01;

  return err;
}

esp_err_t unit_pahub_i2c_read( uint8_t channel, uint16_t device_address,
                               uint32_t register_address, uint8_t *data,
                               uint16_t length )
{
  if( pahub_mutex == NULL )
  {
    ESP_LOGE( TAG, "PaHUB not initialized. Call unit_pahub_init() first." );
    return ESP_ERR_INVALID_STATE;
  }

  if( channel >= UNIT_PAHUB_CHANNELS_NUM || data == NULL )
  {
    return ESP_ERR_INVALID_ARG;
  }

  // Take mutex with timeout
  if( xSemaphoreTake( pahub_mutex, pdMS_TO_TICKS( 1000 ) ) != pdTRUE )
  {
    ESP_LOGE( TAG, "Failed to acquire PaHUB mutex for read operation" );
    return ESP_ERR_TIMEOUT;
  }

  esp_err_t err = ESP_OK;

  // Only switch channel if different from current
  if( current_channel != channel )
  {
    err = unit_pahub_channel_set( channel );
    if( err != ESP_OK )
    {
      ESP_LOGE( TAG, "Failed to set PaHUB channel %d for read", channel );
      xSemaphoreGive( pahub_mutex );
      return err;
    }
  }

  // Perform I2C read operation
  err = core2foraws_expports_i2c_read( device_address, register_address, data,
                                       length );
  if( err != ESP_OK )
  {
    ESP_LOGE( TAG, "I2C read failed on channel %d, device 0x%02X", channel,
              device_address );
  }

  // Release mutex
  xSemaphoreGive( pahub_mutex );

  return err;
}

esp_err_t unit_pahub_i2c_write( uint8_t channel, uint16_t device_address,
                                uint32_t register_address, const uint8_t *data,
                                uint16_t length )
{
  if( pahub_mutex == NULL )
  {
    ESP_LOGE( TAG, "PaHUB not initialized. Call unit_pahub_init() first." );
    return ESP_ERR_INVALID_STATE;
  }

  if( channel >= UNIT_PAHUB_CHANNELS_NUM || data == NULL )
  {
    return ESP_ERR_INVALID_ARG;
  }

  // Take mutex with timeout
  if( xSemaphoreTake( pahub_mutex, pdMS_TO_TICKS( 1000 ) ) != pdTRUE )
  {
    ESP_LOGE( TAG, "Failed to acquire PaHUB mutex for write operation" );
    return ESP_ERR_TIMEOUT;
  }

  esp_err_t err = ESP_OK;

  // Only switch channel if different from current
  if( current_channel != channel )
  {
    err = unit_pahub_channel_set( channel );
    if( err != ESP_OK )
    {
      ESP_LOGE( TAG, "Failed to set PaHUB channel %d for write", channel );
      xSemaphoreGive( pahub_mutex );
      return err;
    }
  }

  // Perform I2C write operation
  err = core2foraws_expports_i2c_write( device_address, register_address, data,
                                        length );
  if( err != ESP_OK )
  {
    ESP_LOGE( TAG, "I2C write failed on channel %d, device 0x%02X", channel,
              device_address );
  }

  // Release mutex
  xSemaphoreGive( pahub_mutex );

  return err;
}

esp_err_t unit_pahub_deinit( void )
{
  if( pahub_mutex != NULL )
  {
    vSemaphoreDelete( pahub_mutex );
    pahub_mutex = NULL;
    current_channel = 0xFF;
    ESP_LOGI( TAG, "PaHUB deinitialized" );
  }
  return ESP_OK;
}
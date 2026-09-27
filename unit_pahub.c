/*!
 * @file unit_pahub.c
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

#include "unit_pahub.h"
#include "core2foraws.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdbool.h>

/*
 * All state below is guarded by the BSP's recursive Port A bus lock rather
 * than a private mutex. That makes a channel selection and the transfer that
 * depends on it atomic against every Port A user, not only PaHub callers, and
 * lets unit_pahub_lock() holders call back into this driver. Transfers go
 * through the core2foraws_i2c_* layer because the core2foraws_expports_i2c_*
 * wrappers take the expansion-port lock first, which would invert the order.
 */

#define PAHUB_PORT CORE2FORAWS_I2C_EXTERNAL

/* The control register has no sub-address, so each access is a bare byte. */
#define PAHUB_NO_REG CORE2FORAWS_I2C_NO_REG

/* Retry a control write that is NACKed transiently rather than failing the
 * downstream transfer that depends on it. */
#define PAHUB_WRITE_ATTEMPTS 3

/* An address probe is one byte; a healthy bus answers in well under 1 ms. */
#define PAHUB_PROBE_TIMEOUT_MS 50

static const char *_TAG = "UNIT_PAHUB";

static i2c_master_dev_handle_t s_dev;
static uint32_t s_refs;

/* Last control byte written or read. Advisory: a hub power cycle resets the
 * register to 0x00 without telling us, which transfers detect and repair. */
static uint8_t s_mask;
static bool s_mask_known;

static TaskHandle_t s_hold_owner;
static uint32_t s_hold_depth;

static esp_err_t bus_lock( void )
{
  esp_err_t err = core2foraws_i2c_lock( PAHUB_PORT );
  /* The BSP reports a bus that was never opened as an invalid argument. */
  return err == ESP_ERR_INVALID_ARG ? ESP_ERR_INVALID_STATE : err;
}

static void bus_unlock( void )
{
  core2foraws_i2c_unlock( PAHUB_PORT );
}

/* Take the bus lock and require an initialized hub. */
static esp_err_t hub_enter( void )
{
  esp_err_t err = bus_lock();
  if( err == ESP_OK && s_dev == NULL )
  {
    bus_unlock();
    err = ESP_ERR_INVALID_STATE;
  }
  return err;
}

static bool channel_valid( uint8_t channel )
{
  return channel < UNIT_PAHUB_CHANNELS_NUM;
}

static bool address_valid( uint16_t address )
{
  /* 0x00-0x07 and 0x78-0x7F are reserved by the I2C specification. */
  return address >= 0x08 && address <= 0x77 && address != UNIT_PAHUB_ADDR;
}

/* Clock out a device holding SDA low. The next access reselects the port. */
static esp_err_t bus_reset_locked( void )
{
  s_mask_known = false;

  i2c_master_bus_handle_t bus = NULL;
  esp_err_t err = core2foraws_i2c_get_bus_handle( PAHUB_PORT, &bus );
  if( err == ESP_OK )
  {
    err = i2c_master_bus_reset( bus );
  }
  if( err != ESP_OK )
  {
    ESP_LOGW( _TAG, "Bus reset failed: %s", esp_err_to_name( err ) );
  }
  return err;
}

static esp_err_t mask_write_locked( uint8_t mask )
{
  esp_err_t err = ESP_FAIL;
#ifdef CONFIG_PAHUB_AUTO_RECOVER
  bool reset_done = false;
#endif
  for( int attempt = 0; attempt < PAHUB_WRITE_ATTEMPTS; attempt++ )
  {
    err = core2foraws_i2c_write( PAHUB_PORT, s_dev, PAHUB_NO_REG, &mask, 1 );
    if( err == ESP_OK )
    {
      s_mask = mask;
      s_mask_known = true;
      return ESP_OK;
    }
#ifdef CONFIG_PAHUB_AUTO_RECOVER
    if( err == ESP_ERR_TIMEOUT && !reset_done )
    {
      reset_done = true;
      ESP_LOGW( _TAG, "Control write timed out; resetting Port A" );
      if( bus_reset_locked() == ESP_OK )
      {
        continue;
      }
    }
#endif
    /* Each timeout already cost a full transfer timeout, and a closed bus or
     * released handle will not heal, so only a NACK is retried at once. */
    if( err != ESP_ERR_INVALID_RESPONSE )
    {
      break;
    }
  }

  s_mask_known = false;
  return err;
}

static esp_err_t mask_read_locked( uint8_t *mask )
{
  esp_err_t err = core2foraws_i2c_read( PAHUB_PORT, s_dev, PAHUB_NO_REG, mask, 1 );
  s_mask_known = err == ESP_OK;
  if( s_mask_known )
  {
    s_mask = *mask;
  }
  return err;
}

/* Select a channel, skipping the write when the hub already routes it. */
static esp_err_t select_locked( uint8_t channel )
{
  const uint8_t mask = ( uint8_t )( 1u << channel );
  if( s_mask_known && s_mask == mask )
  {
    return ESP_OK;
  }
  return mask_write_locked( mask );
}

/* After a failed transfer, true only when a read-back proves the hub was not
 * routing the channel, so the device never saw the transfer and a retry is
 * safe even for writes. */
static bool selection_lost_locked( uint8_t channel )
{
  uint8_t mask = 0;
  return mask_read_locked( &mask ) == ESP_OK &&
         mask != ( uint8_t )( 1u << channel );
}

/* Write 0x00 and confirm it, which also proves a PCA9548A answers. */
static esp_err_t verify_off_locked( void )
{
  esp_err_t err = mask_write_locked( 0 );
  if( err == ESP_ERR_INVALID_RESPONSE )
  {
    return ESP_ERR_NOT_FOUND;
  }

  uint8_t mask = 0xFF;
  if( err == ESP_OK )
  {
    err = mask_read_locked( &mask );
  }
  if( err == ESP_OK && mask != 0 )
  {
    s_mask_known = false;
    err = ESP_ERR_INVALID_RESPONSE;
  }
  return err;
}

static esp_err_t probe_locked( uint16_t address )
{
  i2c_master_bus_handle_t bus = NULL;
  esp_err_t err = core2foraws_i2c_get_bus_handle( PAHUB_PORT, &bus );
  if( err == ESP_OK )
  {
    err = i2c_master_probe( bus, address, PAHUB_PROBE_TIMEOUT_MS );
  }
  return err;
}

/* Called before releasing the bus after downstream traffic. */
static void idle_locked( void )
{
#ifdef CONFIG_PAHUB_DESELECT_WHEN_IDLE
  if( s_hold_depth == 0 && !( s_mask_known && s_mask == 0 ) )
  {
    esp_err_t err = mask_write_locked( 0 );
    if( err != ESP_OK )
    {
      ESP_LOGW( _TAG, "Could not turn ports off: %s", esp_err_to_name( err ) );
    }
  }
#endif
}

esp_err_t unit_pahub_init( void )
{
  esp_err_t err = core2foraws_expports_i2c_begin();
  if( err != ESP_OK )
  {
    ESP_LOGE( _TAG, "Could not open Port A: %s", esp_err_to_name( err ) );
    return err;
  }

  err = bus_lock();
  if( err != ESP_OK )
  {
    return err;
  }

  if( s_refs > 0 )
  {
    s_refs++;
    bus_unlock();
    return ESP_OK;
  }

  /* A failed earlier init or deinit may have kept the handle. */
  if( s_dev == NULL )
  {
    err = core2foraws_i2c_device_add( PAHUB_PORT, UNIT_PAHUB_ADDR,
                                      CONFIG_PAHUB_I2C_SPEED_HZ, &s_dev );
    if( err != ESP_OK )
    {
      s_dev = NULL;
    }
  }

  if( err == ESP_OK )
  {
    /* A software reset does not power-cycle the hub, so ports selected by
     * the previous boot may still be on. */
    s_mask_known = false;
    err = verify_off_locked();
    if( err == ESP_OK )
    {
      s_refs = 1;
      ESP_LOGI( _TAG, "Hub ready at 0x%02x", UNIT_PAHUB_ADDR );
    }
    else if( core2foraws_i2c_device_remove( s_dev ) == ESP_OK )
    {
      s_dev = NULL;
    }
  }

  if( err != ESP_OK )
  {
    ESP_LOGE( _TAG, "Hub init at 0x%02x failed: %s", UNIT_PAHUB_ADDR,
              esp_err_to_name( err ) );
  }

  bus_unlock();
  return err;
}

esp_err_t unit_pahub_deinit( void )
{
  esp_err_t err = bus_lock();
  if( err == ESP_ERR_INVALID_STATE )
  {
    return ESP_OK;
  }
  if( err != ESP_OK )
  {
    return err;
  }

  if( s_refs == 0 )
  {
    bus_unlock();
    return ESP_OK;
  }

  /* Only the holder can get here while a hold is active. */
  if( s_hold_depth > 0 )
  {
    bus_unlock();
    return ESP_ERR_INVALID_STATE;
  }

  if( s_refs > 1 )
  {
    s_refs--;
    bus_unlock();
    return ESP_OK;
  }

  err = mask_write_locked( 0 );
  if( err != ESP_OK )
  {
    ESP_LOGW( _TAG, "Could not turn ports off: %s", esp_err_to_name( err ) );
  }

  err = core2foraws_i2c_device_remove( s_dev );
  /* Closing Port A already released every device on it. */
  if( err == ESP_ERR_NOT_FOUND )
  {
    err = ESP_OK;
  }
  if( err == ESP_OK )
  {
    s_dev = NULL;
    s_refs = 0;
    s_mask_known = false;
    ESP_LOGI( _TAG, "Hub released" );
  }

  bus_unlock();
  return err;
}

static esp_err_t transfer( uint8_t channel, i2c_master_dev_handle_t dev_handle,
                           uint32_t register_address, uint8_t *rx,
                           const uint8_t *tx, uint16_t length )
{
  if( !channel_valid( channel ) || dev_handle == NULL || length == 0 ||
      ( rx == NULL && tx == NULL ) )
  {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = hub_enter();
  if( err != ESP_OK )
  {
    return err;
  }

  bool selected = false;
  for( int attempt = 0;; attempt++ )
  {
    const bool cached = s_mask_known && s_mask == ( uint8_t )( 1u << channel );
    err = select_locked( channel );
    selected = err == ESP_OK;
    if( !selected )
    {
      ESP_LOGE( _TAG, "Could not select port %u: %s", channel,
                esp_err_to_name( err ) );
      break;
    }

    err = rx != NULL
              ? core2foraws_i2c_read( PAHUB_PORT, dev_handle, register_address,
                                      rx, length )
              : core2foraws_i2c_write( PAHUB_PORT, dev_handle,
                                       register_address, tx, length );
    if( err == ESP_OK || attempt > 0 )
    {
      break;
    }
    if( err == ESP_ERR_TIMEOUT )
    {
      /* A downstream device may be holding SDA low. */
#ifdef CONFIG_PAHUB_AUTO_RECOVER
      bus_reset_locked();
#else
      s_mask_known = false;
#endif
      break;
    }
    /* Only an address NACK on a route we did not just write can mean the hub
     * lost its selection; anything else is the device's own answer. */
    if( err != ESP_ERR_INVALID_RESPONSE || !cached ||
        !selection_lost_locked( channel ) )
    {
      break;
    }
    ESP_LOGW( _TAG, "Hub dropped port %u (power glitch?); retrying", channel );
  }

  if( err != ESP_OK && selected )
  {
    /* A NACK can be a normal answer, e.g. an idle SMBus alert response. */
    if( rx != NULL && err == ESP_ERR_INVALID_RESPONSE )
    {
      ESP_LOGD( _TAG, "Read NACKed on port %u", channel );
    }
    else
    {
      ESP_LOGE( _TAG, "%s failed on port %u: %s", rx != NULL ? "Read" : "Write",
                channel, esp_err_to_name( err ) );
    }
  }

  idle_locked();
  bus_unlock();
  return err;
}

esp_err_t unit_pahub_i2c_read( uint8_t channel,
                               i2c_master_dev_handle_t dev_handle,
                               uint32_t register_address, uint8_t *data,
                               uint16_t length )
{
  if( data == NULL )
  {
    return ESP_ERR_INVALID_ARG;
  }
  return transfer( channel, dev_handle, register_address, data, NULL, length );
}

esp_err_t unit_pahub_i2c_write( uint8_t channel,
                                i2c_master_dev_handle_t dev_handle,
                                uint32_t register_address, const uint8_t *data,
                                uint16_t length )
{
  if( data == NULL )
  {
    return ESP_ERR_INVALID_ARG;
  }
  return transfer( channel, dev_handle, register_address, NULL, data, length );
}

esp_err_t unit_pahub_lock( uint8_t channel )
{
  if( !channel_valid( channel ) )
  {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = hub_enter();
  if( err != ESP_OK )
  {
    return err;
  }

  err = select_locked( channel );
  if( err != ESP_OK )
  {
    bus_unlock();
    return err;
  }

  /* Keep the bus lock until the matching unit_pahub_unlock(). */
  s_hold_owner = xTaskGetCurrentTaskHandle();
  s_hold_depth++;
  return ESP_OK;
}

esp_err_t unit_pahub_unlock( void )
{
  /* The holder re-enters at once; any other task cannot hold a lock. */
  if( bus_lock() != ESP_OK )
  {
    return ESP_ERR_INVALID_STATE;
  }

  if( s_hold_depth == 0 || s_hold_owner != xTaskGetCurrentTaskHandle() )
  {
    bus_unlock();
    return ESP_ERR_INVALID_STATE;
  }

  if( --s_hold_depth == 0 )
  {
    s_hold_owner = NULL;
    idle_locked();
  }

  bus_unlock();
  bus_unlock();
  return ESP_OK;
}

esp_err_t unit_pahub_channel_set( uint8_t channel )
{
  if( !channel_valid( channel ) )
  {
    return ESP_ERR_INVALID_ARG;
  }
  return unit_pahub_mask_set( ( uint8_t )( 1u << channel ) );
}

esp_err_t unit_pahub_channel_get( uint8_t *channel )
{
  uint8_t mask = 0;
  if( channel == NULL )
  {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = unit_pahub_mask_get( &mask );
  if( err == ESP_OK )
  {
    *channel = UNIT_PAHUB_CHANNEL_NONE;
    for( uint8_t i = 0; i < UNIT_PAHUB_CHANNELS_NUM; i++ )
    {
      if( mask == ( uint8_t )( 1u << i ) )
      {
        *channel = i;
        break;
      }
    }
  }
  return err;
}

esp_err_t unit_pahub_mask_set( uint8_t mask )
{
  if( mask & ~UNIT_PAHUB_CHANNEL_MASK_ALL )
  {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = hub_enter();
  if( err == ESP_OK )
  {
    err = mask_write_locked( mask );
    bus_unlock();
  }
  return err;
}

esp_err_t unit_pahub_mask_get( uint8_t *mask )
{
  if( mask == NULL )
  {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = hub_enter();
  if( err == ESP_OK )
  {
    err = mask_read_locked( mask );
    bus_unlock();
  }
  return err;
}

esp_err_t unit_pahub_disable_all( void )
{
  return unit_pahub_mask_set( 0 );
}

esp_err_t unit_pahub_probe( uint8_t channel, uint16_t address )
{
  if( !channel_valid( channel ) || !address_valid( address ) )
  {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = hub_enter();
  if( err != ESP_OK )
  {
    return err;
  }

  err = select_locked( channel );
  if( err == ESP_OK )
  {
    err = probe_locked( address );
  }

  idle_locked();
  bus_unlock();
  return err;
}

esp_err_t unit_pahub_scan( uint16_t address, uint8_t *channel_mask )
{
  if( !address_valid( address ) || channel_mask == NULL )
  {
    return ESP_ERR_INVALID_ARG;
  }
  *channel_mask = 0;

  esp_err_t err = hub_enter();
  if( err != ESP_OK )
  {
    return err;
  }

  uint8_t found = 0;
  err = mask_write_locked( 0 );
  if( err == ESP_OK )
  {
    esp_err_t probe = probe_locked( address );
    err = probe == ESP_OK ? ESP_ERR_INVALID_STATE
          : probe == ESP_ERR_NOT_FOUND ? ESP_OK
                                       : probe;
  }

  for( uint8_t channel = 0; err == ESP_OK && channel < UNIT_PAHUB_CHANNELS_NUM;
       channel++ )
  {
    err = select_locked( channel );
    if( err == ESP_OK )
    {
      esp_err_t probe = probe_locked( address );
      if( probe == ESP_OK )
      {
        found |= ( uint8_t )( 1u << channel );
      }
      else if( probe != ESP_ERR_NOT_FOUND )
      {
        err = probe;
      }
    }
  }

  if( !( s_mask_known && s_mask == 0 ) )
  {
    esp_err_t off = mask_write_locked( 0 );
    if( err == ESP_OK )
    {
      err = off;
    }
  }

  if( err == ESP_OK )
  {
    *channel_mask = found;
  }

  bus_unlock();
  return err;
}

esp_err_t unit_pahub_recover( void )
{
  esp_err_t err = hub_enter();
  if( err != ESP_OK )
  {
    return err;
  }

  /* Resynchronise even if the reset failed; the bus may not have been hung. */
  bus_reset_locked();
  err = verify_off_locked();
  if( err != ESP_OK )
  {
    ESP_LOGE( _TAG, "Hub did not recover: %s", esp_err_to_name( err ) );
  }

  bus_unlock();
  return err;
}

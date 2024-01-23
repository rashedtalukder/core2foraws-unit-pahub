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

#include "math.h"
#include "core2foraws.h"
#include "unit_pahub.h"

esp_err_t unit_pahub_channel_set( uint8_t channel )
{
    esp_err_t err = ESP_ERR_INVALID_ARG;

    if( channel < UNIT_PAHUB_CHANNELS_NUM )
    {
        const uint8_t l_shift_channel = ( uint8_t ) 1 << channel;
        return err = core2foraws_expports_i2c_write( UNIT_PAHUB_ADDR, I2C_NO_REG, &l_shift_channel, 1 );
    }

    return err;
}

esp_err_t unit_pahub_channel_get( uint8_t *channel )
{
    esp_err_t err = core2foraws_expports_i2c_read( UNIT_PAHUB_ADDR, I2C_NO_REG, channel, 1 );
    *channel = log2( *channel >> 1 ) + 0x01;

    return err;
}
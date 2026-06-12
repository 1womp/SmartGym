/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file   esp_panel_board_custom_conf.h
 * @brief  Custom board configuration for Viewe UEDX80480070E-WB-A
 */

#pragma once

// *INDENT-OFF*

#ifndef ESP_PANEL_BOARD_DEFAULT_USE_CUSTOM
#define ESP_PANEL_BOARD_DEFAULT_USE_CUSTOM  (1)
#endif

#if ESP_PANEL_BOARD_DEFAULT_USE_CUSTOM

#define ESP_PANEL_BOARD_NAME                "Viewe:UEDX80480070E-WB-A"
#define ESP_PANEL_BOARD_WIDTH               (800)
#define ESP_PANEL_BOARD_HEIGHT              (480)

#define ESP_PANEL_BOARD_USE_LCD             (1)

#if ESP_PANEL_BOARD_USE_LCD
#define ESP_PANEL_BOARD_LCD_CONTROLLER      ST7262
#define ESP_PANEL_BOARD_LCD_BUS_TYPE        (ESP_PANEL_BUS_TYPE_RGB)

#if ESP_PANEL_BOARD_LCD_BUS_TYPE == ESP_PANEL_BUS_TYPE_RGB
#define ESP_PANEL_BOARD_LCD_RGB_USE_CONTROL_PANEL       (0)
#define ESP_PANEL_BOARD_LCD_RGB_CLK_HZ                  (16 * 1000 * 1000)
#define ESP_PANEL_BOARD_LCD_RGB_HPW                     (48)
#define ESP_PANEL_BOARD_LCD_RGB_HBP                     (40)
#define ESP_PANEL_BOARD_LCD_RGB_HFP                     (88)
#define ESP_PANEL_BOARD_LCD_RGB_VPW                     (6)
#define ESP_PANEL_BOARD_LCD_RGB_VBP                     (26)
#define ESP_PANEL_BOARD_LCD_RGB_VFP                     (10)
#define ESP_PANEL_BOARD_LCD_RGB_PCLK_ACTIVE_NEG         (1)
#define ESP_PANEL_BOARD_LCD_RGB_DATA_WIDTH              (16)
#define ESP_PANEL_BOARD_LCD_RGB_PIXEL_BITS              (ESP_PANEL_LCD_COLOR_BITS_RGB565)
#define ESP_PANEL_BOARD_LCD_RGB_BOUNCE_BUF_SIZE         (ESP_PANEL_BOARD_WIDTH * 20)
#define ESP_PANEL_BOARD_LCD_RGB_IO_HSYNC                (39)
#define ESP_PANEL_BOARD_LCD_RGB_IO_VSYNC                (41)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DE                   (40)
#define ESP_PANEL_BOARD_LCD_RGB_IO_PCLK                 (42)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DISP                 (-1)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DATA0                (8)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DATA1                (3)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DATA2                (46)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DATA3                (9)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DATA4                (1)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DATA5                (5)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DATA6                (6)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DATA7                (7)
#if ESP_PANEL_BOARD_LCD_RGB_DATA_WIDTH > 8
#define ESP_PANEL_BOARD_LCD_RGB_IO_DATA8                (15)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DATA9                (16)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DATA10               (4)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DATA11               (45)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DATA12               (48)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DATA13               (47)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DATA14               (21)
#define ESP_PANEL_BOARD_LCD_RGB_IO_DATA15               (14)
#endif
#endif

#define ESP_PANEL_BOARD_LCD_COLOR_BITS          (ESP_PANEL_LCD_COLOR_BITS_RGB565)
#define ESP_PANEL_BOARD_LCD_COLOR_BGR_ORDER     (0)
#define ESP_PANEL_BOARD_LCD_COLOR_INEVRT_BIT    (0)
#define ESP_PANEL_BOARD_LCD_SWAP_XY             (0)
#define ESP_PANEL_BOARD_LCD_MIRROR_X            (0)
#define ESP_PANEL_BOARD_LCD_MIRROR_Y            (0)
#define ESP_PANEL_BOARD_LCD_GAP_X               (0)
#define ESP_PANEL_BOARD_LCD_GAP_Y               (0)
#define ESP_PANEL_BOARD_LCD_RST_IO              (-1)
#define ESP_PANEL_BOARD_LCD_RST_LEVEL           (0)
#endif

#define ESP_PANEL_BOARD_USE_TOUCH               (1)

#if ESP_PANEL_BOARD_USE_TOUCH
#define ESP_PANEL_BOARD_TOUCH_CONTROLLER        GT911
#define ESP_PANEL_BOARD_TOUCH_BUS_TYPE          (ESP_PANEL_BUS_TYPE_I2C)
#define ESP_PANEL_BOARD_TOUCH_BUS_SKIP_INIT_HOST        (0)

#if ESP_PANEL_BOARD_TOUCH_BUS_TYPE == ESP_PANEL_BUS_TYPE_I2C
#define ESP_PANEL_BOARD_TOUCH_I2C_HOST_ID               (0)
#if !ESP_PANEL_BOARD_TOUCH_BUS_SKIP_INIT_HOST
#define ESP_PANEL_BOARD_TOUCH_I2C_CLK_HZ                (400 * 1000)
#define ESP_PANEL_BOARD_TOUCH_I2C_SCL_PULLUP            (1)
#define ESP_PANEL_BOARD_TOUCH_I2C_SDA_PULLUP            (1)
#define ESP_PANEL_BOARD_TOUCH_I2C_IO_SCL                (20)
#define ESP_PANEL_BOARD_TOUCH_I2C_IO_SDA                (19)
#endif
#define ESP_PANEL_BOARD_TOUCH_I2C_ADDRESS               (0)
#endif

#define ESP_PANEL_BOARD_TOUCH_SWAP_XY           (0)
#define ESP_PANEL_BOARD_TOUCH_MIRROR_X          (0)
#define ESP_PANEL_BOARD_TOUCH_MIRROR_Y          (0)
#define ESP_PANEL_BOARD_TOUCH_RST_IO            (-1)
#define ESP_PANEL_BOARD_TOUCH_RST_LEVEL         (0)
#define ESP_PANEL_BOARD_TOUCH_INT_IO            (-1)
#define ESP_PANEL_BOARD_TOUCH_INT_LEVEL         (0)
#endif

#define ESP_PANEL_BOARD_USE_BACKLIGHT           (1)

#if ESP_PANEL_BOARD_USE_BACKLIGHT
#define ESP_PANEL_BOARD_BACKLIGHT_TYPE          (ESP_PANEL_BACKLIGHT_TYPE_PWM_LEDC)
#define ESP_PANEL_BOARD_BACKLIGHT_IO            (2)
#define ESP_PANEL_BOARD_BACKLIGHT_ON_LEVEL      (1)
#define ESP_PANEL_BOARD_BACKLIGHT_IDLE_OFF      (0)
#endif

#define ESP_PANEL_BOARD_USE_EXPANDER            (0)

#define ESP_PANEL_BOARD_CUSTOM_FILE_VERSION_MAJOR 1
#define ESP_PANEL_BOARD_CUSTOM_FILE_VERSION_MINOR 2
#define ESP_PANEL_BOARD_CUSTOM_FILE_VERSION_PATCH 0

#endif // ESP_PANEL_BOARD_DEFAULT_USE_CUSTOM

// *INDENT-ON*

#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// ================== Audio 配置（ES8311+NS4150B）=====================
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// ES8311 I2C 控制（I2C_NUM_1 总线）
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_3
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_2

// ES8311 I2S 接口（双工模式：单 BCLK/WS + 双向数据）
#define AUDIO_I2S_GPIO_MCLK  GPIO_NUM_1
#define AUDIO_I2S_GPIO_BCLK  GPIO_NUM_5
#define AUDIO_I2S_GPIO_WS    GPIO_NUM_4
#define AUDIO_I2S_GPIO_DIN   GPIO_NUM_6  // ESP32 I2S RX ← codec DOUT（麦克风数据）
#define AUDIO_I2S_GPIO_DOUT  GPIO_NUM_7  // ESP32 I2S TX → codec DIN（喇叭数据）

// NS4150B 功放控制
#define AUDIO_CODEC_PA_PIN   GPIO_NUM_10

// ES8311 I2C 地址
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

// ================== LCD 显示屏配置 ==================
#define DISPLAY_SPI_SCK_PIN     GPIO_NUM_38
#define DISPLAY_SPI_MOSI_PIN    GPIO_NUM_39
#define DISPLAY_SPI_CS_PIN      GPIO_NUM_21
#define DISPLAY_DC_PIN          GPIO_NUM_45
#define DISPLAY_RST_PIN         GPIO_NUM_40
#define DISPLAY_BACKLIGHT_PIN   GPIO_NUM_46

// JD9853 实际显示区域为 172x320，但 X 方向有 34 像素的 offset
#define DISPLAY_WIDTH           172
#define DISPLAY_HEIGHT          320
#define DISPLAY_OFFSET_X        0
#define DISPLAY_OFFSET_Y        0

// 方向配置（正常竖屏模式）
#define DISPLAY_MIRROR_X        false
#define DISPLAY_MIRROR_Y        false
#define DISPLAY_SWAP_XY         true
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_SPI_MODE        0

#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

// ================== 触摸屏配置 ==================
#define TOUCH_I2C_SDA_PIN       GPIO_NUM_42
#define TOUCH_I2C_SCL_PIN       GPIO_NUM_41
#define TOUCH_RST_PIN           GPIO_NUM_47
#define TOUCH_INT_PIN           GPIO_NUM_48


// ========== SDCard配置（SDMMC模式） ============
#define SD_CARD_ENABLED
#define SDCARD_SDMMC_ENABLED
#define SDCARD_SDMMC_CLK_PIN     GPIO_NUM_16
#define SDCARD_SDMMC_CMD_PIN     GPIO_NUM_15
#define SDCARD_SDMMC_D0_PIN      GPIO_NUM_17
#define SDCARD_SDMMC_D1_PIN      GPIO_NUM_18
#define SDCARD_SDMMC_D2_PIN      GPIO_NUM_13
#define SDCARD_SDMMC_D3_PIN      GPIO_NUM_14
#define SDCARD_SDMMC_BUS_WIDTH   4
#define SDCARD_MOUNT_POINT       "/sdcard"

// ================== 按钮配置 ==================
#define BOOT_BUTTON_GPIO        GPIO_NUM_0  // 或使用其他可用GPIO

// ================== LED配置 ==================
#define BUILTIN_LED_GPIO        GPIO_NUM_NC


#endif // _BOARD_CONFIG_H_

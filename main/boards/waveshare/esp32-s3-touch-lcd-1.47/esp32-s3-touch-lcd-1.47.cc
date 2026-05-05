#include "wifi_board.h"
#include "audio/codecs/es8311_audio_codec.h"
#include "audio/codecs/dummy_audio_codec.h"
#include "es8311_codec.h"
#include "display/lcd_display.h"
#include "custom_lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "esp_lcd_jd9853.h"
#include "esp_lcd_touch_axs5106.h"
#include "sdcard_service.h"
#include "lvgl_fs_driver.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>

#define TAG "Esp32S3TouchLcd147"

class Esp32S3TouchLcd147 : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    i2c_master_bus_handle_t codec_i2c_bus_ = nullptr;
    LcdDisplay* display_ = nullptr;
    Button boot_button_;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = TOUCH_I2C_SDA_PIN,
            .scl_io_num = TOUCH_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeJd9853Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        ESP_LOGD(TAG, "Install LCD driver (JD9853)");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_jd9853(panel_io, &panel_config, &panel));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR));
        ESP_LOGI(TAG, "HW set_gap: x_gap=34 y_gap=0");
        ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, 34, 0));
        ESP_LOGI(TAG, "HW mirror: mirror_x=%d mirror_y=%d", DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_LOGI(TAG, "HW swap_xy: %d", DISPLAY_SWAP_XY);
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        display_ = new CustomLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                    DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTouch() {
        static i2c_master_dev_handle_t tp_dev_handle;
        i2c_device_config_t tp_dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = ESP_LCD_TOUCH_IO_I2C_AXS5106_ADDRESS,
            .scl_speed_hz = 400 * 1000,
        };
        esp_err_t ret = i2c_master_bus_add_device(i2c_bus_, &tp_dev_cfg, &tp_dev_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Touch I2C device add failed: %s, touch disabled", esp_err_to_name(ret));
            return;
        }

        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH - 1,
            .y_max = DISPLAY_HEIGHT - 1,
            .rst_gpio_num = TOUCH_RST_PIN,
            .int_gpio_num = TOUCH_INT_PIN,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 0,
                .mirror_x = 1,
                .mirror_y = 0,
            },
        };
        ESP_LOGI(TAG, "Initialize touch controller AXS5106");
        ret = esp_lcd_touch_new_i2c_axs5106(tp_dev_handle, &tp_cfg, &tp);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Touch controller init failed: %s, touch disabled", esp_err_to_name(ret));
            return;
        }
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp,
        };
        lvgl_port_add_touch(&touch_cfg);
        ESP_LOGI(TAG, "Touch panel initialized successfully");
    }

    void InitializeSdCard() {
        SdCardConfig cfg = {
            .clk_pin = SDCARD_SDMMC_CLK_PIN,
            .cmd_pin = SDCARD_SDMMC_CMD_PIN,
            .d0_pin = SDCARD_SDMMC_D0_PIN,
            .d1_pin = SDCARD_SDMMC_D1_PIN,
            .d2_pin = SDCARD_SDMMC_D2_PIN,
            .d3_pin = SDCARD_SDMMC_D3_PIN,
            .bus_width = SDCARD_SDMMC_BUS_WIDTH,
            .mount_point = SDCARD_MOUNT_POINT,
        };
        esp_err_t ret = SdCardService::Initialize(cfg);
        if (ret == ESP_OK) {
            LvglFsDriver::Initialize();
        }
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

public:
    Esp32S3TouchLcd147() : boot_button_(BOOT_BUTTON_GPIO) {
        // 在ES8311初始化之前拉低PA引脚，防止NS4150B功放浮空使能产生噪声
        gpio_config_t pa_cfg = {
            .pin_bit_mask = 1ULL << AUDIO_CODEC_PA_PIN,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&pa_cfg);
        gpio_set_level(AUDIO_CODEC_PA_PIN, 0);

        InitializeI2c();
        InitializeCodecI2c();
        InitializeSpi();
        InitializeJd9853Display();
        InitializeTouch();
        InitializeSdCard();
        InitializeButtons();
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        }
    }

    virtual Display* GetDisplay() override {
        return display_;
    }


    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        // Probe ES8311 quickly to avoid long I2C timeout when not powered
        if (i2c_master_probe(codec_i2c_bus_, AUDIO_CODEC_ES8311_ADDR, 50) != ESP_OK) {
            ESP_LOGW(TAG, "ES8311 not detected on I2C bus, using dummy audio codec");
            static DummyAudioCodec dummy_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE);
            return &dummy_codec;
        }
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, I2C_NUM_1,
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }
};

DECLARE_BOARD(Esp32S3TouchLcd147);

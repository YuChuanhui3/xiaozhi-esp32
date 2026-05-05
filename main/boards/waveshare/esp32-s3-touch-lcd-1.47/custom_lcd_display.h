#ifndef CUSTOM_LCD_DISPLAY_H
#define CUSTOM_LCD_DISPLAY_H

#include "lcd_display.h"
#include <lvgl.h>

// Screen resolution for ESP32-S3-Touch-LCD-1.47
#define CUSTOM_LCD_WIDTH  320
#define CUSTOM_LCD_HEIGHT 172

// Color definitions (RGB565)
// Dark Tech Theme
#define COLOR_BG_MAIN       lv_color_hex(0x0079)    // #0B0F19
#define COLOR_CARD_BG       lv_color_hex(0x1A202C)  // Semi-transparent
#define COLOR_PRIMARY       lv_color_hex(0x9E60)    // #00D4FF
#define COLOR_TEXT_PRI      lv_color_hex(0xF75E)    // #E2E8F0
#define COLOR_GLASS_BD_OPA  LV_OPA_10               // 10% opacity for glass border

// Font sizes
#define FONT_L1 16  // Page title / Core status
#define FONT_L2 13  // Card labels / Song names / Tabs
#define FONT_L3 11  // Subtitles / Hints
#define FONT_L0 18  // Monospace numbers (clock/volume/progress)

// Page IDs
typedef enum {
    PAGE_AI_INTERACTION = 0,
    PAGE_SD_MUSIC,
    PAGE_MUSIC_PLAYER,
    PAGE_ALARM_CLOCK,
    PAGE_COUNT
} CustomPageId;

class CustomLcdDisplay : public LcdDisplay {
public:
    CustomLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                     int width, int height, int offset_x, int offset_y,
                     bool mirror_x, bool mirror_y, bool swap_xy);
    ~CustomLcdDisplay();

    void SetupUI() override;
    
    // Page management
    void SwitchToPage(CustomPageId page_id);
    CustomPageId GetCurrentPage() const { return current_page_; }
    
    // AI Interaction Page methods
    void SetAiStatus(const char* status);
    void SetWaveformData(int16_t* data, int length);
    
    // Music Player Page methods
    void SetMusicInfo(const char* title, const char* artist, const char* album);
    void SetPlaybackProgress(int current_ms, int total_ms);
    void SetPlayingState(bool is_playing);
    
    // Alarm Clock Page methods
    void AddAlarm(uint8_t hour, uint8_t minute, const char* label, bool enabled);
    void UpdateAlarmState(int index, bool enabled);

protected:
    CustomPageId current_page_ = PAGE_AI_INTERACTION;
    
    // Screen objects
    lv_obj_t* page_containers_[PAGE_COUNT] = {nullptr};
    
    // Status bar objects (shared)
    lv_obj_t* time_label_ = nullptr;
    lv_obj_t* wifi_icon_ = nullptr;
    lv_obj_t* battery_icon_ = nullptr;
    
    // Page A: AI Interaction
    lv_obj_t* ai_status_label_ = nullptr;
    lv_obj_t* ai_substatus_label_ = nullptr;
    lv_obj_t* waveform_chart_ = nullptr;
    lv_obj_t* weather_btn_ = nullptr;
    lv_obj_t* music_btn_ = nullptr;
    lv_obj_t* wiki_btn_ = nullptr;
    
    // Page B: SD Music List
    lv_obj_t* music_list_ = nullptr;
    lv_obj_t* tab_bar_ = nullptr;
    
    // Page C: Music Player
    lv_obj_t* album_cover_ = nullptr;
    lv_obj_t* song_title_label_ = nullptr;
    lv_obj_t* artist_label_ = nullptr;
    lv_obj_t* progress_chart_ = nullptr;
    lv_obj_t* progress_bar_ = nullptr;
    lv_obj_t* prev_btn_ = nullptr;
    lv_obj_t* play_pause_btn_ = nullptr;
    lv_obj_t* next_btn_ = nullptr;
    
    // Page D: Alarm Clock
    lv_obj_t* alarm_list_ = nullptr;
    lv_obj_t* add_alarm_btn_ = nullptr;

private:
    void CreateStatusBar(lv_obj_t* parent);
    void CreateAiInteractionPage(lv_obj_t* parent);
    void CreateSdMusicPage(lv_obj_t* parent);
    void CreateMusicPlayerPage(lv_obj_t* parent);
    void CreateAlarmClockPage(lv_obj_t* parent);
    
    static void ButtonClickCallback(lv_event_t* e);
    static void TabChangeCallback(lv_event_t* e);
};

#endif // CUSTOM_LCD_DISPLAY_H

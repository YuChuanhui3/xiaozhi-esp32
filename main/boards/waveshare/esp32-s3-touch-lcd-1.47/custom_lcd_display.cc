#include "custom_lcd_display.h"
#include "lvgl_theme.h"
#include "board.h"
#include "application.h"

#include <esp_log.h>
#include <esp_err.h>
#include <font_awesome.h>
#include <cstring>

#define TAG "CustomLcdDisplay"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(BUILTIN_ICON_FONT);

CustomLcdDisplay::CustomLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                   int width, int height, int offset_x, int offset_y,
                                   bool mirror_x, bool mirror_y, bool swap_xy)
    : LcdDisplay(panel_io, panel, width, height) {
}

CustomLcdDisplay::~CustomLcdDisplay() {
    // Clean up page containers
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (page_containers_[i] != nullptr) {
            lv_obj_del(page_containers_[i]);
        }
    }
}

void CustomLcdDisplay::SetupUI() {
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }

    Display::SetupUI();
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lvgl_theme->text_color(), 0);
    lv_obj_set_style_bg_color(screen, COLOR_BG_MAIN, 0);

    // Create main container with flex column layout
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_radius(container_, 0, 0);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, COLOR_BG_MAIN, 0);

    // Create status bar (shared across all pages)
    CreateStatusBar(container_);

    // Create page container for content
    content_ = lv_obj_create(container_);
    lv_obj_set_size(content_, LV_HOR_RES, LV_VER_RES - 24);  // Subtract status bar height
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_style_bg_opa(content_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);

    // Create all four pages as children of content_
    CreateAiInteractionPage(content_);
    CreateSdMusicPage(content_);
    CreateMusicPlayerPage(content_);
    CreateAlarmClockPage(content_);

    // Show only AI Interaction page initially
    SwitchToPage(PAGE_AI_INTERACTION);

    // Hide the default emoji_label_ since we have custom UI
    lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
}

void CustomLcdDisplay::CreateStatusBar(lv_obj_t* parent) {
    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto icon_font = lvgl_theme->icon_font()->font();

    // Status bar at top: y=4, h=20
    top_bar_ = lv_obj_create(parent);
    lv_obj_set_size(top_bar_, LV_HOR_RES, 20);
    lv_obj_set_style_radius(top_bar_, 0, 0);
    lv_obj_set_style_bg_opa(top_bar_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_bar_, 0, 0);
    lv_obj_set_style_pad_all(top_bar_, 0, 0);
    lv_obj_set_flex_flow(top_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Time label (left side)
    time_label_ = lv_label_create(top_bar_);
    lv_label_set_text(time_label_, "14:30");
    lv_obj_set_style_text_font(time_label_, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(time_label_, COLOR_TEXT_PRI, 0);

    // Right icons container
    lv_obj_t* right_icons = lv_obj_create(top_bar_);
    lv_obj_set_size(right_icons, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(right_icons, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_icons, 0, 0);
    lv_obj_set_style_pad_all(right_icons, 0, 0);
    lv_obj_set_flex_flow(right_icons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right_icons, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // WiFi icon
    wifi_icon_ = lv_label_create(right_icons);
    lv_label_set_text(wifi_icon_, FONT_AWESOME_WIFI);
    lv_obj_set_style_text_font(wifi_icon_, icon_font, 0);
    lv_obj_set_style_text_color(wifi_icon_, COLOR_TEXT_PRI, 0);

    // Battery icon
    battery_icon_ = lv_label_create(right_icons);
    lv_label_set_text(battery_icon_, FONT_AWESOME_BATTERY_FULL);
    lv_obj_set_style_text_font(battery_icon_, icon_font, 0);
    lv_obj_set_style_text_color(battery_icon_, COLOR_TEXT_PRI, 0);
    lv_obj_set_style_margin_left(battery_icon_, 8, 0);
}

void CustomLcdDisplay::CreateAiInteractionPage(lv_obj_t* parent) {
    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();

    // Page container
    page_containers_[PAGE_AI_INTERACTION] = lv_obj_create(parent);
    lv_obj_set_size(page_containers_[PAGE_AI_INTERACTION], LV_HOR_RES, LV_VER_RES - 24);
    lv_obj_set_style_radius(page_containers_[PAGE_AI_INTERACTION], 0, 0);
    lv_obj_set_style_bg_opa(page_containers_[PAGE_AI_INTERACTION], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page_containers_[PAGE_AI_INTERACTION], 0, 0);
    lv_obj_set_style_pad_all(page_containers_[PAGE_AI_INTERACTION], 0, 0);
    lv_obj_set_scrollbar_mode(page_containers_[PAGE_AI_INTERACTION], LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* page = page_containers_[PAGE_AI_INTERACTION];

    // Waveform area: y=32, h=28 (static placeholder)
    waveform_chart_ = lv_obj_create(page);
    lv_obj_set_size(waveform_chart_, 200, 28);
    lv_obj_set_style_radius(waveform_chart_, 8, 0);
    lv_obj_set_style_bg_opa(waveform_chart_, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(waveform_chart_, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(waveform_chart_, 1, 0);
    lv_obj_set_style_border_opa(waveform_chart_, LV_OPA_10, 0);
    lv_obj_set_style_border_color(waveform_chart_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(waveform_chart_, LV_OPA_10, 0);
    lv_obj_align(waveform_chart_, LV_ALIGN_TOP_MID, 0, 8);

    // AI status label: y=64, L1 font
    ai_status_label_ = lv_label_create(page);
    lv_label_set_text(ai_status_label_, "小智 AI");
    lv_obj_set_style_text_font(ai_status_label_, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(ai_status_label_, COLOR_TEXT_PRI, 0);
    lv_obj_align(ai_status_label_, LV_ALIGN_TOP_MID, 0, 40);

    // AI substatus label: y=82, L3 font
    ai_substatus_label_ = lv_label_create(page);
    lv_label_set_text(ai_substatus_label_, "已就绪 · 等待唤醒");
    lv_obj_set_style_text_font(ai_substatus_label_, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(ai_substatus_label_, lv_color_hex(0x888888), 0);
    lv_obj_align(ai_substatus_label_, LV_ALIGN_TOP_MID, 0, 58);

    // Quick action buttons container: y=108, h=32
    lv_obj_t* btn_container = lv_obj_create(page);
    lv_obj_set_size(btn_container, 280, 32);
    lv_obj_set_style_radius(btn_container, 0, 0);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(btn_container, LV_ALIGN_TOP_MID, 0, 84);

    // Weather button
    weather_btn_ = lv_btn_create(btn_container);
    lv_obj_set_size(weather_btn_, 80, 32);
    lv_obj_set_style_radius(weather_btn_, 8, 0);
    lv_obj_set_style_bg_opa(weather_btn_, LV_OPA_85, 0);
    lv_obj_set_style_bg_color(weather_btn_, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(weather_btn_, 1, 0);
    lv_obj_set_style_border_opa(weather_btn_, LV_OPA_10, 0);
    lv_obj_set_style_border_color(weather_btn_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_shadow_width(weather_btn_, 2, 0);
    lv_obj_add_event_cb(weather_btn_, ButtonClickCallback, LV_EVENT_PRESSED, this);
    
    lv_obj_t* weather_label = lv_label_create(weather_btn_);
    lv_label_set_text(weather_label, FONT_AWESOME_CLOUD " 天气");
    lv_obj_set_style_text_font(weather_label, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(weather_label, COLOR_TEXT_PRI, 0);
    lv_obj_center(weather_label);

    // Music button
    music_btn_ = lv_btn_create(btn_container);
    lv_obj_set_size(music_btn_, 80, 32);
    lv_obj_set_style_radius(music_btn_, 8, 0);
    lv_obj_set_style_bg_opa(music_btn_, LV_OPA_85, 0);
    lv_obj_set_style_bg_color(music_btn_, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(music_btn_, 1, 0);
    lv_obj_set_style_border_opa(music_btn_, LV_OPA_10, 0);
    lv_obj_set_style_border_color(music_btn_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_shadow_width(music_btn_, 2, 0);
    lv_obj_add_event_cb(music_btn_, ButtonClickCallback, LV_EVENT_PRESSED, this);
    
    lv_obj_t* music_label = lv_label_create(music_btn_);
    lv_label_set_text(music_label, FONT_AWESOME_MUSIC " 音乐");
    lv_obj_set_style_text_font(music_label, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(music_label, COLOR_TEXT_PRI, 0);
    lv_obj_center(music_label);

    // Wiki button
    wiki_btn_ = lv_btn_create(btn_container);
    lv_obj_set_size(wiki_btn_, 80, 32);
    lv_obj_set_style_radius(wiki_btn_, 8, 0);
    lv_obj_set_style_bg_opa(wiki_btn_, LV_OPA_85, 0);
    lv_obj_set_style_bg_color(wiki_btn_, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(wiki_btn_, 1, 0);
    lv_obj_set_style_border_opa(wiki_btn_, LV_OPA_10, 0);
    lv_obj_set_style_border_color(wiki_btn_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_shadow_width(wiki_btn_, 2, 0);
    lv_obj_add_event_cb(wiki_btn_, ButtonClickCallback, LV_EVENT_PRESSED, this);
    
    lv_obj_t* wiki_label = lv_label_create(wiki_btn_);
    lv_label_set_text(wiki_label, FONT_AWESOME_BOOK " 百科");
    lv_obj_set_style_text_font(wiki_label, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(wiki_label, COLOR_TEXT_PRI, 0);
    lv_obj_center(wiki_label);

    // Hint label: y=150
    lv_obj_t* hint_label = lv_label_create(page);
    lv_label_set_text(hint_label, "← 横向滑动切换功能 →");
    lv_obj_set_style_text_font(hint_label, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0x666666), 0);
    lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, -8);
}

void CustomLcdDisplay::CreateSdMusicPage(lv_obj_t* parent) {
    // Page container
    page_containers_[PAGE_SD_MUSIC] = lv_obj_create(parent);
    lv_obj_set_size(page_containers_[PAGE_SD_MUSIC], LV_HOR_RES, LV_VER_RES - 24);
    lv_obj_set_style_radius(page_containers_[PAGE_SD_MUSIC], 0, 0);
    lv_obj_set_style_bg_opa(page_containers_[PAGE_SD_MUSIC], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page_containers_[PAGE_SD_MUSIC], 0, 0);
    lv_obj_set_style_pad_all(page_containers_[PAGE_SD_MUSIC], 0, 0);
    lv_obj_set_scrollbar_mode(page_containers_[PAGE_SD_MUSIC], LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* page = page_containers_[PAGE_SD_MUSIC];

    // Tab bar: y=4, h=20
    tab_bar_ = lv_obj_create(page);
    lv_obj_set_size(tab_bar_, LV_HOR_RES, 20);
    lv_obj_set_style_radius(tab_bar_, 0, 0);
    lv_obj_set_style_bg_opa(tab_bar_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tab_bar_, 0, 0);
    lv_obj_set_style_pad_all(tab_bar_, 0, 0);
    lv_obj_set_flex_flow(tab_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_align(tab_bar_, LV_ALIGN_TOP_MID, 0, 0);

    // Album tab (inactive)
    lv_obj_t* album_tab = lv_label_create(tab_bar_);
    lv_label_set_text(album_tab, "相册");
    lv_obj_set_style_text_font(album_tab, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(album_tab, lv_color_hex(0x666666), 0);
    lv_obj_set_style_pad_right(album_tab, 16, 0);
    lv_obj_set_style_pad_left(album_tab, 16, 0);

    // Music tab (active)
    lv_obj_t* music_tab = lv_label_create(tab_bar_);
    lv_label_set_text(music_tab, "[ 音乐 ]");
    lv_obj_set_style_text_font(music_tab, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(music_tab, COLOR_PRIMARY, 0);
    lv_obj_set_style_pad_right(music_tab, 16, 0);
    lv_obj_set_style_pad_left(music_tab, 16, 0);

    // Music list container
    music_list_ = lv_obj_create(page);
    lv_obj_set_size(music_list_, LV_HOR_RES - 16, 110);
    lv_obj_set_style_radius(music_list_, 8, 0);
    lv_obj_set_style_bg_opa(music_list_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(music_list_, 0, 0);
    lv_obj_set_style_pad_all(music_list_, 8, 0);
    lv_obj_set_flex_flow(music_list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(music_list_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(music_list_, 8, 0);
    lv_obj_align(music_list_, LV_ALIGN_TOP_MID, 0, 28);

    // Add sample music items (3 visible items)
    const char* songs[] = {
        "晴天 - 周杰伦         03:22",
        "起风了 - 买辣椒也...    04:15",
        "夜曲 - 周杰伦         03:58"
    };

    for (int i = 0; i < 3; i++) {
        lv_obj_t* item = lv_obj_create(music_list_);
        lv_obj_set_size(item, LV_HOR_RES - 32, 34);
        lv_obj_set_style_radius(item, 8, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_85, 0);
        lv_obj_set_style_bg_color(item, COLOR_CARD_BG, 0);
        lv_obj_set_style_border_width(item, 1, 0);
    lv_obj_set_style_border_opa(item, LV_OPA_10, 0);
        lv_obj_set_style_border_color(item, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_pad_all(item, 4, 0);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* song_label = lv_label_create(item);
        lv_label_set_text(song_label, songs[i]);
        lv_obj_set_style_text_font(song_label, &BUILTIN_TEXT_FONT, 0);
        lv_obj_set_style_text_color(song_label, COLOR_TEXT_PRI, 0);

        lv_obj_t* play_icon = lv_label_create(item);
        lv_label_set_text(play_icon, FONT_AWESOME_PLAY);
        lv_obj_set_style_text_font(play_icon, &BUILTIN_ICON_FONT, 0);
        lv_obj_set_style_text_color(play_icon, COLOR_PRIMARY, 0);
    }

    // Song count hint
    lv_obj_t* count_label = lv_label_create(page);
    lv_label_set_text(count_label, "(共 128 首 · 滑动加载)");
    lv_obj_set_style_text_font(count_label, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(count_label, lv_color_hex(0x666666), 0);
    lv_obj_align(count_label, LV_ALIGN_BOTTOM_MID, 0, -8);
}

void CustomLcdDisplay::CreateMusicPlayerPage(lv_obj_t* parent) {
    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);

    // Page container
    page_containers_[PAGE_MUSIC_PLAYER] = lv_obj_create(parent);
    lv_obj_set_size(page_containers_[PAGE_MUSIC_PLAYER], LV_HOR_RES, LV_VER_RES - 24);
    lv_obj_set_style_radius(page_containers_[PAGE_MUSIC_PLAYER], 0, 0);
    lv_obj_set_style_bg_opa(page_containers_[PAGE_MUSIC_PLAYER], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page_containers_[PAGE_MUSIC_PLAYER], 0, 0);
    lv_obj_set_style_pad_all(page_containers_[PAGE_MUSIC_PLAYER], 0, 0);
    lv_obj_set_scrollbar_mode(page_containers_[PAGE_MUSIC_PLAYER], LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* page = page_containers_[PAGE_MUSIC_PLAYER];

    // "Now Playing" title: y=4, h=16
    lv_obj_t* title_label = lv_label_create(page);
    lv_label_set_text(title_label, "正在播放");
    lv_obj_set_style_text_font(title_label, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(title_label, COLOR_TEXT_PRI, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 4);

    // Album cover + song info: y=26
    lv_obj_t* info_container = lv_obj_create(page);
    lv_obj_set_size(info_container, LV_HOR_RES, 56);
    lv_obj_set_style_radius(info_container, 0, 0);
    lv_obj_set_style_bg_opa(info_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(info_container, 0, 0);
    lv_obj_set_style_pad_all(info_container, 0, 0);
    lv_obj_set_flex_flow(info_container, LV_FLEX_FLOW_ROW);
    lv_obj_align(info_container, LV_ALIGN_TOP_MID, 0, 26);

    // Album cover: 48x48, r=8
    album_cover_ = lv_obj_create(info_container);
    lv_obj_set_size(album_cover_, 48, 48);
    lv_obj_set_style_radius(album_cover_, 8, 0);
    lv_obj_set_style_bg_opa(album_cover_, LV_OPA_85, 0);
    lv_obj_set_style_bg_color(album_cover_, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(album_cover_, 1, 0);
    lv_obj_set_style_border_opa(album_cover_, LV_OPA_10, 0);
    lv_obj_set_style_border_color(album_cover_, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t* cover_icon = lv_label_create(album_cover_);
    lv_label_set_text(cover_icon, FONT_AWESOME_MUSIC);
    lv_obj_set_style_text_font(cover_icon, &BUILTIN_ICON_FONT, 0);
    lv_obj_set_style_text_color(cover_icon, COLOR_PRIMARY, 0);
    lv_obj_center(cover_icon);

    // Song info container
    lv_obj_t* song_info = lv_obj_create(info_container);
    lv_obj_set_size(song_info, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(song_info, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(song_info, 0, 0);
    lv_obj_set_style_pad_all(song_info, 0, 0);
    lv_obj_set_flex_flow(song_info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(song_info, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_margin_left(song_info, 8, 0);

    song_title_label_ = lv_label_create(song_info);
    lv_label_set_text(song_title_label_, "夜曲");
    lv_obj_set_style_text_font(song_title_label_, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(song_title_label_, COLOR_TEXT_PRI, 0);

    artist_label_ = lv_label_create(song_info);
    lv_label_set_text(artist_label_, "周杰伦 · 十一月的萧邦");
    lv_obj_set_style_text_font(artist_label_, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(artist_label_, lv_color_hex(0x888888), 0);

    // Progress chart (waveform): y=88, h=28
    progress_chart_ = lv_obj_create(page);
    lv_obj_set_size(progress_chart_, 280, 28);
    lv_obj_set_style_radius(progress_chart_, 8, 0);
    lv_obj_set_style_bg_opa(progress_chart_, LV_OPA_30, 0);
    lv_obj_set_style_border_width(progress_chart_, 0, 0);
    lv_obj_align(progress_chart_, LV_ALIGN_TOP_MID, 0, 88);

    // Progress bar: y=122, h=4
    progress_bar_ = lv_bar_create(page);
    lv_obj_set_size(progress_bar_, 280, 4);
    lv_obj_set_style_radius(progress_bar_, 2, 0);
    lv_obj_set_style_bg_opa(progress_bar_, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(progress_bar_, lv_color_hex(0x333333), 0);
    lv_obj_set_style_indicator_color(progress_bar_, COLOR_PRIMARY, 0);
    lv_bar_set_range(progress_bar_, 0, 100);
    lv_bar_set_value(progress_bar_, 35, LV_ANIM_OFF);
    lv_obj_align(progress_bar_, LV_ALIGN_TOP_MID, 0, 122);

    // Time labels
    lv_obj_t* current_time = lv_label_create(page);
    lv_label_set_text(current_time, "01:24");
    lv_obj_set_style_text_font(current_time, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(current_time, lv_color_hex(0x888888), 0);
    lv_obj_align(current_time, LV_ALIGN_TOP_LEFT, 20, 130);

    lv_obj_t* total_time = lv_label_create(page);
    lv_label_set_text(total_time, "03:58");
    lv_obj_set_style_text_font(total_time, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(total_time, lv_color_hex(0x888888), 0);
    lv_obj_align(total_time, LV_ALIGN_TOP_RIGHT, -20, 130);

    // Control buttons: y=132, h=30
    lv_obj_t* controls = lv_obj_create(page);
    lv_obj_set_size(controls, 200, 30);
    lv_obj_set_style_radius(controls, 0, 0);
    lv_obj_set_style_bg_opa(controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_style_pad_all(controls, 0, 0);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(controls, LV_ALIGN_BOTTOM_MID, 0, -20);

    // Previous button
    prev_btn_ = lv_btn_create(controls);
    lv_obj_set_size(prev_btn_, 50, 30);
    lv_obj_set_style_radius(prev_btn_, 8, 0);
    lv_obj_set_style_bg_opa(prev_btn_, LV_OPA_85, 0);
    lv_obj_set_style_bg_color(prev_btn_, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(prev_btn_, 1, 0);
    lv_obj_set_style_border_opa(prev_btn_, LV_OPA_10, 0);
    lv_obj_set_style_border_color(prev_btn_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_event_cb(prev_btn_, ButtonClickCallback, LV_EVENT_PRESSED, this);
    
    lv_obj_t* prev_icon = lv_label_create(prev_btn_);
    lv_label_set_text(prev_icon, FONT_AWESOME_BACKWARD);
    lv_obj_set_style_text_font(prev_icon, &BUILTIN_ICON_FONT, 0);
    lv_obj_set_style_text_color(prev_icon, COLOR_TEXT_PRI, 0);
    lv_obj_center(prev_icon);

    // Play/Pause button
    play_pause_btn_ = lv_btn_create(controls);
    lv_obj_set_size(play_pause_btn_, 60, 30);
    lv_obj_set_style_radius(play_pause_btn_, 8, 0);
    lv_obj_set_style_bg_opa(play_pause_btn_, LV_OPA_85, 0);
    lv_obj_set_style_bg_color(play_pause_btn_, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(play_pause_btn_, 1, 0);
    lv_obj_set_style_border_opa(play_pause_btn_, LV_OPA_10, 0);
    lv_obj_set_style_border_color(play_pause_btn_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_event_cb(play_pause_btn_, ButtonClickCallback, LV_EVENT_PRESSED, this);
    
    lv_obj_t* play_icon = lv_label_create(play_pause_btn_);
    lv_label_set_text(play_icon, FONT_AWESOME_PLAY);
    lv_obj_set_style_text_font(play_icon, &BUILTIN_ICON_FONT, 0);
    lv_obj_set_style_text_color(play_icon, COLOR_TEXT_PRI, 0);
    lv_obj_center(play_icon);

    // Next button
    next_btn_ = lv_btn_create(controls);
    lv_obj_set_size(next_btn_, 50, 30);
    lv_obj_set_style_radius(next_btn_, 8, 0);
    lv_obj_set_style_bg_opa(next_btn_, LV_OPA_85, 0);
    lv_obj_set_style_bg_color(next_btn_, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(next_btn_, 1, 0);
    lv_obj_set_style_border_opa(next_btn_, LV_OPA_10, 0);
    lv_obj_set_style_border_color(next_btn_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_event_cb(next_btn_, ButtonClickCallback, LV_EVENT_PRESSED, this);
    
    lv_obj_t* next_icon = lv_label_create(next_btn_);
    lv_label_set_text(next_icon, FONT_AWESOME_FORWARD);
    lv_obj_set_style_text_font(next_icon, &BUILTIN_ICON_FONT, 0);
    lv_obj_set_style_text_color(next_icon, COLOR_TEXT_PRI, 0);
    lv_obj_center(next_icon);
}

void CustomLcdDisplay::CreateAlarmClockPage(lv_obj_t* parent) {
    // Page container
    page_containers_[PAGE_ALARM_CLOCK] = lv_obj_create(parent);
    lv_obj_set_size(page_containers_[PAGE_ALARM_CLOCK], LV_HOR_RES, LV_VER_RES - 24);
    lv_obj_set_style_radius(page_containers_[PAGE_ALARM_CLOCK], 0, 0);
    lv_obj_set_style_bg_opa(page_containers_[PAGE_ALARM_CLOCK], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page_containers_[PAGE_ALARM_CLOCK], 0, 0);
    lv_obj_set_style_pad_all(page_containers_[PAGE_ALARM_CLOCK], 0, 0);
    lv_obj_set_scrollbar_mode(page_containers_[PAGE_ALARM_CLOCK], LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* page = page_containers_[PAGE_ALARM_CLOCK];

    // Header: "闹钟管理" + "+" button
    lv_obj_t* header = lv_obj_create(page);
    lv_obj_set_size(header, LV_HOR_RES, 20);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* title_label = lv_label_create(header);
    lv_label_set_text(title_label, "闹钟管理");
    lv_obj_set_style_text_font(title_label, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(title_label, COLOR_TEXT_PRI, 0);

    add_alarm_btn_ = lv_btn_create(header);
    lv_obj_set_size(add_alarm_btn_, 24, 24);
    lv_obj_set_style_radius(add_alarm_btn_, 8, 0);
    lv_obj_set_style_bg_opa(add_alarm_btn_, LV_OPA_85, 0);
    lv_obj_set_style_bg_color(add_alarm_btn_, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(add_alarm_btn_, 0, 0);
    lv_obj_add_event_cb(add_alarm_btn_, ButtonClickCallback, LV_EVENT_PRESSED, this);
    
    lv_obj_t* plus_label = lv_label_create(add_alarm_btn_);
    lv_label_set_text(plus_label, "+");
    lv_obj_set_style_text_font(plus_label, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(plus_label, COLOR_BG_MAIN, 0);
    lv_obj_center(plus_label);

    // Alarm list container
    alarm_list_ = lv_obj_create(page);
    lv_obj_set_size(alarm_list_, LV_HOR_RES - 16, 120);
    lv_obj_set_style_radius(alarm_list_, 0, 0);
    lv_obj_set_style_bg_opa(alarm_list_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(alarm_list_, 0, 0);
    lv_obj_set_style_pad_all(alarm_list_, 8, 0);
    lv_obj_set_flex_flow(alarm_list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(alarm_list_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(alarm_list_, 8, 0);
    lv_obj_align(alarm_list_, LV_ALIGN_TOP_MID, 0, 28);

    // Sample alarm items
    struct AlarmSample {
        const char* time;
        const char* label;
        bool enabled;
    };
    AlarmSample alarms[] = {
        {"07:00", "工作日早课", true},
        {"12:30", "午休提醒", false},
        {"22:00", "睡眠模式", true}
    };

    for (int i = 0; i < 3; i++) {
        lv_obj_t* item = lv_obj_create(alarm_list_);
        lv_obj_set_size(item, LV_HOR_RES - 32, 36);
        lv_obj_set_style_radius(item, 8, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_85, 0);
        lv_obj_set_style_bg_color(item, COLOR_CARD_BG, 0);
        lv_obj_set_style_border_width(item, 1, 0);
    lv_obj_set_style_border_opa(item, LV_OPA_10, 0);
        lv_obj_set_style_border_color(item, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_pad_all(item, 4, 0);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* time_label = lv_label_create(item);
        lv_label_set_text(time_label, alarms[i].time);
        lv_obj_set_style_text_font(time_label, &BUILTIN_TEXT_FONT, 0);
        lv_obj_set_style_text_color(time_label, COLOR_TEXT_PRI, 0);

        lv_obj_t* label_label = lv_label_create(item);
        lv_label_set_text(label_label, alarms[i].label);
        lv_obj_set_style_text_font(label_label, &BUILTIN_TEXT_FONT, 0);
        lv_obj_set_style_text_color(label_label, lv_color_hex(0x888888), 0);

        lv_obj_t* switch_label = lv_label_create(item);
        lv_label_set_text(switch_label, alarms[i].enabled ? "⬤ ON" : "○ OFF");
        lv_obj_set_style_text_font(switch_label, &BUILTIN_TEXT_FONT, 0);
        lv_obj_set_style_text_color(switch_label, alarms[i].enabled ? COLOR_PRIMARY : lv_color_hex(0x666666), 0);
    }

    // Hint label
    lv_obj_t* hint_label = lv_label_create(page);
    lv_label_set_text(hint_label, "(点击 ＋ 添加新闹钟)");
    lv_obj_set_style_text_font(hint_label, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0x666666), 0);
    lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, -8);
}

void CustomLcdDisplay::SwitchToPage(CustomPageId page_id) {
    if (page_id >= PAGE_COUNT || page_id < 0) {
        return;
    }

    DisplayLockGuard lock(this);

    // Hide all pages
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (page_containers_[i] != nullptr) {
            lv_obj_add_flag(page_containers_[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Show selected page
    if (page_containers_[page_id] != nullptr) {
        lv_obj_remove_flag(page_containers_[page_id], LV_OBJ_FLAG_HIDDEN);
    }

    current_page_ = page_id;
}

void CustomLcdDisplay::ButtonClickCallback(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED) {
        return;
    }

    lv_obj_t* btn = lv_event_get_target(e);
    CustomLcdDisplay* display = static_cast<CustomLcdDisplay*>(lv_event_get_user_data(e));

    // Handle button presses based on which button was pressed
    if (btn == display->weather_btn_) {
        ESP_LOGI(TAG, "Weather button pressed");
        // Navigate to weather page or show weather info
    } else if (btn == display->music_btn_) {
        ESP_LOGI(TAG, "Music button pressed");
        display->SwitchToPage(PAGE_MUSIC_PLAYER);
    } else if (btn == display->wiki_btn_) {
        ESP_LOGI(TAG, "Wiki button pressed");
        // Navigate to wiki page or show wiki info
    } else if (btn == display->prev_btn_) {
        ESP_LOGI(TAG, "Previous button pressed");
        // Previous track
    } else if (btn == display->play_pause_btn_) {
        ESP_LOGI(TAG, "Play/Pause button pressed");
        // Toggle play/pause
    } else if (btn == display->next_btn_) {
        ESP_LOGI(TAG, "Next button pressed");
        // Next track
    } else if (btn == display->add_alarm_btn_) {
        ESP_LOGI(TAG, "Add alarm button pressed");
        // Open add alarm dialog
    }
}

void CustomLcdDisplay::SetAiStatus(const char* status) {
    if (ai_status_label_ == nullptr) return;
    DisplayLockGuard lock(this);
    lv_label_set_text(ai_status_label_, status);
}

void CustomLcdDisplay::SetWaveformData(int16_t* data, int length) {
    // Placeholder for waveform update - would use lv_chart in production
    (void)data;
    (void)length;
}

void CustomLcdDisplay::SetMusicInfo(const char* title, const char* artist, const char* album) {
    if (song_title_label_ == nullptr || artist_label_ == nullptr) return;
    DisplayLockGuard lock(this);
    lv_label_set_text(song_title_label_, title);
    lv_label_set_text(artist_label_, artist);
    (void)album;  // Unused for now
}

void CustomLcdDisplay::SetPlaybackProgress(int current_ms, int total_ms) {
    if (progress_bar_ == nullptr || total_ms <= 0) return;
    DisplayLockGuard lock(this);
    int percent = (current_ms * 100) / total_ms;
    lv_bar_set_value(progress_bar_, percent, LV_ANIM_OFF);
}

void CustomLcdDisplay::SetPlayingState(bool is_playing) {
    if (play_pause_btn_ == nullptr) return;
    DisplayLockGuard lock(this);
    
    lv_obj_t* icon = lv_obj_get_child(play_pause_btn_, 0);
    if (icon != nullptr) {
        lv_label_set_text(icon, is_playing ? FONT_AWESOME_PAUSE : FONT_AWESOME_PLAY);
    }
}

void CustomLcdDisplay::AddAlarm(uint8_t hour, uint8_t minute, const char* label, bool enabled) {
    // Placeholder for adding alarms dynamically
    (void)hour;
    (void)minute;
    (void)label;
    (void)enabled;
}

void CustomLcdDisplay::UpdateAlarmState(int index, bool enabled) {
    // Placeholder for updating alarm state
    (void)index;
    (void)enabled;
}

void CustomLcdDisplay::TabChangeCallback(lv_event_t* e) {
    // Placeholder for tab change handling
    (void)e;
}

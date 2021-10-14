#ifndef DISPLAY_OLED_96IN_H
#define DISPLAY_OLED_96IN_H

#include <U8g2lib.h>

#include "DisplaySystem.hpp"

// Change these based on where the board is connected to the ESP32
// May want to make this more parameterized down the road?
#define I2C_SDA 23
#define I2C_SCL 22

#define OLED96in_X_PIXELS 128
#define OLED96in_Y_PIXELS 64

#define OLED96in_WIDTH_IN_MM 22
#define OLED96in_HEIGHT_IN_MM 11

class DisplayOLED96in : public DisplaySystem {
public:
    DisplayOLED96in();
    ~DisplayOLED96in();

    void draw_box(int x_start, int y_start, int x_end, int y_end) override;

    void draw_text() override;
    void draw_text(String new_text) override;
    void draw_text(String new_text, int x, int y) override;
    void draw_text_in_box(String new_text, int start_x, int start_y, int end_x, int end_y) override;
    void draw_big_word_in_box(String new_text, int start_x, int start_y, int end_x,
                              int end_y) override;

    auto get_text_height() -> int override;
    auto get_text_width(const char *buffer) -> int override;

    void set_font(const uint8_t *new_font);

    void display_splash_screen() override;

    void set_partial_refresh();
    void unset_partial_refresh();
    auto get_partial_refresh() -> int;

protected:
    void draw_text_cmds_only(const String &new_text, int x, int y);
    void draw_box_cmds_only(int x_start, int y_start, int x_end, int y_end);

    const uint8_t *font;
    const uint8_t *big_font;

    U8G2_SSD1306_128X64_NONAME_F_HW_I2C *u8g2;
};

#endif
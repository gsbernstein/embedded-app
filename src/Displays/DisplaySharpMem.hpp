#ifndef DISPLAY_SHARPMEM_H
#define DISPLAY_SHARPMEM_H

#include <U8g2lib.h>

#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

#define SHARPMEM_X_PIXELS 144
#define SHARPMEM_Y_PIXELS 168

#define SHARPMEM_WIDTH_IN_MM 21.5
#define SHARPMEM_HEIGHT_IN_MM 24.5

class DisplaySharpMem : public DisplaySystem {
public:
    DisplaySharpMem();
    ~DisplaySharpMem();

    void draw_box(int x_start, int y_start, int x_end, int y_end) override;

    void draw_text() override;
    void draw_text(String new_text) override;
    void draw_text(String new_text, int x, int y) override;

    void draw_text_in_box(String new_text, int start_x, int start_y, int end_x, int end_y) override;

    auto get_text_height() -> int override;
    auto get_text_width(const char *buffer) -> int override;

    void set_partial_refresh();
    void unset_partial_refresh();
    auto get_partial_refresh() -> int;

    void display_splash_screen() override;

    void draw_big_word_in_box(String new_text, int start_x, int start_y, int end_x,
                              int end_y) override;

    void set_font(const uint8_t *new_font);

protected:
    void draw_text_cmds_only(const String &new_text, int x, int y);
    void draw_box_cmds_only(int x_start, int y_start, int x_end, int y_end);
    void setup_display_for_text();

    const uint8_t *font;
    const uint8_t *big_font;
    // U8G2_SSD1306_128X64_NONAME_F_HW_I2C* u8g2;
    U8G2_LS013B7DH05_144X168_F_4W_HW_SPI *u8g2;
};

#endif
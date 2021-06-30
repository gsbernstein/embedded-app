#ifndef DISPLAY_SHARPMEM_H
#define DISPLAY_SHARPMEM_H

#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

#include <U8g2lib.h>


#define SHARPMEM_X_PIXELS 144
#define SHARPMEM_Y_PIXELS 168

#define SHARPMEM_WIDTH_IN_MM 21.5
#define SHARPMEM_HEIGHT_IN_MM 24.5

class DisplaySharpMem : public DisplaySystem {
    public:
        DisplaySharpMem();
        ~DisplaySharpMem();

        void draw_box(int x_start, int y_start, int x_end, int y_end);

        void draw_text();
        void draw_text(String new_text);
        void draw_text(String new_text, int x, int y);

        void draw_text_in_box(String new_text, int start_x, int start_y, int end_x, int end_y);

        int get_text_height();
        int get_text_width(const char* buffer);

        void set_partial_refresh();
        void unset_partial_refresh();
        int get_partial_refresh();

        void display_splash_screen();

        void draw_big_word_in_box(String new_text, int start_x, int start_y, int end_x, int end_y);
        
        void set_font(const uint8_t* new_font);

    protected:

        void draw_text_cmds_only(String new_text, int x, int y);
        void draw_box_cmds_only(int x_start, int y_start, int x_end, int y_end);
        void setup_display_for_text();

        const uint8_t* font;
        const uint8_t* big_font;
        //U8G2_SSD1306_128X64_NONAME_F_HW_I2C* u8g2;
        U8G2_LS013B7DH05_144X168_F_4W_HW_SPI* u8g2;
        

};

#endif
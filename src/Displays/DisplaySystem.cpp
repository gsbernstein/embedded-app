#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

DisplaySystem::DisplaySystem() {
    x_to_draw = 0;
    y_to_draw = 0;
    string_to_draw = "";
}

DisplaySystem::~DisplaySystem(){

}
/*
void DisplaySystem::draw_box(int start_x, int start_y, int end_x, int end_y) {
    DEBUG_LN("Drawing box in wrong class :(");

}*/

void DisplaySystem::set_x(int new_x) {
    x = new_x;

}

void DisplaySystem::set_y(int new_y) {
    y = new_y;
}

int DisplaySystem::get_x() {
    return x;
}

int DisplaySystem::get_y() {
    return y;
}

void DisplaySystem::set_width(float new_width) {
    width_in_mm = new_width;
}

void DisplaySystem::set_height(float new_height) {
    height_in_mm = new_height;
}

float DisplaySystem::get_width() {
    return width_in_mm;
}

float DisplaySystem::get_height() {
    return height_in_mm;
}

void DisplaySystem::set_partial_refresh() {
    partial_refresh = 1;
}

void DisplaySystem::unset_partial_refresh() {
    partial_refresh = 0;
}

int DisplaySystem::get_partial_refresh() {
   return partial_refresh; 
}

void DisplaySystem::set_coordinates(int init_x, int init_y) {
  x_to_draw = init_x;
  y_to_draw = init_y;
}

void DisplaySystem::set_text(String new_text) {
    string_to_draw = new_text;
}

// Takes the member variable string_to_draw and splits it 
// based on the \n characater and populates the String* lines;
void DisplaySystem::split_string_to_lines() {
    int len, i, start_pos, end_pos;

    len = string_to_draw.length();

    // Track where we are in the string
    start_pos = 0;
    end_pos = 0;
    num_lines = 1;
    //if (Serial) Serial.println("Got to split");
    //if (Serial) Serial.println(string_to_draw);

    for (i = 0; i < len; i++) {
        // We found a new line. Split off the previous line and 
        // start processing the new line
        //if (Serial) Serial.print(string_to_draw.charAt(i));
        if (string_to_draw.charAt(i) == '\n') {
            //if (Serial) Serial.println("saw a new line");
            lines[num_lines - 1] = string_to_draw.substring(start_pos, end_pos+1);
            num_lines++;
            assert(num_lines < MAX_LINES_IN_SPLIT);
            start_pos = i + 1;
            end_pos = i + 1;
        } else {
            end_pos = i;
        }
    }

    // If \n is *not* the last character, we need to add the last line to the
    // array, but if \n is the last character, we don't need to do anything and
    // we can decrement num_lines since the above loop assumes that there will
    // be more characters after the \n.
    if (start_pos != end_pos)
        lines[num_lines-1] = string_to_draw.substring(start_pos, end_pos+1);
    else
        num_lines--;

    //if (Serial) Serial.println("Finishing split");

}
#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <Arduino.h>

#include "sindarin-debug.h"

#define MAX_NUM_MENU_ITEMS 10

class MenuSystem {

public:
    MenuSystem();

    void add_menu_item(const String &new_item);

    void reset_menu_items();

    auto get_num_items() -> int;

    void set_curr_selection(int new_selection);

    auto get_curr_selection() -> int;

    // Returns 1 if the selection was updated, 0 otherwise
    // Helps client to know if it needs to refresh (at beginning or end of menu)
    auto next_selection() -> int;
    auto prev_selection() -> int;

    // Includes the name of the item and it's number preceding it for easy display
    // Offset by two spaces
    auto get_item_at(int index) -> String;

    // Easy \n separated list of the items in the menu that can simply be passed
    // to a display. May want to handle this in a Renderer? But for now, this
    // is simple enough.
    auto get_whole_menu_as_string() -> String;

    // Allows the client to determine how many items from the menu it wants and
    // to generate a string accordingly.
    // Note it's up to the client to figure out what to do if the current
    // selection is outside of this range.
    auto get_menu_range_as_string(int start_item, int end_item) -> String;

protected:
    // Doesn't check for max size yet -- assumes all fit on one page
    int num_menu_items = 0;
    String menu_items[MAX_NUM_MENU_ITEMS];
    int curr_selection = 0;
};

#endif
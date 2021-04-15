#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include "sindarin-debug.h"
#include <Arduino.h>

#define MAX_NUM_MENU_ITEMS 7

class MenuSystem {

    public:
        MenuSystem();
        
        void add_menu_item(String new_item);
        
        void reset_menu_items();

        int get_num_items();
        
        // Includes the name of the item and it's number preceding it for easy display
        // Offset by two spaces
        String get_item_at(int index);

        // Easy \n separated list of the items in the menu that can simply be passed
        // to a display. May want to handle this in a Renderer? But for now, this 
        // is simple enough.
        String get_whole_menu_as_string();
        

    protected:
        // Doesn't check for max size yet -- assumes all fit on one page
        int num_menu_items = 0;
        String menu_items[MAX_NUM_MENU_ITEMS];
};

#endif
#include "MenuSystem.hpp"

 MenuSystem::MenuSystem() {
     num_menu_items = 0;
 }
        

void MenuSystem::add_menu_item(String new_item) {
    menu_items[num_menu_items] = new_item;
    num_menu_items++;
}
        
void MenuSystem::reset_menu_items() {
    num_menu_items = 0;
}

int MenuSystem::get_num_items() {
    return num_menu_items;
}
        
// Includes the name of the item and it's number preceding it for easy display
// Offset by two spaces
String MenuSystem::get_item_at(int index) {
    String all;
    assert(index < num_menu_items);
    all = "  " + String(index + 1) + " " + menu_items[index];
    return all;
}

String MenuSystem::get_whole_menu_as_string() {
    int i;
    String complete = "";
    for (i = 0; i < num_menu_items; i++) {
        complete = complete + get_item_at(i);
        complete = complete + "\n";
    }
    return complete;
}
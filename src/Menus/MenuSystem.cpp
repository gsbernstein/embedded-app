#include "MenuSystem.hpp"

MenuSystem::MenuSystem() { num_menu_items = 0; }

void MenuSystem::add_menu_item(String new_item) {
    menu_items[num_menu_items] = new_item;
    num_menu_items++;
}

void MenuSystem::reset_menu_items() { num_menu_items = 0; }

int MenuSystem::get_num_items() { return num_menu_items; }

void MenuSystem::set_curr_selection(int new_selection) { curr_selection = new_selection; }

int MenuSystem::get_curr_selection() { return curr_selection; }

// Returns 1 if the selection was updated, 0 otherwise
int MenuSystem::next_selection() {
    int old_sel = curr_selection;
    curr_selection++;
    // Don't go further than the last item in the menu
    if (curr_selection >= num_menu_items)
        curr_selection = num_menu_items - 1;

    return curr_selection - old_sel;
}

// Returns 1 if the selection was updated, 0 otherwise
int MenuSystem::prev_selection() {
    int old_sel = curr_selection;
    curr_selection--;
    // Don't go before the first menu item
    if (curr_selection < 0)
        curr_selection = 0;
    return old_sel - curr_selection;
}

// Includes the name of the item and it's number preceding it for easy display
// Offset by two spaces
String MenuSystem::get_item_at(int index) {
    String all;
    assert(index < num_menu_items);
    // Add the indicator for where we are in the menu based on the current selection
    if (index == curr_selection)
        all = " ->";
    else
        all = "   ";
    all = all + " " + menu_items[index];
    return all;
}

String MenuSystem::get_whole_menu_as_string() {
    return get_menu_range_as_string(0, num_menu_items - 1);
}

String MenuSystem::get_menu_range_as_string(int start_item, int end_item) {
    int i;
    String complete = "";
    // If the client gives us an end_item past the end of the range, just go to the
    // end only
    for (i = start_item - 1; i < end_item && i < num_menu_items; i++) {
        complete = complete + get_item_at(i);
        complete = complete + "\n";
    }
    return complete;
}
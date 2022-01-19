#include "PreferencesStore.hpp"

#include "sindarin-debug.h"

void PreferencesStore::begin() { prefs_.begin("sindarin-app", false); }

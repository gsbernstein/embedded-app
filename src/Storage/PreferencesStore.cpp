#include "PreferencesStore.hpp"

#include "sindarin-debug.h"

auto PreferencesStore::begin() -> bool { return prefs_.begin("sindarin-app", false); }

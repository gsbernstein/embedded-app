// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _PREFERENCES_H_
#define _PREFERENCES_H_

#include <inicpp.h>

#include "Arduino.h"

static const char *prefsKey = "prefs";
static const char *simprefsIni = "simprefs.ini";

template <typename T>
size_t WriteKeyValue(ini::IniFile &iniFile, const char *key, T value) {
    iniFile[prefsKey][key] = value;
    iniFile.save(simprefsIni);
    return 0;
}

template <typename T>
T ReadKeyValue(ini::IniFile &iniFile, const char *key, T defaultValue) {
    return iniFile[prefsKey][key].as<T>();
}

class Preferences {
protected:
    ini::IniFile iniFile_;

public:
    Preferences(){};
    ~Preferences() { end(); }

    bool begin(const char *name, bool readOnly = false, const char *partition_label = NULL) {
        iniFile_.load(simprefsIni);
        return true;
    }
    void end() {}

    bool clear() {
        iniFile_.clear();
        return true;
    }
    bool remove(const char *key) {
        iniFile_[prefsKey].erase(key);
        iniFile_.save(simprefsIni);
        return true;
    }

    size_t putChar(const char *key, int8_t value) {
        return WriteKeyValue(iniFile_, key, (int16_t)value);
    }
    size_t putUChar(const char *key, uint8_t value) { return WriteKeyValue(iniFile_, key, value); }
    size_t putShort(const char *key, int16_t value) { return WriteKeyValue(iniFile_, key, value); }
    size_t putUShort(const char *key, uint16_t value) {
        return WriteKeyValue(iniFile_, key, value);
    }
    size_t putInt(const char *key, int32_t value) { return WriteKeyValue(iniFile_, key, value); }
    size_t putUInt(const char *key, uint32_t value) { return WriteKeyValue(iniFile_, key, value); }
    size_t putLong(const char *key, int32_t value) { return WriteKeyValue(iniFile_, key, value); }
    size_t putULong(const char *key, uint32_t value) { return WriteKeyValue(iniFile_, key, value); }
    // size_t putLong64(const char *key, int64_t value) { return WriteKeyValue(iniFile_, key,
    // value); } size_t putULong64(const char *key, uint64_t value) {
    //     return WriteKeyValue(iniFile_, key, value);
    // }
    size_t putFloat(const char *key, float_t value) { return WriteKeyValue(iniFile_, key, value); }
    size_t putDouble(const char *key, double_t value) {
        return WriteKeyValue(iniFile_, key, value);
    }
    size_t putBool(const char *key, bool value) { return WriteKeyValue(iniFile_, key, value); }
    size_t putString(const char *key, const char *value) {
        return WriteKeyValue(iniFile_, key, value);
    }
    size_t putString(const char *key, String value) {
        return WriteKeyValue(iniFile_, key, value.c_str());
    }
    // size_t putBytes(const char *key, const void *value, size_t len) {
    // return WriteKeyValue(iniFile_, key, value);
    // }

    bool isKey(const char *key);
    int8_t getChar(const char *key, int8_t defaultValue = 0) {
        return (int8_t)ReadKeyValue(iniFile_, key, (int16_t)defaultValue);
    }
    uint8_t getUChar(const char *key, uint8_t defaultValue = 0) {
        return ReadKeyValue(iniFile_, key, defaultValue);
    }
    int16_t getShort(const char *key, int16_t defaultValue = 0) {
        return ReadKeyValue(iniFile_, key, defaultValue);
    }
    uint16_t getUShort(const char *key, uint16_t defaultValue = 0) {
        return ReadKeyValue(iniFile_, key, defaultValue);
    }
    int32_t getInt(const char *key, int32_t defaultValue = 0) {
        return ReadKeyValue(iniFile_, key, defaultValue);
    }
    uint32_t getUInt(const char *key, uint32_t defaultValue = 0) {
        return ReadKeyValue(iniFile_, key, defaultValue);
    }
    int32_t getLong(const char *key, int32_t defaultValue = 0) {
        return ReadKeyValue(iniFile_, key, defaultValue);
    }
    uint32_t getULong(const char *key, uint32_t defaultValue = 0) {
        return ReadKeyValue(iniFile_, key, defaultValue);
    }
    // int64_t getLong64(const char *key, int64_t defaultValue = 0) {
    //     return ReadKeyValue(iniFile_, key, defaultValue);
    // }
    // uint64_t getULong64(const char *key, uint64_t defaultValue = 0) {
    //     return ReadKeyValue(iniFile_, key, defaultValue);
    // }
    float_t getFloat(const char *key, float_t defaultValue = NAN) {
        return ReadKeyValue(iniFile_, key, defaultValue);
    }
    double_t getDouble(const char *key, double_t defaultValue = NAN) {
        return ReadKeyValue(iniFile_, key, defaultValue);
    }
    bool getBool(const char *key, bool defaultValue = false) {
        return ReadKeyValue(iniFile_, key, defaultValue);
    }
    size_t getString(const char *key, char *value, size_t maxLen) {
        auto str = iniFile_[prefsKey][key].as<const char *>();
        strncpy(value, str, maxLen);
        return 0;
    }
    String getString(const char *key, String defaultValue = String()) {
        const char *str = iniFile_[prefsKey][key].as<const char *>();
        return String(str);
    }
    // size_t getBytesLength(const char *key);
    // size_t getBytes(const char *key, void *buf, size_t maxLen);
};

#endif

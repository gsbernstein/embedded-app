#pragma once

#include <FS.h>
#include <vector>

#include "Contents/Readable.hpp"

// TODO: It's probably worth exploring `esp_vfs_t` as a generic filesystem interface
class FileSystem {
public:
    // Convenience methods
    void getReadables(std::vector<Readable> *readables);

    // Underlying methods to init and read data
    virtual void begin(){};
    // Note: File overrides bool and will be false if there's an error
    virtual File open(const String &path) { assert(0); };
    virtual bool exists(const String &path) { assert(0); };

protected:
    FileSystem() = default;
};

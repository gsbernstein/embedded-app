#include <FS.h>
#include <vector>

#include "Contents/Readable.hpp"

// TODO: It's probably worth exploring `esp_vfs_t` as a generic filesystem interface
class FileSystem {
public:
    // Convenience methods
    static auto getInstance() -> FileSystem *;
    void getReadables(std::vector<Readable> *readables);

    // Underlying methods to init and read data
    void begin();
    // Note: File overrides bool and will be false if there's an error
    File open(const String &path);

protected:
    static FileSystem *instance;
    FileSystem() = default;
};

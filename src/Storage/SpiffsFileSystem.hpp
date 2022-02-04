#include "Storage/FileSystem.hpp"

class SpiffsFileSystem : public FileSystem {
public:
    SpiffsFileSystem() : FileSystem(){};

    void begin() override;
    File open(const String &path) override;
    bool exists(const String &path) override;
};

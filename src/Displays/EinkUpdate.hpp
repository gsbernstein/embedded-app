#pragma once

#include <cstring>

#include "EinkTypes.hpp"

namespace eink {

inline void fillSourceData(uint8_t *sourceData, const uint8_t *newBuffer, const uint8_t *oldBuffer,
                           const std::array<uint8_t, 256> &lut,
                           const std::array<uint8_t, 256> &altLut, const EinkBox &box, int xStart,
                           int y) {
    for (int pixel = 0; pixel < CONFIG_DISPLAY_WIDTH / 2; pixel += 4) {
        int x = pixel + xStart;
        int shift = pixel % 8 == 0 ? 4 : 0;
        auto index =
            ((oldBuffer[pixel / 8] >> shift) & 0xF) << 4 | ((newBuffer[pixel / 8] >> shift) & 0xF);
        if (y < box.topY || y >= box.bottomY || x + 4 <= box.leftX || x >= box.rightX) {
            sourceData[pixel / 4] = lut[index];
        } else if (x >= box.leftX && x + 4 <= box.rightX) {
            sourceData[pixel / 4] = altLut[index];
        } else {
            uint8_t mask = 0;
            for (int bit = 0; bit < 4; ++bit) {
                if (box.contains(x + bit, y)) {
                    mask |= 0xC0 >> (bit * 2);
                }
            }
            sourceData[pixel / 4] = (lut[index] & ~mask) | (altLut[index] & mask);
        }
    }
}

inline void updateBufferHistory(uint8_t *previous, const uint8_t *next,
                                const EinkET013TT1Waveform &waveform,
                                const EinkET013TT1Waveform &altWaveform, EinkBox box) {
    bool emptyBox = box.rightX <= box.leftX || box.bottomY <= box.topY;
    if (emptyBox && waveform.frameCount == 0) {
        return;
    }
    if ((emptyBox && waveform.frameCount != 0) ||
        (waveform.frameCount != 0 && altWaveform.frameCount != 0)) {
        memcpy(previous, next, CONFIG_DISPLAY_WIDTH * CONFIG_DISPLAY_HEIGHT / 8);
        return;
    }
    for (int y = 0; y < CONFIG_DISPLAY_HEIGHT; ++y) {
        for (int x = 0; x < CONFIG_DISPLAY_WIDTH; x += 8) {
            uint8_t mask = 0;
            for (int bit = 0; bit < 8; ++bit) {
                if ((box.contains(x + bit, y) ? altWaveform : waveform).frameCount != 0) {
                    mask |= 0x80 >> bit;
                }
            }
            int index = (y * CONFIG_DISPLAY_WIDTH + x) / 8;
            // No-frame regions retain their physical image, not the submitted canvas.
            previous[index] = (previous[index] & ~mask) | (next[index] & mask);
        }
    }
}

} // namespace eink

#if CONFIG_DISPLAY_EINKET013TT1 && SIMULATOR

#include "FakeEinkET013TT1.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdio.h>

#include "EinkUpdate.hpp"
#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

// How much each e-ink particle will move each frame it is charged
int FakeEinkET013TT1ParticleMovement = PARTICLE_MOVEMENT_DEFAULT;

void EinkET013TT1::init() {
    if (renderQueue_ == nullptr) {
        renderQueue_ = xQueueCreate(1, sizeof(uint8_t));
    }
    xQueueReset(renderQueue_);
}

void EinkET013TT1::setVcom(int mV) {}

void EinkET013TT1::setFramerate(int frameRate) { frameRate = std::clamp(frameRate, 0, 100); }

void EinkET013TT1::setBorder(BorderSetting border) {}

void EinkET013TT1::clear(const EinkET013TT1Waveform &waveform) {
    update(waveform, previousBuffers_[0], INT32_MAX, BC_WAVEFORM_NONE, ScreenSide::Both,
           EinkBox::zero(), previousBuffers_[1]);
}

auto EinkET013TT1::getPixel(const uint8_t *buffer, int x, int y) -> uint8_t {
    int byteIndex = (y * CONFIG_DISPLAY_WIDTH + x) / 8;
    int bitIndex = (y * CONFIG_DISPLAY_WIDTH + x) % 8;
    return (buffer[byteIndex] >> (7 - bitIndex)) & 0x01;
}

void EinkET013TT1::update(const EinkET013TT1Waveform &waveform, const uint8_t *buffer,
                          int32_t overrideVcom, const EinkET013TT1Waveform &altWaveform,
                          ScreenSide screen, EinkBox box, const uint8_t *rightBuffer) {
    if (rightBuffer == nullptr) {
        rightBuffer = buffer;
    }

    struct Eye {
        ScreenSide side;
        const uint8_t *newBuffer;
        uint8_t *previousBuffer;
        uint8_t *particles;
    };
    std::array<Eye, 2> eyes = {{
        {ScreenSide::Left, buffer, previousBuffers_[0], particlePositions_.get()},
        {ScreenSide::Right, rightBuffer, previousBuffers_[1], rightParticlePositions_.get()},
    }};

    if (skipWaveform_) {
        uint8_t in = 1;
        while (xQueuePeekFromISR(renderQueue_, &in) == pdTRUE) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Skip waveform processing and just move the pixel all the way one way or another
        log_d("Skipping waveform processing (would have used \"%s\")", waveform.name);
        for (auto &eye : eyes) {
            if ((screen & eye.side) == 0) {
                continue;
            }
            for (int x = 0; x < CONFIG_DISPLAY_WIDTH; x++) {
                for (int y = 0; y < CONFIG_DISPLAY_HEIGHT; y++) {
                    const auto &pixelWaveform = box.contains(x, y) ? altWaveform : waveform;
                    if (pixelWaveform.frameCount == 0) {
                        continue;
                    }
                    auto pixel = getPixel(eye.newBuffer, x, y);
                    eye.particles[y * CONFIG_DISPLAY_WIDTH + x] = pixel * 255;
                }
            }
        }
        // Signal that we've drawn something
        if (!skipSignalling_) {
            xQueueSend(renderQueue_, &in, portMAX_DELAY);
        }
    } else {
        log_d("Updating display with waveform \"%s\"", waveform.name);
        // For each frame in the waveform
        log_d("Alt waveform: \"%s\"", altWaveform.name);

        int maxFrames = std::max(waveform.frameCount, altWaveform.frameCount);

        for (int frameIndex = 0; frameIndex < maxFrames; frameIndex++) {
            auto frame = waveform.frameOrDefault(frameIndex);
            auto altFrame = altWaveform.frameOrDefault(frameIndex);

            // For each pixel, calculate the correct way to move it for the waveform
            for (auto &eye : eyes) {
                if ((screen & eye.side) == 0) {
                    continue;
                }
                for (int x = 0; x < CONFIG_DISPLAY_WIDTH; x++) {
                    for (int y = 0; y < CONFIG_DISPLAY_HEIGHT; y++) {
                        auto pixel = getPixel(eye.newBuffer, x, y);
                        auto previousPixel = getPixel(eye.previousBuffer, x, y);
                        uint8_t setting = box.contains(x, y)
                                              ? altFrame.getSetting(previousPixel, pixel)
                                              : frame.getSetting(previousPixel, pixel);
                        auto position = eye.particles[y * CONFIG_DISPLAY_WIDTH + x];
                        // TODO: We could probably get math-y here and simulate the movement of the
                        // particles with physics. For now we're just estimating.
                        switch (setting) {
                        case WHITE:
                            position = std::min(position + FakeEinkET013TT1ParticleMovement, 255);
                            break;
                        case BLACK:
                            position = std::max(position - FakeEinkET013TT1ParticleMovement, 0);
                            break;
                        case NOCHG:
                            break;
                        }
                        eye.particles[y * CONFIG_DISPLAY_WIDTH + x] = position;
                    }
                }
            }
            uint8_t in = 1;
            if (!skipSignalling_) {
                xQueueSend(renderQueue_, &in, portMAX_DELAY);
            }

            // This doesn't account for processing time above, but probably real CPUs are fast
            // enough it doesn't matter?
            if (!skipSignalling_) {
                Delay(1000 / framerate_);
            }
        }
    }
    for (auto &eye : eyes) {
        if ((screen & eye.side) != 0) {
            eink::updateBufferHistory(eye.previousBuffer, eye.newBuffer, waveform, altWaveform,
                                      box);
        }
    }
}

auto EinkET013TT1::readTemperature(ScreenSide screen) -> int16_t { return 24; }

#endif

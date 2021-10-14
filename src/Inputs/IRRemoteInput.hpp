#ifndef IRRemoteInput_H
#define IRRemoteInput_H

#include "RemoteInput.hpp"

#define ONE 0x10
#define TWO 0x11
#define THREE 0x12
#define FOUR 0x14
#define FIVE 0x15
#define SIX 0x16
#define SEVEN 0x18
#define EIGHT 0x19
#define NINE 0x1A
#define MAX_TIME 150 // max ms between codes

class IRRemoteInput : public RemoteInput {

public:
    void start_remote(int receive_pin_input) override;
    void start_remote() override;
    auto check_press() -> int override;
    auto check_release() -> int override;

protected:
    long lastPressTime;
    int state;
    int receive_pin;
};

#endif
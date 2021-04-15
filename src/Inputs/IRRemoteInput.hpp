#ifndef IRRemoteInput_H
#define IRRemoteInput_H

#define VOL_DOWN 0x0
#define PLAY 0x1
#define VOL_UP 0x2
#define SETUP 0x4
#define PREV 0x5
#define STOP 0x6
#define CHANNEL_MINUS 0x8
#define ENTER 0x9
#define CHANNEL_PLUS 0xA
#define ZERO 0xC
#define NEXT 0xD
#define BACK 0xE
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

void print_out_code(const char* prec_msg, int code);

class IRRemoteInput {

  public:
    //IRRemoteInput(int reveive_pin_input);
    void start_remote(int receive_pin_input);
    int check_press();
    int check_release();
    int get_code();


  protected:
    int ir_last_code;
    long lastPressTime;
    int state;
    int receive_pin;

};

#endif
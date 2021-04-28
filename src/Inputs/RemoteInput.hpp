#ifndef RemoteInput_H
#define RemoteInput_H

#define VOL_DOWN 0x0
#define PLAY 0x1
#define VOL_UP 0x2
#define SETUP 0x4
#define PREV 0x5
#define STOP 0x6
#define CHANNEL_MINUS 0x8
#define ENTER 0x9
#define CHANNEL_PLUS 0xA
#define NOP_BUTTON 0xC
#define NEXT 0xD
#define BACK 0xE

void print_out_code(const char* prec_msg, int code);

class RemoteInput {

  public:
    virtual void start_remote() = 0;
    virtual void start_remote(int data) = 0;
    virtual int check_press() = 0;
    virtual int check_release() = 0;
    int get_code();


  protected:
    int last_code;
};

#endif
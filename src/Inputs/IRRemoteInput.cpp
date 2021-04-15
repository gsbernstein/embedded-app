#include <IRremote.h>
#include "IRRemoteInput.hpp"

void print_out_code(const char *prec_msg, int code) {
  Serial.print(prec_msg);
  Serial.print(" ");
  switch (code)
  {
  case VOL_DOWN:
    Serial.println("Volume Down");
    break;
  case PLAY:
    Serial.println("Play");
    break;
  case VOL_UP:
    Serial.println("Volume Up");
    break;
  case SETUP:
    Serial.println("Setup");
    break;
  case PREV:
    Serial.println("Previous");
    break;
  case STOP:
    Serial.println("Stop");
    break;
  case CHANNEL_MINUS:
    Serial.println("Channel -");
    break;
  case ENTER:
    Serial.println("Enter");
    break;
  case CHANNEL_PLUS:
    Serial.println("Channel +");
    break;
  case ZERO:
    Serial.println("0");
    break;
  case NEXT:
    Serial.println("Next");
    break;
  case BACK:
    Serial.println("Back");
    break;
  case ONE:
    Serial.println("1");
    break;
  case TWO:
    Serial.println("2");
    break;
  case THREE:
    Serial.println("3");
    break;
  case FOUR:
    Serial.println("4");
    break;
  case FIVE:
    Serial.println("5");
    break;
  case SIX:
    Serial.println("6");
    break;
  case SEVEN:
    Serial.println("7");
    break;
  case EIGHT:
    Serial.println("8");
    break;
  case NINE:
    Serial.println("9");
    break;
  default:
    Serial.println("Unknown code");
  }
}

void IRRemoteInput::start_remote(int receive_pin_input) {
  receive_pin = receive_pin_input;
  // Start the receiver, enable feedback LED,
  // take LED feedback pin from the internal boards definition
  IrReceiver.begin(receive_pin, true);
  ir_last_code = -1;
  lastPressTime = 0;
  state = LOW;
}

int IRRemoteInput::check_press() {
  int new_press = 0;
  if (IrReceiver.decode())
  {
    ir_last_code = IrReceiver.decodedIRData.command;
    if (state == LOW)
    {
      state = HIGH; // Button pressed, so set state to HIGH
      print_out_code("Pressed down on", ir_last_code);
      new_press = 1;
      // digitalWrite(LED_PIN, HIGH);
    }
    lastPressTime = millis();
    IrReceiver.resume(); // Receive the next value
  }
  return new_press;
}

int IRRemoteInput::check_release() {
  int found_release = 0;
  if ((state == HIGH) && ((millis() - lastPressTime) > MAX_TIME))
  {
    state = LOW; // Haven't heard from the button for a while, so not pressed
    print_out_code("Pressed up on", ir_last_code);
    found_release = 1;
    //digitalWrite(LED_PIN, LOW);
  }
  return found_release;
}

// Returns the IR code of the last button pressed or released
int IRRemoteInput::get_code() {
  return ir_last_code;
}
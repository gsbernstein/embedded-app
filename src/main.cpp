/**************************************************************************
 This is an example for our Monochrome OLEDs based on SSD1306 drivers

 Pick one up today in the adafruit shop!
 ------> http://www.adafruit.com/category/63_98

 This example is for a 128x64 pixel display using I2C to communicate
 3 pins are required to interface (two I2C and one reset).

 Adafruit invests time and resources providing this open
 source code, please support Adafruit and open-source
 hardware by purchasing products from Adafruit!

 Written by Limor Fried/Ladyada for Adafruit Industries,
 with contributions from the open source community.
 BSD license, check license.txt for more information
 All text above, and the splash screen below must be
 included in any redistribution.
 **************************************************************************/


// Obvious BUGS
// (1) Prev doesn't work at all
// (2) When you turn it off and back on, if you hadn't gone forward before,
// it goes to the screen you were on, but if you had gone forward, it goes
// to the next screen
// (3) When you go back to the main menu from the text, and then go back
// to the text, it returns you to the NEXT page instead of the page you were
// on (similar to the above).
//

#include <Arduino.h>
#include <Preferences.h>

#include "SPIFFS.h"

//#define EINK154in
#define OLED96in

//#define PlanoConvex20x40
#define PlanoConvex16x36

#include "Displays/AllDisplays.hpp"
#include "Optics/AllOptics.hpp"
#include "Renderers/Renderer.hpp"
#include "Renderers/FileRenderer.hpp"
#include "Menus/MenuSystem.hpp"
#include "Inputs/IRRemoteInput.hpp"

#include "sindarin-debug.h"

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#ifdef EINK154in
DisplayEink154in* my_display;
#endif

#ifdef OLED96in
DisplayOLED96in* my_display;
#endif

#ifdef PlanoConvex20x40
OpticsPlanoConvex20mmX40mm* my_optics;
#endif

#ifdef PlanoConvex16x36
OpticsPlanoConvex16mmX36mm* my_optics;
#endif

Renderer* box_renderer;
MenuSystem* main_menu;
IRRemoteInput *remote_input;


#define IR_REMOTE_INPUT_PIN 33

// Whether to redisplay the box or not
int update_box = 1;

// Areas the user could be in
#define MAIN_MENU_AREA 0
#define MATTHEW_AREA 1
#define MARK_AREA 2
#define LUKE_AREA 3
#define JOHN_AREA 4
#define LORD_OF_THE_WORLD_AREA 5

// Here are all the preferences that we will save off
// Eventually we should put this in an array so it's
// easier to save off, read them all in, and update
// it.
Preferences prefs;
int local_x_offset = 0;
int local_y_offset = 0;
int area_in_browsing = MAIN_MENU_AREA;
// These, for now, will be the lseek offsets into the files
// stored locally on device.
unsigned long matt_place = 0;
unsigned long mark_place = 0;
unsigned long luke_place = 0;
unsigned long john_place = 0;
unsigned long lord_world_place = 0;

unsigned long matt_prev_place = 0;
unsigned long mark_prev_place = 0;
unsigned long luke_prev_place = 0;
unsigned long john_prev_place = 0;
unsigned long lord_world_prev_place = 0;


// Menu Strings
#define MATT_MENU_ITEM "Matthew"
#define MARK_MENU_ITEM "Mark"
#define LUKE_MENU_ITEM "Luke"
#define JOHN_MENU_ITEM "John"
#define LORD_OF_THE_WORLD_MENU_ITEM "Lord of the World"

FileRenderer* matt_render;
FileRenderer* mark_render;
FileRenderer* luke_render;
FileRenderer* john_render;
FileRenderer* lord_of_world_render;

unsigned long new_place = 0;

void (*resetFunc)(void) = 0; //declare reset function @ address 0

// TODO: Refactor this out into a proper abstraction (probably at least two actually) -cojo
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#define SERVICE_UUID "db65598c-04b1-469f-833f-517f285db256"
#define CHARACTERISTIC_UUID "ed0c3501-a3cb-499f-93ad-2acbaa971d63"

BLEServer *server = NULL;
bool deviceConnected = false;
bool wasDeviceConnected = false;

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *server)
  {
    DEBUG_LN("BLE client connected to our server!");
    deviceConnected = true;
  }

  void onDisconnect(BLEServer *server)
  {
    DEBUG_LN("BLE client disconnected from our server!");
    deviceConnected = false;
  }
};

class CharacteristicCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *characteristic)
  {
    std::string value = characteristic->getValue();
    if (value.length() > 0)
    {
      DEBUG_LN("*********");
      DEBUG_F("New value: %.2x \n", value[0]);
      DEBUG_LN("*********");
    }

    switch (value.data()[0])
    {
    case SETUP:
      prefs.clear();
      DEBUG_LN("CLEARED PREFERENCES!");
      break;
    case STOP:
      DEBUG_LN("RESETTING BOARD BY SEG FAULT!");
      resetFunc();
      break;
    case CHANNEL_MINUS:
      // TODO: Check bounds
      //if (start_x != 0) {
      if (area_in_browsing == MAIN_MENU_AREA)
        local_x_offset--;
      else
      {
        switch (area_in_browsing)
        {
        case MATTHEW_AREA:
          matt_place = matt_prev_place;
          prefs.putULong("matt-pr-place", matt_place);

          break;
        case MARK_AREA:
          mark_place = mark_prev_place;
          prefs.putULong("mark-pr-place", mark_place);

          break;
        case LUKE_AREA:
          luke_place = luke_prev_place;
          prefs.putULong("luke-pr-place", luke_place);

          break;
        case JOHN_AREA:
          john_place = john_prev_place;
          prefs.putULong("john-pr-place", john_place);

          break;
        case LORD_OF_THE_WORLD_AREA:
          lord_world_place = lord_world_prev_place;
          prefs.putULong("lord-pr-place", lord_world_place);

          break;
        }
      }
      update_box = 1;
      //}
      break;
    case CHANNEL_PLUS:
      // TODO:
      // Check to be sure we don't go off the end of the screen
      //if (end_x != my_display->get_x()) {
      if (area_in_browsing == MAIN_MENU_AREA)
        local_x_offset++;
      else
      {
        switch (area_in_browsing)
        {
        case MATTHEW_AREA:
          matt_prev_place = matt_place;
          matt_place = new_place;
          prefs.putULong("matt-pr-place", matt_prev_place);
          prefs.putULong("matt-place", matt_place);

          break;
        case MARK_AREA:
          mark_prev_place = mark_place;
          mark_place = new_place;
          prefs.putULong("mark-pr-place", mark_prev_place);
          prefs.putULong("mark-place", mark_place);

          break;
        case LUKE_AREA:
          luke_prev_place = luke_place;
          luke_place = new_place;
          prefs.putULong("luke-pr-place", luke_prev_place);
          prefs.putULong("luke-place", luke_place);

          break;
        case JOHN_AREA:
          john_prev_place = john_place;
          john_place = new_place;
          DEBUG_VAR(john_prev_place);
          DEBUG_LN(" is the john old place");
          DEBUG_VAR(john_place);
          DEBUG_LN(" is the john new place");
          prefs.putULong("john-pr-place", john_prev_place);
          prefs.putULong("john-place", john_place);

          break;
        case LORD_OF_THE_WORLD_AREA:
          lord_world_prev_place = lord_world_place;
          lord_world_place = new_place;
          prefs.putULong("lord-pr-place", lord_world_prev_place);
          prefs.putULong("lord-place", lord_world_place);

          break;
        }
      }
      update_box = 1;
      //}
      break;
    case PREV:
      // TODO: Check bounds
      //if (start_y != 0) {
      if (area_in_browsing == MAIN_MENU_AREA)
        local_y_offset--;
      else
      {
      }
      update_box = 1;
      //}
      break;
    case NEXT:
      // TODO: Check to be sure we don't go off the end of the screen
      // if (end_y != my_display->get_y()) {
      if (area_in_browsing == MAIN_MENU_AREA)
        local_y_offset++;
      else
      {
      }
      update_box = 1;
      //}
      break;
    case ENTER:
      prefs.putInt("x-offset", local_x_offset);
      prefs.putInt("y-offset", local_y_offset);
      DEBUG_LN("Saving off x and y offset to preferences");
      DEBUG_VAR(local_x_offset);
      DEBUG_LN(" is the x offset");
      DEBUG_VAR(local_y_offset);
      DEBUG_LN(" is the y offset");
      update_box = 1;
      break;
    case ONE:
      area_in_browsing = MATTHEW_AREA;
      prefs.putInt("browse-area", area_in_browsing);
      update_box = 1;

      break;
    case TWO:
      area_in_browsing = MARK_AREA;
      prefs.putInt("browse-area", area_in_browsing);
      update_box = 1;

      break;
    case THREE:
      area_in_browsing = LUKE_AREA;
      prefs.putInt("browse-area", area_in_browsing);
      update_box = 1;

      break;
    case FOUR:
      area_in_browsing = JOHN_AREA;
      prefs.putInt("browse-area", area_in_browsing);
      update_box = 1;
      break;
    case FIVE:
      area_in_browsing = LORD_OF_THE_WORLD_AREA;
      prefs.putInt("browse-area", area_in_browsing);

      update_box = 1;
      break;
    case BACK:
      area_in_browsing = MAIN_MENU_AREA;
      prefs.putInt("browse-area", area_in_browsing);
      update_box = 1;
      break;
    default:
      break;
    }
  }
};
// END TOOD "Refactor this out... -cojo"

void setup(void)
{
  if (Serial) Serial.begin(115200);
  DEBUG_LN("About to try drawing something!");

  // Read / Write the preferences
  prefs.begin("sindarin-app", false);
 
  // Only uncomment this if you need to reset the prefs.
  // Then recomment after loading onto board and restart.
   //prefs.clear();

    // Launch SPIFFS file system  
  if(!SPIFFS.begin()){ 
    DEBUG_LN("An Error has occurred while mounting SPIFFS");  
  }

  // TODO: Refactor this out into a proper abstraction (probably at least two actually) -cojo
  BLEDevice::init("Sindarin (April)");
  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(BLEUUID(SERVICE_UUID));

  BLECharacteristic *characteristic = service->createCharacteristic(
      BLEUUID(CHARACTERISTIC_UUID),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  characteristic->setCallbacks(new CharacteristicCallbacks());

  characteristic->setValue("Hello World");

  service->start();

  BLEAdvertising *advertising = server->getAdvertising();
  advertising->addServiceUUID(service->getUUID());
  advertising->start();
  // END TODO "Refactor this out... -cojo"

#ifdef EINK154in
  my_display = new DisplayEink154in();
  //my_display->set_font(u8g2_font_6x10_tf);
#endif

#ifdef OLED96in
  my_display = new DisplayOLED96in();
#endif

#ifdef PlanoConvex20x40
  my_optics = new OpticsPlanoConvex20mmX40mm();
#endif
  
#ifdef PlanoConvex16x36
  my_optics = new OpticsPlanoConvex16mmX36mm();
#endif

  box_renderer = new Renderer(my_optics, my_display);

  main_menu = new MenuSystem();
  main_menu->add_menu_item(MATT_MENU_ITEM);
  main_menu->add_menu_item(MARK_MENU_ITEM);
  main_menu->add_menu_item(LUKE_MENU_ITEM);
  main_menu->add_menu_item(JOHN_MENU_ITEM);
  main_menu->add_menu_item(LORD_OF_THE_WORLD_MENU_ITEM);

  remote_input = new IRRemoteInput();
  remote_input->start_remote(IR_REMOTE_INPUT_PIN);

  // Load the preferences
  //
  //my_display->draw_text("testing testing hi there!\ntwo lines\nthreelines\n\n\n\nskipthreelines\n", 0, 0);
  delay(1000);
  // ugly way to clear the screen
  local_x_offset = prefs.getInt("x-offset");
  local_y_offset = prefs.getInt("y-offset");
  area_in_browsing = prefs.getInt("browse-area");
  matt_place = prefs.getULong("matt-place");
  mark_place = prefs.getULong("mark-place");
  luke_place = prefs.getULong("luke-place");
  john_place = prefs.getULong("john-place");
  lord_world_place = prefs.getULong("lord-place");

  matt_prev_place = prefs.getULong("matt-pr-place");
  mark_prev_place = prefs.getULong("mark-pr-place");
  luke_prev_place = prefs.getULong("luke-pr-place");
  john_prev_place = prefs.getULong("john-pr-place");
  lord_world_prev_place = prefs.getULong("lord-pr-place");
  
  DEBUG_VAR(area_in_browsing);
  DEBUG_LN(" was the area read in");

  //box_renderer->render_max_box_with_text(local_x_offset,local_y_offset, "The Lord is my shepherd\n");
  box_renderer->render_max_box(local_x_offset, local_y_offset);
  delay(250);
  update_box = 1;
    
  //box_renderer->render_max_box(local_x_offset, local_y_offset);
  my_display->set_partial_refresh();

  //box_renderer->render_max_box_with_text(local_x_offset, local_y_offset, main_menu->get_whole_menu_as_string());

  matt_render = new FileRenderer(my_optics, my_display, "/matt-all.txt", matt_place, matt_prev_place);
  mark_render = new FileRenderer(my_optics, my_display, "/mark-all.txt", mark_place, mark_prev_place);
  luke_render = new FileRenderer(my_optics, my_display, "/luke-all.txt", luke_place, luke_prev_place);
  john_render = new FileRenderer(my_optics, my_display, "/john-all.txt", john_place, john_prev_place);
  lord_of_world_render = new FileRenderer(my_optics, my_display, "/lordof.txt", lord_world_place, lord_world_prev_place);   
}

const int MAX_ANALOG_VAL = 4095;
const float MAX_BATTERY_VOLTAGE = 4.2; // Max LiPoly voltage of a 3.7 battery is 4.2

String get_battery_life() {
  // A13 pin is not exposed on Huzzah32 board because it's tied to
  // measuring voltage level of battery. Note: you must
  // multiply the analogRead value by 2x to get the true battery
  // level. See: 
  // https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/esp32-faq
  int rawValue = analogRead(A13);

  // Reference voltage on ESP32 is 1.1V
  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#adc-calibration
  // See also: https://bit.ly/2zFzfMT
  float voltageLevel = (rawValue / 4095.0) * 2 * 1.1 * 3.3; // calculate voltage level
  float batteryFraction = voltageLevel / MAX_BATTERY_VOLTAGE;
  String battery_output = "  Percent: " + String(batteryFraction * 100) + "%";
  DEBUG_LN(battery_output.c_str());
  return battery_output;
}

void loop(void) {
  //int i;
  String menu_string;
  String bat_life;
  String total_to_display;
  //delay(2000);
  //for (i = 0; i < 60; i+=40) {
  
  //my_display->draw_box(34, 12, 94, 52);
  //delay(10000);
  //FileRenderer* current_render = 0;

  // TODO: Refactor this out into a proper abstraction (probably at least two actually) -cojo
  if (!deviceConnected && wasDeviceConnected)
  {
    delay(500);                 // give the bluetooth stack the chance to get things ready for restart
    server->startAdvertising(); // restart advertising
    DEBUG_LN("Restarting bluetooth advertising");
    wasDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !wasDeviceConnected)
  {
    // do any additional desired stuff here on connecting
    wasDeviceConnected = deviceConnected;
  }
  // END TODO "Refactor this out... -cojo"

  if (update_box) {
    //my_display->draw_text("Finally", 50, 50);
    
    // box_renderer->render_max_box(local_x_offset, local_y_offset);
    DEBUG_LN("UPDATING THE BOX!");

    switch (area_in_browsing) {
      case MAIN_MENU_AREA:
        menu_string = main_menu->get_whole_menu_as_string();
        bat_life = get_battery_life();
        total_to_display = menu_string + String("\n\n") + bat_life;
        box_renderer->render_max_box_with_text(local_x_offset, local_y_offset, total_to_display);
        break;
      case MATTHEW_AREA:
          new_place = matt_render->render_page_with_box(matt_place, local_x_offset, local_y_offset);
        //current_render = matt_render;
        break;
      case MARK_AREA:
          new_place = mark_render->render_page_with_box(mark_place, local_x_offset, local_y_offset);
        //current_render = mark_render;
        break;
      case LUKE_AREA:
          new_place = luke_render->render_page_with_box(luke_place, local_x_offset, local_y_offset);
        //current_render = luke_render;
        break;
      case JOHN_AREA:
          DEBUG_VAR(john_place);
          DEBUG_LN(" is where we are rendering JOHN");
          new_place = john_render->render_page_with_box(john_place, local_x_offset, local_y_offset);
          DEBUG_VAR(new_place);
          DEBUG_LN(" is the new place in JOHN");
        //current_render = john_render;
        break;
      case LORD_OF_THE_WORLD_AREA:
        new_place = lord_of_world_render->render_page_with_box(lord_world_place, local_x_offset, local_y_offset);
        //current_render = lord_of_world_render;
        break;    
    }
    //my_display->draw_box(start_x, start_y, end_x, end_y);
    update_box = 0;
  }


  // Always have to do a check press and check release. In this case, we don't care about press.
  remote_input->check_press();

  if (remote_input->check_release()) {
    switch(remote_input->get_code()) {
        case SETUP:
          prefs.clear();
          DEBUG_LN("CLEARED PREFERENCES!");
          break;
        case STOP:
          DEBUG_LN("RESETTING BOARD BY SEG FAULT!");
          resetFunc();
          break;
        case CHANNEL_MINUS:
          // TODO: Check bounds
          //if (start_x != 0) {
          if (area_in_browsing == MAIN_MENU_AREA)
            local_x_offset--;
          else {
            switch (area_in_browsing) {
              case MATTHEW_AREA:
                 matt_place = matt_prev_place;
                 prefs.putULong("matt-pr-place", matt_place);
                
              break;
              case MARK_AREA:
                 mark_place = mark_prev_place;
                 prefs.putULong("mark-pr-place", mark_place);
            
              break;
              case LUKE_AREA:
                 luke_place = luke_prev_place;
                 prefs.putULong("luke-pr-place", luke_place);
            
              break;
              case JOHN_AREA:
                 john_place = john_prev_place;
                 prefs.putULong("john-pr-place", john_place);
            
              break;
              case LORD_OF_THE_WORLD_AREA:
                 lord_world_place = lord_world_prev_place;
                 prefs.putULong("lord-pr-place", lord_world_place);
          
              break;
            }
          }
          update_box = 1;
          //}
          break;
        case CHANNEL_PLUS:
          // TODO:
          // Check to be sure we don't go off the end of the screen
          //if (end_x != my_display->get_x()) {
         if (area_in_browsing == MAIN_MENU_AREA)
            local_x_offset++;
          else {
           switch (area_in_browsing) {
              case MATTHEW_AREA:
                matt_prev_place = matt_place;
                matt_place = new_place;
                prefs.putULong("matt-pr-place", matt_prev_place);
                prefs.putULong("matt-place", matt_place);
               
              break;
              case MARK_AREA:
                mark_prev_place = mark_place;
                mark_place = new_place;
                prefs.putULong("mark-pr-place", mark_prev_place);
                prefs.putULong("mark-place", mark_place);
             
              break;
              case LUKE_AREA:
                luke_prev_place = luke_place;
                luke_place = new_place;
                prefs.putULong("luke-pr-place", luke_prev_place);
                prefs.putULong("luke-place", luke_place);
                
              break;
              case JOHN_AREA:
                john_prev_place = john_place;
                john_place = new_place;
                DEBUG_VAR(john_prev_place);
                DEBUG_LN(" is the john old place");
                DEBUG_VAR(john_place);
                DEBUG_LN(" is the john new place");
                prefs.putULong("john-pr-place", john_prev_place);
                prefs.putULong("john-place", john_place);
              
              break;
              case LORD_OF_THE_WORLD_AREA:
                lord_world_prev_place = lord_world_place;
                lord_world_place = new_place;
                prefs.putULong("lord-pr-place", lord_world_prev_place);
                prefs.putULong("lord-place", lord_world_place);
               
              break;
           }
          }
          update_box = 1;
          //}
          break;
        case PREV:
          // TODO: Check bounds
          //if (start_y != 0) {
          if (area_in_browsing == MAIN_MENU_AREA)
            local_y_offset--;
          else {

          }
          update_box = 1;
          //}
          break;
        case NEXT:
          // TODO: Check to be sure we don't go off the end of the screen
          // if (end_y != my_display->get_y()) {
          if (area_in_browsing == MAIN_MENU_AREA)
            local_y_offset++;
          else {

          }
          update_box = 1;
          //}
          break;
        case ENTER:
          prefs.putInt("x-offset", local_x_offset);
          prefs.putInt("y-offset", local_y_offset);
          DEBUG_LN("Saving off x and y offset to preferences");
          DEBUG_VAR(local_x_offset);
          DEBUG_LN(" is the x offset");
          DEBUG_VAR(local_y_offset);
          DEBUG_LN(" is the y offset");
          update_box = 1;
          break;
        case ONE:
          area_in_browsing = MATTHEW_AREA;
          prefs.putInt("browse-area", area_in_browsing);
          update_box = 1;
          
          break;
        case TWO:
          area_in_browsing = MARK_AREA;
          prefs.putInt("browse-area", area_in_browsing);
          update_box = 1;
          
          break;
        case THREE:
          area_in_browsing = LUKE_AREA;
          prefs.putInt("browse-area", area_in_browsing);
          update_box = 1;
          
          break;
        case FOUR:
          area_in_browsing = JOHN_AREA;
          prefs.putInt("browse-area", area_in_browsing);
          update_box = 1;
          break;
        case FIVE:
          area_in_browsing = LORD_OF_THE_WORLD_AREA;
          prefs.putInt("browse-area", area_in_browsing);

          update_box = 1;
          break;
        case BACK:
          area_in_browsing = MAIN_MENU_AREA;
          prefs.putInt("browse-area", area_in_browsing);
          update_box = 1;
        break;
        default:
        break;
      }

  }

}
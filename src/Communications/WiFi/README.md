
# WiFi Library based off the Arduino Wifi library

# This Wifi library was converted to not require any other Arduino libraries.
# Espressif exmaples were also used in the conversion of the library

# Original location of the files

# components/arduino/libraries/WiFi/src/
  WiFi.cpp
  WiFi.h
  WiFiAP.cpp
  WiFiAP.h
  WiFiClient.cpp
  WiFiClient.h
  WiFiGeneric.cpp
  WiFiGeneric.h
  WiFiMulti.cpp -> Unused - Deleted
  WiFiMulti.h -> Unused - Deleted
  WiFiScan.cpp
  WiFiScan.h
  WiFiServer.cpp
  WiFiServer.h
  WiFiSTA.cpp
  WiFiSTA.h
  WiFiType.h
  WiFiUdp.cpp
  WiFiUdp.h

# components/arduino/libraries/WiFiProv/src/
  WiFiProv.cpp -> Unused - Deleted
  WiFiProv.h -> Unused - Deleted


# Include Tree

# SindarinWiFi.h
  WiFi.h
  esp_wifi.h -> components/arduino/tools/sdk/esp32s3/esp_wifi/include/

# WiFi.h
  Print.h -> components/arduino/cores/esp32/
  IPAddress.h -> components/arduino/cores/esp32/
  IPv6Address.h -> components/arduino/cores/esp32/




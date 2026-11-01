FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

# Remove the /src/Ulp directory since it's only for ULP things
list(FILTER app_sources EXCLUDE REGEX "${CMAKE_SOURCE_DIR}/src/Ulp/*.*")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(components
    miniz
    ulp
    astubs
    improv
    ArduinoJson
    Adafruit-GFX-Library
    driver
    mbedtls
    vfs
    lwip
    efuse
    esp_hw_support
    soc
    esp_eth
    spi_flash
    bt
    nvs_flash
    esp_system
    esp_event
    wpa_supplicant
    esp_hw_support
    freertos
    heap
    esp_pm
    esp_adc
    esp_http_client
    esp_http_server
    wifi_provisioning
    esp_littlefs
    esp-nimble-cpp
    espcoredump
    app_update
    usb
    freetype
    tiny-font
)

# Note on conditional components:
# We cannot add components conditionally based on sdkconfig CONFIG_ values
# because component requirements are expanded before the configuration is loaded.
# Instead, we're using BUILD_TARGET passed from the Makefile to determine which
# components to include.
#
# See: https://docs.espressif.com/projects/esp-idf/en/v5.2.2/esp32/api-guides/build-system.html#when-writing-a-component
# > The values of REQUIRES and PRIV_REQUIRES should not depend on any configuration choices
# > (CONFIG_xxx macros). This is because requirements are expanded before the configuration
# > is loaded. Other component variables (like include paths or source files) can depend on
# > configuration choices.

# Extract the target from the build directory path
# The CMAKE_BINARY_DIR will be something like "/path/to/build-${TARGET}"
string(REGEX MATCH "build-([a-zA-Z0-9_-]+)" _ "${CMAKE_BINARY_DIR}")
set(DETECTED_TARGET ${CMAKE_MATCH_1})
message(STATUS "Detected build target from directory: ${DETECTED_TARGET}")

# Use DETECTED_TARGET for component selection
# Add components based on display types used by different targets
if(DETECTED_TARGET MATCHES "p4-function-ev.*|sharp-27in|waveshare-6inhd|p4-seeya049")
    message(STATUS "Adding esp_lcd component for display")
    list(APPEND components
        esp_lcd
        fatfs
        sdmmc
    )
endif()

if(DETECTED_TARGET MATCHES "p4-seeya049")
    message(STATUS "Adding esp_lcd_seeya049 component")
    list(APPEND components
        esp_lcd_seeya049
    )
else()
    message(STATUS "NOT USING esp_lcd_seeya049")
endif()

# Add M5GFX for M5Stack display targets
if(DETECTED_TARGET MATCHES "m5stack-(atom|module)-display")
    message(STATUS "Adding M5GFX component for M5Stack display")
    list(APPEND components
        M5GFX
    )
endif()

# Add architecture-specific components
if(IDF_TARGET STREQUAL "esp32s3")
    list(APPEND components
        xtensa
    )
endif()

# Set compile options
set(compile_options

    # This _should_ be included by default I think but it's not
    -includesdkconfig.h

    -includeconfig.h
    -Wno-unknown-pragmas
    -Wno-missing-field-initializers
    -DIDF_LOG
)

message(STATUS "Compiling with components: ${components}")

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."
    REQUIRES ${components}
)

# ULP setup
if(IDF_TARGET STREQUAL "esp32s3")
    set(ulp_app_name ulp_${COMPONENT_NAME})
    set(ulp_riscv_sources "Ulp/main.c" "Ulp/UlpTableLookup.c")
    set(ulp_exp_dep_srcs "Misc/PowerManager.cpp")
    ulp_embed_binary(${ulp_app_name} "${ulp_riscv_sources}" "${ulp_exp_dep_srcs}")
endif()

target_compile_options(${COMPONENT_TARGET} PUBLIC ${compile_options})

# Wireless flashing signs images with a per-checkout key (see scripts/ota_push.py). The key is
# created on first build, kept out of git, and baked into the firmware so only this checkout can
# push builds to the glasses.
if(CONFIG_DEV_WIRELESS_FLASH)
    set(ota_key_file "${CMAKE_SOURCE_DIR}/.ota-key")
    if(NOT EXISTS "${ota_key_file}")
        execute_process(
            COMMAND ${PYTHON} -c "import secrets; print(secrets.token_hex(32))"
            OUTPUT_FILE "${ota_key_file}"
            RESULT_VARIABLE ota_key_result)
        if(NOT ota_key_result EQUAL 0)
            message(FATAL_ERROR "Failed to generate ${ota_key_file}")
        endif()
        message(STATUS "Generated wireless flash key at ${ota_key_file}")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ota_key_file}")
    file(READ "${ota_key_file}" WIRELESS_FLASH_KEY)
    string(STRIP "${WIRELESS_FLASH_KEY}" WIRELESS_FLASH_KEY)
    if(NOT WIRELESS_FLASH_KEY MATCHES "^[0-9a-fA-F]+$")
        message(FATAL_ERROR "${ota_key_file} must contain a hex key")
    endif()
    set(ota_key_dir "${CMAKE_BINARY_DIR}/wireless_flash")
    file(CONFIGURE OUTPUT "${ota_key_dir}/WirelessFlashKey.h"
         CONTENT "#pragma once\n#define WIRELESS_FLASH_KEY \"@WIRELESS_FLASH_KEY@\"\n" @ONLY)
    target_include_directories(${COMPONENT_LIB} PRIVATE "${ota_key_dir}")
endif()

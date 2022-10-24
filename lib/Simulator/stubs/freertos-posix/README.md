# FreeRTOS Posix Simulator

This provides away to use more advanced FreeRTOS primitives (like Tasks and Queues) on a Posix-y system.

## Creating this library

Clone FreeRTOS. Do this from some other directory on your computer outside of this project.

```sh
git clone git@github.com:FreeRTOS/FreeRTOS.git FreeRTOS
cd FreeRTOS
git submodule update --init --recursive
```

Record the FreeRTOS dir to an environment variable

```sh
export FREERTOS_DIR=$(pwd)
```

Now come back to this directory and vendor in a bunch of things (see the Makefile for more details)

Public facing headers:

```sh
mkdir -p freertos
cp ${FREERTOS_DIR}/FreeRTOS/Source/portable/ThirdParty/GCC/Posix/*.h ./freertos/
cp ${FREERTOS_DIR}/FreeRTOS/Source/include/*.h ./freertos/
cp ${FREERTOS_DIR}/FreeRTOS/Demo/Posix_GCC/FreeRTOSConfig.h ./
```

Code and internal-only headers:

```sh
mkdir -p src/Source/
cp ${FREERTOS_DIR}/FreeRTOS/Source/*.c src/Source/
mkdir -p src/Source/portable/MemMang
cp ${FREERTOS_DIR}/FreeRTOS/Source/portable/MemMang/heap_3.c src/Source/portable/MemMang
mkdir -p src/Source/portable/ThirdParty/GCC/Posix/utils
cp ${FREERTOS_DIR}/FreeRTOS/Source/portable/ThirdParty/GCC/Posix/utils/wait_for_event.c src/Source/portable/ThirdParty/GCC/Posix/utils/
cp ${FREERTOS_DIR}/FreeRTOS/Source/portable/ThirdParty/GCC/Posix/port.c src/Source/portable/ThirdParty/GCC/Posix/
mkdir -p src/Demo/Common/Minimal/
cp ${FREERTOS_DIR}/FreeRTOS/Demo/Common/Minimal/*.c src/Demo/Common/Minimal/
cp ${FREERTOS_DIR}/FreeRTOS/Source/portable/ThirdParty/GCC/Posix/utils/wait_for_event.h src/Source/portable/ThirdParty/GCC/Posix/utils/
cp ${FREERTOS_DIR}/FreeRTOS/Source/portable/ThirdParty/GCC/Posix/portmacro.h src/Source/portable/ThirdParty/GCC/Posix/
mkdir -p src/Demo/Common/include
cp ${FREERTOS_DIR}/FreeRTOS/Demo/Common/include/*.h src/Demo/Common/include/
```

Add `#include "Esp32Shim.h"` near the bottom of `freertos/task.h` (adds in the ESP32-specific `xTaskCreatePinnedToCore` method)

You'll also want to comment out `prvSuspendSelf` from the end of `vPortEndScheduler` in `port.c` so that you can end the scheduler and shut down the simulator gracefully.

See also the `Makefile`. This is not used in the PlatformIO build, but is handy for testing compilation without all the bulk of PlatformIO.

## Other notes

This is mostly a library-ified version of the [FreeRTOS posix port](https://github.com/FreeRTOS/FreeRTOS/tree/main/FreeRTOS/Demo/Posix_GCC). There were some possible useful things there that I stripped out like tracing.

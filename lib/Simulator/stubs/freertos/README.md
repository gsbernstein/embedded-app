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

Now come back to this directory and copy in some things.

```sh
mkdir -p freertos
cp ${FREERTOS_DIR}/FreeRTOS/Source/portable/ThirdParty/GCC/Posix/*.h ./freertos/
cp ${FREERTOS_DIR}/FreeRTOS/Source/include/*.h ./freertos/
cp ${FREERTOS_DIR}/FreeRTOS/Demo/Posix_GCC/FreeRTOSConfig.h ./
```

Add `#include "Esp32Shim.h"` near the bottom of `freertos/task.h` (adds in the ESP32-specific `xTaskCreatePinnedToCore` method)

Now compile `libfreertos.a`:

```sh
make
```

## Other notes

This is mostly a library-ified version of the [FreeRTOS posix port](https://github.com/FreeRTOS/FreeRTOS/tree/main/FreeRTOS/Demo/Posix_GCC). There were some possible useful things there that I stripped out like tracing.

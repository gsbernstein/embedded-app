CC                    := gcc

BUILD_DIR             := ./build
BUILD_DIR_ABS         := $(abspath $(BUILD_DIR))

# Get the environment variable for FREERTOS_DIR
FREERTOS_SUBDIR       := $(abspath $(FREERTOS_DIR))/FreeRTOS

KERNEL_DIR            := ${FREERTOS_SUBDIR}/Source

INCLUDE_DIRS          := -I.
INCLUDE_DIRS          += -I${KERNEL_DIR}/include
INCLUDE_DIRS          += -I${KERNEL_DIR}/portable/ThirdParty/GCC/Posix
INCLUDE_DIRS          += -I${KERNEL_DIR}/portable/ThirdParty/GCC/Posix/utils
INCLUDE_DIRS          += -I${FREERTOS_SUBDIR}/Demo/Common/include

SOURCE_FILES          := libfreertos.c
SOURCE_FILES          += run-time-stats-utils.c
SOURCE_FILES          += $(wildcard ${FREERTOS_SUBDIR}/Source/*.c)
# Memory manager (use malloc() / free() )
SOURCE_FILES          += ${KERNEL_DIR}/portable/MemMang/heap_3.c
# posix port
SOURCE_FILES          += ${KERNEL_DIR}/portable/ThirdParty/GCC/Posix/utils/wait_for_event.c
SOURCE_FILES          += ${KERNEL_DIR}/portable/ThirdParty/GCC/Posix/port.c

# Demo library.
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/AbortDelay.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/BlockQ.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/blocktim.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/countsem.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/death.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/dynamic.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/EventGroupsDemo.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/flop.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/GenQTest.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/integer.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/IntSemTest.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/MessageBufferAMP.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/MessageBufferDemo.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/PollQ.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/QPeek.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/QueueOverwrite.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/QueueSet.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/QueueSetPolling.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/recmutex.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/semtest.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/StaticAllocation.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/StreamBufferDemo.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/StreamBufferInterrupt.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/TaskNotify.c
SOURCE_FILES          += ${FREERTOS_SUBDIR}/Demo/Common/Minimal/TimerDemo.c

CFLAGS                :=    -ggdb3
LDFLAGS               :=    -ggdb3 -pthread
CPPFLAGS              :=    $(INCLUDE_DIRS) -DBUILD_DIR=\"$(BUILD_DIR_ABS)\"
# CPPFLAGS              +=    -D_WINDOWS_

CFLAGS              +=   -O3
LDFLAGS             +=   -O3

# ifdef SANITIZE_ADDRESS
#   CFLAGS              +=   -fsanitize=address -fsanitize=alignment
#   LDFLAGS             +=   -fsanitize=address -fsanitize=alignment
# endif

# ifdef SANITIZE_LEAK
#   LDFLAGS             +=   -fsanitize=leak
# endif

OBJ_FILES = $(SOURCE_FILES:%.c=$(BUILD_DIR)/%.o)

DEP_FILE = $(OBJ_FILES:%.o=%.d)

${BUILD_DIR}/%.o : %.c Makefile check-env
	-mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -c $< -o $@

libfreertos.a : ${OBJ_FILES}
	ar rcs $@ $^

lib : libfreertos.a

.PHONY: clean check-env

clean:
	-rm -rf $(BUILD_DIR)

check-env:
ifndef FREERTOS_DIR
	$(error FREERTOS_DIR is undefined)
endif

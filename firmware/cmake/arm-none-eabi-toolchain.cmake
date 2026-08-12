# CMake toolchain file for cross-compiling the Helm firmware with
# arm-none-eabi-gcc, targeting the Cortex-M3 core QEMU emulates for the
# mps2-an385 machine.
#
# Usage: cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-toolchain.cmake ..

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy CACHE FILEPATH "")
set(CMAKE_SIZE arm-none-eabi-size CACHE FILEPATH "")

# No OS, no working linker script until the firmware target itself supplies
# one -- ask CMake to only build a static lib during compiler-capability
# detection so the initial ABI check doesn't try (and fail) to link an exe.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(HELM_MCU_FLAGS "-mthumb -mcpu=cortex-m3" CACHE STRING "")
set(CMAKE_C_FLAGS_INIT "${HELM_MCU_FLAGS} -ffreestanding")
set(CMAKE_ASM_FLAGS_INIT "${HELM_MCU_FLAGS}")

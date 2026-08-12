cmake_minimum_required(VERSION 3.25)
project(helm_firmware C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Phase 1: host-side unit tests for the control math, buildable with the
# system C compiler (no ARM toolchain / RTOS / QEMU required).
# Phase 2: the actual FreeRTOS firmware, cross-compiled with arm-none-eabi-gcc
# via cmake/arm-none-eabi-toolchain.cmake. The two are mutually exclusive in
# a single configure (host tests can't run a cross-compiled binary), so this
# defaults on the natural thing for whichever toolchain is active.
if(CMAKE_CROSSCOMPILING)
    option(HELM_BUILD_HOST_TESTS "Build host-side unit tests for control math" OFF)
else()
    option(HELM_BUILD_HOST_TESTS "Build host-side unit tests for control math" ON)
endif()

if(HELM_BUILD_HOST_TESTS AND NOT CMAKE_CROSSCOMPILING)
    add_executable(helm_host_tests
        test/unit/test_main.c
        test/unit/test_pid.c
        test/unit/test_plausibility.c
        test/unit/test_protocol.c
        src/plant_sim.c
        src/pid.c
        src/plausibility.c
        src/protocol.c
    )
    target_include_directories(helm_host_tests PRIVATE src test/unit)
    if(NOT MSVC)
        target_compile_options(helm_host_tests PRIVATE -Wall -Wextra)
    endif()
    find_library(HELM_MATH_LIB m)
    if(HELM_MATH_LIB)
        target_link_libraries(helm_host_tests PRIVATE ${HELM_MATH_LIB})
    endif()

    enable_testing()
    add_test(NAME host_control_math_tests COMMAND helm_host_tests)
endif()

if(CMAKE_CROSSCOMPILING)
    set(FREERTOS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/FreeRTOS-Kernel)
    set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/mps2_m3.ld)

    set(HELM_FW_SOURCES
        src/startup_mps2_m3.c
        src/main.c
        src/uart.c
        src/protocol.c
        src/plant_sim.c
        src/pid.c
        src/plausibility.c
        src/watchdog.c
        src/tasks/sensor_task.c
        src/tasks/control_task.c
        src/tasks/actuator_task.c
        src/tasks/diag_task.c
        src/tasks/comms_task.c
        ${FREERTOS_DIR}/tasks.c
        ${FREERTOS_DIR}/queue.c
        ${FREERTOS_DIR}/list.c
        ${FREERTOS_DIR}/timers.c
        ${FREERTOS_DIR}/portable/GCC/ARM_CM3/port.c
        ${FREERTOS_DIR}/portable/MemMang/heap_4.c
    )

    set(HELM_FW_INCLUDES
        src
        ${FREERTOS_DIR}/include
        ${FREERTOS_DIR}/portable/GCC/ARM_CM3
    )

    set(HELM_FW_COMPILE_OPTIONS
        -Os -g3 -ffunction-sections -fdata-sections -Wall -Wextra
    )

    set(HELM_FW_LINK_OPTIONS
        -T${LINKER_SCRIPT}
        -nostartfiles
        --specs=nosys.specs
        -Wl,--gc-sections
        -Wl,-Map=helm.map
    )

    # Production firmware.
    add_executable(helm ${HELM_FW_SOURCES})
    target_include_directories(helm PRIVATE ${HELM_FW_INCLUDES})
    target_compile_options(helm PRIVATE ${HELM_FW_COMPILE_OPTIONS})
    target_link_options(helm PRIVATE ${HELM_FW_LINK_OPTIONS})
    set_target_properties(helm PROPERTIES SUFFIX ".elf" LINK_DEPENDS ${LINKER_SCRIPT})

    # Debug-only variant that intentionally stalls control_task once, to
    # verify the watchdog trips and forces fail-safe. Not part of the normal
    # SiL demo build.
    add_executable(helm_watchdog_test ${HELM_FW_SOURCES})
    target_include_directories(helm_watchdog_test PRIVATE ${HELM_FW_INCLUDES})
    target_compile_options(helm_watchdog_test PRIVATE ${HELM_FW_COMPILE_OPTIONS})
    target_compile_definitions(helm_watchdog_test PRIVATE HELM_TEST_WATCHDOG_STALL)
    target_link_options(helm_watchdog_test PRIVATE
        -T${LINKER_SCRIPT} -nostartfiles --specs=nosys.specs -Wl,--gc-sections
        -Wl,-Map=helm_watchdog_test.map
    )
    set_target_properties(helm_watchdog_test PROPERTIES SUFFIX ".elf" LINK_DEPENDS ${LINKER_SCRIPT})
endif()

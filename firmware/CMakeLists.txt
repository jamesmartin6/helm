cmake_minimum_required(VERSION 3.25)
project(helm_firmware C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Phase 1: host-side unit tests for the control math, buildable with the
# system C compiler (no ARM toolchain / RTOS / QEMU required). Phase 2 adds
# a separate cross-compiled firmware target behind arm-none-eabi-gcc.
option(HELM_BUILD_HOST_TESTS "Build host-side unit tests for control math" ON)

if(HELM_BUILD_HOST_TESTS)
    add_executable(helm_host_tests
        test/unit/test_main.c
        test/unit/test_pid.c
        test/unit/test_plausibility.c
        src/plant_sim.c
        src/pid.c
        src/plausibility.c
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

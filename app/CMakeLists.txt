cmake_minimum_required(VERSION 3.20)
project(ps3-240p-app C)
option(CRT_ENABLE_HD540 "Build the unverified HDMI-only timed 540p experiment" OFF)
option(CRT_ENABLE_PRESETS "Build timed native HDMI PC preset menu" OFF)
option(CRT_ENABLE_XGA "Include guarded CFW XGA test in the preset menu" OFF)
option(CRT_ENABLE_CUSTOM_HDMI "Build confirmed session HDMI resolution menu" OFF)
if(CRT_ENABLE_CUSTOM_HDMI AND (CRT_ENABLE_PRESETS OR CRT_ENABLE_HD540))
    message(FATAL_ERROR "Build the unified HDMI edition separately")
endif()
if(CRT_ENABLE_XGA AND NOT CRT_ENABLE_PRESETS)
    message(FATAL_ERROR "The XGA menu requires the preset variant")
endif()
if(CRT_ENABLE_PRESETS AND CRT_ENABLE_HD540)
    message(FATAL_ERROR "Build PC presets and the experimental 540p package separately")
endif()
set(CRT_APP_VERSION "" CACHE STRING "Override the displayed version for isolated test builds")

include(ps3-self)

add_executable(ps3-240p-app source/main.c source/lv1_patch.c source/sysmgr.c source/switch_flow.c source/ui_layout.c source/sd_default.c source/plugin_lifecycle.c source/boot_plugins.c source/gamma.c source/gamma_ipc.c)
target_compile_features(ps3-240p-app PRIVATE c_std_11)
target_compile_definitions(ps3-240p-app PRIVATE PS3_ENABLE_TIMING_SWITCH=1)
if(CRT_APP_VERSION)
    target_compile_definitions(ps3-240p-app PRIVATE CRT_BUILD_VERSION="${CRT_APP_VERSION}")
endif()
if(CRT_ENABLE_PRESETS)
    target_compile_definitions(ps3-240p-app PRIVATE CRT_ENABLE_PRESETS=1)
endif()
if(CRT_ENABLE_XGA)
    target_compile_definitions(ps3-240p-app PRIVATE CRT_XGA_PROBE=1)
endif()
if(CRT_ENABLE_CUSTOM_HDMI)
    target_compile_definitions(ps3-240p-app PRIVATE CRT_ENABLE_CUSTOM_HDMI=1)
    target_sources(ps3-240p-app PRIVATE ../experiments/hdmi_custom/confirm.c)
endif()
if(CRT_ENABLE_HD540)
    target_compile_definitions(ps3-240p-app PRIVATE CRT_ENABLE_HD540=1)
endif()

target_compile_options(ps3-240p-app PRIVATE
    -O2 -Wall -Wextra -Werror -Wno-misleading-indentation
)

target_link_libraries(ps3-240p-app PRIVATE
    rsx gcm_cmd gcm_sys io sysutil net sysmodule rt lv2 m
)

ps3_add_self(ps3-240p-app OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")


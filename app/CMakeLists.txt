cmake_minimum_required(VERSION 3.20)
project(ps3-240p-app C)

include(ps3-self)

add_executable(ps3-240p-app source/main.c)
target_compile_features(ps3-240p-app PRIVATE c_std_11)

target_compile_options(ps3-240p-app PRIVATE
    -O2 -Wall -Wextra -Wno-unused-parameter
)

target_link_libraries(ps3-240p-app PRIVATE
    dbgfont gcm_cmd gcm_sys io sysutil rt lv2 m
)

ps3_add_self(ps3-240p-app OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")


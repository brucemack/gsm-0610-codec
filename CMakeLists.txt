# GSM 06.10 CODEC
# Copyright (C) 2024, Bruce MacKinnon 
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# ( at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# NOT FOR COMMERCIAL USE WITHOUT PERMISSION.

# UNCOMMENT FOR PI PICO
#set(TARGET2 "pico")

cmake_minimum_required(VERSION 3.13)
if (TARGET2 STREQUAL "pico")
include(pico_sdk_import.cmake)
endif()
project(gsm-0610-codec C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
file(MAKE_DIRECTORY tmp)

if (TARGET2 STREQUAL "pico")
pico_sdk_init()
#add_compile_options(-fstack-protector-all -Wall -g -DPICO_BUILD=1)
add_compile_options(-DPICO_BUILD=1)
else()
#add_compile_options(-fstack-protector-all -Wall -Wpedantic -g)
endif()

# ----- gsm-test-0 -----------------------------------------------------------
# Very basic unit test which would be expected to pass on the 
# embedded platform.

add_executable(gsm-test-0
  tests/gsm-test-0.cpp
  src/fixed_math.cpp
  src/Parameters.cpp
  src/Encoder.cpp
  src/Decoder.cpp
)

target_include_directories(gsm-test-0 PUBLIC include)
target_include_directories(gsm-test-0 PRIVATE src)

if (TARGET2 STREQUAL "pico")
pico_enable_stdio_usb(gsm-test-0 1)
pico_enable_stdio_uart(gsm-test-0 1)
target_link_libraries(gsm-test-0 pico_stdlib hardware_i2c)
endif()

# ----- gsm-test-1 -----------------------------------------------------------
# A more complex test case that will require the desktop environment

add_executable(gsm-test-1
  tests/unit-test-1.cpp
  src/fixed_math.cpp
  src/wav_util.cpp
  src/Parameters.cpp
  src/Encoder.cpp
  src/Decoder.cpp
)

target_include_directories(gsm-test-1 PUBLIC include)
target_include_directories(gsm-test-1 PRIVATE src)

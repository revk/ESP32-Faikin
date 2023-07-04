# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/adrian/ESP32/5.0.2/components/bootloader/subproject"
  "/projects/github/ESP32-Faikin/ESP32/build/bootloader"
  "/projects/github/ESP32-Faikin/ESP32/build/bootloader-prefix"
  "/projects/github/ESP32-Faikin/ESP32/build/bootloader-prefix/tmp"
  "/projects/github/ESP32-Faikin/ESP32/build/bootloader-prefix/src/bootloader-stamp"
  "/projects/github/ESP32-Faikin/ESP32/build/bootloader-prefix/src"
  "/projects/github/ESP32-Faikin/ESP32/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/projects/github/ESP32-Faikin/ESP32/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/projects/github/ESP32-Faikin/ESP32/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()

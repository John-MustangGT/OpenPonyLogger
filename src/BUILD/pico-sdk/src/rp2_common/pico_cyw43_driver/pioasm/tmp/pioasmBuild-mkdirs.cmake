# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/john/Repos/pico-sdk/tools/pioasm"
  "/home/john/Repos/OpenPonyLogger/src/BUILD/pioasm"
  "/home/john/Repos/OpenPonyLogger/src/BUILD/pioasm-install"
  "/home/john/Repos/OpenPonyLogger/src/BUILD/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/tmp"
  "/home/john/Repos/OpenPonyLogger/src/BUILD/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
  "/home/john/Repos/OpenPonyLogger/src/BUILD/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src"
  "/home/john/Repos/OpenPonyLogger/src/BUILD/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/john/Repos/OpenPonyLogger/src/BUILD/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/john/Repos/OpenPonyLogger/src/BUILD/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()

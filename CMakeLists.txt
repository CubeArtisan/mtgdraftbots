#########################
#                       #
#         CMake         #
#                       #
#########################

# The minimum version of 'cmake' necessary to build this project
cmake_minimum_required (VERSION 3.19)

project (MtgDraftBots LANGUAGES CXX VERSION 0.0.1)

list (APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list (APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

add_library (MtgDraftBots INTERFACE
    "include/mtgdraftbots/mtgdraftbots.h"
    "include/mtgdraftbots/generated/constants.h"
)

target_include_directories (MtgDraftBots INTERFACE "include")

target_compile_features (MtgDraftBots INTERFACE cxx_std_20)

set_target_properties (MtgDraftBots PROPERTIES CXX_STANDARD 20
                                              CXX_STANDARD_REQUIRED ON
                                              CXX_EXTENSIONS OFF)

if (NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
  message (STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
  file (DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/develop/conan.cmake"
                 "${CMAKE_BINARY_DIR}/conan.cmake"
                 EXPECTED_HASH SHA256=ddf0fafacf48b5c4912ecce5701c252532437c40277734cad5f4a8084470adbc
                 TLS_VERIFY ON)
endif ()

include (${CMAKE_BINARY_DIR}/conan.cmake)

conan_cmake_configure (REQUIRES frozen/1.0.1 eigen/3.3.9
                       GENERATORS cmake_find_package_multi)

foreach (TYPE ${CMAKE_CONFIGURATION_TYPES})
    conan_cmake_autodetect(settings BUILD_TYPE ${TYPE})
    conan_cmake_install(PATH_OR_REFERENCE .
                        BUILD missing
                        REMOTE conan-center
                        SETTINGS ${settings})
endforeach ()

find_package (frozen CONFIG REQUIRED)
target_link_libraries (MtgDraftBots INTERFACE frozen::frozen)

find_package (Eigen3 CONFIG REQUIRED)
target_link_libraries (MtgDraftBots INTERFACE Eigen3::Eigen)

# This is used for getting compile_commands.json
add_executable (MtgDraftBotsTemp "src/temp.cpp")
target_link_libraries (MtgDraftBotsTemp PUBLIC MtgDraftBots)

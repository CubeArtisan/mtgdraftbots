#########################
#                       #
#         CMake         #
#                       #
#########################

# The minimum version of 'cmake' necessary to build this project
cmake_minimum_required (VERSION 3.19)

project (MtgDraftBots LANGUAGES CXX VERSION 0.0.1)

add_subdirectory(extern/frozen)

add_library(MtgDraftBots INTERFACE
    "include/mtgdraftbots/mtgdraftbots.h"
    "include/mtgdraftbots/generated/constants.h"
)

target_link_libraries(MtgDraftBots INTERFACE frozen::frozen)
target_include_directories(MtgDraftBots INTERFACE "include" "extern/pcg-cpp/include")

target_compile_features(MtgDraftBots INTERFACE cxx_std_20)

set_target_properties(MtgDraftBots PROPERTIES CXX_STANDARD 20
                                              CXX_STANDARD_REQUIRED ON
                                              CXX_EXTENSIONS OFF)

add_executable(MtgDraftBotsTemp "src/temp.cpp")

target_link_libraries(MtgDraftBotsTemp PRIVATE MtgDraftBots)

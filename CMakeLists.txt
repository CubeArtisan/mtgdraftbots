#########################
#                       #
#         CMake         #
#                       #
#########################

# The minimum version of 'cmake' necessary to build this project
cmake_minimum_required (VERSION 3.19)

project (MtgDraftBots LANGUAGES CXX VERSION 0.0.1)

add_library(MtgDraftBots INTERFACE "include/mtgdraftbots.h")

target_include_directories(MtgDraftBots INTERFACE "include")

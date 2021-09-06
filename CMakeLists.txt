#########################
#                       #
#         CMake         #
#                       #
#########################

# The minimum version of 'cmake' necessary to build this project
cmake_minimum_required (VERSION 3.20)

project (MtgDraftBots LANGUAGES CXX VERSION 0.0.1)

include (CheckCCompilerFlag)
include (CheckCXXCompilerFlag)

macro (ADD_FLAG_IF_AVAIL TARGET_NAME ACCESS_MODIFIER FLAG)
  if (NOT "${FLAG}" STREQUAL "")
    # create a variable for checking the flag if supported, e.g.:
    #   -fp-model=precise --> cxx_fp_model_precise
    string (REGEX REPLACE "^-" "cxx_" FLAG_NAME "${FLAG}")
    string (REPLACE "-" "_" FLAG_NAME "${FLAG_NAME}")
    string (REPLACE " " "_" FLAG_NAME "${FLAG_NAME}")
    string (REPLACE "=" "_" FLAG_NAME "${FLAG_NAME}")

    # runs check to see flag is supported by compiler
    check_cxx_compiler_flag ("${FLAG}" ${FLAG_NAME})
    if (${FLAG_NAME})
      target_compile_options (${TARGET_NAME} ${ACCESS_MODIFIER} $<$<COMPILE_LANGUAGE:CXX>:${FLAG}>)
    endif ()
  endif ()
endmacro ()

list (APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list (APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

message(${CMAKE_BINARY_DIR})


if (NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
  message (STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
  file (DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/develop/conan.cmake"
                 "${CMAKE_BINARY_DIR}/conan.cmake"
                 EXPECTED_HASH SHA256=ddf0fafacf48b5c4912ecce5701c252532437c40277734cad5f4a8084470adbc
                 TLS_VERIFY ON)
endif ()

include (${CMAKE_BINARY_DIR}/conan.cmake)

if (EMSCRIPTEN)
conan_cmake_configure (
  REQUIRES
    frozen/1.0.1
    mpark-variant/1.4.0
    vectorclass/2.01.03
  GENERATORS cmake_find_package_multi
)
else ()
conan_cmake_configure (
  REQUIRES
    frozen/1.0.1
    mpark-variant/1.4.0
    vectorclass/2.01.03
    simdjson/0.9.5
    concurrentqueue/1.0.2
    fmt/7.1.3
  GENERATORS cmake_find_package_multi
)

endif()
if (NOT CMAKE_CONFIGURATION_TYPES)
  conan_cmake_autodetect(settings)
  conan_cmake_install(PATH_OR_REFERENCE .
                      BUILD missing
                      REMOTE conan-center
                      SETTINGS ${settings})
else()
  foreach (TYPE ${CMAKE_CONFIGURATION_TYPES})
    message("Configuring for config type ${TYPE}")
    conan_cmake_autodetect(settings BUILD_TYPE ${TYPE})
    conan_cmake_install(PATH_OR_REFERENCE .
                        BUILD missing
                        REMOTE conan-center
                        SETTINGS ${settings})
  endforeach ()
endif()

add_library (MtgDraftBots INTERFACE
  "include/mtgdraftbots/mtgdraftbots.hpp"
  "include/mtgdraftbots/oracles.hpp"
  "include/mtgdraftbots/details/cardcost.hpp"
  "include/mtgdraftbots/details/constants.hpp"
  "include/mtgdraftbots/types.hpp"
  "include/mtgdraftbots/generated/prob_table.hpp"
 "include/mtgdraftbots/details/generate_probs.hpp" "include/mtgdraftbots/details/cardvalues.hpp" "include/mtgdraftbots/details/simd.hpp")

target_include_directories (MtgDraftBots INTERFACE "include" "extern/range")

target_compile_features (MtgDraftBots INTERFACE cxx_std_20)

set_target_properties (MtgDraftBots PROPERTIES CXX_STANDARD 20
                                               CXX_STANDARD_REQUIRED ON
                                               CXX_EXTENSIONS OFF)

message(${CMAKE_MODULE_PATH})
find_package (frozen CONFIG REQUIRED)
find_package (mpark_variant CONFIG REQUIRED)
target_link_libraries (MtgDraftBots INTERFACE mpark_variant::mpark_variant frozen::frozen)


include (CMakePackageConfigHelpers)
write_basic_package_version_file (
  "${PROJECT_BINARY_DIR}/MtgDraftBotsConfigVersion.cmake"
  VERSION 0.0.1
  COMPATIBILITY AnyNewerVersion
)
if (EMSCRIPTEN)
  get_filename_component(PREJS ./emscripten/pre.js ABSOLUTE)
                                                
  add_executable(MtgDraftBotsWasmWebWorker "src/emscripten.cpp")
  set_target_properties(MtgDraftBotsWasmWebWorker PROPERTIES SUFFIX ".js")
  target_link_libraries (MtgDraftBotsWasmWebWorker PUBLIC MtgDraftBots)
  target_compile_options(MtgDraftBotsWasmWebWorker PUBLIC --bind $<$<CONFIG:Debug>:-gsource-map>
                                                          $<$<CONFIG:RelWithDebInfo>:-gsource-map>
                                                          $<$<CONFIG:RelWithDebInfo>:-flto>
                                                          $<$<CONFIG:Release>:-flto>)
  target_link_options (MtgDraftBotsWasmWebWorker PUBLIC --bind --no-entry --pre-js "${PREJS}" "-sEVAL_CTORS=1"
                                                        "-sINITIAL_MEMORY=39976960" "-sSTRICT=1"
                                                        "-sALLOW_MEMORY_GROWTH=1" "-sMALLOC=dlmalloc"
                                                        "-sEXPORT_ES6=1"
                                                        "-sMODULARIZE=1" "-sFORCE_FILESYSTEM=0"
                                                        "-sEXPORT_NAME=createMtgDraftBots" "-sASSERTIONS=1"
                                                        "-sENVIRONMENT=web,worker" "-sFILESYSTEM=0"
                                                        $<$<CONFIG:Debug>:-gsource-map>
                                                        $<$<CONFIG:RelWithDebInfo>:-flto> $<$<CONFIG:RelWithDebInfo>:-gsource-map>
                                                        $<$<CONFIG:Release>:--closure=1> $<$<CONFIG:Release>:-flto>)

  add_executable(MtgDraftBotsWasmNodeWorker "src/emscripten.cpp")
  set_target_properties(MtgDraftBotsWasmNodeWorker PROPERTIES SUFFIX ".cjs")
  target_link_libraries (MtgDraftBotsWasmNodeWorker PUBLIC MtgDraftBots)
  target_compile_options(MtgDraftBotsWasmNodeWorker PUBLIC --bind $<$<CONFIG:Debug>:-gsource-map>
                                                           $<$<CONFIG:RelWithDebInfo>:-gsource-map>
                                                           $<$<CONFIG:RelWithDebInfo>:-flto>
                                                           $<$<CONFIG:Release>:-flto>)
  target_link_options (MtgDraftBotsWasmNodeWorker PUBLIC --bind --no-entry --pre-js "${PREJS}" "-sEVAL_CTORS=1"
                                                         "-sINITIAL_MEMORY=39976960" "-sSTRICT=1"
                                                         "-sALLOW_MEMORY_GROWTH=1" "-sMALLOC=dlmalloc"
                                                         "-sMODULARIZE=1" "-sFORCE_FILESYSTEM=0"
                                                         "-sEXPORT_NAME=createMtgDraftBots" "-sASSERTIONS=1"
                                                         "-sENVIRONMENT=node,worker" "-sFILESYSTEM=0"
                                                         $<$<CONFIG:Debug>:-gsource-map>
                                                         $<$<CONFIG:RelWithDebInfo>:-flto> $<$<CONFIG:RelWithDebInfo>:-gsource-map>
                                                         $<$<CONFIG:Release>:--closure=1> $<$<CONFIG:Release>:-flto>)
else()
find_package (Vectorclass CONFIG REQUIRED)
target_link_libraries (MtgDraftBots INTERFACE vectorclass::vectorclass)
target_compile_definitions (MtgDraftBots INTERFACE VCL_NAMESPACE=vcl USE_VECTORCLASS)

add_executable (ParsePicks "src/parse_picks.cpp")
find_package (fmt CONFIG REQUIRED)
find_package (simdjson CONFIG REQUIRED)
find_package (concurrentqueue CONFIG REQUIRED)
target_link_libraries (ParsePicks PUBLIC MtgDraftBots simdjson::simdjson concurrentqueue::concurrentqueue fmt::fmt)
add_flag_if_avail (ParsePicks PRIVATE -Wall)
add_flag_if_avail (ParsePicks PRIVATE -Wextra)
add_flag_if_avail (ParsePicks PRIVATE /W3)
add_flag_if_avail (ParsePicks PRIVATE -march=native)
add_flag_if_avail (ParsePicks PRIVATE -ffast-math)
add_flag_if_avail (ParsePicks PRIVATE /march:AVX2)
add_flag_if_avail (ParsePicks PRIVATE -fdiagnostics-color)

# This is used for getting compile_commands.json
add_executable (MtgDraftBotsTemp "src/temp.cpp")
target_link_libraries (MtgDraftBotsTemp PUBLIC MtgDraftBots)
add_flag_if_avail (MtgDraftBotsTemp PRIVATE -Wall)
add_flag_if_avail (MtgDraftBotsTemp PRIVATE -Wextra)
add_flag_if_avail (MtgDraftBotsTemp PRIVATE /W3)
endif()


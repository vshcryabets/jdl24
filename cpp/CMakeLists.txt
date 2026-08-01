CMAKE_MINIMUM_REQUIRED(VERSION 3.31)

project(dl24 LANGUAGES C CXX)

enable_testing()
include(CTest)

set(CPP_VERSION "23" CACHE STRING "C++ standard version to use (17 or 23)")
set_property(CACHE CPP_VERSION PROPERTY STRINGS "17" "23")
if(NOT CPP_VERSION STREQUAL "17" AND NOT CPP_VERSION STREQUAL "23")
    message(FATAL_ERROR "CPP_VERSION must be 17 or 23 (got '${CPP_VERSION}')")
endif()
set(CMAKE_CXX_STANDARD ${CPP_VERSION})
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

include(cmake/CPM.cmake)

if (ENABLE_CATCH2)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.3
    )
    FetchContent_MakeAvailable(Catch2)
    include(Catch)
endif()

# For GCC and Clang compilers
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Werror=return-type)

# For the Microsoft MSVC compiler
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/we4716)
endif()

add_compile_definitions(BufferSize_t=uint16_t)

add_subdirectory(modules/dl24)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(samples/linux-cli)
endif()

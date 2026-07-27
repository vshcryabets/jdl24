CMAKE_MINIMUM_REQUIRED(VERSION 3.31)

add_library(dl24cpp STATIC)

target_sources(dl24cpp
    PRIVATE
        src/Source.cpp
        src/Controller.cpp
    )

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Threads REQUIRED)
    target_sources(dl24cpp PRIVATE src/SourceLinuxImpl.cpp)
    target_link_libraries(dl24cpp PUBLIC Threads::Threads)
endif()

target_include_directories(dl24cpp PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

if (ENABLE_CATCH2)
    add_executable(module_l4_tests
        test/test_ChannelUart.cpp
    )
    if (USE_STD_MEMORY)
        target_compile_definitions(module_l4_tests PRIVATE USE_STD_MEMORY)
    endif()
    if(CMAKE_CXX_STANDARD GREATER_EQUAL 23)
        message("Build tests for C++23")
        target_sources(module_l4_tests PRIVATE
            test/test_ChannelUnixTcp.cpp
            test/test_ChannelUnixPipeImpl.cpp
        )
        # This helps CMake provide better error messages if the compiler doesn't fully
        # support the C++23 standard.
        target_compile_features(module_l4_tests PRIVATE cxx_std_23)
    endif()
    target_include_directories(module_l4_tests PRIVATE ./include/)
    target_link_libraries(module_l4_tests PUBLIC Catch2::Catch2WithMain module_l4)

    catch_discover_tests(module_l4_tests 
        PROPERTIES 
        TIMEOUT 10)
endif()
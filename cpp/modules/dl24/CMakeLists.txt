CMAKE_MINIMUM_REQUIRED(VERSION 3.31)

add_library(dl24cpp STATIC)

target_sources(dl24cpp
    PRIVATE
        src/Source.cpp
        src/Controller.cpp
        src/ControllerImpl.cpp
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
    add_executable(dl24cpp_tests
        test/test_ControllerImpl.cpp
    )
    target_include_directories(dl24cpp_tests PRIVATE ./include/)
    target_link_libraries(dl24cpp_tests PUBLIC Catch2::Catch2WithMain dl24cpp)

    catch_discover_tests(dl24cpp_tests 
        PROPERTIES 
        TIMEOUT 10)
endif()
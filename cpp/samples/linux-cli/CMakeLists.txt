CMAKE_MINIMUM_REQUIRED(VERSION 3.31)

add_executable(dl24-linux-cli)

target_sources(dl24-linux-cli
    PRIVATE
        src/main.cpp
    )

target_link_libraries(dl24-linux-cli PUBLIC dl24cpp)

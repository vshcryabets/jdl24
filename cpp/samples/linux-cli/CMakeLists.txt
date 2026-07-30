CMAKE_MINIMUM_REQUIRED(VERSION 3.31)

CPMAddPackage("gh:ArthurSonzogni/FTXUI@7.0.1")
CPMAddPackage("gh:nlohmann/json@3.12.0")

add_executable(dl24-linux-cli)

target_sources(dl24-linux-cli
    PRIVATE
        src/main.cpp
        src/SampleDl24App.cpp
    )

target_include_directories(dl24-linux-cli PRIVATE
    include
    )

target_link_libraries(dl24-linux-cli PRIVATE 
    dl24cpp 
    ftxui::ftxui 
    nlohmann_json::nlohmann_json
    )

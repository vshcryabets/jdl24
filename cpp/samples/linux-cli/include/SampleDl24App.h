#pragma once

#include <string>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

struct Configuration {
    std::string devicePath = "/dev/ttyUSB0";
};

class SampleDl24App {
private:
    Configuration config_;
    bool show_connect_dialog;
    
    void loadConfiguration(std::string configFilePath);
    void saveConfiguration(std::string configFilePath);

    ftxui::Component connectToDevice();
public:
    SampleDl24App();
    ~SampleDl24App();
    void run();
};

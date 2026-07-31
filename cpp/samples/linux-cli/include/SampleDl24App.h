#pragma once

#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "viewmodel.h"

class SampleDl24App {
private:
    ViewModel &viewModel_;
    std::string device_path;
    ftxui::Component connectToDeviceDialog();
public:
    SampleDl24App(ViewModel &viewModel);
    ~SampleDl24App();
    void run();
};

#pragma once

#include <string>
#include <vector>
#include <memory>

#include "Source.h"
#include "Controller.h"

struct ViewState {
    bool show_connect_dialog = false;
    std::string device_path = "";
    std::vector<std::string> uart_logs = {};
};

struct UiEvent {
    virtual ~UiEvent() = default;
};
struct ConnectDialogCanceled: public UiEvent {};
struct OnConnectRequested: public UiEvent {};
struct OnLoadStart: public UiEvent {};
struct OnLoadStop: public UiEvent {};
struct ResetStatistics: public UiEvent {};
struct Open: public UiEvent {
    Open(std::string path) : filepath(path), UiEvent() {}
    std::string filepath;
};
struct LoadSetCurrent: public UiEvent {
    LoadSetCurrent(float current) : current(current), UiEvent() {}
    float current;
};
struct LoadSetVoltage: public UiEvent {
    LoadSetVoltage(float voltage) : voltage(voltage), UiEvent() {}
    float voltage;
};

class ViewModel {
private:
    void loadConfiguration(std::string configFilePath);
    void saveConfiguration(std::string configFilePath);

    ViewState state;

    std::unique_ptr<dl24::Source> source_;
    std::unique_ptr<dl24::Controller> controller_;

    void onOpenDevice(const Open* event);
public:
    ViewModel();
    virtual ~ViewModel() = default;
    void onUiAction(const UiEvent& event);
    const ViewState& getState() const { return state; }
};
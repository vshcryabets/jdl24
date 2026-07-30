#pragma once

#include <string>
#include <vector>

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
struct Open: public UiEvent {
    Open(std::string path) : filepath(path), UiEvent() {}
    std::string filepath;
};

class ViewModel {
private:
    void loadConfiguration(std::string configFilePath);
    void saveConfiguration(std::string configFilePath);

    ViewState state;

public:
    ViewModel();
    virtual ~ViewModel() = default;
    void onUiAction(const UiEvent& event);
    const ViewState& getState() const { return state; }
};
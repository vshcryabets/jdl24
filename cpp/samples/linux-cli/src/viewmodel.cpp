#include "viewmodel.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ViewModel::ViewModel() : state() {
    state.show_connect_dialog = false;
    loadConfiguration("dl24.conf");
    
    state.uart_logs = {
        "JDL24 UART Sample Application",
        "Waiting for commands...",
    };
}

void ViewModel::onUiAction(const UiEvent& event) {
    if (const auto* canceled = dynamic_cast<const ConnectDialogCanceled*>(&event)) {
        state.show_connect_dialog = false;
    } else if (const auto* openEvent = dynamic_cast<const OnConnectRequested*>(&event)) {
        state.show_connect_dialog = true;
    } else if (const auto* openEvent = dynamic_cast<const Open*>(&event)) {
        state.show_connect_dialog = false; // Close the dialog after handling
        state.device_path = openEvent->filepath;
        saveConfiguration("dl24.conf");
        state.uart_logs.push_back("Connecting to device at: " + state.device_path);
    }
}

void ViewModel::loadConfiguration(std::string configFilePath) {
    // Load configuration from file (if exists).
    std::ifstream configFile(configFilePath);
    if (configFile.is_open()) {
        json configJson;
        configFile >> configJson;
        if (configJson.contains("devicePath")) {
            state.device_path = configJson["devicePath"];
        }
    }
}

void ViewModel::saveConfiguration(std::string configFilePath) {
    // Save configuration to file.
    json configJson;
    configJson["devicePath"] = state.device_path;
    std::ofstream configFile(configFilePath);
    if (configFile.is_open()) {
        configFile << configJson.dump(4);  // Pretty print with 4 spaces indentation
    }
}

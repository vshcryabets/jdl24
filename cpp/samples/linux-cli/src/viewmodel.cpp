#include "viewmodel.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "SourceLinuxImpl.h"
#include "ControllerImpl.h"

using json = nlohmann::json;

ViewModel::ViewModel() : state() {
    state.show_connect_dialog = false;
    loadConfiguration("dl24.conf");
    
    state.uart_logs = {
        "JDL24 UART Sample Application",
        "Waiting for commands...",
    };
}

void ViewModel::onOpenDevice(const Open* event) {
    state.device_path = event->filepath;
    saveConfiguration("dl24.conf");
    addLogMessage("Connecting to device at: " + state.device_path);
    source_ = std::make_unique<dl24::SourceLinuxImpl>(state.device_path);
    controller_ = std::make_unique<dl24::ControllerImpl>(*source_);
    controller_->subscribeToDebugLogs(this);

    dl24::Error err = controller_->connect();
    if (!err.isSuccess()) {
        addLogMessage("Failed to start controller: " + std::string(err.what()));
    } else {
        addLogMessage("Controller started successfully.");
    }
}

void ViewModel::onUiAction(const UiEvent& event) {
    if (const auto* canceled = dynamic_cast<const ConnectDialogCanceled*>(&event)) {
        state.show_connect_dialog = false;
    } else if (const auto* openEvent = dynamic_cast<const OnConnectRequested*>(&event)) {
        state.show_connect_dialog = true;
    } else if (const auto* openEvent = dynamic_cast<const Open*>(&event)) {
        state.show_connect_dialog = false; // Close the dialog after handling
        onOpenDevice(openEvent);
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

void ViewModel::onDebugMessage(dl24::DebugListener::Level level, const std::string& message) {
    std::string levelStr;
    switch (level) {
        case dl24::DebugListener::Level::Raw:
            levelStr = "[RAW]";
            break;
        case dl24::DebugListener::Level::Paket:
            levelStr = "[PAKET]";
            break;
        case dl24::DebugListener::Level::Answer:
            levelStr = "[ANSWER]";
            break;
    }
    addLogMessage(levelStr + " " + message);
}

void ViewModel::addLogMessage(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    std::string timestampedMessage = ss.str() + " " + message;
    state.uart_logs.push_back(timestampedMessage);
    while (state.uart_logs.size() > 15) {
        state.uart_logs.erase(state.uart_logs.begin()); // Keep only the last 15 messages
    }
    if (stateListener_) {
        stateListener_->onStateChanged(state);
    }
}
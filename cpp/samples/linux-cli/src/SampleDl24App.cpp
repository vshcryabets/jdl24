#include "SampleDl24App.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <vector>
#include <string>

using namespace ftxui;

using json = nlohmann::json;

SampleDl24App::SampleDl24App(): show_connect_dialog(false) {
    loadConfiguration("dl24.conf");
}

SampleDl24App::~SampleDl24App() {
    saveConfiguration("dl24.conf");
}

ftxui::Component SampleDl24App::connectToDevice() {
    auto input_path = Input(&config_.devicePath, "e.g. /dev/ttyUSB0 or COM3");
    
    // Buttons for the dialog
    auto btn_connect = Button("Connect", [&] {
        // uart_logs.push_back("Connecting to " + config_.devicePath + "...");
        show_connect_dialog = false; // Close the dialog
    });
    
    auto btn_cancel = Button("Cancel", [&] {
        show_connect_dialog = false; // Close without doing anything
    });

    // Group the interactive dialog elements together
    auto dialog_container = Container::Vertical({
        input_path,
        Container::Horizontal({btn_cancel, btn_connect}),
    });

    // Render how the dialog looks visually
    ftxui::Component dialog_renderer = Renderer(dialog_container, [=] {
        return window(text(" Enter UART Path "),
            vbox({
                hbox(text(" Path: "), input_path->Render()),
                separator(),
                hbox({
                    filler(), // Pushes buttons to the right
                    btn_cancel->Render(),
                    text(" "), // Small gap between buttons
                    btn_connect->Render(),
                })
            })
        ) | clear_under | center; 
        // clear_under prevents text from bleeding through the background
        // center puts the box right in the middle of the screen
    });
    return dialog_renderer;
}

void SampleDl24App::run() {
    auto screen = ScreenInteractive::Fullscreen();

    // 1. Define State
    std::vector<std::string> menu_entries = {
        "Connect",
        "Quit",
    };
    int menu_selected = 0;

    std::vector<std::string> uart_logs = {
        "System booting...",
        "UART initialized at 115200 8N1",
        "Waiting for data...",
    };

    // 2. Create the Menu Component
    MenuOption option;
    option.on_enter = [&] {
        if (menu_selected == 0) { 
            // Connect
            show_connect_dialog = true; // Show the connect dialog
        } else if (menu_selected == 1) { 
            // Quit
            screen.Exit();
        }
    };
    auto menu = Menu(&menu_entries, &menu_selected, option);

    // 3. Create the Main Layout
    // We wrap the interactive menu in a Renderer to define how the screen is drawn
    auto layout = Renderer(menu, [&] {
        // Build the UART log elements dynamically
        Elements log_elements;
        for (const auto& log : uart_logs) {
            log_elements.push_back(text(log));
        }

        // Create the UART output window
        // yframe and vscroll_indicator allow the box to handle overflowing text safely
        auto uart_window = window(text(" UART Output "),
            vbox(std::move(log_elements)) | vscroll_indicator | yframe
        ) | flex; // 'flex' makes this box take up all remaining horizontal space

        // Create the Menu window
        auto menu_window = window(text(" Menu "),
            menu->Render()
        ) | size(WIDTH, EQUAL, 25); // Fixed width of 25 characters

        // Combine them side-by-side (split screen)
        return hbox({
            menu_window,
            uart_window
        });
    });
    ftxui::Component dialog_renderer = connectToDevice();
    auto root = Modal(layout, dialog_renderer, &show_connect_dialog);
    // 4. Run the application
    screen.Loop(root);
}

void SampleDl24App::loadConfiguration(std::string configFilePath) {
    // Load configuration from file (if exists).
    std::ifstream configFile(configFilePath);
    if (configFile.is_open()) {
        json configJson;
        configFile >> configJson;
        if (configJson.contains("devicePath")) {
            config_.devicePath = configJson["devicePath"];
        }
    }
}

void SampleDl24App::saveConfiguration(std::string configFilePath) {
    // Save configuration to file.
    json configJson;
    configJson["devicePath"] = config_.devicePath;
    std::ofstream configFile(configFilePath);
    if (configFile.is_open()) {
        configFile << configJson.dump(4);  // Pretty print with 4 spaces indentation
    }
}
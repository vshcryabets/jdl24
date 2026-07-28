#include <cstdio>
#include <iostream>

#include "SourceLinuxImpl.h"
#include "ControllerImpl.h"

namespace {

void printMenu() {
    std::printf("\n=== DL24 control ===\n");
    std::printf("  1) Set voltage\n");
    std::printf("  2) Set current\n");
    std::printf("  3) Start\n");
    std::printf("  4) Stop\n");
    std::printf("  5) Reset counters\n");
    std::printf("  0) Exit\n");
    std::printf("Select: ");
    std::fflush(stdout);
}

float readFloat(const char* prompt) {
    std::printf("%s", prompt);
    std::fflush(stdout);
    float value = 0.0f;
    std::cin >> value;
    return value;
}

void reportResult(const char* action, bool ok) {
    std::printf("%s: %s\n", action, ok ? "OK" : "FAILED");
}

}  // namespace

int main() {
    dl24::SourceLinuxImpl source("/dev/ttyUSB0");
    dl24::ControllerImpl controller(source);

    if (!controller.connect()) {
        std::printf("Failed to connect to the device.\n");
        return 1;
    }

    bool running = true;
    while (running) {
        printMenu();

        int choice = -1;
        if (!(std::cin >> choice)) {
            // EOF or non-numeric input: exit cleanly.
            std::printf("\n");
            break;
        }

        switch (choice) {
            case 1:
                reportResult("Set voltage",
                             controller.setVoltage(readFloat("Voltage (V): ")));
                break;
            case 2:
                reportResult("Set current",
                             controller.setCurrent(readFloat("Current (A): ")));
                break;
            case 3:
                reportResult("Start", controller.start());
                break;
            case 4:
                reportResult("Stop", controller.stop());
                break;
            case 5:
                reportResult("Reset counters", controller.resetCounters());
                break;
            case 0:
                running = false;
                break;
            default:
                std::printf("Unknown option: %d\n", choice);
                break;
        }
    }

    controller.disconnect();
    std::printf("Bye.\n");
    return 0;
}

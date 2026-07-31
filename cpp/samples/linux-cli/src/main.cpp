#include <cstdio>
#include <iostream>

#include "SourceLinuxImpl.h"
#include "ControllerImpl.h"

#include "SampleDl24App.h"

int main() {
    ViewModel viewModel;
    SampleDl24App app(viewModel);
    app.run();

    // dl24::SourceLinuxImpl source("/dev/ttyUSB0");
    // dl24::ControllerImpl controller(source);

    // if (!controller.connect()) {
    //     std::printf("Failed to connect to the device.\n");
    //     return 1;
    // }

    // bool running = true;
    // while (running) {
    //     printMenu();

    //     int choice = -1;
    //     if (!(std::cin >> choice)) {
    //         // EOF or non-numeric input: exit cleanly.
    //         std::printf("\n");
    //         break;
    //     }

    //     switch (choice) {
    //         case 1:
    //             reportResult("Set voltage",
    //                          controller.setVoltage(readFloat("Voltage (V): ")));
    //             break;
    //         case 2:
    //             reportResult("Set current",
    //                          controller.setCurrent(readFloat("Current (A): ")));
    //             break;
    //         case 3:
    //             reportResult("Start", controller.start());
    //             break;
    //         case 4:
    //             reportResult("Stop", controller.stop());
    //             break;
    //         case 5:
    //             reportResult("Reset counters", controller.resetCounters());
    //             break;
    //         case 0:
    //             running = false;
    //             break;
    //         default:
    //             std::printf("Unknown option: %d\n", choice);
    //             break;
    //     }
    // }

    // controller.disconnect();
    // std::printf("Bye.\n");
    return 0;
}

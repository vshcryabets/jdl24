#include <cstdio>
#include <unistd.h>
#include "SourceLinuxImpl.h"
#include "ControllerImpl.h"

int main() {
    dl24::SourceLinuxImpl source("/dev/ttyUSB0");
    dl24::ControllerImpl controller(source);

    if (!controller.connect()) {
        std::printf("Failed to connect to the device.\n");
        return 1;
    }

    if (!controller.setVoltage(12.34f)) {
        std::printf("Failed to set voltage.\n");
        return 1;
    }
    sleep(5);  // Wait for the device to process the command
    controller.resetCounters();

    controller.disconnect();

    std::printf("Hello, world!\n");
    return 0;
}

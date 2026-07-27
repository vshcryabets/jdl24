#include "SourceLinuxImpl.h"

#include <cstdio>

int main() {
    dl24::SourceLinuxImpl source("/dev/ttyUSB0");
    std::printf("Hello, world!\n");
    return 0;
}

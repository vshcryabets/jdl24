#include <cstdio>
#include <iostream>

#include "SourceLinuxImpl.h"
#include "ControllerImpl.h"

#include "SampleDl24App.h"

int main() {
    ViewModel viewModel;
    SampleDl24App app(viewModel);
    app.run();
    return 0;
}

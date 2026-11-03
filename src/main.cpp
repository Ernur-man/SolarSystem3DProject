#include "Application.h"

#include <iostream>

int main() {
    Application app;
    if (!app.init()) {
        std::cerr << "Failed to initialize Solar System 3D. Check the console diagnostics.\n";
        return 1;
    }
    app.run();
    return 0;
}

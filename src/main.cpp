#include "Application.h"

#include <iostream>
#include <string_view>

int main(int argc, char **argv) {
    bool introEnabled = true;
    for (int i = 1; i < argc; ++i)
        if (std::string_view(argv[i]) == "--no-intro") introEnabled = false;
    Application app;
    if (!app.init(introEnabled)) {
        std::cerr << "Failed to initialize Solar System 3D. Check the console diagnostics.\n";
        return 1;
    }
    app.run();
    return 0;
}

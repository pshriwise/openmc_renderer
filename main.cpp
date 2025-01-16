#include "render.h"

// Entry point for the OpenMC Renderer application
int main(int argc, char* argv[]) {
    try {
        // Create the renderer instance using modern C++ memory management
        auto renderer = std::make_unique<OpenMCRenderer>(argc, argv);

        // Start the rendering process
        renderer->render();
    } catch (const std::exception& e) {
        // Log any exceptions and exit gracefully
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

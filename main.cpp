#include "render.h"

int main(int argc, char* argv[]) {
    auto renderer = std::make_unique<OpenMCRenderer>(argc, argv);
    renderer->render();
    return 0;
}
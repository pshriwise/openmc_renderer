#include <iostream>

#include "plotter.h"

int main(int argc, char* argv[]) {
  auto openmc_plotter = OpenMCPlotter(argc, argv);
  openmc_plotter.create_image();
  return 0;
}
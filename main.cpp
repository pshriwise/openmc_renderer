
#include "openmc/capi.h"


int main(int argc, char* argv[]) {
  int err;

  err = openmc_init(argc, argv, nullptr);
  err = openmc_plot_geometry();
  err = openmc_finalize();

  return 0;
}
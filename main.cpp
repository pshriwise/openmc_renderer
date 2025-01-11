
#include "openmc/capi.h"


int main(int argc, char* argv[]) {
  openmc_init(argc, argv, nullptr);
  openmc_run();
  openmc_finalize();
  return 0;
}
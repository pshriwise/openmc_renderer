#include <iostream>

#include "openmc/capi.h"
#include "openmc/plot.h"
#include "openmc/settings.h"


class OpenMCPlotter {

public:
  OpenMCPlotter(int argc, char* argv[]) {
    int err = openmc_init(argc, argv, nullptr);
    if (err) {
      throw std::runtime_error("Error initializing OpenMC");
    }

    plot_ = dynamic_cast<openmc::PhongPlot*>(openmc::model::plots[0].get());

    if (!plot_) {
      throw std::runtime_error("Plot zero is not a PhongPlot");
    }
  }

  openmc::ImageData create_image() {
    return plot()->create_image();
  }

  ~OpenMCPlotter() {
    int err  = openmc_finalize();
    if (err) {
      std::cerr << "Error finalizing OpenMC" << std::endl;
    }
  }

  const openmc::PhongPlot* plot() const {
    return plot_;
  }

private:
  const openmc::PhongPlot* plot_;
};
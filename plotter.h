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

  openmc::PhongPlot* plot() {
    return plot_;
  }

  void set_camera_position(openmc::Position position) {
    plot()->camera_position_ = position;
  }

  void set_look_at(openmc::Position look_at) {
    plot()->look_at_ = look_at;
  }

  void set_up_vector(openmc::Direction up) {
    plot()->up_ = up;
  }

  void set_field_of_view(double fov) {
    plot()->horizontal_field_of_view_ = fov;
  }

private:
  openmc::PhongPlot* plot_;
};

// void transferCameraInfo(OpenMCPlotter& plotter, const Camera& camera) {
//     plotter.set_camera_position(camera.position);
//     plotter.set_look_at(camera.lookAt);
//     plotter.set_up_vector(camera.upVector);
//     plotter.set_field_of_view(camera.fov);
// }
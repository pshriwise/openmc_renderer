#include <iostream>
#include <memory>

#include "openmc/capi.h"
#include "openmc/material.h"
#include "openmc/plot.h"
#include "openmc/settings.h"


class OpenMCPlotter {

public:
  OpenMCPlotter(int argc, char* argv[]) {
    int err = openmc_init(argc, argv, nullptr);
    if (err) {
      throw std::runtime_error("Error initializing OpenMC");
    }

    // create a new plot object
    plot_ = std::make_unique<openmc::PhongPlot>();

    set_plot_defaults();

    if (!plot_) {
      throw std::runtime_error("Plot zero is not a PhongPlot");
    }
  }

  openmc::ImageData create_image() {
    plot()->print_info();
    auto img = xt::transpose(plot()->create_image());
    return img;
  }

  ~OpenMCPlotter() {
    int err  = openmc_finalize();
    if (err) {
      std::cerr << "Error finalizing OpenMC" << std::endl;
    }
  }

  void set_plot_defaults() {
    plot()->color_by_ = openmc::PlottableInterface::PlotColorBy::mats;
    plot()->pixels() = {400, 400};
    plot()->set_default_colors();

    plot()->opaque_ids().clear();

    for (const auto& mat : openmc::model::materials) {
       plot()->opaque_ids().insert(mat->id_);
    }

    // set opaque "IDs" by index
    for (int i = 0; i < openmc::model::materials.size(); i++) {
      plot()->opaque_ids().insert(i);
    }

  }

  void set_material_visibility(int32_t id, bool visibility) {
    // have to convert from material ID to index
    int32_t mat_index = openmc::model::material_map[id];
    if (visibility) {
      plot()->opaque_ids().insert(mat_index);
    } else {
      plot()->opaque_ids().erase(mat_index);
    }
  }

  std::unordered_map<int32_t, openmc::RGBColor> color_map() {
    auto map_out = std::unordered_map<int32_t, openmc::RGBColor>();
    if (plot()->color_by() == openmc::PlottableInterface::PlotColorBy::mats) {
      for (int i = 0; i < openmc::model::materials.size(); i++) {
        const auto& mat = openmc::model::materials[i];
        map_out[mat->id_] = plot()->colors_[i];
      }
    } else if (plot()->color_by() == openmc::PlottableInterface::PlotColorBy::cells) {
      for (int i = 0; i < openmc::model::cells.size(); i++) {
        const auto& cell = openmc::model::cells[i];
        map_out[cell->id_] = plot()->colors_[i];
      }
    }
    return map_out;
  }

  const std::unique_ptr<openmc::PhongPlot>& plot() {
    return plot_;
  }

  void set_camera_position(openmc::Position position) {
    plot()->camera_position() = position;
  }

  void set_look_at(openmc::Position look_at) {
    plot()->look_at() = look_at;
  }

  void set_light_position(openmc::Position light_position) {
    plot()->light_location() = light_position;
  }

  void set_up_vector(openmc::Direction up) {
    plot()->up() = up;
  }

  void set_field_of_view(double fov) {
    plot()->horizontal_field_of_view() = fov;
  }

private:
  std::unique_ptr<openmc::PhongPlot> plot_;
};

// void transferCameraInfo(OpenMCPlotter& plotter, const Camera& camera) {
//     plotter.set_camera_position(camera.position);
//     plotter.set_look_at(camera.lookAt);
//     plotter.set_up_vector(camera.upVector);
//     plotter.set_field_of_view(camera.fov);
// }
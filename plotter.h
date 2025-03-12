#include <iostream>
#include <memory>

#include "openmc/capi.h"
#include "openmc/material.h"
#include "openmc/plot.h"
#include "openmc/settings.h"

#ifndef OPENMC_PLOTTER_H
#define OPENMC_PLOTTER_H
class OpenMCPlotter {

private:
  OpenMCPlotter() {};

public:
  static OpenMCPlotter& get_instance() {
    static OpenMCPlotter instance;
    return instance;
  }

  OpenMCPlotter(OpenMCPlotter const&) = delete;
  void operator=(OpenMCPlotter const&) = delete;
  OpenMCPlotter(OpenMCPlotter const&&) = delete;
  void operator=(OpenMCPlotter const&&) = delete;


  void initialize(int argc, char* argv[]) {
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

  void set_pixels(int32_t width, int32_t height) {
    plot()->pixels()[0] = width;
    plot()->pixels()[1] = height;
  }

  openmc::ImageData create_image() {
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

  void set_color(int32_t id, openmc::RGBColor color) {
    // have to convert from material ID to index
    int32_t mat_index = openmc::model::material_map[id];
    plot()->colors_[mat_index] = color;
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

  int32_t query_cell(openmc::Position position, openmc::Direction direction) {
    // fire a ray into the geometry and get the cell ID on the other side
    // of the first surface
    openmc::GeometryState g;
    g.r() = position;
    g.u() = direction;
    g.coord(0).universe = openmc::model::root_universe;

    std::cout << "Position: " << g.r() << std::endl;
    std::cout << "Direction: " << g.u() << std::endl;

    // attempt to find the cell, move the ray up to the
    // model boundary if necessary
    if (!openmc::exhaustive_find_cell(g)) {
      g.advance_to_boundary_from_void();
      openmc::exhaustive_find_cell(g);
    }
    std::cout << "Cell ID: " << g.lowest_coord().cell << std::endl;

    int32_t cell_id = openmc::model::cells[g.lowest_coord().cell]->id_;
    // if this cell ID isn't visible, continue the ray until we find a visible cell
    while (plot()->opaque_ids().count(cell_id) == 0) {
      auto boundary = openmc::distance_to_boundary(g);
      // move the ray forward by the distance to the boundary
      g.move_distance(boundary.distance);
      g.surface() = boundary.surface_index;
      g.n_coord_last() = g.n_coord();
      g.n_coord() = boundary.coord_level;
      if (boundary.lattice_translation[0] != 0 || boundary.lattice_translation[1] != 0 || boundary.lattice_translation[2] != 0) {
        openmc::cross_lattice(g, boundary);
      }
      return -1;
      openmc::exhaustive_find_cell(g);
      cell_id = openmc::model::cells[g.lowest_coord().cell]->id_;
    }
    std::cout << "Cell ID: " << cell_id << std::endl;
    return cell_id;
  }

private:
  std::unique_ptr<openmc::PhongPlot> plot_;
};

#endif // include guard

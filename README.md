# openmc_renderer
Basic rendering app for OpenMC Geometry

A real-timey 3D visualization tool for OpenMC geometry models. This application provides interactive viewing of OpenMC geometries with features like dynamic lighting, material/cell coloring, and geometry querying.

## Dependencies

### OpenMC Requirements

Requires OpenMC isntalled with [this branch](https://github.com/pshriwise/openmc/tree/raytrace_plots_render) in my fork of the main repo.

### Ubuntu System Dependencies
```bash
# OpenGL and GLFW dependencies
sudo apt-get install libgl1-mesa-dev
sudo apt-get install libglfw3-dev

# Other build dependencies
sudo apt-get install cmake
sudo apt-get install build-essential
```

## Controls

### Camera Controls
- **Left Mouse Button + Drag**: Rotate camera around model
- **Middle Mouse Button + Drag**: Pan camera
- **Mouse Wheel**: Zoom in/out
- **I**: Reset to isometric view
- **X**: View along X axis (positive direction)
- **Y**: View along Y axis (positive direction)
- **Z**: View along Z axis (positive direction)
- **Shift + X**: View along X axis (negative direction)
- **Shift + Y**: View along Y axis (negative direction)
- **Shift + Z**: View along Z axis (negative direction)

### Lighting Controls
- **L + Left Mouse Button**: Rotate light around model
- **L + Middle Mouse Button**: Move light closer/further
- **Shift + L**: Toggle "Light Follows Camera" mode

### Geometry Query Controls
- **Q**: Query cell ID at cursor position
- **ESC**: Close cell query display

### Display Controls
- **?**: Toggle help overlay
- **ESC**: Close overlay or exit application
- **Ctrl + W/Q**: Exit application

### Interface Features
- **Color Legend**: Toggle between material and cell coloring
- **Material/Cell Visibility**: Toggle visibility of individual materials/cells
- **Color Customization**: Customize colors for materials/cells
- **Camera Settings**:
  - Adjust image resolution
  - Toggle light following camera
  - Adjust pan sensitivity
  - Adjust zoom sensitivity
  - Adjust rotation sensitivity

## Building and Running

[Add build instructions here once confirmed]
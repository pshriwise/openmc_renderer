#include <iostream>
#include <stdexcept>

#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include <vector>
#include <xtensor/xarray.hpp>
#include <xtensor/xio.hpp>
#include <xtensor/xrandom.hpp>
#include <xtensor/xview.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imguiwrap.h"
#include "imguiwrap.dear.h"
#include "imguiwrap.helpers.h"

#include "plotter.h"

class Camera {
public:
    float zoom;
    float panX;
    float panY;
    float zoomSensitivity;  // Added zoom sensitivity control
    float panSensitivity;  // Added pan sensitivity control
    float rotationSensitivity;  // Added rotation sensitivity control

    // Quaternion for rotation
    struct Quaternion {
        float w, x, y, z;

        Quaternion() : w(1.0f), x(0.0f), y(0.0f), z(0.0f) {}

        Quaternion(float w, float x, float y, float z)
            : w(w), x(x), y(y), z(z) {}

        static Quaternion fromAxisAngle(float angle, float ax, float ay, float az) {
            float halfAngle = angle * 0.5f;
            float s = std::sin(halfAngle);
            float length = std::sqrt(ax * ax + ay * ay + az * az);
            if (length > 0.0f) {
                s /= length;
            }
            return Quaternion(std::cos(halfAngle), ax * s, ay * s, az * s);
        }

        void normalize() {
            float len = std::sqrt(w*w + x*x + y*y + z*z);
            if (len > 0) {
                w /= len;
                x /= len;
                y /= len;
                z /= len;
            }
        }

        Quaternion operator*(const Quaternion& q) const {
            return Quaternion(
                w*q.w - x*q.x - y*q.y - z*q.z,
                w*q.x + x*q.w + y*q.z - z*q.y,
                w*q.y - x*q.z + y*q.w + z*q.x,
                w*q.z + x*q.y - y*q.x + z*q.w
            );
        }
    };

    // Camera properties
    double fov;
    openmc::Position position;
    openmc::Position lookAt;
    openmc::Position upVector;
    openmc::Position lightPosition {0, 10, -10};
    Quaternion rotation;
    openmc::Position right;  // Added to track right vector

    Camera()
        : zoom(-5.0f), panX(0.0f), panY(0.0f),
          zoomSensitivity(2.5f), panSensitivity(0.02f),
          rotationSensitivity(0.5f), fov(45.0) {  // Initialize rotation sensitivity
        position = {10, 10, 10};
        lookAt = {0.0, 0.0, 0.0};
        upVector = {0.0, 0.0, 1.0};
        rotation = Quaternion();
        updateVectors();
    }

    void updateVectors() {
        // Calculate the forward vector (camera direction)
        openmc::Position forward = lookAt - position;
        forward = forward / forward.norm();

        // Calculate the right vector
        right = forward.cross(upVector);
        right = right / right.norm();

        // Recalculate up vector to ensure orthogonality
        upVector = right.cross(forward);
        upVector = upVector / upVector.norm();
    }

    void rotate(float deltaX, float deltaY) {
        // Apply rotation sensitivity
        deltaX *= rotationSensitivity;
        deltaY *= rotationSensitivity;

        // Convert deltas to radians
        float radiansX = deltaX * M_PI / 180.0f;
        float radiansY = deltaY * M_PI / 180.0f;

        // Create rotation quaternions around right and up vectors
        Quaternion pitchRotation = Quaternion::fromAxisAngle(radiansY, right[0], right[1], right[2]);
        Quaternion yawRotation = Quaternion::fromAxisAngle(radiansX, upVector[0], upVector[1], upVector[2]);

        // Combine rotations
        rotation = yawRotation * pitchRotation * rotation;
        rotation.normalize();

        // Update camera vectors after rotation
        updateVectors();
    }

    void applyTransformations() {
        glLoadIdentity();

        // Calculate view-aligned pan offsets
        openmc::Position forward = lookAt - position;
        forward = forward / forward.norm();

        openmc::Position viewRight = forward.cross(upVector);
        viewRight = viewRight / viewRight.norm();

        openmc::Position viewUp = viewRight.cross(forward);
        viewUp = viewUp / viewUp.norm();

        // Apply pan offset to both position and lookAt
        openmc::Position adjustedPosition = position + viewRight * panX + viewUp * panY;
        openmc::Position adjustedLookAt = lookAt + viewRight * panX + viewUp * panY;

        // Apply zoom to position (camera moves along view direction)
        openmc::Position zoomedDirection = (adjustedLookAt - adjustedPosition);
        zoomedDirection = zoomedDirection / zoomedDirection.norm();
        adjustedPosition = adjustedPosition + zoomedDirection * zoom;

        // Apply rotation
        applyRotation(adjustedPosition);
        applyRotation(adjustedLookAt);

        gluLookAt(adjustedPosition[0], adjustedPosition[1], adjustedPosition[2],
                adjustedLookAt[0], adjustedLookAt[1], adjustedLookAt[2],
                upVector[0], upVector[1], upVector[2]);
    }

    void updateView(int width, int height) {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(fov, static_cast<double>(width) / height, 1.0, 500.0);
        glMatrixMode(GL_MODELVIEW);
    }

    openmc::Position getTransformedPosition() const {
        // Calculate view-aligned pan offsets
        openmc::Position forward = lookAt - position;
        forward = forward / forward.norm();

        openmc::Position viewRight = forward.cross(upVector);
        viewRight = viewRight / viewRight.norm();

        openmc::Position viewUp = viewRight.cross(forward);
        viewUp = viewUp / viewUp.norm();

        // Start with base position
        openmc::Position transformedPosition = position;

        // Apply pan offset
        transformedPosition = transformedPosition + viewRight * panX + viewUp * panY;

        // Apply zoom after panning
        openmc::Position zoomedDirection = (lookAt - position);
        zoomedDirection = zoomedDirection / zoomedDirection.norm();
        transformedPosition = transformedPosition + zoomedDirection * zoom;

        // Apply rotation
        applyRotation(transformedPosition);
        return transformedPosition;
    }

    openmc::Position getTransformedLookAt() const {
        // Calculate view-aligned pan offsets
        openmc::Position forward = lookAt - position;
        forward = forward / forward.norm();

        openmc::Position viewRight = forward.cross(upVector);
        viewRight = viewRight / viewRight.norm();

        openmc::Position viewUp = viewRight.cross(forward);
        viewUp = viewUp / viewUp.norm();

        // Start with base lookAt
        openmc::Position transformedLookAt = lookAt;

        // Apply pan offset
        transformedLookAt = transformedLookAt + viewRight * panX + viewUp * panY;

        // Apply rotation
        applyRotation(transformedLookAt);
        return transformedLookAt;
    }

    openmc::Position getTransformedUpVector() const {
        openmc::Position transformedUp = upVector;
        applyRotation(transformedUp);
        return transformedUp;
    }

    void setIsometricView() {
        // Calculate current distance from camera to look-at point
        openmc::Position currentDir = getTransformedPosition() - getTransformedLookAt();
        float currentDistance = currentDir.norm();

        // Reset rotation
        rotation = Quaternion();

        // Set camera to isometric position (equal angles to all axes)
        float angle = 120.0f;    // Angle from negative Z-axis (30 degrees from XY plane)
        float phi = 45.0f;       // Azimuthal angle

        // Convert spherical coordinates to Cartesian
        float theta = angle * M_PI / 180.0f;  // Convert to radians
        float phiRad = phi * M_PI / 180.0f;

        position = {
            currentDistance * sin(theta) * cos(phiRad),
            currentDistance * sin(theta) * sin(phiRad),
            currentDistance * cos(theta)
        };

        // Reset other parameters
        lookAt = {0.0, 0.0, 0.0};
        upVector = {0.0, 0.0, 1.0};
        panX = 0.0f;
        panY = 0.0f;

        updateVectors();
    }

    void applyRotation(openmc::Position& vec) const {
        // Apply quaternion rotation
        float x = vec[0], y = vec[1], z = vec[2];

        // Convert quaternion to rotation matrix and apply
        float wx = rotation.w * rotation.x;
        float wy = rotation.w * rotation.y;
        float wz = rotation.w * rotation.z;
        float xx = rotation.x * rotation.x;
        float xy = rotation.x * rotation.y;
        float xz = rotation.x * rotation.z;
        float yy = rotation.y * rotation.y;
        float yz = rotation.y * rotation.z;
        float zz = rotation.z * rotation.z;

        vec[0] = (1 - 2*(yy + zz)) * x + 2*(xy - wz) * y + 2*(xz + wy) * z;
        vec[1] = 2*(xy + wz) * x + (1 - 2*(xx + zz)) * y + 2*(yz - wx) * z;
        vec[2] = 2*(xz - wy) * x + 2*(yz + wx) * y + (1 - 2*(xx + yy)) * z;
    }

    enum class Axis {
        X,
        Y,
        Z
    };

    void setAxisView(Axis axis, bool negative = false) {
        // Calculate current distance from camera to look-at point
        openmc::Position currentDir = getTransformedPosition() - getTransformedLookAt();
        float currentDistance = currentDir.norm();
        if (negative) {
            currentDistance = -currentDistance;
        }

        // Reset rotation
        rotation = Quaternion();

        // Set common parameters
        position = {0.0f, 0.0f, 0.0f};
        lookAt = {0.0, 0.0, 0.0};
        panX = 0.0f;
        panY = 0.0f;

        // Set position and up vector based on axis
        switch (axis) {
            case Axis::X:
                position[0] = currentDistance;  // Camera on X axis
                upVector = {0.0, 0.0, 1.0};  // Z is up
                break;
            case Axis::Y:
                position[1] = currentDistance;  // Camera on Y axis
                upVector = {0.0, 0.0, 1.0};  // Z is up
                break;
            case Axis::Z:
                position[2] = currentDistance;  // Camera on Z axis
                upVector = {0.0, 1.0, 0.0};  // Y is up for top view
                break;
        }

        updateVectors();
    }

private:
};

class OpenMCRenderer {

public:
  // Add state for light control
  bool light_control_mode = false;

  OpenMCRenderer(int argc, char* argv[]) {
    openmc_plotter_.initialize(argc, argv);

    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    window_ = glfwCreateWindow(800, 600, "OpenMC Geometry", nullptr, nullptr);
    if (!window_) {
        throw std::runtime_error("Failed to create GLFW window");
        glfwTerminate();
    }

    glfwMakeContextCurrent(window_);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetCursorPosCallback(window_, cursorPositionCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
    glfwSetKeyCallback(window_, keyCallback);
    glfwSetWindowUserPointer(window_, this);

    // Create ImGui context
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    glEnable(GL_DEPTH_TEST);

    glfwGetFramebufferSize(window_, &frame_width_, &frame_height_);
    framebufferSizeCallback(window_, frame_width_, frame_height_);

    texture_ = createTextureFromImageData(openmc_plotter_.create_image());

    // Start in isometric view
    camera_.setIsometricView();
    transferCameraInfo();

    // Add help overlay state and show it on startup
    show_help_overlay = true;
  }

  void render() {
       while (!glfwWindowShouldClose(window_)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        camera_.applyTransformations();
        transferCameraInfo();
        updateVisibleMaterials();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Add help button in lower-right corner
        const float windowWidth = ImGui::GetIO().DisplaySize.x;
        const float windowHeight = ImGui::GetIO().DisplaySize.y;
        ImGui::SetNextWindowPos(ImVec2(windowWidth - 50, windowHeight - 40), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGui::Begin("Help Button", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings);
        if (ImGui::Button("?")) {
            show_help_overlay = !show_help_overlay;
        }
        ImGui::End();

        // Render help overlay if active
        if (show_help_overlay) {
            renderHelpOverlay();
        }

        // Only show other windows if help overlay is not active
        if (!show_help_overlay) {
            displayColorLegend();
            displaySettings();
        }

        // Update the texture with new image data if the camera has changed
        auto newImageData = openmc_plotter_.create_image();
        updateTexture(newImageData);

        // Draw the background
        drawBackground();

        glfwPollEvents();

        // Render Dear ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }
  }

  void updateTexture(const openmc::ImageData& imageData) {
    int width = imageData.shape()[0];
    int height = imageData.shape()[1];

    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, imageData.data());
  }


  // Function to create a texture from ImageData
  GLuint createTextureFromImageData(const openmc::ImageData& imageData) {
    int width = imageData.shape()[0];
    int height = imageData.shape()[1];

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, imageData.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return texture;
  }

    // Function to draw the background
  void drawBackground() {
      glDisable(GL_DEPTH_TEST);
      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glLoadIdentity();
      glOrtho(0, 1, 0, 1, -1, 1);
      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glLoadIdentity();

      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, texture_);

      glBegin(GL_QUADS);
      glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
      glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, 0.0f);
      glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, 1.0f);
      glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 1.0f);
      glEnd();

      glDisable(GL_TEXTURE_2D);

      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();
      glEnable(GL_DEPTH_TEST);
  }

  // Callbacks

  static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto renderer = static_cast<OpenMCRenderer*>(glfwGetWindowUserPointer(window));
    if (!renderer) {
      throw std::runtime_error("Failed to get renderer from window user pointer");
    }
    renderer->mouseButtonUpdate(button, action, mods);
  }

  void mouseButtonUpdate(int button, int action, int mods) {
    // Skip if help overlay is visible or ImGui is handling this event
    ImGuiIO& io = ImGui::GetIO();
    if (show_help_overlay || io.WantCaptureMouse) {
        return;
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
      if (action == GLFW_PRESS) {
        draggingLeft = true;
        glfwGetCursorPos(window_, &lastMouseX, &lastMouseY);
      } else if (action == GLFW_RELEASE) {
        draggingLeft = false;
      }
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
      if (action == GLFW_PRESS) {
        draggingMiddle = true;
        glfwGetCursorPos(window_, &lastMouseX, &lastMouseY);
      } else if (action == GLFW_RELEASE) {
        draggingMiddle = false;
      }
    }
  }

  // Mouse motion callback
  static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
    auto renderer = static_cast<OpenMCRenderer*>(glfwGetWindowUserPointer(window));
    if (!renderer) {
      throw std::runtime_error("Failed to get renderer from window user pointer");
    }
    renderer->cursorPositionUpdate(xpos, ypos);
  }

  void cursorPositionUpdate(double xpos, double ypos) {
    // Skip if help overlay is visible or ImGui is handling this event
    ImGuiIO& io = ImGui::GetIO();
    if (show_help_overlay || io.WantCaptureMouse) {
        return;
    }

    if (light_control_mode) {
        // Handle light position control
        if (draggingLeft) {
            // Rotate light around the model
            float deltaX = (xpos - lastMouseX) * 0.1f;
            float deltaY = (ypos - lastMouseY) * 0.1f;

            // Get current light distance from origin
            float distance = std::sqrt(
                camera_.lightPosition[0] * camera_.lightPosition[0] +
                camera_.lightPosition[1] * camera_.lightPosition[1] +
                camera_.lightPosition[2] * camera_.lightPosition[2]
            );

            // Convert to spherical coordinates
            float theta = static_cast<float>(std::acos(camera_.lightPosition[2] / distance));
            float phi = static_cast<float>(std::atan2(camera_.lightPosition[1], camera_.lightPosition[0]));

            // Update angles
            theta = std::max(0.1f, std::min(static_cast<float>(M_PI) - 0.1f, theta + deltaY));
            phi += deltaX;

            // Convert back to Cartesian coordinates
            camera_.lightPosition[0] = distance * std::sin(theta) * std::cos(phi);
            camera_.lightPosition[1] = distance * std::sin(theta) * std::sin(phi);
            camera_.lightPosition[2] = distance * std::cos(theta);
        }
        if (draggingMiddle) {
            // Move light closer/further from origin
            float deltaY = (ypos - lastMouseY) * 0.1f;
            float currentDistance = std::sqrt(
                camera_.lightPosition[0] * camera_.lightPosition[0] +
                camera_.lightPosition[1] * camera_.lightPosition[1] +
                camera_.lightPosition[2] * camera_.lightPosition[2]
            );
            float newDistance = std::max(5.0f, currentDistance + deltaY);
            float scale = newDistance / currentDistance;
            camera_.lightPosition[0] *= scale;
            camera_.lightPosition[1] *= scale;
            camera_.lightPosition[2] *= scale;
        }
        lastMouseX = xpos;
        lastMouseY = ypos;
        transferCameraInfo();
    } else {
        // Handle existing camera controls
        if (draggingLeft) {
            float deltaX = (xpos - lastMouseX) * 0.5f;
            float deltaY = (ypos - lastMouseY) * 0.5f;
            camera_.rotate(deltaX, deltaY);
            lastMouseX = xpos;
            lastMouseY = ypos;
            transferCameraInfo();
        }
        if (draggingMiddle) {
            camera_.panX -= (xpos - lastMouseX) * camera_.panSensitivity;
            camera_.panY -= (ypos - lastMouseY) * camera_.panSensitivity;
            lastMouseX = xpos;
            lastMouseY = ypos;
            transferCameraInfo();
        }
    }
  }


  static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto renderer = static_cast<OpenMCRenderer*>(glfwGetWindowUserPointer(window));
    if (!renderer) {
      throw std::runtime_error("Failed to get renderer from window user pointer");
    }
    renderer->scrollUpdate(xoffset, yoffset);
  }

  void scrollUpdate(double xoffset, double yoffset) {
      // Skip if help overlay is visible or ImGui is handling this event
      ImGuiIO& io = ImGui::GetIO();
      if (show_help_overlay || io.WantCaptureMouse) {
          return;
      }

      float zoomFactor = yoffset * camera_.zoomSensitivity;

      if (light_control_mode) {
          // Only adjust light position when in light control mode
          float currentDistance = std::sqrt(
              camera_.lightPosition[0] * camera_.lightPosition[0] +
              camera_.lightPosition[1] * camera_.lightPosition[1] +
              camera_.lightPosition[2] * camera_.lightPosition[2]
          );

          float scale = 1.0f + (zoomFactor / 10.0f);
          float newDistance = currentDistance * scale;

          // Ensure minimum light distance of 5.0f
          if (newDistance >= 5.0f) {
              camera_.lightPosition[0] *= scale;
              camera_.lightPosition[1] *= scale;
              camera_.lightPosition[2] *= scale;
          }
      } else {
          // Only update camera zoom when not in light control mode
          camera_.zoom -= zoomFactor;  // Invert zoom direction for more intuitive control
      }

      transferCameraInfo();
  }

  void transferCameraInfo() {
      openmc_plotter_.set_camera_position(camera_.getTransformedPosition());
      openmc_plotter_.set_look_at(camera_.getTransformedLookAt());
      openmc_plotter_.set_up_vector(camera_.getTransformedUpVector());
      openmc_plotter_.set_field_of_view(camera_.fov);
      openmc_plotter_.set_light_position(camera_.lightPosition);
  }

  // using VisibilityCallback = std::function<void(int32_t, bool)>;


  // VisibilityCallback onVisibilityChange = [](int32_t materialID, bool visibility) {
  //     if (materialVisibility.find(materialID) != materialVisibility.end()) {
  //         materialVisibility[materialID] = visibility;
  //     }
  // };

  // Framebuffer size callback to adjust the viewport
  static void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto renderer = static_cast<OpenMCRenderer*>(glfwGetWindowUserPointer(window));
    if (!renderer) {
      throw std::runtime_error("Failed to get renderer from window user pointer");
    }
    renderer->framebufferUpdate(width, height);
  }

  void framebufferUpdate(int width, int height) {
      glViewport(0, 0, width, height);
      camera_.updateView(width, height);
  }

  // A map to track visibility state for each material and cell
  std::unordered_map<int32_t, bool> materialVisibility;
  std::unordered_map<int32_t, bool> cellVisibility;
  // Cache for custom colors
  std::unordered_map<int32_t, openmc::RGBColor> materialColors;
  std::unordered_map<int32_t, openmc::RGBColor> cellColors;

  void cacheCurrentColors() {
    const auto& colorMap = openmc_plotter_.color_map();
    auto& targetCache = openmc_plotter_.plot()->color_by() == openmc::PlottableInterface::PlotColorBy::mats ?
                       materialColors : cellColors;

    // Cache current colors
    for (const auto& [id, color] : colorMap) {
      targetCache[id] = color;
    }
  }

  void restoreColorCache() {
    const auto& sourceCache = openmc_plotter_.plot()->color_by() == openmc::PlottableInterface::PlotColorBy::mats ?
                            materialColors : cellColors;

    // Restore cached colors
    for (const auto& [id, color] : sourceCache) {
      openmc_plotter_.set_color(id, color);
    }
  }

  void displayColorLegend() {
    const auto colorMap = openmc_plotter_.color_map();

    static int selected_id = -1; // Track which material/cell color is being edited
    static openmc::RGBColor temp_color = {0, 0, 0}; // Temporary color for editing

    ImGui::Begin("Color Legend");

    // Add radio buttons for color mode selection
    static bool colorByMaterials = (openmc_plotter_.plot()->color_by() == openmc::PlottableInterface::PlotColorBy::mats);
    bool previousMode = colorByMaterials;

    ImGui::Text("Color by:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Material", colorByMaterials)) {
        if (!colorByMaterials) { // Only if actually changing to materials mode
            cacheCurrentColors(); // Cache current cell colors
            colorByMaterials = true;
            openmc_plotter_.plot()->color_by_ = openmc::PlottableInterface::PlotColorBy::mats;
            restoreColorCache(); // Restore material colors
        }
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Cell", !colorByMaterials)) {
        if (colorByMaterials) { // Only if actually changing to cells mode
            cacheCurrentColors(); // Cache current material colors
            colorByMaterials = false;
            openmc_plotter_.plot()->color_by_ = openmc::PlottableInterface::PlotColorBy::cells;
            restoreColorCache(); // Restore cell colors
        }
    }

    ImGui::Separator();
    ImGui::Text("Legend:"); // Title of the legend

    auto& currentVisibilityMap = colorByMaterials ? materialVisibility : cellVisibility;
    std::string idPrefix = colorByMaterials ? "Material" : "Cell";

    for (const auto& [id, color] : colorMap) {
        // Ensure ID has a visibility entry
        if (currentVisibilityMap.find(id) == currentVisibilityMap.end()) {
            currentVisibilityMap[id] = true; // Default visibility is true
        }

        // Convert color values from [0-255] to [0-1] for ImGui
        float color_array[3] = {
            color.red / 255.0f,
            color.green / 255.0f,
            color.blue / 255.0f
        };

        ImGui::PushID(id);

        // Color button that opens the picker
        if (ImGui::ColorButton("##ColorBtn", ImVec4(color_array[0], color_array[1], color_array[2], 1.0f))) {
            ImGui::OpenPopup("ColorPicker");
            temp_color = color;
            selected_id = id;
        }

        ImGui::SameLine();
        ImGui::Text("%s ID: %d", idPrefix.c_str(), id);

        // Add a visibility checkbox
        ImGui::SameLine();
        bool visibility = currentVisibilityMap[id];
        if (ImGui::Checkbox(("Visible##" + std::to_string(id)).c_str(), &visibility)) {
            currentVisibilityMap[id] = visibility;
            openmc_plotter_.set_material_visibility(id, visibility);
        }

        // Color picker popup
        if (ImGui::BeginPopup("ColorPicker")) {
            float temp_array[3] = {
                temp_color.red / 255.0f,
                temp_color.green / 255.0f,
                temp_color.blue / 255.0f
            };

            if (ImGui::ColorPicker3("##picker", temp_array,
                ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB)) {
                temp_color.red = static_cast<uint8_t>(temp_array[0] * 255);
                temp_color.green = static_cast<uint8_t>(temp_array[1] * 255);
                temp_color.blue = static_cast<uint8_t>(temp_array[2] * 255);

                // Apply color changes immediately
                openmc_plotter_.set_color(selected_id, temp_color);
                auto& targetCache = colorByMaterials ? materialColors : cellColors;
                targetCache[selected_id] = temp_color;
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    ImGui::End();
  }

  void updateVisibleMaterials() {
    auto& currentVisibilityMap = openmc_plotter_.plot()->color_by() == openmc::PlottableInterface::PlotColorBy::mats ?
                                materialVisibility : cellVisibility;
    for (const auto& [id, visibility] : currentVisibilityMap) {
        openmc_plotter_.set_material_visibility(id, visibility);
    }
  }

  // Modify key callback to remove 'Y' key handling
  static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto renderer = static_cast<OpenMCRenderer*>(glfwGetWindowUserPointer(window));
    if (!renderer) return;

    // Handle help overlay toggle with '?' key
    if (key == GLFW_KEY_SLASH && (mods & GLFW_MOD_SHIFT) && action == GLFW_PRESS) {
        renderer->show_help_overlay = !renderer->show_help_overlay;
        return;
    }

    // Handle Escape key
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        if (renderer->show_help_overlay) {
            renderer->show_help_overlay = false;
            return;
        }
    }

    // Handle light control mode with 'L' key
    if (key == GLFW_KEY_L) {
        if (action == GLFW_PRESS) {
            renderer->light_control_mode = true;
        } else if (action == GLFW_RELEASE) {
            renderer->light_control_mode = false;
        }
        return;
    }

    // Only process other shortcuts if help overlay is not visible
    if (!renderer->show_help_overlay) {
        if ((key == GLFW_KEY_W || key == GLFW_KEY_Q) && action == GLFW_PRESS && (mods & GLFW_MOD_CONTROL)) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        // Handle isometric view
        if (key == GLFW_KEY_I && action == GLFW_PRESS) {
            renderer->camera_.setIsometricView();
            renderer->transferCameraInfo();
        }

        // Handle orthographic views
        if (action == GLFW_PRESS) {
            bool negative = (mods & GLFW_MOD_SHIFT) != 0;
            switch (key) {
                case GLFW_KEY_X:
                    renderer->camera_.setAxisView(Camera::Axis::X, negative);
                    renderer->transferCameraInfo();
                    break;
                case GLFW_KEY_Y:
                    renderer->camera_.setAxisView(Camera::Axis::Y, negative);
                    renderer->transferCameraInfo();
                    break;
                case GLFW_KEY_Z:
                    renderer->camera_.setAxisView(Camera::Axis::Z, negative);
                    renderer->transferCameraInfo();
                    break;
            }
        }
    }
  }

  void displaySettings() {
    // Position the window in the right portion of the screen
    const float windowWidth = ImGui::GetIO().DisplaySize.x;
    const float windowHeight = ImGui::GetIO().DisplaySize.y;
    const float settingsWidth = 300.0f;
    const float settingsHeight = 150.0f;  // Increased height for new control

    // Position in the right third of the screen, below the top
    ImGui::SetNextWindowPos(
        ImVec2(windowWidth - settingsWidth - 10, windowHeight * 0.3f),
        ImGuiCond_FirstUseEver
    );
    ImGui::SetNextWindowSize(ImVec2(settingsWidth, settingsHeight), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Camera Settings")) {
        // Pan sensitivity
        ImGui::Text("Pan Sensitivity");
        float panSens = camera_.panSensitivity;
        if (ImGui::SliderFloat("##Pan", &panSens, 0.001f, 0.1f, "%.3f")) {
            camera_.panSensitivity = panSens;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Adjusts the speed of camera panning");
        }

        // Zoom sensitivity
        ImGui::Text("Zoom Sensitivity");
        float zoomSens = camera_.zoomSensitivity;
        if (ImGui::SliderFloat("##Zoom", &zoomSens, 0.5f, 5.0f, "%.1f")) {
            camera_.zoomSensitivity = zoomSens;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Adjusts the speed of camera zooming");
        }

        // Rotation sensitivity
        ImGui::Text("Rotation Sensitivity");
        float rotSens = camera_.rotationSensitivity;
        if (ImGui::SliderFloat("##Rotation", &rotSens, 0.1f, 2.0f, "%.2f")) {
            camera_.rotationSensitivity = rotSens;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Adjusts the speed of camera rotation");
        }
    }
    ImGui::End();
  }

  int frame_width_;
  int frame_height_;

  // Data members
  bool draggingLeft {false};
  bool draggingMiddle {false};
  double lastMouseX;
  double lastMouseY;

  GLFWwindow* window_ {nullptr};
  GLuint texture_;

  OpenMCPlotter& openmc_plotter_ {OpenMCPlotter::get_instance()};
  Camera camera_;

  // Add this new method to convert screen coordinates to world ray direction
  openmc::Direction screenToWorldDirection(double screenX, double screenY) {
      // Convert screen coordinates to normalized device coordinates (-1 to 1)
      float ndcX = (2.0f * screenX) / frame_width_ - 1.0f;
      float ndcY = 1.0f - (2.0f * screenY) / frame_height_;

      // Convert to view space considering FOV and aspect ratio
      float aspectRatio = static_cast<float>(frame_width_) / frame_height_;
      float tanHalfFov = tan(camera_.fov * 0.5f * M_PI / 180.0f);

      openmc::Position viewRay = {
          ndcX * aspectRatio * tanHalfFov,
          ndcY * tanHalfFov,
          -1.0  // Forward in view space is -Z
      };

      // Transform the ray direction from view space to world space
      openmc::Position forward = camera_.getTransformedLookAt() - camera_.getTransformedPosition();
      forward = forward / forward.norm();

      openmc::Position right = forward.cross(camera_.getTransformedUpVector());
      right = right / right.norm();

      openmc::Position up = right.cross(forward);
      up = up / up.norm();

      // Construct world space direction
      openmc::Direction worldDir = {
          right[0] * viewRay[0] + up[0] * viewRay[1] + forward[0] * viewRay[2],
          right[1] * viewRay[0] + up[1] * viewRay[1] + forward[1] * viewRay[2],
          right[2] * viewRay[0] + up[2] * viewRay[1] + forward[2] * viewRay[2]
      };

      // Normalize the direction
      float length = sqrt(worldDir[0] * worldDir[0] + worldDir[1] * worldDir[1] + worldDir[2] * worldDir[2]);
      worldDir = worldDir / length;

      return worldDir;
  }

  // Add this method to handle cursor movement and cell querying
  void handleCursorQuery(double xpos, double ypos) {
      // Skip if ImGui is handling this event
      ImGuiIO& io = ImGui::GetIO();
      if (!io.WantCaptureMouse) {
          openmc::Position rayOrigin = camera_.getTransformedPosition();
          openmc::Direction rayDir = screenToWorldDirection(xpos, ypos);

          try {
              int32_t cell_id = openmc_plotter_.query_cell(rayOrigin, rayDir);
              // For now, just print the cell ID - we can enhance this later
              std::cout << "Cell ID at cursor: " << cell_id << std::endl;
          } catch (const std::exception& e) {
              // Silently handle cases where we might be pointing outside the geometry
          }
      }
  }

  // Add help overlay state
  bool show_help_overlay = false;

  void renderHelpOverlay() {
      ImGuiIO& io = ImGui::GetIO();
      const ImGuiViewport* viewport = ImGui::GetMainViewport();

      // Set up full-screen overlay
      ImGui::SetNextWindowPos(viewport->Pos);
      ImGui::SetNextWindowSize(viewport->Size);

      ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoSavedSettings;

      // Create semi-transparent dark overlay
      ImGui::SetNextWindowBgAlpha(0.85f);

      if (ImGui::Begin("Help Overlay", &show_help_overlay, window_flags)) {
          ImGui::PushFont(io.Fonts->Fonts[0]);

          ImGui::Text("OpenMC Renderer Controls");
          ImGui::Separator();

          ImGui::Text("Camera Controls:");
          ImGui::BulletText("Left Mouse Button + Drag: Rotate camera");
          ImGui::BulletText("Middle Mouse Button + Drag: Pan camera");
          ImGui::BulletText("Mouse Wheel: Zoom in/out");

          ImGui::Spacing();
          ImGui::Text("Light Controls:");
          ImGui::BulletText("Hold L + Left Mouse Button: Rotate light around model");
          ImGui::BulletText("Hold L + Middle Mouse Button: Move light closer/further");

          ImGui::Spacing();
          ImGui::Text("View Shortcuts:");
          ImGui::BulletText("I: Reset to isometric view");
          ImGui::BulletText("X: View along X axis (positive direction)");
          ImGui::BulletText("Y: View along Y axis (positive direction)");
          ImGui::BulletText("Z: View along Z axis (positive direction)");
          ImGui::BulletText("Shift + X: View along X axis (negative direction)");
          ImGui::BulletText("Shift + Y: View along Y axis (negative direction)");
          ImGui::BulletText("Shift + Z: View along Z axis (negative direction)");

          ImGui::Spacing();
          ImGui::Text("General Controls:");
          ImGui::BulletText("?: Toggle this help overlay");
          ImGui::BulletText("Esc: Close overlay or exit application");
          ImGui::BulletText("Ctrl + W/Q: Exit application");

          ImGui::Spacing();
          ImGui::Text("Press ESC or click anywhere to close this overlay");

          ImGui::PopFont();

          // Close overlay if clicked outside or Esc pressed
          if (ImGui::IsMouseClicked(0) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
              show_help_overlay = false;
          }
      }
      ImGui::End();
  }
};



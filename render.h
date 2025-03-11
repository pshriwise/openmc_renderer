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
        // Reset rotation
        rotation = Quaternion();

        // Set camera to isometric position (equal angles to all axes)
        float distance = 15.0f;  // Distance from origin
        float angle = 54.736f;   // arctan(sqrt(2)) in degrees - standard isometric angle
        float phi = 45.0f;       // Azimuthal angle

        // Convert spherical coordinates to Cartesian
        float theta = angle * M_PI / 180.0f;  // Convert to radians
        float phiRad = phi * M_PI / 180.0f;

        position = {
            distance * sin(theta) * cos(phiRad),
            distance * sin(theta) * sin(phiRad),
            distance * cos(theta)
        };

        // Reset other parameters
        lookAt = {0.0, 0.0, 0.0};
        upVector = {0.0, 0.0, 1.0};
        zoom = -5.0f;
        panX = 0.0f;
        panY = 0.0f;

        updateVectors();
    }

private:
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
};

class OpenMCRenderer {

public:
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
    transferCameraInfo();
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

        // Retrieve color map and display the legend
        displayColorLegend();
        displaySettings();  // Add settings window

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
    // Skip if ImGui is handling this event
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
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
    // Skip if ImGui is handling this event
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        return;
    }

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


  static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto renderer = static_cast<OpenMCRenderer*>(glfwGetWindowUserPointer(window));
    if (!renderer) {
      throw std::runtime_error("Failed to get renderer from window user pointer");
    }
    renderer->scrollUpdate(xoffset, yoffset);
  }

  void scrollUpdate(double xoffset, double yoffset) {
      // Skip if ImGui is handling this event
      ImGuiIO& io = ImGui::GetIO();
      if (io.WantCaptureMouse) {
          return;
      }

      camera_.zoom += yoffset * camera_.zoomSensitivity;  // Apply zoom sensitivity
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

  // void displayColorLegend(const std::unordered_map<int32_t, openmc::RGBColor>& colorMap, VisibilityCallback onVisibilityChange = nullptr) {
  void displayColorLegend() {
    const auto colorMap = openmc_plotter_.color_map();

    static int selected_material = -1; // Track which material's color is being edited
    static openmc::RGBColor temp_color = {0, 0, 0}; // Temporary color for editing

    ImGui::Begin("Color Legend");
    ImGui::Text("Legend:"); // Title of the legend

    for (const auto& [material_id, color] : colorMap) {
        // Ensure material ID has a visibility entry
        if (materialVisibility.find(material_id) == materialVisibility.end()) {
            materialVisibility[material_id] = true; // Default visibility is true
        }

        // Convert color values from [0-255] to [0-1] for ImGui
        float r = color.red / 255.0f;
        float g = color.green / 255.0f;
        float b = color.blue / 255.0f;

        // Draw a color square
        if (ImGui::ColorButton(("##Color" + std::to_string(material_id)).c_str(), ImVec4(r, g, b, 1.0f))) {
            // Open the color picker when the color is clicked
            selected_material = material_id;
            temp_color = color;
            ImGui::OpenPopup("Edit Color");
        }
        ImGui::SameLine();

        ImGui::Text("Material ID: %d", material_id);

      // Add a visibility checkbox with a callback
        ImGui::SameLine();
        bool visibility = materialVisibility[material_id];
        if (ImGui::Checkbox(("Visible##" + std::to_string(material_id)).c_str(), &visibility)) {
            // Update visibility in the map
            materialVisibility[material_id] = visibility;
        }
    }

    // Color editing modal dialog
    if (ImGui::BeginPopupModal("Edit Color", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Show color picker
        float temp_r = temp_color.red / 255.0f;
        float temp_g = temp_color.green / 255.0f;
        float temp_b = temp_color.blue / 255.0f;

        if (ImGui::ColorEdit3("Material Color", &temp_r)) {
            // Update temporary color as user interacts
            temp_color.red = static_cast<uint8_t>(temp_r * 255);
            temp_color.green = static_cast<uint8_t>(temp_g * 255);
            temp_color.blue = static_cast<uint8_t>(temp_b * 255);
        }

        if (ImGui::Button("Save")) {
            // Save the new color to the color map
            std::cout << "Saving color for material " << selected_material << std::endl;
            openmc_plotter_.set_color(selected_material, temp_color);
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

      ImGui::End();
  }

  void updateVisibleMaterials() {
      for (const auto& [material_id, visibility] : materialVisibility) {
          openmc_plotter_.set_material_visibility(material_id, visibility);
      }
  }

  // Add key callback
  static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_W && action == GLFW_PRESS && (mods & GLFW_MOD_CONTROL)) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    // Handle isometric view
    if (key == GLFW_KEY_I && action == GLFW_PRESS) {
        auto renderer = static_cast<OpenMCRenderer*>(glfwGetWindowUserPointer(window));
        if (renderer) {
            renderer->camera_.setIsometricView();
            renderer->transferCameraInfo();
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
        // Pan sensitivity slider (0.001 to 0.1)
        float panSens = camera_.panSensitivity;
        if (ImGui::SliderFloat("Pan Sensitivity", &panSens, 0.001f, 0.1f, "%.3f")) {
            camera_.panSensitivity = panSens;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Adjusts the speed of camera panning");
        }

        // Zoom sensitivity slider (0.5 to 5.0)
        float zoomSens = camera_.zoomSensitivity;
        if (ImGui::SliderFloat("Zoom Sensitivity", &zoomSens, 0.5f, 5.0f, "%.1f")) {
            camera_.zoomSensitivity = zoomSens;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Adjusts the speed of camera zooming");
        }

        // Rotation sensitivity slider (0.1 to 2.0)
        float rotSens = camera_.rotationSensitivity;
        if (ImGui::SliderFloat("Rotation Sensitivity", &rotSens, 0.1f, 2.0f, "%.2f")) {
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

  // A map to track visibility state for each material
  std::unordered_map<int32_t, bool> materialVisibility;

  GLFWwindow* window_ {nullptr};
  GLuint texture_;

  OpenMCPlotter& openmc_plotter_ {OpenMCPlotter::get_instance()};
  Camera camera_;
};



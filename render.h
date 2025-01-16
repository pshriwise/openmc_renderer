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
    float rotationX;
    float rotationY;
    float zoom;
    float panX;
    float panY;

    // New fields
    double fov;
    openmc::Position position;
    openmc::Position lookAt;
    openmc::Position upVector;
    openmc::Position lightPosition {0, 10 , -10};

    Camera()
        : rotationX(0.0f), rotationY(0.0f), zoom(-5.0f), panX(0.0f), panY(0.0f),
          fov(45.0)  {
            position = {10, 10, 10};
            lookAt = {0.0, 0.0, 0.0};
            upVector = {0.0, 0.0, 1.0};
          }

    void applyTransformations() {
        glLoadIdentity();

        // Adjust position and lookAt with panning
        openmc::Position adjustedPosition = position;
        openmc::Position adjustedLookAt = lookAt;

        applyPanAndRotation(adjustedPosition);
        applyPanAndRotation(adjustedLookAt);

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
        openmc::Position transformedPosition = position + (lookAt - position) * zoom;
        applyPanAndRotation(transformedPosition);
        return transformedPosition;
    }

    openmc::Position getTransformedLookAt() const {
        openmc::Position transformedLookAt = lookAt;
        applyPanAndRotation(transformedLookAt);
        return transformedLookAt;
    }

    openmc::Position getTransformedUpVector() const {
        openmc::Position transformedUp = upVector;
        applyPanAndRotation(transformedUp);
        return transformedUp;
    }

private:
    void applyPanAndRotation(openmc::Position& vec) const {
        // Calculate the forward vector (camera direction)
        openmc::Position forward = lookAt - position;
        forward = forward / forward.norm(); // Normalize

        // Calculate the right vector (cross product of forward and up)
        openmc::Position right = forward.cross(upVector);
        right = right / right.norm(); // Normalize

        // Calculate the true up vector (cross product of right and forward)
        openmc::Position trueUp = right.cross(forward);

        // Apply pan in the view plane
        vec = vec + panY * right + panX * trueUp;

        // Apply rotation (yaw and pitch)
        double cosY = std::cos(rotationY * M_PI / 180.0);
        double sinY = std::sin(rotationY * M_PI / 180.0);
        double cosX = std::cos(rotationX * M_PI / 180.0);
        double sinX = std::sin(rotationX * M_PI / 180.0);

        double x = vec[0] * cosY - vec[2] * sinY;
        double z = vec[0] * sinY + vec[2] * cosY;
        vec[0] = x;
        vec[2] = z;

        double y = vec[1] * cosX - vec[2] * sinX;
        z = vec[1] * sinX + vec[2] * cosX;
        vec[1] = y;
        vec[2] = z;
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
    glfwSetWindowUserPointer(window_, this);


    // Create ImGui context
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 460"); // Replace with your OpenGL version

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

        // Update the texture with new image data if the camera has changed
        auto newImageData = openmc_plotter_.create_image(); // Assume this generates a new image based on the camera
        updateTexture(newImageData);

        // Draw the background
        drawBackground();

        glfwPollEvents();

        //  // Render Dear ImGui
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
      if (draggingLeft) {
          camera_.rotationX -= (ypos - lastMouseY) * 0.5f;
          camera_.rotationY -= (xpos - lastMouseX) * 0.5f;
          lastMouseX = xpos;
          lastMouseY = ypos;
      }
      if (draggingMiddle) {
          camera_.panX += (xpos - lastMouseX) * 0.01f;
          camera_.panY -= (ypos - lastMouseY) * 0.01f;
          lastMouseX = xpos;
          lastMouseY = ypos;
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
      camera_.zoom += yoffset;
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

  void transferCameraInfo() {
      openmc_plotter_.set_camera_position(camera_.getTransformedPosition());
      openmc_plotter_.set_look_at(camera_.getTransformedLookAt());
      openmc_plotter_.set_light_position(camera_.lightPosition);
      openmc_plotter_.set_field_of_view(camera_.fov);
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



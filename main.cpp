#include <iostream>
#include "plotter.h"
#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include <vector>
#include <xtensor/xarray.hpp>
#include <xtensor/xio.hpp>
#include <xtensor/xrandom.hpp>
#include <xtensor/xview.hpp>

// Camera class to manage view parameters
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

    Camera()
        : rotationX(0.0f), rotationY(0.0f), zoom(-5.0f), panX(0.0f), panY(0.0f),
          fov(45.0)  {
            position = {10, 10, 10};
            lookAt = {0.0, 0.0, 0.0};
            upVector = {0.0, 0.0, 1.0};

          }

    void applyTransformations() const {
        glLoadIdentity();
        gluLookAt(position[0], position[1], position[2],
                  lookAt[0], lookAt[1], lookAt[2],
                  upVector[0], upVector[1], upVector[2]);
        glTranslatef(panX, panY, zoom);
        glRotatef(rotationX, 1.0f, 0.0f, 0.0f);
        glRotatef(rotationY, 0.0f, 1.0f, 0.0f);
    }

    void updateView(int width, int height) {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(fov, static_cast<double>(width) / height, 1.0, 500.0);
        glMatrixMode(GL_MODELVIEW);
    }

openmc::Position getTransformedPosition() const {
        openmc::Position transformedPosition = position;
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
        // Apply zoom
        vec[2] += zoom;

        // Apply pan
        vec[0] += panX;
        vec[1] += panY;

        // Apply rotation around Y-axis (yaw)
        double cosY = std::cos(rotationY * M_PI / 180.0);
        double sinY = std::sin(rotationY * M_PI / 180.0);
        double x = vec[0] * cosY - vec[2] * sinY;
        double z = vec[0] * sinY + vec[2] * cosY;
        vec[0] = x;
        vec[2] = z;

        // Apply rotation around X-axis (pitch)
        double cosX = std::cos(rotationX * M_PI / 180.0);
        double sinX = std::sin(rotationX * M_PI / 180.0);
        double y = vec[1] * cosX - vec[2] * sinX;
        z = vec[1] * sinX + vec[2] * cosX;
        vec[1] = y;
        vec[2] = z;
    }};

// Global camera instance
Camera camera;
bool draggingLeft = false;
bool draggingMiddle = false;
double lastMouseX, lastMouseY;

// Function to update the texture with new image data
void updateTexture(GLuint texture, const openmc::ImageData& imageData) {
    auto& transposedData = imageData;
    // auto transposedData = xt::transpose(imageData, {1, 0});

    int width = transposedData.shape()[0];
    int height = transposedData.shape()[1];

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, transposedData.data());
}

// Mouse button callback
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            draggingLeft = true;
            glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
        } else if (action == GLFW_RELEASE) {
            draggingLeft = false;
        }
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        if (action == GLFW_PRESS) {
            draggingMiddle = true;
            glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
        } else if (action == GLFW_RELEASE) {
            draggingMiddle = false;
        }
    }
}

// Mouse motion callback
void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
    if (draggingLeft) {
        camera.rotationX += (ypos - lastMouseY) * 0.5f;
        camera.rotationY += (xpos - lastMouseX) * 0.5f;
        lastMouseX = xpos;
        lastMouseY = ypos;
    }
    if (draggingMiddle) {
        camera.panX += (xpos - lastMouseX) * 0.01f;
        camera.panY -= (ypos - lastMouseY) * 0.01f;
        lastMouseX = xpos;
        lastMouseY = ypos;
    }
}

// Scroll callback
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.zoom += yoffset;
}

// Framebuffer size callback to adjust the viewport
void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    camera.updateView(width, height);
}

// Function to create a texture from ImageData
GLuint createTextureFromImageData(const openmc::ImageData& imageData) {
    auto& transposedData = imageData;
    // auto transposedData = xt::transpose(imageData, {1, 0});

    int width = transposedData.shape()[0];
    int height = transposedData.shape()[1];

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, transposedData.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return texture;
}

// Function to draw the background
void drawBackground(GLuint texture) {
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 1, 0, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);

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

void transferCameraInfo(OpenMCPlotter& plotter, const Camera& camera) {
    plotter.set_camera_position(camera.getTransformedPosition());
    plotter.set_look_at(camera.getTransformedLookAt());
    plotter.set_up_vector(camera.getTransformedUpVector());
    plotter.set_field_of_view(camera.fov);
}

int main(int argc, char* argv[]) {
    auto openmc_plotter = OpenMCPlotter(argc, argv);
    auto image = openmc_plotter.create_image();

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(800, 600, "OpenMC Geometry", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    glEnable(GL_DEPTH_TEST);

    // Set initial viewport and projection
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    framebufferSizeCallback(window, width, height);

    // Create the background texture from ImageData
    GLuint backgroundTexture = createTextureFromImageData(image);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        camera.applyTransformations();
        transferCameraInfo(openmc_plotter, camera);

        // Update the texture with new image data if the camera has changed
        auto newImageData = openmc_plotter.create_image(); // Assume this generates a new image based on the camera
        updateTexture(backgroundTexture, newImageData);

        // Draw the background
        drawBackground(backgroundTexture);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteTextures(1, &backgroundTexture);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
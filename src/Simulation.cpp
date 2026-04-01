#include "Simulation.hpp"
#include "BlackHoleScene.hpp"
#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
constexpr float kMinPitch = -89.0f;
constexpr float kMaxPitch = 89.0f;
constexpr float kMinOrbitDistance = 2.0f;
constexpr float kMaxOrbitDistance = 500.0f;
}

Simulation::Simulation() : renderer(1280, 720) {
    if (!renderer.init()) {
        std::cerr << "Failed to initialize renderer.\n";
        exit(-1);
    }
    renderer.setBlackHoleGridEnabled(false);
    switchScene(0);
}

Simulation::~Simulation() {
    exporter.close();
}

glm::vec3 Simulation::getOrbitTarget() const {
    if (!currentScene) {
        return glm::vec3(0.0f);
    }

    BlackHoleRenderParams params;
    if (currentScene->getBlackHoleRenderParams(params)) {
        return params.position;
    }

    return glm::vec3(0.0f);
}

void Simulation::syncOrbitDistanceFromCamera() {
    orbitCamera.target = getOrbitTarget();
    // Use camera->target direction so yaw/pitch stay consistent with free-look forward.
    glm::vec3 toTarget = orbitCamera.target - cameraPos;
    float offsetLen = glm::length(toTarget);
    if (offsetLen < 1e-4f) {
        offsetLen = orbitCamera.distance;
    }

    orbitCamera.distance = std::clamp(offsetLen, kMinOrbitDistance, kMaxOrbitDistance);
    glm::vec3 dir = toTarget / orbitCamera.distance; // camera -> target

    yaw = glm::degrees(std::atan2(dir.z, dir.x));
    pitch = glm::degrees(std::asin(std::clamp(dir.y, -1.0f, 1.0f)));
    pitch = std::clamp(pitch, kMinPitch, kMaxPitch);
}

void Simulation::updateOrbitCameraPosition() {
    orbitCamera.target = getOrbitTarget();
    orbitCamera.distance = std::clamp(orbitCamera.distance, kMinOrbitDistance, kMaxOrbitDistance);

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(front);

    // In orbit mode, front points from camera to target.
    cameraPos = orbitCamera.target - front * orbitCamera.distance;
}

void Simulation::switchScene(int index) {
    sceneIndex = index;
    switch (sceneIndex) {
        case 0:
            currentScene = std::make_unique<BlackHoleScene>(200, true);
            break;
        default:
            std::cerr << "Unknown scene index " << sceneIndex << ", falling back to scene 0.\n";
            sceneIndex = 0;
            currentScene = std::make_unique<BlackHoleScene>(200, true);
            break;
    }
    currentScene->init();
    
    // Black Hole: elevated, farther default view to frame the full disk.
    cameraPos = glm::vec3(0.0f, 15.0f, 45.0f);
    pitch = -25.0f;
    yaw = -90.0f;
    orbitCamera.enabled = false;
    orbitCamera.target = getOrbitTarget();
    syncOrbitDistanceFromCamera();
    
    time = 0.0;
    accumulator = 0.0;
    firstMouse = true;
    
    std::cout << "Loaded Scene: " << currentScene->getName() << std::endl;
}

void Simulation::processInput(double frameDt) {
    GLFWwindow* window = renderer.window;
    
    // Exit
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    // Camera Direction based on pitch/yaw
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(front);

    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::normalize(glm::cross(right, front));

    float cameraSpeed = 20.0f * static_cast<float>(frameDt);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) cameraSpeed *= 3.0f;

    // Toggle orbit camera mode (debounced)
    static bool pPressed = false;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (!pPressed) {
            pPressed = true;
            orbitCamera.enabled = !orbitCamera.enabled;
            firstMouse = true;
            if (orbitCamera.enabled) {
                syncOrbitDistanceFromCamera();
                updateOrbitCameraPosition();
                std::cout << "Orbit Camera: ON (anchored to singularity)\n";
            } else {
                std::cout << "Orbit Camera: OFF (free fly)\n";
            }
        }
    } else {
        pPressed = false;
    }

    // Movement / zoom
    if (orbitCamera.enabled) {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            orbitCamera.distance -= cameraSpeed * 2.5f;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            orbitCamera.distance += cameraSpeed * 2.5f;
        }
        orbitCamera.distance = std::clamp(orbitCamera.distance, kMinOrbitDistance, kMaxOrbitDistance);
        updateOrbitCameraPosition();
    } else {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            cameraPos += cameraSpeed * front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            cameraPos -= cameraSpeed * front;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            cameraPos -= cameraSpeed * right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            cameraPos += cameraSpeed * right;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            cameraPos += cameraSpeed * up;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            cameraPos -= cameraSpeed * up;
    }

    // Mouse Look (Right-click drag)
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        
        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = static_cast<float>(xpos - lastX);
        float yoffset = static_cast<float>(lastY - ypos); // reversed since y-coordinates range from bottom to top
        lastX = xpos;
        lastY = ypos;

        float sensitivity = 0.2f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        yaw += xoffset;
        pitch += yoffset;

        if (pitch > kMaxPitch) pitch = kMaxPitch;
        if (pitch < kMinPitch) pitch = kMinPitch;

        if (orbitCamera.enabled) {
            updateOrbitCameraPosition();
        }
    } else {
        firstMouse = true;
    }

    // Reset view
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        cameraPos = glm::vec3(0.0f, 15.0f, 45.0f);
        pitch = -25.0f;
        yaw = -90.0f;
        syncOrbitDistanceFromCamera();
        if (orbitCamera.enabled) {
            updateOrbitCameraPosition();
        }
    }

    // Toggle pause (debounced)
    static bool spacePressed = false;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if (!spacePressed) {
            paused = !paused;
            spacePressed = true;
            std::cout << (paused ? "Paused" : "Resumed") << "\n";
        }
    } else {
        spacePressed = false;
    }

    // (Scene switching controls removed)

    // Data Export toggle
    static bool mPressed = false;
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) {
        if (!mPressed) {
            mPressed = true;
            static bool exporting = false;
            if (!exporting) {
                exporter.open("export.csv");
                exporting = true;
                std::cout << "Data Export Started: export.csv\n";
            } else {
                exporter.close();
                exporting = false;
                std::cout << "Data Export Stopped.\n";
            }
        }
    } else mPressed = false;

    // Toggle BH gravity grid overlay
    static bool gPressed = false;
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) {
        if (!gPressed) {
            gPressed = true;
            bool enabled = !renderer.isBlackHoleGridEnabled();
            renderer.setBlackHoleGridEnabled(enabled);
            std::cout << "Black-Hole Grid Overlay: " << (enabled ? "ON" : "OFF") << "\n";
        }
    } else {
        gPressed = false;
    }

    // Toggle VSync
    static bool vPressed = false;
    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
        if (!vPressed) {
            vPressed = true;
            bool enabled = !renderer.isVsyncEnabled();
            renderer.setVsyncEnabled(enabled);
            std::cout << "VSync: " << (enabled ? "ON" : "OFF") << "\n";
        }
    } else {
        vPressed = false;
    }
}

void Simulation::run() {
    std::cout << "Simulation Started!\n";
    std::cout << "Controls:\n";
    std::cout << " P: Toggle Orbit Camera (anchor to singularity)\n";
    std::cout << " Space: Pause/Resume\n";
    std::cout << " R: Reset Camera\n";
    std::cout << " WASD/QE: Fly Camera (free mode)\n";
    std::cout << " W/S or Up/Down: Zoom (orbit mode)\n";
    std::cout << " Right-Click Drag: Rotate View\n";
    std::cout << " G: Toggle BH Gravity Grid Overlay\n";
    std::cout << " V: Toggle VSync\n";
    std::cout << " M: Toggle Data Export to CSV\n";

    double lastTime = glfwGetTime();
    const char* baseWindowTitle = "C++ Gravitational Simulator";
    double fpsTimeAccum = 0.0;
    int fpsFrameCount = 0;

    while (!glfwWindowShouldClose(renderer.window)) {
        double currentTime = glfwGetTime();
        double frameTime = currentTime - lastTime;
        lastTime = currentTime;
        frameTime = std::clamp(frameTime, 0.0, maxFrameTime);

        fpsTimeAccum += frameTime;
        ++fpsFrameCount;
        if (fpsTimeAccum >= 0.5) {
            double fps = static_cast<double>(fpsFrameCount) / fpsTimeAccum;
            std::ostringstream title;
            title << baseWindowTitle << " | FPS: " << std::fixed << std::setprecision(1) << fps;
            std::string titleText = title.str();
            glfwSetWindowTitle(renderer.window, titleText.c_str());
            fpsTimeAccum = 0.0;
            fpsFrameCount = 0;
        }

        processInput(frameTime);

        if (orbitCamera.enabled) {
            updateOrbitCameraPosition();
        }

        if (!paused) {
            accumulator += frameTime;
            int substeps = 0;
            while (accumulator >= dt && substeps < maxSubstepsPerFrame) {
                currentScene->update(dt);
                time += dt;
                exporter.logState(time, currentScene->getBodies());
                accumulator -= dt;
                ++substeps;
            }

            if (substeps == maxSubstepsPerFrame && accumulator > dt) {
                // Drop excess accumulated time to avoid spiral-of-death under heavy load.
                accumulator = 0.0;
            }
        }

        // Setup matrices
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(front);
        
        glm::mat4 view = orbitCamera.enabled
            ? glm::lookAt(cameraPos, orbitCamera.target, glm::vec3(0.0f, 1.0f, 0.0f))
            : glm::lookAt(cameraPos, cameraPos + front, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)renderer.width / (float)renderer.height, 0.1f, 1000.0f);

        renderer.render(currentScene.get(), view, projection);

        glfwPollEvents();
    }
}

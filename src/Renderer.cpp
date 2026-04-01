#include "Renderer.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <sstream>

namespace {

std::string readFile(const std::string &path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "ERROR: Failed to open shader file: " << path << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool linkProgram(GLuint program, const char* label) {
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success) {
        return true;
    }

    char infoLog[1024];
    glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
    std::cerr << "ERROR::PROGRAM::LINK_FAILED (" << label << ")\n" << infoLog << std::endl;
    return false;
}

} // namespace

Renderer::Renderer(int width, int height)
    : window(nullptr), width(width), height(height),
      shaderProgram(0), computeProgram(0), quadProgram(0),
      renderTexture(0), quadVAO(0), quadVBO(0),
      cameraUBO(0), diskUBO(0), VAO(0), VBO(0),
      pointVAO(0), pointVBO(0), gridVAO(0), gridVBO(0),
      gridVertexCount(0), blackHoleGridEnabled(false), vsyncEnabled(false) {}

Renderer::~Renderer() {
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

bool Renderer::init() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, "C++ Gravitational Simulator", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    setVsyncEnabled(false); // Default to uncapped framerate.

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    if (!setupShaders()) {
        return false;
    }
    setupBuffers();
    if (!setupQuad()) {
        return false;
    }
    if (!setupComputeShader()) {
        return false;
    }
    if (!setupGravityWellGrid()) {
        std::cerr << "WARNING: Gravity grid overlay disabled (failed to initialize grid shaders)." << std::endl;
        blackHoleGridEnabled = false;
    }

    return true;
}

void Renderer::setBlackHoleGridEnabled(bool enabled) {
    blackHoleGridEnabled = enabled;
}

bool Renderer::isBlackHoleGridEnabled() const {
    return blackHoleGridEnabled;
}

void Renderer::setVsyncEnabled(bool enabled) {
    vsyncEnabled = enabled;
    if (window != nullptr) {
        glfwSwapInterval(vsyncEnabled ? 1 : 0);
    }
}

bool Renderer::isVsyncEnabled() const {
    return vsyncEnabled;
}

GLuint Renderer::compileShader(GLenum type, const char *source) {
    if (source == nullptr || source[0] == '\0') {
        std::cerr << "ERROR::SHADER::EMPTY_SOURCE" << std::endl;
        return 0;
    }

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool Renderer::setupShaders() {
    std::string vertCode = readFile("shaders/basic.vert");
    std::string fragCode = readFile("shaders/basic.frag");
    if (vertCode.empty() || fragCode.empty()) {
        return false;
    }

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertCode.c_str());
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragCode.c_str());
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader != 0) {
            glDeleteShader(vertexShader);
        }
        if (fragmentShader != 0) {
            glDeleteShader(fragmentShader);
        }
        return false;
    }

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    bool linked = linkProgram(shaderProgram, "basic");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (!linked) {
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
        return false;
    }

    return true;
}

bool Renderer::setupQuad() {
    float quadVertices[] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    std::string vertCode = readFile("shaders/quad.vert");
    std::string fragCode = readFile("shaders/quad.frag");
    if (vertCode.empty() || fragCode.empty()) {
        return false;
    }

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertCode.c_str());
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragCode.c_str());
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader != 0) {
            glDeleteShader(vertexShader);
        }
        if (fragmentShader != 0) {
            glDeleteShader(fragmentShader);
        }
        return false;
    }

    quadProgram = glCreateProgram();
    glAttachShader(quadProgram, vertexShader);
    glAttachShader(quadProgram, fragmentShader);
    bool linked = linkProgram(quadProgram, "quad");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (!linked) {
        glDeleteProgram(quadProgram);
        quadProgram = 0;
        return false;
    }

    return true;
}

bool Renderer::setupComputeShader() {
    std::string compCode = readFile("shaders/geodesic.comp");
    if (compCode.empty()) {
        return false;
    }

    GLuint compShader = compileShader(GL_COMPUTE_SHADER, compCode.c_str());
    if (compShader == 0) {
        return false;
    }

    computeProgram = glCreateProgram();
    glAttachShader(computeProgram, compShader);
    bool linked = linkProgram(computeProgram, "compute");
    glDeleteShader(compShader);

    if (!linked) {
        glDeleteProgram(computeProgram);
        computeProgram = 0;
        return false;
    }

    glGenTextures(1, &renderTexture);
    glBindTexture(GL_TEXTURE_2D, renderTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenBuffers(1, &cameraUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
    glBufferData(GL_UNIFORM_BUFFER, 128, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, cameraUBO);

    glGenBuffers(1, &diskUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, diskUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(float) * 4, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, diskUBO);

    return true;
}

void Renderer::updateComputeUBOs(Scene *scene, const glm::mat4 &view,
                                 const glm::mat4 &projection) {
    (void)projection;

    glm::mat4 invView = glm::inverse(view);
    glm::vec3 camPos = glm::vec3(invView[3]);
    glm::vec3 camRight = glm::vec3(view[0][0], view[1][0], view[2][0]);
    glm::vec3 camUp = glm::vec3(view[0][1], view[1][1], view[2][1]);
    glm::vec3 camForward = -glm::vec3(view[0][2], view[1][2], view[2][2]);

    struct CameraUBOData {
        glm::vec3 pos;
        float _pad0;
        glm::vec3 right;
        float _pad1;
        glm::vec3 up;
        float _pad2;
        glm::vec3 forward;
        float _pad3;
        float tanHalfFov;
        float aspect;
        float time;
        float _pad4;
    } camData;

    camData.pos = camPos;
    camData.right = camRight;
    camData.up = camUp;
    camData.forward = camForward;
    camData.tanHalfFov = tan(glm::radians(45.0f * 0.5f));
    camData.aspect = static_cast<float>(width) / static_cast<float>(height);
    camData.time = static_cast<float>(glfwGetTime());

    glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraUBOData), &camData);

    BlackHoleRenderParams params;
    if (!scene->getBlackHoleRenderParams(params)) {
        params = BlackHoleRenderParams{};
    }

    float diskData[4] = {
        params.diskInnerRadius,
        params.diskOuterRadius,
        std::max(params.schwarzschildRadius, 0.001f),
        params.diskThickness
    };

    glBindBuffer(GL_UNIFORM_BUFFER, diskUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(diskData), diskData);
}

void Renderer::buildGrid() {
    float size = 100.0f;
    float step = 5.0f;
    for (float i = -size; i <= size; i += step) {
        gridVertices.push_back(i);
        gridVertices.push_back(0.0f);
        gridVertices.push_back(-size);
        gridVertices.push_back(i);
        gridVertices.push_back(0.0f);
        gridVertices.push_back(size);

        gridVertices.push_back(-size);
        gridVertices.push_back(0.0f);
        gridVertices.push_back(i);
        gridVertices.push_back(size);
        gridVertices.push_back(0.0f);
        gridVertices.push_back(i);
    }
    gridVertexCount = static_cast<int>(gridVertices.size() / 3);
}

void Renderer::setupBuffers() {
    float originPoint[] = {0.0f, 0.0f, 0.0f};
    glGenVertexArrays(1, &pointVAO);
    glGenBuffers(1, &pointVBO);
    glBindVertexArray(pointVAO);
    glBindBuffer(GL_ARRAY_BUFFER, pointVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(originPoint), originPoint, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    buildGrid();
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float),
                 gridVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
}

bool Renderer::setupGravityWellGrid() {
    std::string vertCode = readFile("shaders/grid.vert");
    std::string fragCode = readFile("shaders/grid.frag");
    if (vertCode.empty() || fragCode.empty()) {
        return false;
    }

    GLuint vs = compileShader(GL_VERTEX_SHADER, vertCode.c_str());
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragCode.c_str());
    if (vs == 0 || fs == 0) {
        if (vs != 0) {
            glDeleteShader(vs);
        }
        if (fs != 0) {
            glDeleteShader(fs);
        }
        return false;
    }

    gwGridProgram = glCreateProgram();
    glAttachShader(gwGridProgram, vs);
    glAttachShader(gwGridProgram, fs);
    bool linked = linkProgram(gwGridProgram, "gravity-well-grid");
    glDeleteShader(vs);
    glDeleteShader(fs);

    if (!linked) {
        glDeleteProgram(gwGridProgram);
        gwGridProgram = 0;
        return false;
    }

    glGenVertexArrays(1, &gwGridVAO);
    glGenBuffers(1, &gwGridVBO);
    glGenBuffers(1, &gwGridEBO);
    return true;
}

void Renderer::generateGravityWellGrid(Scene* scene) {
    const int gridSize = 40;
    const float spacing = 2.0f;

    BlackHoleRenderParams params;
    if (!scene->getBlackHoleRenderParams(params)) {
        params = BlackHoleRenderParams{};
    }

    const float rs = std::max(params.schwarzschildRadius, 0.001f);
    const glm::vec3 bhPos = params.position;

    std::vector<glm::vec3> vertices;
    std::vector<GLuint> indices;
    vertices.reserve((gridSize + 1) * (gridSize + 1));
    indices.reserve(gridSize * gridSize * 4);

    for (int z = 0; z <= gridSize; ++z) {
        for (int x = 0; x <= gridSize; ++x) {
            float worldX = (x - gridSize / 2.0f) * spacing;
            float worldZ = (z - gridSize / 2.0f) * spacing;
            float y = -10.0f;

            float dx = worldX - bhPos.x;
            float dz = worldZ - bhPos.z;
            float dist = std::sqrt(dx * dx + dz * dz);
            if (dist > rs) {
                float deltaY = 2.0f * std::sqrt(rs * (dist - rs));
                y = deltaY - 15.0f;
            } else {
                y = 2.0f * rs - 15.0f;
            }

            vertices.emplace_back(worldX, y, worldZ);
        }
    }

    for (int z = 0; z < gridSize; ++z) {
        for (int x = 0; x < gridSize; ++x) {
            int i = z * (gridSize + 1) + x;
            indices.push_back(i);
            indices.push_back(i + 1);
            indices.push_back(i);
            indices.push_back(i + gridSize + 1);
        }
    }

    gwGridIndexCount = static_cast<int>(indices.size());

    glBindVertexArray(gwGridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gwGridVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gwGridEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);
}

void Renderer::drawGravityWellGrid(const glm::mat4& view, const glm::mat4& projection) {
    glm::mat4 viewProj = projection * view;
    glUseProgram(gwGridProgram);
    glUniformMatrix4fv(glGetUniformLocation(gwGridProgram, "viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
    glBindVertexArray(gwGridVAO);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawElements(GL_LINES, gwGridIndexCount, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::drawTrailsAndBodies(
    Scene* scene,
    const glm::mat4& view,
    const glm::mat4& projection,
    bool isBlackHoleLensing,
    const BlackHoleRenderParams& blackHoleParams
) {
    glUseProgram(shaderProgram);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE,
                       glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1,
                       GL_FALSE, glm::value_ptr(projection));

    const float pointSizeScale = isBlackHoleLensing ? 260.0f : 800.0f;
    const float pointMinSize = isBlackHoleLensing ? 0.75f : 2.0f;
    glUniform1f(glGetUniformLocation(shaderProgram, "pointSizeScale"), pointSizeScale);
    glUniform1f(glGetUniformLocation(shaderProgram, "pointMinSize"), pointMinSize);

    glm::vec2 lensCenterNDC(0.0f, 0.0f);
    if (isBlackHoleLensing) {
        glm::vec4 bhClip = projection * view * glm::vec4(blackHoleParams.position, 1.0f);
        if (std::abs(bhClip.w) > 1e-6f) {
            lensCenterNDC = glm::vec2(bhClip.x, bhClip.y) / bhClip.w;
        }
    }
    glUniform1f(glGetUniformLocation(shaderProgram, "lensingEnabled"), isBlackHoleLensing ? 1.0f : 0.0f);
    glUniform2f(glGetUniformLocation(shaderProgram, "lensCenterNDC"), lensCenterNDC.x, lensCenterNDC.y);
    glUniform1f(
        glGetUniformLocation(shaderProgram, "lensStrength"),
        isBlackHoleLensing ? (0.03f * std::max(blackHoleParams.schwarzschildRadius, 0.001f)) : 0.0f
    );
    glUniform1f(glGetUniformLocation(shaderProgram, "lensEpsilon"), isBlackHoleLensing ? 0.04f : 0.01f);

    glUniform2f(glGetUniformLocation(shaderProgram, "viewportSize"), static_cast<float>(width), static_cast<float>(height));
    glUniform1f(glGetUniformLocation(shaderProgram, "useLensingMask"), isBlackHoleLensing ? 1.0f : 0.0f);
    glUniform1i(glGetUniformLocation(shaderProgram, "lensingMaskTexture"), 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, renderTexture);
    glActiveTexture(GL_TEXTURE0);

    glBindVertexArray(VAO);
    std::vector<float> lineVertices;
    lineVertices.reserve(50 * 3);

    for (const auto &body : scene->getBodies()) {
        if (isBlackHoleLensing && body.type == BodyType::BlackHole) {
            continue;
        }
        if (body.history.size() < 2) {
            continue;
        }

        lineVertices.clear();
        for (const auto &pt : body.history) {
            lineVertices.push_back(static_cast<float>(pt.x));
            lineVertices.push_back(static_cast<float>(pt.y));
            lineVertices.push_back(static_cast<float>(pt.z));
        }

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(float),
                     lineVertices.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);

        glUniform3f(glGetUniformLocation(shaderProgram, "offset"), 0.0f, 0.0f, 0.0f);
        glUniform1f(glGetUniformLocation(shaderProgram, "scale"), 0.0f);
        glUniform1f(glGetUniformLocation(shaderProgram, "isPointObject"), 0.0f);
        float trailAlpha = isBlackHoleLensing ? 0.2f : 0.5f;
        glUniform4f(glGetUniformLocation(shaderProgram, "color"),
                    body.color.r * 0.5f, body.color.g * 0.5f, body.color.b * 0.5f, trailAlpha);

        glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(lineVertices.size() / 3));
    }

    glBindVertexArray(pointVAO);
    for (const auto &body : scene->getBodies()) {
        if (isBlackHoleLensing && body.type == BodyType::BlackHole) {
            // The compute pass already renders the hole and horizon.
            continue;
        }
        glUniform3f(glGetUniformLocation(shaderProgram, "offset"),
                    static_cast<float>(body.position.x),
                    static_cast<float>(body.position.y),
                    static_cast<float>(body.position.z));
        glUniform1f(glGetUniformLocation(shaderProgram, "scale"), static_cast<float>(body.radius));
        glUniform1f(glGetUniformLocation(shaderProgram, "isPointObject"), 1.0f);

        float particleAlpha = isBlackHoleLensing ? 0.7f : body.color.a;
        glUniform4f(glGetUniformLocation(shaderProgram, "color"),
                    body.color.r, body.color.g, body.color.b, particleAlpha);

        glDrawArrays(GL_POINTS, 0, 1);
    }
}

void Renderer::render(Scene *scene, const glm::mat4 &view,
                      const glm::mat4 &projection) {
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    const bool isBlackHoleLensing = (scene->getRenderMode() == SceneRenderMode::BlackHoleLensing);
    BlackHoleRenderParams blackHoleParams;
    if (!scene->getBlackHoleRenderParams(blackHoleParams)) {
        blackHoleParams = BlackHoleRenderParams{};
    }

    if (isBlackHoleLensing) {
        updateComputeUBOs(scene, view, projection);

        glUseProgram(computeProgram);
        glBindTexture(GL_TEXTURE_2D, renderTexture);

        static int texW = 0;
        static int texH = 0;
        if (texW != width || texH != height) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
            texW = width;
            texH = height;
        }

        glBindImageTexture(0, renderTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        GLuint groupsX = static_cast<GLuint>(std::ceil(width / 16.0f));
        GLuint groupsY = static_cast<GLuint>(std::ceil(height / 16.0f));
        glDispatchCompute(groupsX, groupsY, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (blackHoleGridEnabled && gwGridProgram != 0 && gwGridVAO != 0) {
            generateGravityWellGrid(scene);
            drawGravityWellGrid(view, projection);
        }

        glUseProgram(quadProgram);
        glBindVertexArray(quadVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderTexture);
        glUniform1i(glGetUniformLocation(quadProgram, "screenTexture"), 0);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    } else {
        glClearColor(0.01f, 0.01f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE,
                           glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1,
                           GL_FALSE, glm::value_ptr(projection));
        glUniform1f(glGetUniformLocation(shaderProgram, "pointSizeScale"), 800.0f);
        glUniform1f(glGetUniformLocation(shaderProgram, "pointMinSize"), 2.0f);
        glUniform1f(glGetUniformLocation(shaderProgram, "lensingEnabled"), 0.0f);
        glUniform2f(glGetUniformLocation(shaderProgram, "lensCenterNDC"), 0.0f, 0.0f);
        glUniform1f(glGetUniformLocation(shaderProgram, "lensStrength"), 0.0f);
        glUniform1f(glGetUniformLocation(shaderProgram, "lensEpsilon"), 0.01f);
        glUniform2f(glGetUniformLocation(shaderProgram, "viewportSize"), static_cast<float>(width), static_cast<float>(height));
        glUniform1f(glGetUniformLocation(shaderProgram, "useLensingMask"), 0.0f);
        glUniform1i(glGetUniformLocation(shaderProgram, "lensingMaskTexture"), 1);

        glBindVertexArray(gridVAO);
        glUniform3f(glGetUniformLocation(shaderProgram, "offset"), 0.0f, 0.0f, 0.0f);
        glUniform1f(glGetUniformLocation(shaderProgram, "scale"), 0.0f);
        glUniform1f(glGetUniformLocation(shaderProgram, "isPointObject"), 0.0f);
        glUniform4f(glGetUniformLocation(shaderProgram, "color"), 0.2f, 0.2f, 0.3f, 0.4f);
        glDrawArrays(GL_LINES, 0, gridVertexCount);
    }

    drawTrailsAndBodies(scene, view, projection, isBlackHoleLensing, blackHoleParams);

    glfwSwapBuffers(window);
}

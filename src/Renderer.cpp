#include "Renderer.hpp"
#include <cmath>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <sstream>

Renderer::Renderer(int width, int height)
    : window(nullptr), width(width), height(height) {}

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

  window = glfwCreateWindow(width, height, "C++ Gravitational Simulator",
                            nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return false;
  }

  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Failed to initialize GLAD" << std::endl;
    return false;
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // 3D necessities
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_PROGRAM_POINT_SIZE);

  setupShaders();
  setupBuffers();
  setupQuad();
  setupComputeShader();
  setupGravityWellGrid();

  return true;
}

std::string readFile(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open())
    return "";
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

GLuint Renderer::compileShader(GLenum type, const char *source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);

  int success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetShaderInfoLog(shader, 512, nullptr, infoLog);
    std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
  }
  return shader;
}

void Renderer::setupShaders() {
  std::string vertCode = readFile("shaders/basic.vert");
  std::string fragCode = readFile("shaders/basic.frag");

  GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertCode.c_str());
  GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragCode.c_str());

  shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
}

void Renderer::setupQuad() {
  float quadVertices[] = {// positions   // texCoords
                          -1.0f, 1.0f,  0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f,
                          1.0f,  -1.0f, 1.0f, 0.0f, -1.0f, 1.0f,  0.0f, 1.0f,
                          1.0f,  -1.0f, 1.0f, 0.0f, 1.0f,  1.0f,  1.0f, 1.0f};
  glGenVertexArrays(1, &quadVAO);
  glGenBuffers(1, &quadVBO);
  glBindVertexArray(quadVAO);
  glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices,
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);

  std::string vertCode = readFile("shaders/quad.vert");
  std::string fragCode = readFile("shaders/quad.frag");
  GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertCode.c_str());
  GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragCode.c_str());
  quadProgram = glCreateProgram();
  glAttachShader(quadProgram, vertexShader);
  glAttachShader(quadProgram, fragmentShader);
  glLinkProgram(quadProgram);
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
}

void Renderer::setupComputeShader() {
  std::string compCode = readFile("shaders/geodesic.comp");
  if (compCode.empty()) {
    std::cerr << "ERROR: Could not read shaders/geodesic.comp" << std::endl;
  }
  GLuint compShader = compileShader(GL_COMPUTE_SHADER, compCode.c_str());
  computeProgram = glCreateProgram();
  glAttachShader(computeProgram, compShader);
  glLinkProgram(computeProgram);
  
  // Check link status
  int linkSuccess;
  glGetProgramiv(computeProgram, GL_LINK_STATUS, &linkSuccess);
  if (!linkSuccess) {
    char infoLog[1024];
    glGetProgramInfoLog(computeProgram, 1024, nullptr, infoLog);
    std::cerr << "ERROR::PROGRAM::LINK_FAILED\n" << infoLog << std::endl;
  }
  glDeleteShader(compShader);

  glGenTextures(1, &renderTexture);
  glBindTexture(GL_TEXTURE_2D, renderTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Setup UBOs for Compute Shader
  glGenBuffers(1, &cameraUBO);
  glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
  glBufferData(GL_UNIFORM_BUFFER, 128, nullptr, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 1, cameraUBO);

  glGenBuffers(1, &diskUBO);
  glBindBuffer(GL_UNIFORM_BUFFER, diskUBO);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(float) * 4, nullptr, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 2, diskUBO);

  glGenBuffers(1, &objectsUBO);
  glBindBuffer(GL_UNIFORM_BUFFER, objectsUBO);
  // Must match std140 layout: int + 3 pad floats + 16*vec4 + 16*vec4 + 4*vec4
  GLsizeiptr objUBOSize = 16 + 16 * sizeof(glm::vec4) + 16 * sizeof(glm::vec4) + 4 * sizeof(glm::vec4);
  glBufferData(GL_UNIFORM_BUFFER, objUBOSize, nullptr, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 3, objectsUBO);
}

void Renderer::updateComputeUBOs(Scene *scene, const glm::mat4 &view,
                                 const glm::mat4 &projection) {
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

  // Match the 45 deg fov used in Simulation.cpp's projection matrix
  camData.pos = camPos;
  camData.right = camRight;
  camData.up = camUp;
  camData.forward = camForward;
  camData.tanHalfFov = tan(glm::radians(45.0f * 0.5f));
  camData.aspect = (float)width / (float)height;
  camData.time = (float)glfwGetTime();

  glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraUBOData), &camData);

  // Get Black hole and object data
  struct ObjectUBOData {
    int numObjects;
    float _pad[3];
    glm::vec4 posRadius[16];
    glm::vec4 color[16];
    glm::vec4 massVec[4]; // 16 masses packed as 4 vec4s for std140 compatibility
  } objData = {};

  objData.numObjects = std::min((int)scene->getBodies().size(), 16);
  int bhIndex = -1;
  for (int i = 0; i < objData.numObjects; ++i) {
    const auto &b = scene->getBodies()[i];
    objData.posRadius[i] = glm::vec4(b.position, b.radius);
    objData.color[i] = glm::vec4(b.color.r, b.color.g, b.color.b, b.color.a);
    objData.massVec[i / 4][i % 4] = (float)b.mass;
    if (b.type == BodyType::BlackHole)
      bhIndex = i;
  }

  glBindBuffer(GL_UNIFORM_BUFFER, objectsUBO);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ObjectUBOData), &objData);

  // Update Disk UBO based on normalized Black Hole properties
  if (bhIndex != -1) {
    // Disk inner/outer radius scaled to match the BlackHoleScene body orbits
    float diskData[4] = { 3.0f, 15.0f, 2.0f, 0.5f };
    glBindBuffer(GL_UNIFORM_BUFFER, diskUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(diskData), diskData);
  }
}

void Renderer::buildGrid() {
  float size = 100.0f;
  float step = 5.0f;
  for (float i = -size; i <= size; i += step) {
    // Line parallel to Z
    gridVertices.push_back(i);
    gridVertices.push_back(0.0f);
    gridVertices.push_back(-size);
    gridVertices.push_back(i);
    gridVertices.push_back(0.0f);
    gridVertices.push_back(size);
    // Line parallel to X
    gridVertices.push_back(-size);
    gridVertices.push_back(0.0f);
    gridVertices.push_back(i);
    gridVertices.push_back(size);
    gridVertices.push_back(0.0f);
    gridVertices.push_back(i);
  }
  gridVertexCount = gridVertices.size() / 3;
}

void Renderer::setupBuffers() {
  // 1. Point Buffer (Origin)
  float originPoint[] = {0.0f, 0.0f, 0.0f};
  glGenVertexArrays(1, &pointVAO);
  glGenBuffers(1, &pointVBO);
  glBindVertexArray(pointVAO);
  glBindBuffer(GL_ARRAY_BUFFER, pointVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(originPoint), originPoint,
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // 2. Line Buffer (for dynamic trails)
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  // 3. Grid Buffer
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

void Renderer::setupGravityWellGrid() {
  std::string vertCode = readFile("shaders/grid.vert");
  std::string fragCode = readFile("shaders/grid.frag");
  
  GLuint vs = compileShader(GL_VERTEX_SHADER, vertCode.c_str());
  GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragCode.c_str());
  gwGridProgram = glCreateProgram();
  glAttachShader(gwGridProgram, vs);
  glAttachShader(gwGridProgram, fs);
  glLinkProgram(gwGridProgram);
  glDeleteShader(vs);
  glDeleteShader(fs);

  glGenVertexArrays(1, &gwGridVAO);
  glGenBuffers(1, &gwGridVBO);
  glGenBuffers(1, &gwGridEBO);
}

void Renderer::generateGravityWellGrid(Scene* scene) {
  const int gridSize = 40;
  const float spacing = 2.0f;

  std::vector<glm::vec3> vertices;
  std::vector<GLuint> indices;

  for (int z = 0; z <= gridSize; ++z) {
    for (int x = 0; x <= gridSize; ++x) {
      float worldX = (x - gridSize / 2.0f) * spacing;
      float worldZ = (z - gridSize / 2.0f) * spacing;
      float y = -10.0f; // Base level

      // Warp grid using gravity well
      for (const auto& b : scene->getBodies()) {
        if (b.type != BodyType::BlackHole) continue;
        float dx = worldX - b.position.x;
        float dz = worldZ - b.position.z;
        float dist = std::sqrt(dx * dx + dz * dz);
        float rs = 2.0f; // Schwarzschild radius
        if (dist > rs) {
          float deltaY = 2.0f * std::sqrt(rs * (dist - rs));
          y = deltaY - 15.0f;
        } else {
          y = 2.0f * rs - 15.0f;
        }
      }

      vertices.emplace_back(worldX, y, worldZ);
    }
  }

  // Generate line indices
  for (int z = 0; z < gridSize; ++z) {
    for (int x = 0; x < gridSize; ++x) {
      int i = z * (gridSize + 1) + x;
      indices.push_back(i);
      indices.push_back(i + 1);
      indices.push_back(i);
      indices.push_back(i + gridSize + 1);
    }
  }

  gwGridIndexCount = (int)indices.size();

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

void Renderer::render(Scene *scene, const glm::mat4 &view,
                      const glm::mat4 &projection) {
  glfwGetFramebufferSize(window, &width, &height);
  glViewport(0, 0, width, height);

  if (scene->getName() == "Black Hole Simulation") {
    updateComputeUBOs(scene, view, projection);

    // 1. Run compute shader
    glUseProgram(computeProgram);
    glBindTexture(GL_TEXTURE_2D, renderTexture);
    static int texW = 0, texH = 0;
    if(texW != width || texH != height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        texW = width; texH = height;
    }
    glBindImageTexture(0, renderTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    GLuint groupsX = (GLuint)std::ceil(width / 16.0f);
    GLuint groupsY = (GLuint)std::ceil(height / 16.0f);
    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // 2. Clear screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 3. Draw gravity well grid FIRST (behind everything)
    generateGravityWellGrid(scene);
    drawGravityWellGrid(view, projection);

    // 4. Overlay the raytraced quad WITH ALPHA BLENDING
    // Where compute shader outputs alpha=0 (escaped rays), the grid shows through
    glUseProgram(quadProgram);
    glBindVertexArray(quadVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderTexture);
    glUniform1i(glGetUniformLocation(quadProgram, "screenTexture"), 0);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glfwSwapBuffers(window);
    return;
  }

  glClearColor(0.01f, 0.01f, 0.02f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glUseProgram(shaderProgram);

  glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE,
                     glm::value_ptr(view));
  glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1,
                     GL_FALSE, glm::value_ptr(projection));

  // Draw Grid
  glBindVertexArray(gridVAO);
  glUniform3f(glGetUniformLocation(shaderProgram, "offset"), 0.0f, 0.0f, 0.0f);
  glUniform1f(glGetUniformLocation(shaderProgram, "scale"),
              0.0f); // 0 triggers grid/line layout in vert
  glUniform1f(glGetUniformLocation(shaderProgram, "isPointObject"), 0.0f);
  glUniform4f(glGetUniformLocation(shaderProgram, "color"), 0.2f, 0.2f, 0.3f,
              0.4f);
  glDrawArrays(GL_LINES, 0, gridVertexCount);

  // Draw trails
  glBindVertexArray(VAO);
  std::vector<float> lineVertices;
  lineVertices.reserve(50 * 3); // max history size is 50

  for (const auto &body : scene->getBodies()) {
    if (body.history.size() < 2)
      continue;

    lineVertices.clear();
    for (const auto &pt : body.history) {
      lineVertices.push_back(pt.x);
      lineVertices.push_back(pt.y);
      lineVertices.push_back(pt.z);
    }

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(float),
                 lineVertices.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                          (void *)0);
    glEnableVertexAttribArray(0);

    glUniform3f(glGetUniformLocation(shaderProgram, "offset"), 0.0f, 0.0f,
                0.0f);
    glUniform1f(glGetUniformLocation(shaderProgram, "scale"), 0.0f);
    glUniform1f(glGetUniformLocation(shaderProgram, "isPointObject"), 0.0f);
    glUniform4f(glGetUniformLocation(shaderProgram, "color"),
                body.color.r * 0.5f, body.color.g * 0.5f, body.color.b * 0.5f,
                0.5f);

    glDrawArrays(GL_LINE_STRIP, 0, lineVertices.size() / 3);
  }

  // Draw bodies
  glBindVertexArray(pointVAO);
  for (const auto &body : scene->getBodies()) {
    glUniform3f(glGetUniformLocation(shaderProgram, "offset"), body.position.x,
                body.position.y, body.position.z);
    glUniform1f(glGetUniformLocation(shaderProgram, "scale"), body.radius);
    glUniform1f(glGetUniformLocation(shaderProgram, "isPointObject"), 1.0f);

    // Emphasize Black Hole
    if (body.type == BodyType::BlackHole) {
      glUniform4f(glGetUniformLocation(shaderProgram, "color"), 0.0f, 0.0f,
                  0.0f, 1.0f);
    } else {
      glUniform4f(glGetUniformLocation(shaderProgram, "color"), body.color.r,
                  body.color.g, body.color.b, body.color.a);
    }

    glDrawArrays(GL_POINTS, 0, 1);
  }

  glfwSwapBuffers(window);
}

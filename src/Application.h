#pragma once
#include "Shader.h"
#include "Mesh.h"
#include "Texture.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <array>
struct Body {
    std::string name;
    float orbit;
    float radius;
    float rotationPeriodDays;
    glm::vec3 color;
    std::string type;
    double diameterKm = 0, massKg = 0, distanceMillionKm = 0, orbitalDays = 0;
    double gravity = 0;
    int moons = 0;
    std::vector<std::string> facts;
};

struct QuizQuestion {
    std::string planet, question, answer;
    std::array<std::string, 4> choices;
};

class Application {
  public:
    bool init();
    void run();
    ~Application();

  private:
    GLFWwindow *window = nullptr;
    Shader shader;
    Shader sunShader;
    Shader skyShader;
    Shader orbitShader;
    Mesh sphere;
    std::vector<Mesh> orbitLines;
    Mesh highlightRing;
    Texture skyTexture;
    std::vector<Body> bodies;
    Body sunBody;
    std::vector<QuizQuestion> questions;
    std::vector<size_t> quizRound;

    int width = 1280;
    int height = 720;
    int speed = 1;
    int selected = -1;
    float simTime = 0;
    float yaw = 0;
    float pitch = 0.25f;
    float distance = 22;
    float overviewDistance = 22;
    bool bloom = true;
    bool lensing = true;
    bool paused = false;
    bool help = false;
    bool freeFly = false;
    bool flying = false;
    bool quizOpen = false;
    bool quizWasPaused = false;
    bool quizAnswered = false;
    bool quizFinished = false;
    bool lastFDown = false;
    bool lastLeftDown = false;
    bool lastEscapeDown = false;
    float mouseX = 0, mouseY = 0;
    float flyElapsed = 0;
    size_t quizQuestionIndex = 0;
    int quizScore = 0;
    int quizChoice = -1;
    glm::vec3 currentCamera{0, 0, 22};
    glm::vec3 cameraTarget{0};
    glm::vec3 startCamera{0, 0, 22};
    glm::vec3 startTarget{0};
    glm::vec3 flyCameraOffset{0, 3, 7};
    glm::vec3 freeForward{0, 0, -1};

    glm::vec3 cameraPosition() const;
    void input(float deltaTime);
    void selectBody(int index);
    glm::vec3 bodyPosition(int index, float time) const;
    bool loadData();
    void beginQuiz();
    void draw(const Body &body, bool sun = false);
    void drawSky(const glm::mat4 &view, const glm::mat4 &projection);
};

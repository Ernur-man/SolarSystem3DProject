#pragma once
#include "Shader.h"
#include "Mesh.h"
#include "Texture.h"
#include "BlackHole.h"
#include "ThreatInterface.h"
#include "PostProcess.h"
#include "QuizRandomizer.h"
#include "Audio.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <random>
#include <cstdint>

struct GalaxyTrailPoint { glm::vec3 position; };
struct GalaxyTrail { std::vector<GalaxyTrailPoint> points; std::size_t head = 0, count = 0; };
struct TrailVertex { glm::vec3 position; glm::vec4 color; float edge; };

enum class GameState { StartScreen, FreeExplore, Mission, Won, Lost };

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

struct Meteor {
    glm::vec3 start;
    glm::vec3 direction;
    glm::vec3 color;
    float phase = 0.0f;
    float speed = 1.0f;
    float radius = 0.05f;
};

struct Asteroid {
    float orbitRadius = 0.0f;
    float angle = 0.0f;
    float angularSpeed = 0.0f;
    float verticalOffset = 0.0f;
    float size = 0.0f;
    glm::vec3 color{0.5f};
};

struct CityMarker {
    std::string name;
    std::string country;
    float latitude = 0.0f;
    float longitude = 0.0f;
    double population = 0.0;
};
struct CityPinVertex { glm::vec3 position; float opacity; };

class Application {
public:
    bool init();
    void run();
    ~Application();

private:
    GLFWwindow *window = nullptr;
    Shader shader;
    Shader sunShader;
    Shader earthShader;
    Shader atmosphereShader;
    Shader ringShader;
    Shader skyShader;
    Shader orbitShader;
    Shader meteorShader;
    Shader trailShader;
    Shader asteroidShader;
    Shader cityPinShader;
    Mesh sphere;
    Mesh lowDetailSphere;
    std::vector<Mesh> orbitLines;
    Mesh highlightRing;
    Mesh earthMoonOrbitLine;
    GLuint meteorQuadVao = 0;
    GLuint meteorQuadVbo = 0;
    GLuint trailVao = 0, trailVbo = 0;
    std::array<GalaxyTrail, 10> galaxyTrails;
    std::vector<TrailVertex> trailVertices;
    std::vector<Meteor> meteors;
    Mesh asteroidMesh;
    GLuint asteroidInstanceVbo = 0;
    GLuint cityPinVao = 0, cityPinVbo = 0;
    std::vector<Asteroid> asteroids;
    GLsizei asteroidDrawCalls = 0;
    Mesh saturnRing;
    Texture skyTexture;
    Texture earthDayTexture;
    Texture earthNightTexture;
    Texture earthCloudTexture;
    Texture earthNormalTexture;
    Texture earthSpecularTexture;
    Texture saturnRingTexture;
    GLuint generatedEarthMap = 0;
    std::vector<CityMarker> cities;
    std::array<Texture, 8> worldTextures;
    PostProcess postProcess;
    std::vector<Body> bodies;
    Body sunBody;
    std::vector<QuizQuestion> questions;
    std::vector<std::size_t> quizRound;
    std::array<QuizRandomizer::QuestionBag, 9> quizQuestionBags;
    std::array<std::string, 4> quizDisplayChoices{};
    std::mt19937 quizRandom{};
    std::uint32_t quizSeedSequence = 0;
    std::uint32_t activeQuizSeed = 0;
    BlackHole blackHole;
    Audio audio;
    FallbackThreat fallbackThreat; // Legacy alternative; the real black hole is selected by default.

    int width = 1280;
    int height = 720;
    int windowWidth = 1280;
    int windowHeight = 720;
    int speed = 1;
    int selected = -1;
    int missionScore = 0;
    static constexpr int MISSION_TARGET_SCORE = 60;
    GameState gameState = GameState::StartScreen;
    std::array<bool, 9> visitedBodies{};
    float threatSpeedMultiplier = 1.0f;
    float lightAngle = 0.0f;
    int screenshotIndex = 1;
    float simTime = 0;
    float galaxyTime = 0.0f, galaxyBlend = 0.0f, galaxyTransition = 0.0f;
    float trailSampleClock = 0.0f;
    bool galaxyViewTarget = false;
    bool lastYDown = false;
    bool cityMarkersEnabled = true;
    bool mapPanelOpen = false;
    bool lastNDown = false, lastPDown = false;
    float mapZoom = 1.0f;
    glm::vec2 mapPan{0.0f, 0.0f};
    float pickedLatitude = 0.0f, pickedLongitude = 0.0f;
    bool hasEarthCoordinate = false;
    bool earthMapTargetActive = false;
    float yaw = 0;
    float pitch = 0.25f;
    float distance = 22;
    float overviewDistance = 22;
    bool bloom = true;
    bool lensing = true;
    bool instancing = true;
    bool highQuality = true;
    bool meteorsEnabled = true;
    bool freeExploreSwallowedDismissed = false;
    bool useFallbackThreat = false;
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
    bool lastGDown = false;
    bool lastLDown = false;
    bool lastIDown = false;
    bool lastQDown = false;
    bool lastMDown = false;
    bool lastBDown = false;
    bool lastF1Down = false;
    bool lastHDown = false;
    bool lastKDown = false;
    bool lastRDown = false;
    bool lastF12Down = false;
    bool screenshotRequested = false;
    float mouseX = 0, mouseY = 0;
    float flyElapsed = 0;
    std::size_t quizQuestionIndex = 0;
    int quizScore = 0;
    int quizChoice = -1;
    glm::vec3 currentCamera{0, 0, 22};
    glm::vec3 cameraTarget{0};
    glm::vec3 startCamera{0, 0, 22};
    glm::vec3 startTarget{0};
    glm::vec3 flyCameraOffset{0, 3, 7};
    glm::vec3 earthMapTargetLocal{0.0f, 0.0f, 1.0f};
    glm::vec3 freeForward{0, 0, -1};

    glm::vec3 cameraPosition() const;
    static void framebufferSizeCallback(GLFWwindow *window, int width, int height);
    void input(float deltaTime);
    void selectBody(int index);
    glm::vec3 bodyPosition(int index, float time) const;
    bool loadData();
    void beginQuiz();
    void initializeQuizBags();
    void shuffleDisplayedChoices();
    void startMission(bool summonHole = true);
    void startFreeExplore();
    void returnToMenu();
    void updateMission(float deltaTime);
    void recordVisit(int index);
    void recordQuizAnswer(bool correct);
    void saveScreenshot();
    void draw(const Body &body, const glm::mat4 &view, const glm::mat4 &projection,
              bool sun = false);
    void drawSky(const glm::mat4 &view, const glm::mat4 &projection);
    void drawSaturnRing(const glm::mat4 &view, const glm::mat4 &projection);
    void drawEarthMoon(const glm::mat4 &view, const glm::mat4 &projection);
    void drawMeteors(const glm::mat4 &view, const glm::mat4 &projection);
    void drawAsteroids(const glm::mat4 &view, const glm::mat4 &projection);
    void drawCityPins(const glm::mat4 &view, const glm::mat4 &projection);
    void drawCityLabels(const glm::mat4 &view, const glm::mat4 &projection);
    void loadCities();
    void createEarthMapFallback();
    glm::mat4 earthModelMatrix() const;
    void applyQualitySettings();
    ThreatInterface &activeThreat();
    BodyEffect bodyEffect(int index) const;
    glm::vec3 earthMoonPosition() const;
    void updateGalaxy(float deltaTime, float simulationDelta);
    void drawGalaxyTrails(const glm::mat4 &view, const glm::mat4 &projection);
    glm::vec3 galaxyDisplacement() const;
    void resetGalaxyView();
    void setGalaxyViewTarget(bool enabled);
};

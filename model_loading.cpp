// === Simple 3D Game (drop-in replacement) ===
// File: src/3.model_loading/1.model_loading/model_loading.cpp
// Build: unchanged CMakeLists (uses this demo target)

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>
#include <algorithm>

// --------------------------------------------
// Window settings
// --------------------------------------------
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// --------------------------------------------
// Input / timing
// --------------------------------------------
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// --------------------------------------------
// Camera (we'll *drive* it to follow the player)
// --------------------------------------------
Camera camera(glm::vec3(0.0f, 3.0f, 6.0f));
bool   firstMouse = true;
float  lastX = SCR_WIDTH * 0.5f;
float  lastY = SCR_HEIGHT * 0.5f;

// --------------------------------------------
// Callbacks
// --------------------------------------------
void framebuffer_size_callback(GLFWwindow* w, int width, int height) { glViewport(0, 0, width, height); }
void mouse_callback(GLFWwindow* w, double xposIn, double yposIn) {
    // optional: mouse to rotate player facing; we ignore to keep it simple
}
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// --------------------------------------------
// Simple AABB struct (XZ only = “capsule on plane” approximation)
// --------------------------------------------
struct AABB2D {
    glm::vec2 center;   // xz center
    glm::vec2 halfExt;  // half extents on x and z
};

static bool Intersects(const AABB2D& a, const AABB2D& b) {
    if (std::abs(a.center.x - b.center.x) > (a.halfExt.x + b.halfExt.x)) return false;
    if (std::abs(a.center.y - b.center.y) > (a.halfExt.y + b.halfExt.y)) return false;
    return true;
}

// Resolve player-vs-static (push player out on the shallow axis)
static void ResolveStatic(const AABB2D& statBox, AABB2D& dynBox, glm::vec3& playerPos) {
    float dx = (dynBox.center.x - statBox.center.x);
    float px = (dynBox.halfExt.x + statBox.halfExt.x) - std::abs(dx);
    float dz = (dynBox.center.y - statBox.center.y);
    float pz = (dynBox.halfExt.y + statBox.halfExt.y) - std::abs(dz);
    if (px < 0 || pz < 0) return;
    if (px < pz) {
        float sx = (dx < 0 ? -1.0f : 1.0f);
        playerPos.x += sx * px;
        dynBox.center.x += sx * px;
    }
    else {
        float sz = (dz < 0 ? -1.0f : 1.0f);
        playerPos.z += sz * pz;
        dynBox.center.y += sz * pz;
    }
}

// --------------------------------------------
// Input handling
// --------------------------------------------
void processInput(GLFWwindow* window, glm::vec3& playerPos, float playerSpeed, float& playerYawDeg) {
    glm::vec3 dir(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) dir.z -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) dir.z += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) dir.x -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) dir.x += 1.0f;

    if (glm::length(dir) > 0.0f) {
        dir = glm::normalize(dir);
        // face toward move direction
        playerYawDeg = glm::degrees(std::atan2(dir.x, -dir.z));
        playerPos += glm::vec3(dir.x, 0.0f, dir.z) * playerSpeed * deltaTime;
    }

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// --------------------------------------------
// Follow camera (3rd-person)
// --------------------------------------------
void updateFollowCamera(const glm::vec3& playerPos, float playerYawDeg, float distBack, float height, float smoothAlpha = 0.15f) {
    float yawRad = glm::radians(playerYawDeg);
    glm::vec3 back(-std::sin(yawRad), 0.0f, -std::cos(yawRad));
    glm::vec3 desired = playerPos + back * distBack + glm::vec3(0.0f, height, 0.0f);
    // simple lerp
    camera.Position = glm::mix(camera.Position, desired, smoothAlpha);
    camera.Front = glm::normalize(playerPos - camera.Position);
    camera.Right = glm::normalize(glm::cross(camera.Front, glm::vec3(0, 1, 0)));
    camera.Up = glm::normalize(glm::cross(camera.Right, camera.Front));
}

// --------------------------------------------
// Main
// --------------------------------------------
int main() {
    // Init window
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Simple 3D Game", NULL, NULL);
    if (!window) { std::cout << "Failed to create GLFW window\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD\n"; return -1;
    }
    stbi_set_flip_vertically_on_load(true);
    glEnable(GL_DEPTH_TEST);

    // Shader (simple textured)
    Shader shader("1.model_loading.vs", "1.model_loading.fs");

    // --- Load Models (paths per your constraint) ---
    // Player: nanosuit
    Model playerModel(FileSystem::getPath("resources/objects/nanosuit/nanosuit.obj"));
    // Scene: a few rocks as obstacles
    Model rockModel(FileSystem::getPath("resources/objects/rock/rock.obj"));
    // Item: backpack (pickup)
    Model itemModel(FileSystem::getPath("resources/objects/backpack/backpack.obj"));
    // Enemy: vampire/cyborg
    Model enemyModel(FileSystem::getPath("resources/objects/vampire/dancing_vampire.dae")); // some LearnOpenGL kits use .dae

    // --- Game State ---
    glm::vec3 playerPos(0.0f, 0.0f, 0.0f);
    float     playerYawDeg = 0.0f;
    float     playerScale = 0.15f; // nanosuit is huge

    // Obstacles (rocks) placed in the world
    struct Rock { glm::vec3 pos; float scale; AABB2D box; };
    std::vector<Rock> rocks = {
        {{  3.0f, 0.0f, -2.0f}, 1.2f, {}},
        {{ -4.0f, 0.0f, -1.0f}, 1.6f, {}},
        {{  0.0f, 0.0f, -6.0f}, 2.0f, {}},
        {{  5.0f, 0.0f,  3.0f}, 1.5f, {}}
    };

    // Item (pickup)
    glm::vec3 itemPos(2.0f, 0.0f, -4.0f);
    float     itemScale = 0.8f;
    bool      itemCollected = false;

    // Enemy (simple patrol)
    glm::vec3 enemyPos(-6.0f, 0.0f, 2.0f);
    float     enemyScale = 0.015f; // dancing vampire/cyborg are big
    float     enemyPatrolAmp = 3.0f;
    float     enemyPatrolSpeed = 1.0f;
    float     enemyDir = 1.0f;

    // Simplified AABBs (XZ), tuned for these model scales
    auto makeAABB = [](const glm::vec3& pos, float hx, float hz) -> AABB2D {
        return { glm::vec2(pos.x, pos.z), glm::vec2(hx, hz) };
        };
    auto playerBox = makeAABB(playerPos, 0.5f, 0.4f);         // player (nanosuit, scaled)
    auto itemBox = makeAABB(itemPos, 0.6f, 0.6f);
    auto enemyBox = makeAABB(enemyPos, 0.7f, 0.7f);

    for (auto& r : rocks) {
        r.box = makeAABB(r.pos, 1.0f * r.scale, 1.0f * r.scale);
    }

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        // time
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input: move player
        processInput(window, playerPos, /*speed*/3.0f, playerYawDeg);

        // Patrol enemy (back-and-forth on X)
        enemyPos.x += enemyDir * enemyPatrolSpeed * deltaTime;
        if (enemyPos.x > -6.0f + enemyPatrolAmp) enemyDir = -1.0f;
        if (enemyPos.x < -6.0f - enemyPatrolAmp) enemyDir = 1.0f;

        // Update AABBs
        playerBox.center = { playerPos.x, playerPos.z };
        enemyBox.center = { enemyPos.x,  enemyPos.z };
        itemBox.center = { itemPos.x,   itemPos.z };

        // Collide player vs static rocks
        for (auto& r : rocks) {
            if (Intersects(playerBox, r.box)) {
                ResolveStatic(r.box, playerBox, playerPos);
            }
        }

        // Collide player vs item (pickup)
        if (!itemCollected && Intersects(playerBox, itemBox)) {
            itemCollected = true;
            std::cout << "Picked up the backpack!\n";
        }

        // Collide player vs enemy (simple knockback)
        if (Intersects(playerBox, enemyBox)) {
            glm::vec2 pushDir = glm::normalize(playerBox.center - enemyBox.center);
            playerPos.x += pushDir.x * 1.2f;
            playerPos.z += pushDir.y * 1.2f;
            playerBox.center = { playerPos.x, playerPos.z };
        }

        // Camera follow
        updateFollowCamera(playerPos, playerYawDeg, /*back*/4.5f, /*height*/2.2f);

        // --- Render ---
        glClearColor(0.08f, 0.09f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);

        auto drawModel = [&](Model& m, const glm::vec3& pos, float yawDeg, float scale) {
            glm::mat4 model(1.0f);
            model = glm::translate(model, pos);
            model = glm::rotate(model, glm::radians(yawDeg), glm::vec3(0, 1, 0));
            model = glm::scale(model, glm::vec3(scale));
            shader.setMat4("model", model);
            m.Draw(shader);
            };

        // Draw ground (just a big, low rock pretending to be ground)
        {
            glm::mat4 M(1.0f);
            M = glm::translate(M, glm::vec3(0.0f, -1.3f, 0.0f));
            M = glm::scale(M, glm::vec3(20.0f, 0.5f, 20.0f));
            shader.setMat4("model", M);
            rockModel.Draw(shader);
        }

        // Draw rocks (obstacles)
        for (auto& r : rocks) {
            glm::mat4 M(1.0f);
            M = glm::translate(M, r.pos + glm::vec3(0.0f, -0.5f, 0.0f));
            M = glm::scale(M, glm::vec3(r.scale));
            shader.setMat4("model", M);
            rockModel.Draw(shader);
        }

        // Draw item if not collected
        if (!itemCollected) {
            drawModel(itemModel, itemPos + glm::vec3(0, -1.0f, 0), currentFrame * 50.0f, itemScale);
        }

        // Draw enemy (slow spin so we can see it moving)
        drawModel(enemyModel, enemyPos + glm::vec3(0, -1.0f, 0), currentFrame * 30.0f, enemyScale);

        // Draw player (face yaw)
        drawModel(playerModel, playerPos + glm::vec3(0, -1.0f, 0), playerYawDeg, playerScale);

        // present
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

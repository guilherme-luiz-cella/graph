#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include <stb_easy_font.h>

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <filesystem>
#include <cmath>
#include <vector>

#include "shader.h"
#include "camera.h"
#include "model.h"
#include "skybox.h"
#include "primitives.h"
#include "animated_model.h"
#include "animator.h"
#include "procgen.h"

// ----- window / camera state -----
static int  SCR_W = 1280, SCR_H = 720;
static Camera cam;
static float lastX = SCR_W / 2.0f, lastY = SCR_H / 2.0f;
static bool firstMouse = true;
static float dt = 0.0f, lastFrame = 0.0f;

// ----- HUD overlay state -----
static std::string g_hudText;
static float       g_hudExpireAt = 0.0f;
constexpr float    HUD_DURATION = 4.0f;

// ----- technique toggles (number keys) -----
struct Toggles {
    bool multiTex = true;   // 1
    bool normalMap = true;  // 2
    bool envMap = true;     // 3
    bool fog = false;       // 4 — off by default for "bright showroom" look
    bool blending = true;   // 5
    bool shadows = true;    // 6
    bool spotLight = true;  // 7
    bool pointLight = true; // 8
    bool skybox = true;     // 9
    bool cinematic = false; // C
} tog;

// ----- shadow map -----
constexpr unsigned int SHADOW_W = 2048, SHADOW_H = 2048;
static unsigned int shadowFBO = 0, shadowTex = 0;

static void setupShadow() {
    glGenFramebuffers(1, &shadowFBO);
    glGenTextures(1, &shadowTex);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_W, SHADOW_H, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glClear(GL_DEPTH_BUFFER_BIT); // ensure non-junk depth on first frame
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ----- callbacks -----
static void onResize(GLFWwindow*, int w, int h) { SCR_W = w; SCR_H = h; glViewport(0, 0, w, h); }
static void onMouse(GLFWwindow*, double x, double y) {
    if (firstMouse) { lastX = (float)x; lastY = (float)y; firstMouse = false; }
    float dx = (float)x - lastX, dy = (float)y - lastY;
    lastX = (float)x; lastY = (float)y;
    cam.mouse(dx, dy);
}

static bool wasPressed[10] = {};
static bool edge(GLFWwindow* w, int key, int slot) {
    bool down = glfwGetKey(w, key) == GLFW_PRESS;
    bool fire = down && !wasPressed[slot];
    wasPressed[slot] = down;
    return fire;
}

static void toggle(bool& b, const char* name, const char* tooltip) {
    b = !b;
    std::cout << "\n[" << name << "]  " << (b ? "ON " : "OFF") << "  — " << tooltip << "\n";
    g_hudText = std::string("[") + name + "]  " + (b ? "ON" : "OFF") + "\n" + tooltip;
    g_hudExpireAt = (float)glfwGetTime() + HUD_DURATION;
}

static void input(GLFWwindow* w) {
    if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(w, true);
    if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) cam.key(0, dt);
    if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) cam.key(1, dt);
    if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) cam.key(2, dt);
    if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) cam.key(3, dt);
    if (glfwGetKey(w, GLFW_KEY_SPACE) == GLFW_PRESS) cam.key(4, dt);
    if (glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) cam.key(5, dt);
    if (edge(w, GLFW_KEY_1, 0)) toggle(tog.multiTex,   "1 multi-textura",  "Spec 2d. Chao mistura radial cobble (centro) -> grama (borda) via smoothstep.");
    if (edge(w, GLFW_KEY_2, 1)) toggle(tog.normalMap,  "2 normal map",     "Spec 2g. Parede de tijolos usa normal map procedural derivado de heightfield.");
    if (edge(w, GLFW_KEY_3, 2)) toggle(tog.envMap,     "3 env map",        "Spec 2f. Esfera reflete cubemap via reflect(view, N). Mix 60% reflexao.");
    if (edge(w, GLFW_KEY_4, 3)) toggle(tog.fog,        "4 fog",            "Spec 2h. Neblina linear: intensidade = (fogEnd - dist) / (fogEnd - fogStart).");
    if (edge(w, GLFW_KEY_5, 4)) toggle(tog.blending,   "5 blending",       "Spec 2i. Lanterna usa GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA, alpha textura *0.45.");
    if (edge(w, GLFW_KEY_6, 5)) toggle(tog.shadows,    "6 shadow map",     "Spec 2j. FBO 2048x2048 com depth pass da luz spot. Filtro PCF 3x3 (9 amostras).");
    if (edge(w, GLFW_KEY_7, 6)) toggle(tog.spotLight,  "7 luz spot",       "Spec 2c. Cone com cutoff/outer (smoothstep). Direcao gira lentamente.");
    if (edge(w, GLFW_KEY_8, 7)) toggle(tog.pointLight, "8 luz point POV",  "Spec 2c. Luz quente seguindo a camera (lanterna POV). Attenuation quadratica.");
    if (edge(w, GLFW_KEY_9, 8)) toggle(tog.skybox,     "9 skybox",         "Spec 2e. Cubemap 6 faces (gradiente procedural). Desenhado por ultimo, depth=LEQUAL.");
    if (edge(w, GLFW_KEY_C, 9)) toggle(tog.cinematic,  "C cinematica",     "Camera auto-orbita centro da casa em circulo. Util durante apresentacao.");
}

static void banner() {
    std::cout << R"(
=== Trabalho Final de Computação Gráfica ===
Cena: Casa moderna ao ar livre — apresentação arquitetônica
Author: Guilherme Luiz Cella (105491)

Controles:
  WASD            Movimento
  Mouse           Olhar
  Space / Shift   Subir / Descer
  Esc             Sair

Toggles (técnicas):
  1  Multi-textura (chão: pedra+grama)
  2  Normal mapping (parede de tijolos)
  3  Environment mapping (esfera metálica)
  4  Neblina (Fog)
  5  Transparência (lanterna)
  6  Shadow map (luz spot)
  7  Luz spot
  8  Luz point
  9  Skybox
  C  Câmera cinemática (auto-orbita)
============================================
)";
}


// ----- scene render helper -----
struct DrawItem {
    const Mesh* mesh = nullptr;
    const Model* model = nullptr;
    glm::mat4 xform = glm::mat4(1.0f);
    unsigned int diffuse0 = 0, diffuse1 = 0, normal0 = 0;
    bool useMultiTex = false;
    bool useNormalMap = false;
    bool useEnvMap = false;
    bool useAlpha = false;
};

static void renderItem(const DrawItem& it, const Shader& s) {
    s.setMat4("model", it.xform);
    bool multi  = it.useMultiTex  && tog.multiTex  && it.diffuse0 && it.diffuse1;
    bool normal = it.useNormalMap && tog.normalMap && it.normal0;
    bool env    = it.useEnvMap    && tog.envMap;
    s.setInt("useMultiTex",  multi  ? 1 : 0);
    s.setInt("useNormalMap", normal ? 1 : 0);
    s.setInt("useEnvMap",    env    ? 1 : 0);
    s.setInt("useAlpha", it.useAlpha ? 1 : 0);
    if (it.diffuse0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, it.diffuse0); s.setInt("tex_diffuse0", 0); }
    if (it.diffuse1) { glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, it.diffuse1); s.setInt("tex_diffuse1", 1); }
    if (it.normal0)  { glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, it.normal0);  s.setInt("tex_normal0", 2); }
    if (it.mesh)  it.mesh->draw(s);
    if (it.model) it.model->draw(s);
}

static void renderDepth(const DrawItem& it, const Shader& s) {
    s.setMat4("model", it.xform);
    if (it.mesh)  it.mesh->draw(s);
    if (it.model) it.model->draw(s);
}

// ----- HUD overlay (stb_easy_font → quads → triangle indices) -----
struct HUD {
    unsigned int vao = 0, vbo = 0, ebo = 0;
    Shader shader;
    void setup() {
        shader.load("shaders/text.vs", "shaders/text.fs");
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
    }
    // Draw text at pixel (x,y) from top-left of the screen.
    void draw(const std::string& text, float x, float y, int screenW, int screenH,
              const glm::vec4& color, float scale = 2.0f) {
        if (text.empty()) return;
        static char vbuf[200000];
        int quads = stb_easy_font_print(0.0f, 0.0f, (char*)text.c_str(), nullptr, vbuf, sizeof(vbuf));
        if (quads <= 0) return;

        // stb writes 16-byte verts (vec3 + uint32 color). We only need xy, scaled + translated.
        std::vector<float> verts; verts.reserve(quads * 4 * 2);
        std::vector<unsigned int> idx; idx.reserve(quads * 6);
        for (int v = 0; v < quads * 4; ++v) {
            float* p = (float*)(vbuf + v * 16);
            verts.push_back(p[0] * scale + x);
            verts.push_back(p[1] * scale + y);
        }
        for (int q = 0; q < quads; ++q) {
            unsigned int b = (unsigned int)q * 4u;
            idx.push_back(b);     idx.push_back(b + 1); idx.push_back(b + 2);
            idx.push_back(b);     idx.push_back(b + 2); idx.push_back(b + 3);
        }
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

        shader.use();
        glm::mat4 ortho = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f);
        shader.setMat4("proj", ortho);
        shader.setVec4("textColor", color);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDrawElements(GL_TRIANGLES, (GLsizei)idx.size(), GL_UNSIGNED_INT, 0);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glBindVertexArray(0);
    }
    // Filled background rectangle (used behind text for readability).
    void drawRect(float x, float y, float w, float h, int screenW, int screenH, const glm::vec4& color) {
        float verts[] = {
            x,     y,
            x + w, y,
            x + w, y + h,
            x,     y + h,
        };
        unsigned int idx[] = { 0, 1, 2, 0, 2, 3 };
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

        shader.use();
        glm::mat4 ortho = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f);
        shader.setMat4("proj", ortho);
        shader.setVec4("textColor", color);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glBindVertexArray(0);
    }
};

int main() {
    banner();
    if (!glfwInit()) { std::cerr << "glfw init fail\n"; return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    // Fullscreen on primary monitor at native resolution.
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    glfwWindowHint(GLFW_RED_BITS,   mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS,  mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    SCR_W = mode->width; SCR_H = mode->height;
    GLFWwindow* win = glfwCreateWindow(SCR_W, SCR_H, "CG Final - Modern House", monitor, nullptr);
    if (!win) { std::cerr << "window fail\n"; glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, onResize);
    glfwSetCursorPosCallback(win, onMouse);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) { std::cerr << "glad fail\n"; return 1; }
    glEnable(GL_DEPTH_TEST);

    // ----- shaders -----
    Shader scene;        scene.load("shaders/scene.vs", "shaders/scene.fs");
    Shader sceneSkinned; sceneSkinned.load("shaders/skinned.vs", "shaders/scene.fs");
    Shader depth;        depth.load("shaders/depth.vs", "shaders/depth.fs");
    Shader depthSkinned; depthSkinned.load("shaders/skinned_depth.vs", "shaders/depth.fs");
    setupShadow();
    HUD hud; hud.setup();

    // ----- skybox (real files if present, else procedural gradient) -----
    Skybox skybox;
    namespace fs = std::filesystem;
    bool skyboxFromFile = fs::exists("assets/skybox/px.jpg");
    if (skyboxFromFile) {
        skybox.load({
            "assets/skybox/px.jpg","assets/skybox/nx.jpg","assets/skybox/py.jpg",
            "assets/skybox/ny.jpg","assets/skybox/pz.jpg","assets/skybox/nz.jpg"
        });
    } else {
        std::cerr << "[procgen] skybox files missing — using gradient cubemap fallback\n";
        skybox.shader.load("shaders/skybox.vs", "shaders/skybox.fs");
        skybox.cubemap = genSkyCubemap(256);
        // setup cube VAO/VBO manually (mirrors Skybox::load second half)
        static const float CUBE[] = {
            -1,  1, -1,  -1, -1, -1,   1, -1, -1,   1, -1, -1,   1,  1, -1,  -1,  1, -1,
            -1, -1,  1,  -1, -1, -1,  -1,  1, -1,  -1,  1, -1,  -1,  1,  1,  -1, -1,  1,
             1, -1, -1,   1, -1,  1,   1,  1,  1,   1,  1,  1,   1,  1, -1,   1, -1, -1,
            -1, -1,  1,  -1,  1,  1,   1,  1,  1,   1,  1,  1,   1, -1,  1,  -1, -1,  1,
            -1,  1, -1,   1,  1, -1,   1,  1,  1,   1,  1,  1,  -1,  1,  1,  -1,  1, -1,
            -1, -1, -1,  -1, -1,  1,   1, -1, -1,   1, -1, -1,  -1, -1,  1,   1, -1,  1
        };
        glGenVertexArrays(1, &skybox.vao);
        glGenBuffers(1, &skybox.vbo);
        glBindVertexArray(skybox.vao);
        glBindBuffer(GL_ARRAY_BUFFER, skybox.vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(CUBE), CUBE, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    }
    bool haveSkybox = true; // always available now

    // ----- procedural geometry -----
    Mesh ground = makePlane(80.0f, 16.0f);
    Mesh wall   = makeCube(1.0f);   // scaled per draw
    Mesh orb    = makeSphere(0.6f, 32);
    Mesh lantern = makeCube(0.4f);

    // ----- textures: real if present, procedural fallback otherwise -----
    auto pickTex = [](const std::string& path, unsigned int (*gen)(int), int size) -> unsigned int {
        if (fs::exists(path)) return loadTexture(path, true);
        std::cerr << "[procgen] " << path << " missing — generated\n";
        return gen(size);
    };
    unsigned int texCobble = pickTex("assets/textures/cobble_diffuse.jpg", genCobble, 256);
    unsigned int texGrass  = pickTex("assets/textures/grass_diffuse.jpg",  genGrass,  256);
    unsigned int texBrick  = pickTex("assets/textures/brick_diffuse.jpg",  genBrick,  256);
    unsigned int texBrickN;
    if (fs::exists("assets/textures/brick_normal.jpg")) texBrickN = loadTexture("assets/textures/brick_normal.jpg", false);
    else { std::cerr << "[procgen] brick normal missing — generated\n"; texBrickN = genBrickNormal(256); }
    unsigned int texGlass;
    if (fs::exists("assets/textures/glass.png")) texGlass = loadTexture("assets/textures/glass.png");
    else { std::cerr << "[procgen] glass missing — generated\n"; texGlass = genGlass(64); }

    // House panel colors — "showroom" palette for the procedural modern house.
    unsigned int texHouseWall = genFlat(238, 232, 220); // warm off-white
    unsigned int texHouseRoof = genFlat( 55,  55,  60); // dark charcoal slab
    unsigned int texHouseDoor = genFlat( 70,  45,  30); // walnut
    unsigned int texHouseTrim = genFlat(180, 180, 175); // light gray accent

    // ----- ASSIMP-loaded models -----
    auto reportBBox = [](const char* tag, auto& verts) {
        if (verts.empty()) { std::cerr << "[bbox] " << tag << ": empty\n"; return; }
        glm::vec3 lo(1e9f), hi(-1e9f);
        for (auto& v : verts) { lo = glm::min(lo, v.pos); hi = glm::max(hi, v.pos); }
        glm::vec3 d = hi - lo;
        std::cerr << "[bbox] " << tag << " verts=" << verts.size()
                  << " min=(" << lo.x << "," << lo.y << "," << lo.z << ")"
                  << " max=(" << hi.x << "," << hi.y << "," << hi.z << ")"
                  << " size=(" << d.x << "," << d.y << "," << d.z << ")\n";
    };

    Model castle;
    if (fs::exists("assets/models/castle/castle.3ds")) {
        castle.load("assets/models/castle/castle.3ds");
        std::cerr << "[load] castle meshes=" << castle.meshes.size() << "\n";
        if (!castle.meshes.empty()) reportBBox("castle[0]", castle.meshes[0].vertices);
    }

    Model house;
    const char* houseFbx = "assets/models/house/modern_house/Modern house.fbx";
    if (fs::exists(houseFbx)) {
        house.load(houseFbx);
        glm::vec3 lo(1e9f), hi(-1e9f);
        size_t totalVerts = 0;
        for (auto& m : house.meshes) {
            for (auto& v : m.vertices) { lo = glm::min(lo, v.pos); hi = glm::max(hi, v.pos); }
            totalVerts += m.vertices.size();
        }
        glm::vec3 d = hi - lo;
        std::cerr << "[load] house meshes=" << house.meshes.size() << " verts=" << totalVerts
                  << " size=(" << d.x << "," << d.y << "," << d.z << ")\n";
    }

    AnimatedModel girl;
    Animator girlAnim;
    // Accept either .fbx (Mixamo) or .glb (Khronos CesiumMan etc) — assimp handles both.
    for (const char* p : {"assets/models/girl/girl.fbx", "assets/models/girl/girl.glb"}) {
        if (fs::exists(p)) {
            girl.load(p);
            girlAnim.setModel(&girl);
            std::cerr << "[load] girl=" << p << " meshes=" << girl.meshes.size()
                      << " anims=" << (girl.scene ? girl.scene->mNumAnimations : 0) << "\n";
            if (!girl.meshes.empty()) reportBBox("girl[0]", girl.meshes[0].vertices);
            break;
        }
    }

    // ----- audio -----
    ma_engine audio;
    if (ma_engine_init(nullptr, &audio) != MA_SUCCESS) std::cerr << "audio init fail\n";
    bool haveAmbient  = fs::exists("assets/audio/ambient.ogg");
    bool haveFootstep = fs::exists("assets/audio/footstep.ogg");
    if (haveAmbient) ma_engine_play_sound(&audio, "assets/audio/ambient.ogg", nullptr);
    float nextFootstep = 0.0f;

    // ----- scene description -----
    auto buildItems = [&]() {
        std::vector<DrawItem> items;
        DrawItem g; g.mesh = &ground;
        g.diffuse0 = texCobble; g.diffuse1 = texGrass;
        g.useMultiTex = true;
        items.push_back(g);

        DrawItem w; w.mesh = &wall;
        w.xform = glm::translate(glm::mat4(1.0f), {-14.0f, 2.0f, -8.0f});
        w.xform = glm::scale(w.xform, {6.0f, 4.0f, 0.5f});
        w.diffuse0 = texBrick; w.normal0 = texBrickN;
        w.useNormalMap = true;
        items.push_back(w);

        DrawItem o; o.mesh = &orb;
        o.xform = glm::translate(glm::mat4(1.0f), {3.0f, 1.2f, 0.0f});
        o.useEnvMap = true;
        items.push_back(o);

        DrawItem l; l.mesh = &lantern;
        l.xform = glm::translate(glm::mat4(1.0f), {1.5f, 1.0f, 2.0f});
        l.diffuse0 = texGlass;
        l.useAlpha = true;
        items.push_back(l);

        // Modern house FBX — post PreTransformVertices, model is in meters.
        // Material colors baked into 1x1 textures at load time (color-only Maya mats).
        if (!house.meshes.empty()) {
            DrawItem h; h.model = &house;
            // Bbox post-bake: X(-10.7..12.8) Y(0.56..9.16) Z(-11.3..10.5)
            // Center footprint on origin, base on ground.
            h.xform = glm::translate(glm::mat4(1.0f), {-1.05f, -0.557f, 0.4f});
            items.push_back(h);
        }

        // Spec § 2a coverage — .3DS castle as background prop.
        if (!castle.meshes.empty()) {
            DrawItem c; c.model = &castle;
            c.xform = glm::translate(glm::mat4(1.0f), {22.0f, 0.0f, -25.0f});
            c.xform = glm::scale(c.xform, glm::vec3(3.0f));
            items.push_back(c);
        }
        return items;
    };

    // girl wander path: orbit modern house at radius ~5 with low-freq noise perturbation
    // so the path looks like a person strolling around, not a turntable.
    // VRoid VRM is Y-up + meters → no axis flip. CesiumMan glb is Z-up → flip via flag.
    bool girlNeedsZFlip = (girl.scene && girl.scene->mNumAnimations > 0);
    auto girlXform = [&](float t) {
        float a = t * 0.25f;
        // radial wobble around real FBX house (footprint ~24x22m): stay outside with margin
        float r = 16.0f + 1.5f * std::sin(t * 0.7f) + 0.7f * std::sin(t * 1.7f + 1.0f);
        glm::vec3 pos(std::cos(a) * r, 0.0f, std::sin(a) * r);
        // facing tangent to path (perpendicular to radial)
        float facing = -a + glm::radians(90.0f);
        glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
        m = glm::rotate(m, facing, glm::vec3(0, 1, 0));
        if (girlNeedsZFlip) m = glm::rotate(m, glm::radians(-90.0f), glm::vec3(1, 0, 0));
        return m;
    };

    while (!glfwWindowShouldClose(win)) {
        float now = (float)glfwGetTime();
        dt = now - lastFrame; lastFrame = now;
        input(win);
        auto items = buildItems();
        bool haveGirl = !girl.meshes.empty();
        glm::mat4 girlModel = girlXform(now);
        if (haveGirl) girlAnim.update(dt);

        // cinematic camera: slow orbit around courtyard center, look at fountain area
        // ponytail: fixed radius/height, constant angular speed. Upgrade to keyframed
        // path with easing if cinematic must showcase specific angles.
        if (tog.cinematic) {
            float a = now * 0.15f;
            cam.pos = glm::vec3(std::cos(a) * 9.0f, 2.4f, std::sin(a) * 9.0f);
            glm::vec3 target{0.0f, 1.0f, 0.0f};
            cam.front = glm::normalize(target - cam.pos);
            // keep yaw/pitch in sync so manual control is smooth after disengaging
            cam.pitch = glm::degrees(std::asin(cam.front.y));
            cam.yaw   = glm::degrees(std::atan2(cam.front.z, cam.front.x));
        }

        // footstep cadence — step every 0.45s while girl exists and is "walking"
        // ponytail: fixed period, ignores anim foot-down phase. Upgrade to event-driven
        // (sample root-bone Y or specific anim frame) if asymmetric gait needed.
        if (haveGirl && haveFootstep && now >= nextFootstep) {
            ma_engine_play_sound(&audio, "assets/audio/footstep.ogg", nullptr);
            nextFootstep = now + 0.45f;
        }

        // ---- light setup (spot rotates slowly to show shadows moving) ----
        glm::vec3 spotPos   = {3.0f, 6.0f, 4.0f};
        glm::vec3 spotDir   = glm::normalize(glm::vec3(std::sin(now * 0.2f) * 0.3f, -1.0f, std::cos(now * 0.2f) * 0.3f));
        glm::vec3 spotColor = tog.spotLight ? glm::vec3(1.0f, 0.85f, 0.6f) : glm::vec3(0.0f);
        // POV lantern: held in front and slightly below view direction.
        glm::vec3 camRight  = glm::normalize(glm::cross(cam.front, cam.up));
        glm::vec3 lanternPos = cam.pos + cam.front * 0.7f + camRight * 0.3f - cam.up * 0.25f;
        glm::vec3 pointPos  = lanternPos;
        glm::vec3 pointCol  = tog.pointLight ? glm::vec3(1.0f, 0.75f, 0.45f) : glm::vec3(0.0f);

        // Visible POV lantern body — small glowing cube tracking the camera.
        if (tog.pointLight) {
            DrawItem pov; pov.mesh = &lantern;
            pov.xform = glm::translate(glm::mat4(1.0f), lanternPos);
            pov.xform = glm::scale(pov.xform, glm::vec3(0.25f));
            pov.diffuse0 = texGlass;
            pov.useAlpha = true;
            items.push_back(pov);
        }

        glm::mat4 lightProj = glm::perspective(glm::radians(60.0f), 1.0f, 1.0f, 30.0f);
        glm::mat4 lightView = glm::lookAt(spotPos, spotPos + spotDir, glm::vec3(0, 1, 0));
        glm::mat4 lightSpace = lightProj * lightView;

        // ---- depth pass ----
        if (tog.shadows) {
            glViewport(0, 0, SHADOW_W, SHADOW_H);
            glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
            glClear(GL_DEPTH_BUFFER_BIT);
            depth.use();
            depth.setMat4("lightSpaceMatrix", lightSpace);
            for (auto& it : items) if (!it.useAlpha) renderDepth(it, depth);
            if (haveGirl) {
                depthSkinned.use();
                depthSkinned.setMat4("lightSpaceMatrix", lightSpace);
                depthSkinned.setMat4("model", girlModel);
                depthSkinned.setMat4Array("bones", girlAnim.finalBoneMatrices.data(), MAX_BONES);
                girl.draw(depthSkinned);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, SCR_W, SCR_H);
        }

        // ---- main pass ----
        glClearColor(0.05f, 0.06f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glm::mat4 view = cam.view();
        glm::mat4 proj = glm::perspective(glm::radians(cam.fov), (float)SCR_W / (float)SCR_H, 0.1f, 200.0f);

        auto setSceneUniforms = [&](const Shader& s) {
            s.use();
            s.setMat4("view", view);
            s.setMat4("proj", proj);
            s.setMat4("lightSpaceMatrix", lightSpace);
            s.setVec3("viewPos", cam.pos);
            s.setVec3("pointLightPos", pointPos);
            s.setVec3("pointLightColor", pointCol);
            s.setVec3("spotPos", spotPos);
            s.setVec3("spotDir", spotDir);
            s.setVec3("spotColor", spotColor);
            s.setFloat("spotCutoff", std::cos(glm::radians(20.0f)));
            s.setFloat("spotOuter",  std::cos(glm::radians(28.0f)));
            s.setVec3("fogColor", glm::vec3(0.45f, 0.4f, 0.5f));
            s.setFloat("fogStart", tog.fog ? 8.0f  : 1000.0f);
            s.setFloat("fogEnd",   tog.fog ? 60.0f : 1001.0f);
            s.setInt("useShadows", tog.shadows ? 1 : 0);
            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, shadowTex); s.setInt("shadowMap", 3);
            if (haveSkybox) { glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.cubemap); s.setInt("envMap", 4); }
        };

        // opaque static geometry
        setSceneUniforms(scene);
        for (auto& it : items) if (!it.useAlpha) renderItem(it, scene);

        // animated girl
        if (haveGirl) {
            setSceneUniforms(sceneSkinned);
            sceneSkinned.setMat4Array("bones", girlAnim.finalBoneMatrices.data(), MAX_BONES);
            sceneSkinned.setMat4("model", girlModel);
            sceneSkinned.setInt("useMultiTex", 0);
            sceneSkinned.setInt("useNormalMap", tog.normalMap ? 1 : 0);
            sceneSkinned.setInt("useEnvMap", 0);
            sceneSkinned.setInt("useAlpha", 0);
            girl.draw(sceneSkinned);
        }

        // transparent
        if (tog.blending) {
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            scene.use();
            for (auto& it : items) if (it.useAlpha) renderItem(it, scene);
            glDisable(GL_BLEND);
        }

        if (haveSkybox && tog.skybox) skybox.draw(view, proj);

        // ---- HUD overlay ----
        // Persistent legend bottom-left.
        static const std::string legend =
            "1 Multi-tex   2 Normal   3 EnvMap   4 Fog   5 Blending\n"
            "6 Shadow      7 Spot     8 PtLight  9 Sky   C Cinematica\n"
            "WASD mover  Mouse olhar  Space/Shift sub/desc  Esc sair";
        // Background plate behind legend
        hud.drawRect(20, SCR_H - 110, 760, 90, SCR_W, SCR_H, glm::vec4(0, 0, 0, 0.55f));
        hud.draw(legend, 30, SCR_H - 95, SCR_W, SCR_H, glm::vec4(1, 1, 1, 0.95f), 2.0f);
        // Transient tooltip (top-center) — fades out after HUD_DURATION
        if (now < g_hudExpireAt && !g_hudText.empty()) {
            float alpha = std::min(1.0f, (g_hudExpireAt - now));
            hud.drawRect(20, 20, 900, 70, SCR_W, SCR_H, glm::vec4(0, 0, 0, 0.6f * alpha));
            hud.draw(g_hudText, 30, 35, SCR_W, SCR_H, glm::vec4(1, 0.95f, 0.7f, alpha), 2.5f);
        }

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    ma_engine_uninit(&audio);
    glfwTerminate();
    return 0;
}

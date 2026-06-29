#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <filesystem>
#include <cmath>
#include <vector>
#include <functional>
#include <cstdlib>

#include "shader.h"
#include "camera.h"
#include "model.h"
#include "skybox.h"
#include "primitives.h"
#include "animated_model.h"
#include "animator.h"
#include "procgen.h"
#include "portal.h"
#include "render.h"
#include "hud.h"

// ----- window / camera state -----
static int  SCR_W = 1280, SCR_H = 720;
static Camera cam;
static float lastX = SCR_W / 2.0f, lastY = SCR_H / 2.0f;
static bool firstMouse = true;
static float dt = 0.0f, lastFrame = 0.0f;

// ----- click pick state (consumed by main loop) -----
static bool g_clickPending = false;   // LMB: turret serenade, else blue portal
static bool g_orangePending = false;  // RMB: orange portal
static bool g_gunVisible = true;      // G toggles portal gun viewmodel
static bool g_grabTogglePending = false; // E: grab/drop cube
static bool g_thirdPerson = false;    // F5: 1st/3rd person camera
static bool g_playerMode = false;     // F: free camera <-> grounded player (gravity+jump)
static bool g_npcVisible = true;      // N: show the NPC girl (1st person)
static bool g_npcWalk    = false;     // B: NPC walks a path (true) vs jumps in place (false)

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
static void onMouseButton(GLFWwindow*, int button, int action, int /*mods*/) {
    if (button == GLFW_MOUSE_BUTTON_LEFT  && action == GLFW_PRESS) g_clickPending = true;
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) g_orangePending = true;
}

static bool wasPressed[18] = {};
static bool g_stopMusicPending = false;
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
    if (g_playerMode) {
        // Grounded walk: move on the XZ plane only (look pitch can't lift you off
        // the floor). Vertical motion is gravity + jump, integrated in the loop.
        glm::vec3 flat(cam.front.x, 0.0f, cam.front.z);
        if (glm::length(flat) > 1e-4f) flat = glm::normalize(flat);
        float v = cam.speed * dt;
        if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) cam.pos += flat * v;
        if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) cam.pos -= flat * v;
        if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) cam.key(2, dt); // strafe (already horizontal)
        if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) cam.key(3, dt);
    } else {
        if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) cam.key(0, dt);
        if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) cam.key(1, dt);
        if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) cam.key(2, dt);
        if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) cam.key(3, dt);
        if (glfwGetKey(w, GLFW_KEY_SPACE) == GLFW_PRESS) cam.key(4, dt);
        if (glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) cam.key(5, dt);
    }
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
    if (edge(w, GLFW_KEY_G, 11)) toggle(g_gunVisible,  "G portal gun",     "Mostra/esconde a portal gun na mao. Escondida = nao dispara.");
    if (edge(w, GLFW_KEY_E, 12)) g_grabTogglePending = true;
    if (edge(w, GLFW_KEY_F5, 13)) toggle(g_thirdPerson, "F5 camera",        "Alterna 1a / 3a pessoa (estilo Minecraft).");
    if (edge(w, GLFW_KEY_F, 14)) toggle(g_playerMode,  "F modo jogador",   "Camera livre <-> Jogador no chao (gravidade, Space pula). Voo so na camera livre.");
    if (edge(w, GLFW_KEY_N, 16)) toggle(g_npcVisible,  "N npc",            "Mostra/esconde a garota NPC (1a pessoa).");
    if (edge(w, GLFW_KEY_B, 17)) toggle(g_npcWalk,     "B andar/pular",    "NPC anda em volta (ON) ou pula no lugar (OFF) p/ mostrar a fisica.");
    if (edge(w, GLFW_KEY_P, 10)) g_stopMusicPending = true;
}

static void banner() {
    std::cout << R"(
=== Trabalho Final de Computação Gráfica ===
Cena: Casa moderna ao ar livre — apresentação arquitetônica
Author: Guilherme Luiz Cella (105491)

Controles:
  WASD            Movimento
  Mouse           Olhar
  Space / Shift   Subir / Descer (camera livre) | Space = pular (jogador)
  F               Modo camera livre <-> jogador (gravidade)
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

Interagir:
  Click esq.      Mirar turret = serenata, senao Portal AZUL
  Click dir.      Portal LARANJA
  G               Mostra/esconde a portal gun
  E               Agarrar / soltar o cubo
  F5              Camera 1a / 3a pessoa
  P               Parar musica da turret
  (entre num portal p/ teleportar ao outro)
============================================
)";
}


// Portal quad mesh (GL/Mesh glue; pure portal math lives in portal.h/.cpp).
static Mesh buildPortalQuad() {
    Mesh m;
    glm::vec3 n(0, 0, 1), t(1, 0, 0);
    m.vertices = {
        {{-PORTAL_HW, -PORTAL_HH, 0}, n, {0, 0}, t},
        {{ PORTAL_HW, -PORTAL_HH, 0}, n, {1, 0}, t},
        {{ PORTAL_HW,  PORTAL_HH, 0}, n, {1, 1}, t},
        {{-PORTAL_HW,  PORTAL_HH, 0}, n, {0, 1}, t},
    };
    m.indices = {0, 1, 2, 0, 2, 3};
    m.setup();
    return m;
}

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
    glfwSetMouseButtonCallback(win, onMouseButton);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) { std::cerr << "glad fail\n"; return 1; }
    // Use framebuffer pixels (Retina = 2× window points) so the viewport fills the
    // screen instead of one corner.
    glfwGetFramebufferSize(win, &SCR_W, &SCR_H);
    glViewport(0, 0, SCR_W, SCR_H);
    glEnable(GL_DEPTH_TEST);

    // ----- shaders -----
    Shader scene;        scene.load("shaders/scene.vs", "shaders/scene.fs");
    Shader sceneSkinned; sceneSkinned.load("shaders/skinned.vs", "shaders/scene.fs");
    Shader depth;        depth.load("shaders/depth.vs", "shaders/depth.fs");
    Shader depthSkinned; depthSkinned.load("shaders/skinned_depth.vs", "shaders/depth.fs");
    setupShadow();
    HUD hud; hud.setup();

    // Bone matrices live in a UBO (not a plain uniform array): this rig has 432
    // bones, far past the ~240-mat4 default-block component limit on this GL.
    unsigned int boneUBO = 0;
    glGenBuffers(1, &boneUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, boneUBO);
    glBufferData(GL_UNIFORM_BUFFER, MAX_BONES * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, boneUBO);
    for (const Shader* s : {&sceneSkinned, &depthSkinned}) {
        unsigned int bi = glGetUniformBlockIndex(s->id, "Bones");
        if (bi != GL_INVALID_INDEX) glUniformBlockBinding(s->id, bi, 0);
    }

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
    bool haveSkybox = true;

    // ----- procedural geometry -----
    Mesh ground = makePlane(160.0f, 32.0f); // large enough to cover the forest
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
    unsigned int texGrass  = pickTex("assets/textures/grass_diffuse.png",  genGrass,  256);
    unsigned int texBrick  = pickTex("assets/textures/brick_diffuse.jpg",  genBrick,  256);
    unsigned int texBrickN;
    if (fs::exists("assets/textures/brick_normal.jpg")) texBrickN = loadTexture("assets/textures/brick_normal.jpg", false);
    else { std::cerr << "[procgen] brick normal missing — generated\n"; texBrickN = genBrickNormal(256); }
    unsigned int texGlass;
    if (fs::exists("assets/textures/glass.png")) texGlass = loadTexture("assets/textures/glass.png");
    else { std::cerr << "[procgen] glass missing — generated\n"; texGlass = genGlass(64); }

    unsigned int texSand = genSand(16);

    // Sand path: a flat strip down the forest corridor (z ≈ 0), slightly above ground.
    Mesh pathQuad;
    {
        const float x0 = 16.0f, x1 = 62.0f, zw = 3.0f, y = 0.03f;
        const float tu = (x1 - x0) / 2.0f, tv = (2.0f * zw) / 2.0f; // 1 tile / 2m
        glm::vec3 n(0, 1, 0), t(1, 0, 0);
        pathQuad.vertices = {
            {{x0, y, -zw}, n, {0, 0},   t}, {{x1, y, -zw}, n, {tu, 0},  t},
            {{x1, y,  zw}, n, {tu, tv}, t}, {{x0, y,  zw}, n, {0, tv},  t},
        };
        pathQuad.indices = {0, 1, 2, 0, 2, 3};
        pathQuad.setup();
    }

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
        // Override .3ds materials with a stone-wall diffuse.
        const char* castleTexPath = "assets/textures/castle_diffuse.png";
        if (fs::exists(castleTexPath)) {
            unsigned int t = loadTexture(castleTexPath, true);
            for (auto& m : castle.meshes) m.textures = {{t, "diffuse", castleTexPath}};
        }
    }

    Model minecraftHouse;
    glm::vec3 mcLocalLo(0.0f), mcLocalHi(0.0f);
    if (fs::exists("assets/models/minecraft_house/minecraft_house.glb")) {
        minecraftHouse.load("assets/models/minecraft_house/minecraft_house.glb");
        glm::vec3 lo(1e9f), hi(-1e9f);
        size_t tv = 0;
        for (auto& m : minecraftHouse.meshes) {
            for (auto& v : m.vertices) { lo = glm::min(lo, v.pos); hi = glm::max(hi, v.pos); }
            tv += m.vertices.size();
        }
        mcLocalLo = lo; mcLocalHi = hi;
        glm::vec3 d = hi - lo;
        std::cerr << "[load] minecraft_house meshes=" << minecraftHouse.meshes.size() << " verts=" << tv
                  << " size=(" << d.x << "," << d.y << "," << d.z << ")\n";
    }

    Model turret;
    glm::vec3 turretLocalLo(0.0f), turretLocalHi(0.0f);
    if (fs::exists("assets/models/turret/turret.glb")) {
        turret.load("assets/models/turret/turret.glb");
        glm::vec3 lo(1e9f), hi(-1e9f);
        size_t tv = 0;
        for (auto& m : turret.meshes) {
            for (auto& v : m.vertices) { lo = glm::min(lo, v.pos); hi = glm::max(hi, v.pos); }
            tv += m.vertices.size();
        }
        turretLocalLo = lo; turretLocalHi = hi;
        glm::vec3 d = hi - lo;
        std::cerr << "[load] turret meshes=" << turret.meshes.size() << " verts=" << tv
                  << " size=(" << d.x << "," << d.y << "," << d.z << ")\n";
    }

    AnimatedModel girl;
    Animator girlAnim;
    // Accept either .fbx (Mixamo) or .glb (Khronos CesiumMan etc) — assimp handles both.
    for (const char* p : {"assets/models/girl/a21_-_pc_-_lila_decyrus_swimsuit.glb",
                          "assets/models/girl/girl.fbx", "assets/models/girl/girl.glb"}) {
        if (fs::exists(p)) {
            girl.load(p);
            girlAnim.setModel(&girl);
            std::cerr << "[load] girl=" << p << " meshes=" << girl.meshes.size()
                      << " anims=" << (girl.scene ? girl.scene->mNumAnimations : 0) << "\n";
            if (!girl.meshes.empty()) reportBBox("girl[0]", girl.meshes[0].vertices);
            break;
        }
    }
    // Auto-fit: some rigs export huge / off-origin (this one is ~61 units tall at
    // y≈30). Scale to ~1.7m and bake a feet-to-origin offset so placement is sane.
    glm::vec3 girlFitCtr(0.0f); float girlFitScale = 1.0f;
    if (!girl.meshes.empty()) {
        glm::vec3 lo(1e9f), hi(-1e9f);
        for (auto& m : girl.meshes) for (auto& v : m.vertices) { lo = glm::min(lo, v.pos); hi = glm::max(hi, v.pos); }
        glm::vec3 c = (lo + hi) * 0.5f;
        float h = hi.y - lo.y;
        girlFitScale = (h > 1e-3f) ? (1.7f / h) : 1.0f;
        girlFitCtr = glm::vec3(c.x, lo.y, c.z);   // xz centre, feet at min-y
        std::cerr << "[girl] fit scale=" << girlFitScale << " feetCtr=("
                  << girlFitCtr.x << "," << girlFitCtr.y << "," << girlFitCtr.z << ")\n";
    }
    glm::mat4 girlFit = glm::scale(glm::mat4(1.0f), glm::vec3(girlFitScale))
                      * glm::translate(glm::mat4(1.0f), -girlFitCtr);

    // ----- portal gun (hand viewmodel) + grabbable cube -----
    auto loadProp = [](const char* path, Model& m, glm::vec3& center, float& scale,
                       float targetSize, const char* tag) -> bool {
        if (!fs::exists(path)) { std::cerr << "[load] " << tag << " missing: " << path << "\n"; return false; }
        m.load(path);
        if (m.meshes.empty()) return false;
        glm::vec3 lo(1e9f), hi(-1e9f);
        for (auto& mesh : m.meshes) for (auto& v : mesh.vertices) { lo = glm::min(lo, v.pos); hi = glm::max(hi, v.pos); }
        center = (lo + hi) * 0.5f;
        float md = std::max(hi.x - lo.x, std::max(hi.y - lo.y, hi.z - lo.z));
        scale = (md > 1e-4f) ? (targetSize / md) : 1.0f;
        std::cerr << "[load] " << tag << " meshes=" << m.meshes.size() << " scale=" << scale << "\n";
        return true;
    };

    Model portalGun; glm::vec3 gunCenter(0.0f); float gunScale = 1.0f;
    bool haveGun = loadProp("assets/models/portalgun/hd_portal_gun.glb", portalGun, gunCenter, gunScale, 0.45f, "portal gun");
    // Viewmodel placement in camera space (right, up, forward) + facing fix.
    // ponytail: tune by eye — depends on the model's own forward axis/pivot.
    // Calibration knobs, not magic numbers. If gun faces backward, set yaw to 0.
    glm::vec3 GUN_OFFSET = {0.22f, -0.18f, 0.45f};
    glm::vec3 GUN_EULER  = {0.0f, 180.0f, 0.0f}; // deg pitch/yaw/roll fix

    Model cube; glm::vec3 cubeCenter(0.0f); float cubeScale = 1.0f;
    bool haveCube = loadProp("assets/models/cube/portal_cube.glb", cube, cubeCenter, cubeScale, 0.9f, "cube");
    glm::vec3 cubePos{2.5f, 0.45f, 3.5f};
    glm::vec3 cubeVel{0.0f};
    bool cubeGrabbed = false;
    float cubeCooldown = 0.0f;
    const float CUBE_REST_Y = 0.45f; // half-height; cube sits on ground at this y

    Mesh portalQuad = buildPortalQuad();
    Portal portalBlue, portalOrange;
    float portalCooldownUntil = 0.0f;

    // ----- Lorax tree + falling leaves -----
    Model loraxTree;
    bool haveLorax = false;
    glm::mat4 loraxXform(1.0f);
    glm::vec3 leafSpawn(0.0f);
    float leafSpawnRadius = 1.5f;
    glm::vec3 loraxLo(0.0f), loraxCenter(0.0f); // for grounding forest instances
    float loraxBaseScale = 1.0f;
    if (fs::exists("assets/models/lorax/lorax_tree.glb")) {
        loraxTree.load("assets/models/lorax/lorax_tree.glb");
        haveLorax = !loraxTree.meshes.empty();
        glm::vec3 lo(1e9f), hi(-1e9f);
        for (auto& m : loraxTree.meshes) for (auto& v : m.vertices) { lo = glm::min(lo, v.pos); hi = glm::max(hi, v.pos); }
        glm::vec3 c = (lo + hi) * 0.5f;
        float md = std::max(hi.x - lo.x, std::max(hi.y - lo.y, hi.z - lo.z));
        float s = (md > 1e-4f) ? (8.0f / md) : 1.0f;
        loraxLo = lo; loraxCenter = c; loraxBaseScale = s;
        loraxXform = glm::translate(glm::mat4(1.0f), glm::vec3(12.0f, 0.0f, 8.0f))
                   * glm::scale(glm::mat4(1.0f), glm::vec3(s))
                   * glm::translate(glm::mat4(1.0f), glm::vec3(-c.x, -lo.y, -c.z));
        leafSpawn = glm::vec3(loraxXform * glm::vec4(c.x, hi.y * 0.85f, c.z, 1.0f));
        leafSpawnRadius = (hi.x - lo.x) * s * 0.4f;
        std::cerr << "[load] lorax tree meshes=" << loraxTree.meshes.size() << "\n";
    } else std::cerr << "[load] lorax tree missing\n";

    // A falling leaf = small quad tinted to match the tree foliage (mat0 lime).
    Mesh leafQuad;
    {
        glm::vec3 n(0, 0, 1), t(1, 0, 0);
        leafQuad.vertices = {
            {{-0.5f, -0.5f, 0}, n, {0, 0}, t}, {{0.5f, -0.5f, 0}, n, {1, 0}, t},
            {{0.5f, 0.5f, 0}, n, {1, 1}, t},   {{-0.5f, 0.5f, 0}, n, {0, 1}, t},
        };
        leafQuad.indices = {0, 1, 2, 0, 2, 3};
        leafQuad.setup();
    }
    unsigned int leafTex = 0;
    {
        unsigned char px[3] = {233, 255, 78}; // ≈ mat0 (0.91, 1.0, 0.30)
        glGenTextures(1, &leafTex);
        glBindTexture(GL_TEXTURE_2D, leafTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    struct Leaf { glm::vec3 pos{0.0f}, vel{0.0f}; float rot = 0, rotSpd = 0; bool active = false; };
    std::vector<Leaf> leaves(10);
    float nextLeafAt = 1.0f;
    auto frand = []() { return (float)std::rand() / (float)RAND_MAX; };

    // ----- audio -----
    ma_engine audio;
    if (ma_engine_init(nullptr, &audio) != MA_SUCCESS) std::cerr << "audio init fail\n";
    bool haveAmbient  = fs::exists("assets/audio/ambient.ogg");
    bool haveFootstep = fs::exists("assets/audio/footstep.ogg");
    if (haveAmbient) ma_engine_play_sound(&audio, "assets/audio/ambient.ogg", nullptr);
    float nextFootstep = 0.0f;

    // Turret serenade: stateful sound so we can stop/restart on click.
    ma_sound turretSound;
    bool haveTurretSound = (ma_sound_init_from_file(&audio, "assets/audio/turret_serenade.mp3",
                                                   0, nullptr, nullptr, &turretSound) == MA_SUCCESS);
    if (!haveTurretSound) std::cerr << "[audio] turret_serenade.mp3 load fail\n";

    // Portal-gun fire sfx (converted from .m4r/AAC to wav so miniaudio decodes it).
    const char* gunSfxPath = "assets/audio/portal_fire.wav";
    bool haveGunSfx = fs::exists(gunSfxPath);
    if (!haveGunSfx) std::cerr << "[audio] portal gun sfx missing\n";

    // ----- minecraft house transform: scale to ~12m wide, sit on ground -----
    glm::mat4 mcXform(1.0f);
    {
        glm::vec3 size = mcLocalHi - mcLocalLo;
        float maxDim = std::max(size.x, std::max(size.y, size.z));
        float scale  = (maxDim > 0.001f) ? (14.0f / maxDim) : 1.0f;
        glm::vec3 center = (mcLocalLo + mcLocalHi) * 0.5f;
        mcXform = glm::translate(glm::mat4(1.0f), glm::vec3(-30.0f, 0.0f, -12.0f));
        mcXform = glm::scale(mcXform, glm::vec3(scale));
        mcXform = glm::translate(mcXform, glm::vec3(-center.x, -mcLocalLo.y, -center.z));
    }
    glm::vec3 mcWLo, mcWHi;
    worldAABB(mcLocalLo, mcLocalHi, mcXform, mcWLo, mcWHi);

    // ----- turret transform: scale to ~2m tall, foreground -----
    glm::mat4 turretXform(1.0f);
    glm::vec3 turretWorldLo(0.0f), turretWorldHi(0.0f);
    {
        glm::vec3 size = turretLocalHi - turretLocalLo;
        float scale = (size.y > 0.001f) ? (2.0f / size.y) : 1.0f;
        glm::vec3 center = (turretLocalLo + turretLocalHi) * 0.5f;
        turretXform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 6.0f));
        turretXform = glm::scale(turretXform, glm::vec3(scale));
        turretXform = glm::translate(turretXform, glm::vec3(-center.x, -turretLocalLo.y, -center.z));
        // Pre-compute world AABB by transforming the 8 local corners.
        glm::vec3 lo(1e9f), hi(-1e9f);
        for (int i = 0; i < 8; ++i) {
            glm::vec3 c(
                (i & 1) ? turretLocalHi.x : turretLocalLo.x,
                (i & 2) ? turretLocalHi.y : turretLocalLo.y,
                (i & 4) ? turretLocalHi.z : turretLocalLo.z
            );
            glm::vec3 w = glm::vec3(turretXform * glm::vec4(c, 1.0f));
            lo = glm::min(lo, w); hi = glm::max(hi, w);
        }
        turretWorldLo = lo; turretWorldHi = hi;
    }

    // Ray vs AABB (slab method). Returns true if ray hits within positive t.
    auto rayHitsAABB = [](glm::vec3 ro, glm::vec3 rd, glm::vec3 lo, glm::vec3 hi) {
        float tmin = 0.0f, tmax = 1e9f;
        for (int i = 0; i < 3; ++i) {
            if (std::fabs(rd[i]) < 1e-6f) {
                if (ro[i] < lo[i] || ro[i] > hi[i]) return false;
            } else {
                float t1 = (lo[i] - ro[i]) / rd[i];
                float t2 = (hi[i] - ro[i]) / rd[i];
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) return false;
            }
        }
        return tmax >= 0.0f;
    };

    // Turret scan animation: sweep yaw about its vertical base axis (no baked clip).
    glm::vec3 turretBasePos{0.0f, 0.0f, 6.0f};
    glm::mat4 turretDrawXform = turretXform;

    // Fire a portal: raycast camera ray vs ground + brick wall + minecraft house.
    auto castPortal = [&](Portal& p) -> bool {
        glm::vec3 ro = cam.pos, rd = glm::normalize(cam.front);
        float bestT = 1e9f; glm::vec3 bestN(0.0f), bestHit(0.0f); bool hit = false;
        float t; glm::vec3 n, h;
        if (rayPlaneY(ro, rd, 0.0f, 40.0f, t, n, h) && t < bestT) { bestT = t; bestN = n; bestHit = h; hit = true; }
        glm::vec3 wallLo(-17.0f, 0.0f, -8.25f), wallHi(-11.0f, 4.0f, -7.75f);
        if (rayAABBn(ro, rd, wallLo, wallHi, t, n, h) && t < bestT) { bestT = t; bestN = n; bestHit = h; hit = true; }
        if (!minecraftHouse.meshes.empty() && rayAABBn(ro, rd, mcWLo, mcWHi, t, n, h) && t < bestT) { bestT = t; bestN = n; bestHit = h; hit = true; }
        if (!hit) return false;
        p.pos = bestHit + bestN * 0.02f; p.normal = bestN;
        portalBasis(bestN, p.right, p.up); p.active = true;
        return true;
    };

    // ----- scene description -----
    auto buildItems = [&]() {
        std::vector<DrawItem> items;
        DrawItem g; g.mesh = &ground;
        g.diffuse0 = texCobble; g.diffuse1 = texGrass;
        g.useMultiTex = true;
        items.push_back(g);

        DrawItem path; path.mesh = &pathQuad; path.diffuse0 = texSand;
        items.push_back(path);

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

        // Spec § 2a coverage — .3DS castle as background prop.
        if (!castle.meshes.empty()) {
            DrawItem c; c.model = &castle;
            c.xform = glm::translate(glm::mat4(1.0f), {22.0f, 0.0f, -25.0f});
            c.xform = glm::scale(c.xform, glm::vec3(3.0f));
            items.push_back(c);
        }
        if (haveLorax) {
            DrawItem tr; tr.model = &loraxTree; tr.xform = loraxXform;
            items.push_back(tr);

            // Forest grid with a clear path: a corridor along +X (z ≈ 0) stays empty.
            // Deterministic hash jitter so trees don't flicker frame to frame.
            auto h = [](int a, int b) {
                uint32_t x = (uint32_t)a * 73856093u ^ (uint32_t)b * 19349663u;
                x = (x ^ (x >> 13)) * 1274126177u;
                return (float)((x ^ (x >> 16)) & 0xFFFF) / 65535.0f;
            };
            for (int i = 0; i < 8; ++i) for (int j = 0; j < 9; ++j) {
                float px = 26.0f + i * 4.5f + (h(i, j) - 0.5f) * 3.0f;
                float pz = -18.0f + j * 4.5f + (h(i + 99, j) - 0.5f) * 3.0f;
                if (std::fabs(pz) < 3.0f) continue;        // keep the path clear
                float sc = loraxBaseScale * (0.6f + h(i, j + 7) * 0.6f);
                float yaw = h(i + 3, j + 5) * 6.2832f;
                glm::mat4 x = glm::translate(glm::mat4(1.0f), glm::vec3(px, 0.0f, pz))
                            * glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0, 1, 0))
                            * glm::scale(glm::mat4(1.0f), glm::vec3(sc))
                            * glm::translate(glm::mat4(1.0f), glm::vec3(-loraxCenter.x, -loraxLo.y, -loraxCenter.z));
                DrawItem t; t.model = &loraxTree; t.xform = x;
                items.push_back(t);
            }
        }
        if (!minecraftHouse.meshes.empty()) {
            DrawItem m; m.model = &minecraftHouse; m.xform = mcXform;
            m.useTexAlpha = true; // glass windows: texture alpha → see-through
            items.push_back(m);
        }
        if (!turret.meshes.empty()) {
            DrawItem t; t.model = &turret; t.xform = turretDrawXform;
            items.push_back(t);
        }
        if (haveCube) {
            DrawItem cu; cu.model = &cube;
            cu.xform = glm::translate(glm::mat4(1.0f), cubePos);
            cu.xform = glm::scale(cu.xform, glm::vec3(cubeScale));
            cu.xform = glm::translate(cu.xform, -cubeCenter);
            items.push_back(cu);
        }
        return items;
    };

    // CesiumMan-style Z-up rigs need a -90° X flip; detect via presence of clips.
    bool girlNeedsZFlip = (girl.scene && girl.scene->mNumAnimations > 0);

    // 3rd-person avatar (the girl) facing + procedural limb swing while moving.
    // ponytail: girl.glb has 0 anim clips, so the walk is faked by rotating the
    // arm/leg bones rigidly about their joints. A real Mixamo FBX would animate
    // through the skinned pipeline already wired.
    const float GIRL_YAW_FIX = 0.0f;   // deg; +Z faces look dir → camera sees back
    glm::vec3 prevCamPos = cam.pos;

    // Player-mode physics: simple vertical-only gravity over a flat floor.
    // ponytail: ground is the y=0 plane, eye sits at EYE_H. Upgrade to terrain
    // height sampling / AABB collision if the floor stops being flat.
    const float EYE_H = 1.7f;
    float playerVelY = 0.0f;
    bool  grounded   = true;

    // Spring-bone secondary motion for the chest (VRoid J_Sec_*_Bust bones).
    // Damped harmonic oscillator pushed by the torso's vertical motion → lagged jiggle.
    float bustAng = 0.0f, bustVel = 0.0f;
    float prevBounce = 0.0f;   // last frame's torso vertical offset (for spring drive)

    // Resolve a logical bone to the rig's actual key: exact name first, else the
    // first key containing a candidate substring. Handles VRoid (J_Bip / J_Sec) or
    // the Epic-Seven (SK_ / SWING) skeleton.
    auto resolveBone = [&](std::initializer_list<const char*> cands) -> std::string {
        for (const char* c : cands) if (girl.boneMap.count(c)) return c;
        for (auto& kv : girl.boneMap)
            for (const char* c : cands)
                if (kv.first.find(c) != std::string::npos) return kv.first;
        return std::string();
    };
    // All bone ids in the subtree rooted at `root`. Rotating the WHOLE limb chain
    // (upper, lower, hand, twist bones, fingers) rigidly avoids the stretching you
    // get from moving only the named bones while the rest stay at bind pose.
    auto subtreeBones = [&](const std::string& root) -> std::vector<int> {
        std::vector<int> ids;
        if (root.empty() || !girl.scene) return ids;
        std::function<void(const aiNode*, bool)> dfs = [&](const aiNode* n, bool inside) {
            bool here = inside || root == std::string(n->mName.C_Str());
            if (here) { auto it = girl.boneMap.find(n->mName.C_Str());
                        if (it != girl.boneMap.end()) ids.push_back(it->second.id); }
            for (unsigned i = 0; i < n->mNumChildren; ++i) dfs(n->mChildren[i], here);
        };
        dfs(girl.scene->mRootNode, false);
        return ids;
    };
    struct Limb { std::vector<int> ids; glm::vec3 pivot{0.0f}; };
    auto makeLimb = [&](const std::string& root) -> Limb {
        Limb L; L.ids = subtreeBones(root);
        auto it = girl.boneMap.find(root);
        if (it != girl.boneMap.end()) L.pivot = glm::vec3(glm::inverse(it->second.offset)[3]);
        return L;
    };
    Limb lArm  = makeLimb(resolveBone({"J_Bip_L_UpperArm", "SK_L_Arm_"}));
    Limb rArm  = makeLimb(resolveBone({"J_Bip_R_UpperArm", "SK_R_Arm_"}));
    Limb lLeg  = makeLimb(resolveBone({"J_Bip_L_UpperLeg", "SK_L_UpLeg_"}));
    Limb rLeg  = makeLimb(resolveBone({"J_Bip_R_UpperLeg", "SK_R_UpLeg_"}));
    Limb lBust = makeLimb(resolveBone({"J_Sec_L_Bust1", "SWING000_L_Bust"}));
    Limb rBust = makeLimb(resolveBone({"J_Sec_R_Bust1", "SWING000_R_Bust"}));

    // Rotate every bone in a limb subtree rigidly about its root pivot. ang=0 → bind.
    auto swing = [&](const Limb& L, float ang, glm::vec3 axis) {
        if (L.ids.empty()) return;
        glm::mat4 Rw = glm::translate(glm::mat4(1.0f), L.pivot)
                     * glm::rotate(glm::mat4(1.0f), ang, axis)
                     * glm::translate(glm::mat4(1.0f), -L.pivot);
        for (int id : L.ids)
            if (id < (int)girlAnim.finalBoneMatrices.size())
                girlAnim.finalBoneMatrices[id] = Rw;
    };
    std::cerr << "[girl] limb bone counts: lArm=" << lArm.ids.size() << " lLeg=" << lLeg.ids.size()
              << " bust=" << lBust.ids.size() << "\n";

    // Turret: tracks the player when near, else slow scan (Portal-style sentry).
    float turretYaw = 0.0f;
    const float TURRET_YAW_FIX = 0.0f; // deg; align model front to +Z if needed

    while (!glfwWindowShouldClose(win)) {
        float now = (float)glfwGetTime();
        dt = now - lastFrame; lastFrame = now;
        input(win);

        // Player-mode gravity + jump (free camera is untouched).
        if (g_playerMode) {
            if (glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS && grounded) {
                playerVelY = 6.0f; grounded = false;     // jump impulse
            }
            playerVelY -= 16.0f * dt;                    // gravity
            cam.pos.y += playerVelY * dt;
            if (cam.pos.y <= EYE_H) { cam.pos.y = EYE_H; playerVelY = 0.0f; grounded = true; }
        }

        // LMB: serenade if aiming at turret, else fire blue portal. RMB: orange.
        auto fireSfx = [&]() { if (haveGunSfx) ma_engine_play_sound(&audio, gunSfxPath, nullptr); };
        if (g_clickPending) {
            g_clickPending = false;
            bool onTurret = haveTurretSound && !turret.meshes.empty() &&
                            rayHitsAABB(cam.pos, cam.front, turretWorldLo, turretWorldHi);
            if (onTurret) {
                if (ma_sound_is_playing(&turretSound)) {
                    ma_sound_stop(&turretSound);
                    ma_sound_seek_to_pcm_frame(&turretSound, 0);
                    g_hudText = "Turret: silenciada";
                } else {
                    ma_sound_seek_to_pcm_frame(&turretSound, 0);
                    ma_sound_start(&turretSound);
                    g_hudText = "Turret Wife Serenade — clique novamente p/ parar";
                }
                g_hudExpireAt = (float)glfwGetTime() + HUD_DURATION;
            } else if (g_gunVisible && castPortal(portalBlue)) {
                fireSfx();
                g_hudText = "Portal AZUL"; g_hudExpireAt = (float)glfwGetTime() + HUD_DURATION;
            }
        }
        if (g_orangePending) {
            g_orangePending = false;
            if (g_gunVisible && castPortal(portalOrange)) {
                fireSfx();
                g_hudText = "Portal LARANJA"; g_hudExpireAt = (float)glfwGetTime() + HUD_DURATION;
            }
        }
        if (g_stopMusicPending) {
            g_stopMusicPending = false;
            if (haveTurretSound && ma_sound_is_playing(&turretSound)) {
                ma_sound_stop(&turretSound);
                ma_sound_seek_to_pcm_frame(&turretSound, 0);
                g_hudText = "[P] Musica parada";
                g_hudExpireAt = (float)glfwGetTime() + HUD_DURATION;
            }
        }

        // Cube grab/drop (E). Grab if within reach; drop releases with momentum.
        if (g_grabTogglePending) {
            g_grabTogglePending = false;
            if (haveCube) {
                if (!cubeGrabbed && glm::length(cam.pos - cubePos) < 3.5f) {
                    cubeGrabbed = true; cubeVel = glm::vec3(0.0f);
                    g_hudText = "Cubo agarrado — E p/ soltar";
                } else if (cubeGrabbed) {
                    cubeGrabbed = false;
                    cubeVel = glm::normalize(cam.front) * 4.0f; // toss forward
                    g_hudText = "Cubo solto";
                }
                g_hudExpireAt = (float)glfwGetTime() + HUD_DURATION;
            }
        }
        if (haveCube) {
            if (cubeGrabbed) {
                glm::vec3 target = cam.pos + glm::normalize(cam.front) * 1.8f - glm::vec3(0, 0.3f, 0);
                cubePos = target; cubeVel = glm::vec3(0.0f);
            } else {
                // gravity + ground rest
                cubeVel.y -= 9.8f * dt;
                cubePos += cubeVel * dt;
                if (cubePos.y <= CUBE_REST_Y) { cubePos.y = CUBE_REST_Y; cubeVel = glm::vec3(0.0f); }
                // pass through portals like the game (carries momentum)
                portalTeleportBody(portalBlue, portalOrange, cubePos, cubeVel, now, cubeCooldown);
            }
        }

        // Portal teleport: entering an active portal pops you out the linked one.
        portalTeleport(portalBlue, portalOrange, cam.pos, cam.front, cam.yaw, cam.pitch,
                       now, portalCooldownUntil);

        // Turret AI (Portal sentry): tracks the player when near, else slow scan.
        if (!turret.meshes.empty()) {
            glm::vec3 toPlayer = cam.pos - turretBasePos; toPlayer.y = 0.0f;
            float dist = glm::length(toPlayer);
            float targetYaw;
            if (dist < 14.0f && dist > 0.01f)               // lock on
                targetYaw = std::atan2(toPlayer.x, toPlayer.z) + glm::radians(TURRET_YAW_FIX);
            else                                            // idle scan
                targetYaw = glm::radians(45.0f) * std::sin(now * 0.8f);
            // smooth toward target (snappier when locked)
            float k = (dist < 14.0f) ? 6.0f : 2.0f;
            turretYaw += (targetYaw - turretYaw) * std::min(1.0f, k * dt);
            turretDrawXform = glm::translate(glm::mat4(1.0f), turretBasePos)
                            * glm::rotate(glm::mat4(1.0f), turretYaw, glm::vec3(0, 1, 0))
                            * glm::translate(glm::mat4(1.0f), -turretBasePos)
                            * turretXform;
        }

        auto items = buildItems();

        // Falling leaves: occasionally release one from the tree top; it flutters down.
        if (haveLorax && now >= nextLeafAt) {
            for (auto& lf : leaves) if (!lf.active) {
                lf.active = true;
                lf.pos = leafSpawn + glm::vec3((frand() - 0.5f) * 2.0f * leafSpawnRadius, 0.0f,
                                               (frand() - 0.5f) * 2.0f * leafSpawnRadius);
                lf.vel = glm::vec3((frand() - 0.5f) * 0.6f, -0.5f, (frand() - 0.5f) * 0.6f);
                lf.rot = frand() * 6.28f; lf.rotSpd = (frand() - 0.5f) * 4.0f;
                break;
            }
            nextLeafAt = now + 1.5f + frand() * 2.5f;
        }
        for (auto& lf : leaves) if (lf.active) {
            lf.vel.y -= 1.2f * dt;                              // light gravity
            lf.vel.x += std::sin(now * 2.0f + lf.rot) * 0.4f * dt; // flutter sway
            lf.pos += lf.vel * dt;
            lf.rot += lf.rotSpd * dt;
            if (lf.pos.y <= 0.05f) lf.active = false;
            else {
                DrawItem li; li.mesh = &leafQuad; li.diffuse0 = leafTex;
                li.xform = glm::translate(glm::mat4(1.0f), lf.pos);
                li.xform = glm::rotate(li.xform, lf.rot, glm::vec3(0, 1, 0));
                li.xform = glm::rotate(li.xform, lf.rot * 0.7f, glm::vec3(1, 0, 0));
                li.xform = glm::scale(li.xform, glm::vec3(0.18f));
                items.push_back(li);
            }
        }

        bool haveGirl = !girl.meshes.empty();
        bool moving = glm::length(cam.pos - prevCamPos) > 0.0005f;

        // Flying = free camera, 3rd person, lifted off the floor → superman glide.
        bool flying = g_thirdPerson && !g_playerMode && cam.pos.y > EYE_H + 1.0f;
        // NPC (1st person) either jumps in place or walks a path; the avatar (3rd
        // person) walks while the camera moves.
        bool npcJumping = !g_thirdPerson && !g_npcWalk;
        bool walkActive = (g_thirdPerson && moving) || (!g_thirdPerson && g_npcWalk);
        float npcJump = npcJumping ? std::fabs(std::sin(now * 2.6f)) * 0.4f : 0.0f;

        glm::mat4 girlModel;
        float bounce = 0.0f;   // torso vertical offset this frame → drives the springs
        if (g_thirdPerson) {
            // Avatar stands at the player's ground position, faces the look dir.
            glm::vec3 feet(cam.pos.x, 0.0f, cam.pos.z);
            float yawAvatar = std::atan2(cam.front.x, cam.front.z) + glm::radians(GIRL_YAW_FIX);
            float bob = 0.0f, sway = 0.0f;
            if (moving && !flying) {            // fake walk cycle
                bob  = std::fabs(std::sin(now * 9.0f)) * 0.07f;
                sway = std::sin(now * 9.0f) * 0.06f;
            }
            bounce = bob;
            glm::vec3 base = flying ? glm::vec3(cam.pos.x, cam.pos.y - 1.0f, cam.pos.z)
                                    : feet + glm::vec3(0, bob, 0);
            glm::mat4 m = glm::translate(glm::mat4(1.0f), base);
            m = glm::rotate(m, yawAvatar, glm::vec3(0, 1, 0));
            if (flying) m = glm::rotate(m, glm::radians(80.0f), glm::vec3(1, 0, 0)); // pitch face-down
            else        m = glm::rotate(m, sway, glm::vec3(0, 0, 1));               // lean side-to-side
            if (girlNeedsZFlip) m = glm::rotate(m, glm::radians(-90.0f), glm::vec3(1, 0, 0));
            girlModel = m;
        } else if (g_npcWalk) {
            // Patrol: walk a straight line along X, turn 180 at each end, walk back.
            const float SPD = 1.6f, HALF = 5.0f, ZLANE = 2.5f;
            float oneWay = (HALF * 2.0f) / SPD;
            float ph = std::fmod(now, 2.0f * oneWay);
            float x, yawDeg;
            if (ph < oneWay) { x = -HALF + SPD * ph;             yawDeg = -90.0f; } // → +X
            else             { x =  HALF - SPD * (ph - oneWay);  yawDeg =  90.0f; } // → -X
            bounce = std::fabs(std::sin(now * 9.0f)) * 0.05f;
            glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, ZLANE));
            m = glm::rotate(m, glm::radians(yawDeg), glm::vec3(0, 1, 0));
            if (girlNeedsZFlip) m = glm::rotate(m, glm::radians(-90.0f), glm::vec3(1, 0, 0));
            girlModel = m;
        } else {
            // NPC plants in front of spawn, faces the player, hops in place.
            const glm::vec3 GIRL_NPC_POS(0.0f, 0.0f, 1.5f);
            bounce = npcJump;
            glm::mat4 m = glm::translate(glm::mat4(1.0f), GIRL_NPC_POS + glm::vec3(0, npcJump, 0));
            m = glm::rotate(m, glm::radians(180.0f), glm::vec3(0, 1, 0)); // face +Z toward spawn
            if (girlNeedsZFlip) m = glm::rotate(m, glm::radians(-90.0f), glm::vec3(1, 0, 0));
            girlModel = m;
        }
        girlModel = girlModel * girlFit;   // scale/ground the rig into world space
        if (haveGirl) {
            girlAnim.update(dt);
            // Arms rest in an A-pose diagonal, legs hang straight down. Both swing
            // forward/back about the world left-right axis X (a sagittal pendulum).
            glm::vec3 ax(1, 0, 0);
            if (flying) {
                swing(lArm, glm::radians(-150.0f), ax); swing(rArm, glm::radians(-150.0f), ax);
                swing(lLeg, glm::radians(10.0f), ax);   swing(rLeg, glm::radians(10.0f), ax);
            } else if (npcJumping) {
                // Arms swing forward/up on the rise, legs tuck — symmetric.
                float armUp = -glm::radians(40.0f) - npcJump * glm::radians(50.0f);
                swing(lArm, armUp, ax); swing(rArm, armUp, ax);
                float legTuck = npcJump * glm::radians(25.0f);
                swing(lLeg, legTuck, ax); swing(rLeg, legTuck, ax);
            } else {
                // Walk: arms and legs swing opposite (contralateral).
                float a = walkActive ? std::sin(now * 9.0f) * glm::radians(35.0f) : 0.0f;
                swing(lArm,  a, ax); swing(rArm, -a, ax);
                swing(lLeg, -a, ax); swing(rLeg,  a, ax);
            }

            // Chest spring: torso vertical accel drives a lightly-damped oscillator.
            float bounceVel = (bounce - prevBounce) / std::max(dt, 1e-4f);
            prevBounce = bounce;
            bustVel += (-bounceVel * 60.0f - 22.0f * bustAng - 0.9f * bustVel) * dt;
            bustAng += bustVel * dt;
            bustAng = glm::clamp(bustAng, -1.05f, 1.05f);
            swing(lBust, bustAng, ax);
            swing(rBust, bustAng, ax);
        }
        bool showGirl = haveGirl && (g_thirdPerson || g_npcVisible);
        if (showGirl) {   // upload this frame's bone matrices once; all girl passes share the UBO
            glBindBuffer(GL_UNIFORM_BUFFER, boneUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, MAX_BONES * sizeof(glm::mat4), girlAnim.finalBoneMatrices.data());
        }

        // cinematic camera: slow orbit around scene center
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
            if (showGirl) {
                depthSkinned.use();
                depthSkinned.setMat4("lightSpaceMatrix", lightSpace);
                depthSkinned.setMat4("model", girlModel);
                girl.draw(depthSkinned);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, SCR_W, SCR_H);
        }

        // ---- main pass ----
        glClearColor(0.05f, 0.06f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glm::vec3 eye = cam.pos;
        if (g_thirdPerson) eye = cam.pos - glm::normalize(cam.front) * 4.0f + glm::vec3(0, 1.0f, 0);
        glm::mat4 view = glm::lookAt(eye, cam.pos + cam.front, cam.up);
        glm::mat4 proj = glm::perspective(glm::radians(cam.fov), (float)SCR_W / (float)SCR_H, 0.1f, 200.0f);

        auto setSceneUniforms = [&](const Shader& s, const glm::mat4& V) {
            s.use();
            s.setMat4("view", V);
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

        // Draw the skinned girl (bones uploaded once per frame via the UBO).
        auto drawGirl = [&](const glm::mat4& V) {
            if (!haveGirl) return;
            setSceneUniforms(sceneSkinned, V);
            sceneSkinned.setMat4("model", girlModel);
            sceneSkinned.setInt("useMultiTex", 0);
            sceneSkinned.setInt("useNormalMap", tog.normalMap ? 1 : 0);
            sceneSkinned.setInt("useEnvMap", 0);
            sceneSkinned.setInt("useAlpha", 0);
            sceneSkinned.setInt("useEmissive", 0);
            girl.draw(sceneSkinned);
        };

        // ---- main pass ----
        setSceneUniforms(scene, view);
        for (auto& it : items)
            if (!it.useAlpha)
                renderItem(it, scene, tog.multiTex, tog.normalMap, tog.envMap);

        if (showGirl) drawGirl(view);

        // portals — emissive quads (unlit), pulsing
        auto drawPortal = [&](const Portal& p, glm::vec3 col) {
            if (!p.active) return;
            glm::mat4 m(1.0f);
            m[0] = glm::vec4(p.right, 0); m[1] = glm::vec4(p.up, 0);
            m[2] = glm::vec4(p.normal, 0); m[3] = glm::vec4(p.pos, 1);
            scene.use();
            scene.setInt("useEmissive", 1);
            scene.setFloat("uTime", now);
            scene.setVec3("emissiveColor", col);
            scene.setMat4("model", m);
            portalQuad.draw(scene);
            scene.setInt("useEmissive", 0);
        };
        drawPortal(portalBlue,   glm::vec3(0.25f, 0.55f, 1.0f));
        drawPortal(portalOrange, glm::vec3(1.0f, 0.55f, 0.15f));

        // transparent
        if (tog.blending) {
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            scene.use();
            for (auto& it : items) if (it.useAlpha) renderItem(it, scene, tog.multiTex, tog.normalMap, tog.envMap);
            glDisable(GL_BLEND);
        }

        if (haveSkybox && tog.skybox) skybox.draw(view, proj);

        // portal gun viewmodel — glued to view in 1st person, world avatar in 3rd
        if (g_gunVisible && haveGun && !g_thirdPerson) {
            glm::vec3 f = glm::normalize(cam.front);
            glm::vec3 r = glm::normalize(glm::cross(f, cam.up));
            glm::vec3 u = glm::cross(r, f);
            // View-bob: figure-8 sway while walking so the gun bobs with your steps.
            // Vertical at 2x horizontal frequency = classic FPS bob. Zero when still.
            float bobX = moving ? std::cos(now * 5.0f)  * 0.018f : 0.0f;
            float bobY = moving ? std::sin(now * 10.0f) * 0.012f : 0.0f;
            glm::vec3 handPos = cam.pos + f * GUN_OFFSET.z
                              + r * (GUN_OFFSET.x + bobX) + u * (GUN_OFFSET.y + bobY);
            glm::mat4 basis(1.0f);
            basis[0] = glm::vec4(r, 0); basis[1] = glm::vec4(u, 0); basis[2] = glm::vec4(-f, 0);
            glm::mat4 fix(1.0f);
            fix = glm::rotate(fix, glm::radians(GUN_EULER.y), glm::vec3(0, 1, 0));
            fix = glm::rotate(fix, glm::radians(GUN_EULER.x), glm::vec3(1, 0, 0));
            fix = glm::rotate(fix, glm::radians(GUN_EULER.z), glm::vec3(0, 0, 1));
            glm::mat4 M = glm::translate(glm::mat4(1.0f), handPos) * basis * fix
                        * glm::scale(glm::mat4(1.0f), glm::vec3(gunScale))
                        * glm::translate(glm::mat4(1.0f), -gunCenter);
            if (!g_thirdPerson) glClear(GL_DEPTH_BUFFER_BIT); // always-on-top viewmodel
            setSceneUniforms(scene, view);
            scene.setInt("useMultiTex", 0); scene.setInt("useNormalMap", 0);
            scene.setInt("useEnvMap", 0); scene.setInt("useAlpha", 0); scene.setInt("useEmissive", 0);
            scene.setMat4("model", M);
            portalGun.draw(scene);
        }

        // ---- HUD overlay ----
        static const std::string legend =
            "1 Multi-tex   2 Normal   3 EnvMap   4 Fog   5 Blending\n"
            "6 Shadow      7 Spot     8 PtLight  9 Sky   C Cinematica\n"
            "Click esq mirar na turret = serenata   P parar musica\n"
            "WASD mover  F jogador  F5 1a/3a  N npc  B andar/pular";
        hud.drawRect(20, SCR_H - 130, 760, 110, SCR_W, SCR_H, glm::vec4(0, 0, 0, 0.55f));
        hud.draw(legend, 30, SCR_H - 115, SCR_W, SCR_H, glm::vec4(1, 1, 1, 0.95f), 2.0f);
        // Transient tooltip (top-center) — fades out after HUD_DURATION
        if (now < g_hudExpireAt && !g_hudText.empty()) {
            float alpha = std::min(1.0f, (g_hudExpireAt - now));
            hud.drawRect(20, 20, 900, 70, SCR_W, SCR_H, glm::vec4(0, 0, 0, 0.6f * alpha));
            hud.draw(g_hudText, 30, 35, SCR_W, SCR_H, glm::vec4(1, 0.95f, 0.7f, alpha), 2.5f);
        }

        prevCamPos = cam.pos; // for next-frame movement detection (walk anim)
        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    if (haveTurretSound) ma_sound_uninit(&turretSound);
    ma_engine_uninit(&audio);
    glfwTerminate();
    return 0;
}

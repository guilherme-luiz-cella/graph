#pragma once
#include <glm/glm.hpp>
#include "model.h"
#include "shader.h"

// One drawable in the scene: either a procedural Mesh or a loaded Model, plus
// the textures/technique flags it wants. The per-frame toggles (whether a
// technique is globally on) are passed to renderItem, not stored here.
struct DrawItem {
    const Mesh* mesh = nullptr;
    const Model* model = nullptr;
    glm::mat4 xform = glm::mat4(1.0f);
    unsigned int diffuse0 = 0, diffuse1 = 0, normal0 = 0;
    bool useMultiTex = false;
    bool useNormalMap = false;
    bool useEnvMap = false;
    bool useAlpha = false;     // fixed translucency (lantern glass): a = texA * 0.45
    bool useTexAlpha = false;  // texture-driven alpha (house glass): a = texA
};

// Full lit/textured draw. The three *On flags are the global technique toggles.
void renderItem(const DrawItem& it, const Shader& s,
                bool multiTexOn, bool normalMapOn, bool envMapOn);

// Depth-only draw (shadow pass): just the model matrix + geometry.
void renderDepth(const DrawItem& it, const Shader& s);

#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "animated_model.h"

// Samples one aiAnimation against the AnimatedModel's bone map.
// finalBoneMatrices ready to upload to a uniform mat4[MAX_BONES] each frame.
struct Animator {
    AnimatedModel* model = nullptr;
    int animIndex = 0;
    float timeSec = 0.0f;
    std::vector<glm::mat4> finalBoneMatrices;

    void setModel(AnimatedModel* m) {
        model = m;
        finalBoneMatrices.assign(MAX_BONES, glm::mat4(1.0f));
    }
    void setAnim(int idx) { animIndex = idx; timeSec = 0.0f; }
    void update(float dt);
};

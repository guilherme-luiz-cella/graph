#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "shader.h"

struct Skybox {
    unsigned int vao = 0, vbo = 0, cubemap = 0;
    Shader shader;
    void load(const std::vector<std::string>& faces); // +X, -X, +Y, -Y, +Z, -Z
    void draw(const glm::mat4& view, const glm::mat4& proj);
};

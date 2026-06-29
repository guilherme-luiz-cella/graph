#pragma once
#include <string>
#include <glm/glm.hpp>
#include "shader.h"

// 2D screen overlay: text (via stb_easy_font) and filled rects, in pixel coords
// with the origin at the top-left. Used for the legend + transient tooltips.
struct HUD {
    unsigned int vao = 0, vbo = 0, ebo = 0;
    Shader shader;

    void setup();
    void draw(const std::string& text, float x, float y, int screenW, int screenH,
              const glm::vec4& color, float scale = 2.0f);
    void drawRect(float x, float y, float w, float h, int screenW, int screenH,
                  const glm::vec4& color);
};

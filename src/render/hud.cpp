#include "hud.h"
#include <vector>
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_easy_font.h>

void HUD::setup() {
    shader.load("shaders/text.vs", "shaders/text.fs");
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
}

void HUD::draw(const std::string& text, float x, float y, int screenW, int screenH,
               const glm::vec4& color, float scale) {
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

void HUD::drawRect(float x, float y, float w, float h, int screenW, int screenH,
                   const glm::vec4& color) {
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

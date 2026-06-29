#include "primitives.h"
#include <glm/glm.hpp>
#include <cmath>

static constexpr float PI = 3.14159265358979323846f;

Mesh makePlane(float size, float uvTile) {
    Mesh m;
    float h = size * 0.5f;
    m.vertices = {
        {{-h, 0.0f, -h}, {0,1,0}, {0.0f,     0.0f},     {1,0,0}},
        {{ h, 0.0f, -h}, {0,1,0}, {uvTile,   0.0f},     {1,0,0}},
        {{ h, 0.0f,  h}, {0,1,0}, {uvTile,   uvTile},   {1,0,0}},
        {{-h, 0.0f,  h}, {0,1,0}, {0.0f,     uvTile},   {1,0,0}},
    };
    m.indices = {0, 2, 1, 0, 3, 2};
    m.setup();
    return m;
}

Mesh makeCube(float size) {
    Mesh m;
    float h = size * 0.5f;

    struct F { glm::vec3 n; glm::vec3 t; glm::vec3 v[4]; };
    F faces[6] = {
        {{ 0, 0, 1},{1,0,0},{{-h,-h, h},{ h,-h, h},{ h, h, h},{-h, h, h}}}, // +Z
        {{ 0, 0,-1},{-1,0,0},{{ h,-h,-h},{-h,-h,-h},{-h, h,-h},{ h, h,-h}}}, // -Z
        {{ 1, 0, 0},{0,0,-1},{{ h,-h, h},{ h,-h,-h},{ h, h,-h},{ h, h, h}}}, // +X
        {{-1, 0, 0},{0,0,1},{{-h,-h,-h},{-h,-h, h},{-h, h, h},{-h, h,-h}}}, // -X
        {{ 0, 1, 0},{1,0,0},{{-h, h, h},{ h, h, h},{ h, h,-h},{-h, h,-h}}}, // +Y
        {{ 0,-1, 0},{1,0,0},{{-h,-h,-h},{ h,-h,-h},{ h,-h, h},{-h,-h, h}}}, // -Y
    };
    unsigned int base = 0;
    for (auto& f : faces) {
        m.vertices.push_back({ f.v[0], f.n, {0,0}, f.t });
        m.vertices.push_back({ f.v[1], f.n, {1,0}, f.t });
        m.vertices.push_back({ f.v[2], f.n, {1,1}, f.t });
        m.vertices.push_back({ f.v[3], f.n, {0,1}, f.t });
        m.indices.insert(m.indices.end(), { base, base+1, base+2, base, base+2, base+3 });
        base += 4;
    }
    m.setup();
    return m;
}

Mesh makeSphere(float radius, int segments) {
    Mesh m;
    for (int y = 0; y <= segments; ++y) {
        for (int x = 0; x <= segments; ++x) {
            float u = (float)x / segments;
            float v = (float)y / segments;
            float phi = u * 2.0f * PI;
            float theta = v * PI;
            glm::vec3 n = { std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi) };
            glm::vec3 t = { -std::sin(phi), 0.0f, std::cos(phi) };
            m.vertices.push_back({ n * radius, n, {u, v}, t });
        }
    }
    for (int y = 0; y < segments; ++y) {
        for (int x = 0; x < segments; ++x) {
            unsigned int a = y * (segments + 1) + x;
            unsigned int b = a + segments + 1;
            m.indices.insert(m.indices.end(), { a, b, a + 1, b, b + 1, a + 1 });
        }
    }
    m.setup();
    return m;
}

#include "procgen.h"
#include <glad/gl.h>
#include <vector>
#include <cmath>
#include <cstdint>
#include <array>
#include <algorithm>

namespace {

// integer hash → [0,1)
float hash(int x, int y, int seed = 0) {
    uint32_t h = (uint32_t)(x * 374761393u + y * 668265263u + seed * 1442695040u);
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return (h & 0xFFFFFFu) / float(0xFFFFFFu);
}

float vnoise(float x, float y, int seed = 0) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - xi, fy = y - yi;
    float u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
    float a = hash(xi,     yi,     seed);
    float b = hash(xi + 1, yi,     seed);
    float c = hash(xi,     yi + 1, seed);
    float d = hash(xi + 1, yi + 1, seed);
    return (a * (1 - u) + b * u) * (1 - v) + (c * (1 - u) + d * u) * v;
}

float fbm(float x, float y, int oct = 4, int seed = 0) {
    float s = 0.0f, amp = 0.5f, f = 1.0f;
    for (int i = 0; i < oct; ++i) {
        s += amp * vnoise(x * f, y * f, seed + i);
        f *= 2.0f; amp *= 0.5f;
    }
    return s;
}

unsigned int uploadRGB(const std::vector<uint8_t>& px, int w, int h, bool srgb) {
    unsigned int id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8 : GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return id;
}

unsigned int uploadRGBA(const std::vector<uint8_t>& px, int w, int h) {
    unsigned int id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return id;
}

} // namespace

unsigned int genCobble(int size) {
    std::vector<uint8_t> px(size * size * 3);
    float scale = 8.0f;
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        float u = (float)x / size, v = (float)y / size;
        float n = fbm(u * scale, v * scale, 5, 11);
        // stone tone: warm gray
        uint8_t g = (uint8_t)(80 + n * 130);
        px[(y * size + x) * 3 + 0] = (uint8_t)(g * 1.05f);
        px[(y * size + x) * 3 + 1] = g;
        px[(y * size + x) * 3 + 2] = (uint8_t)(g * 0.92f);
    }
    return uploadRGB(px, size, size, true);
}

unsigned int genGrass(int size) {
    std::vector<uint8_t> px(size * size * 3);
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        float u = (float)x / size, v = (float)y / size;
        float n = fbm(u * 12.0f, v * 12.0f, 5, 27);
        uint8_t r = (uint8_t)(30 + n * 50);
        uint8_t g = (uint8_t)(70 + n * 110);
        uint8_t b = (uint8_t)(20 + n * 40);
        px[(y * size + x) * 3 + 0] = r;
        px[(y * size + x) * 3 + 1] = g;
        px[(y * size + x) * 3 + 2] = b;
    }
    return uploadRGB(px, size, size, true);
}

// Brick pattern: rectangular tiles with mortar, slight color variation
unsigned int genBrick(int size) {
    std::vector<uint8_t> px(size * size * 3);
    float brickW = 64.0f, brickH = 24.0f, mortar = 3.0f;
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        float row = std::floor(y / brickH);
        float xoff = (int(row) & 1) ? brickW * 0.5f : 0.0f;
        float bx = std::fmod(x + xoff, brickW);
        float by = std::fmod((float)y, brickH);
        bool isMortar = (bx < mortar) || (by < mortar);
        float n = fbm(x * 0.05f, y * 0.05f, 3, 51);
        if (isMortar) {
            px[(y * size + x) * 3 + 0] = 80;
            px[(y * size + x) * 3 + 1] = 78;
            px[(y * size + x) * 3 + 2] = 72;
        } else {
            uint8_t r = (uint8_t)(120 + n * 80);
            uint8_t g = (uint8_t)(50  + n * 40);
            uint8_t b = (uint8_t)(40  + n * 30);
            px[(y * size + x) * 3 + 0] = r;
            px[(y * size + x) * 3 + 1] = g;
            px[(y * size + x) * 3 + 2] = b;
        }
    }
    return uploadRGB(px, size, size, true);
}

// Matching normal map: mortar grooves lower, brick faces convex
unsigned int genBrickNormal(int size) {
    std::vector<uint8_t> px(size * size * 3);
    float brickW = 64.0f, brickH = 24.0f, mortar = 3.0f;
    auto heightAt = [&](int x, int y) {
        float row = std::floor(y / brickH);
        float xoff = (int(row) & 1) ? brickW * 0.5f : 0.0f;
        float bx = std::fmod(x + xoff + brickW, brickW);
        float by = std::fmod((float)y + brickH, brickH);
        if (bx < mortar || by < mortar) return 0.0f;
        // convex face: peak in the middle of the brick
        float cx = (bx - brickW * 0.5f) / (brickW * 0.5f);
        float cy = (by - brickH * 0.5f) / (brickH * 0.5f);
        return 1.0f - std::sqrt(cx * cx + cy * cy) * 0.3f;
    };
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        float hL = heightAt((x - 1 + size) % size, y);
        float hR = heightAt((x + 1) % size, y);
        float hD = heightAt(x, (y - 1 + size) % size);
        float hU = heightAt(x, (y + 1) % size);
        float dx = (hR - hL) * 4.0f;
        float dy = (hU - hD) * 4.0f;
        // tangent-space normal: (-dx, -dy, 1) normalized → [0,1] range
        float nx = -dx, ny = -dy, nz = 1.0f;
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        nx /= len; ny /= len; nz /= len;
        px[(y * size + x) * 3 + 0] = (uint8_t)((nx * 0.5f + 0.5f) * 255);
        px[(y * size + x) * 3 + 1] = (uint8_t)((ny * 0.5f + 0.5f) * 255);
        px[(y * size + x) * 3 + 2] = (uint8_t)((nz * 0.5f + 0.5f) * 255);
    }
    return uploadRGB(px, size, size, false); // normal map is linear, not srgb
}

unsigned int genGlass(int size) {
    std::vector<uint8_t> px(size * size * 4);
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        // amber tint, ~40% alpha center, harder edge
        float u = (float)x / size, v = (float)y / size;
        float r = std::sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));
        uint8_t a = (uint8_t)(120 + 80 * (1.0f - std::min(1.0f, r * 2.0f)));
        px[(y * size + x) * 4 + 0] = 255;
        px[(y * size + x) * 4 + 1] = 200;
        px[(y * size + x) * 4 + 2] = 120;
        px[(y * size + x) * 4 + 3] = a;
    }
    return uploadRGBA(px, size, size);
}

unsigned int genFlat(unsigned char r, unsigned char g, unsigned char b) {
    std::vector<uint8_t> px = { r, g, b,  r, g, b,  r, g, b,  r, g, b };
    return uploadRGB(px, 2, 2, true);
}

// 6-face gradient cubemap: dusk sky → horizon → ground
unsigned int genSkyCubemap(int size) {
    unsigned int id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, id);

    auto fill = [&](GLenum face, auto pixelFn) {
        std::vector<uint8_t> px(size * size * 3);
        for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
            float u = (float)x / size, v = (float)y / size;
            auto c = pixelFn(u, v);
            px[(y * size + x) * 3 + 0] = c[0];
            px[(y * size + x) * 3 + 1] = c[1];
            px[(y * size + x) * 3 + 2] = c[2];
        }
        glTexImage2D(face, 0, GL_RGB8, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    };

    // gradient: bright daytime — clear blue sky, soft horizon, light tan ground
    auto sideColor = [](float v) -> std::array<uint8_t, 3> {
        // v=0 top of face (sky), v=1 bottom (ground)
        float skyR = 0.45f, skyG = 0.65f, skyB = 0.95f;
        float horR = 0.85f, horG = 0.90f, horB = 0.95f;
        float gndR = 0.70f, gndG = 0.65f, gndB = 0.55f;
        float r, g, b;
        if (v < 0.5f) {
            float t = v / 0.5f;
            r = skyR + (horR - skyR) * t;
            g = skyG + (horG - skyG) * t;
            b = skyB + (horB - skyB) * t;
        } else {
            float t = (v - 0.5f) / 0.5f;
            r = horR + (gndR - horR) * t;
            g = horG + (gndG - horG) * t;
            b = horB + (gndB - horB) * t;
        }
        return { (uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255) };
    };

    fill(GL_TEXTURE_CUBE_MAP_POSITIVE_X, [&](float, float v) { return sideColor(v); });
    fill(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, [&](float, float v) { return sideColor(v); });
    fill(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, [&](float, float v) { return sideColor(v); });
    fill(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, [&](float, float v) { return sideColor(v); });
    fill(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, [&](float, float) { return std::array<uint8_t,3>{ 115, 165, 240 }; }); // bright zenith
    fill(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, [&](float, float) { return std::array<uint8_t,3>{ 180, 165, 140 }; }); // light tan

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    return id;
}

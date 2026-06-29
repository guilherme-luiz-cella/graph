#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "shader.h"

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 tangent;
};

struct Texture {
    unsigned int id = 0;
    std::string type; // "diffuse" | "specular" | "normal"
    std::string path;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;
    unsigned int vao = 0, vbo = 0, ebo = 0;
    void setup();
    void draw(const Shader& s) const;
};

struct Model {
    std::vector<Mesh> meshes;
    std::string dir;
    void load(const std::string& path);
    void draw(const Shader& s) const;
};

// Loads (or returns cached) 2D texture. flip=true for stb default.
unsigned int loadTexture(const std::string& path, bool srgb = false);

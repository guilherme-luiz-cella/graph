#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <assimp/scene.h>
#include "shader.h"
#include "model.h"

constexpr int MAX_BONE_INFLUENCE = 4;
constexpr int MAX_BONES = 100;

struct AnimVertex {
    glm::vec3 pos{};
    glm::vec3 normal{};
    glm::vec2 uv{};
    glm::vec3 tangent{};
    int   boneIDs[MAX_BONE_INFLUENCE] = {-1, -1, -1, -1};
    float weights[MAX_BONE_INFLUENCE] = {0.0f, 0.0f, 0.0f, 0.0f};
};

struct BoneInfo {
    int id = 0;
    glm::mat4 offset{1.0f}; // mesh-space → bone-space at bind pose
};

struct AnimMesh {
    std::vector<AnimVertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;
    unsigned int vao = 0, vbo = 0, ebo = 0;
    void setup();
    void draw(const Shader& s) const;
};

// Holds raw scene + bone map. One file load brings both rest mesh and animation channels.
struct AnimatedModel {
    std::vector<AnimMesh> meshes;
    std::unordered_map<std::string, BoneInfo> boneMap;
    int boneCounter = 0;
    std::string dir;

    // raw scene retained for animation sampling
    const aiScene* scene = nullptr;
    void* importer = nullptr; // Assimp::Importer*, opaque to header

    void load(const std::string& path);
    void draw(const Shader& s) const;
    ~AnimatedModel();
};

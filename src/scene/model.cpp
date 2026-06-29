#include "model.h"
#include <glad/gl.h>
#include <stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <unordered_map>
#include <filesystem>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cctype>

// Load embedded aiTexture (fbx/glb). mHeight==0 → compressed bytes in pcData.
static unsigned int loadEmbeddedTexture(const aiTexture* t) {
    unsigned int id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    if (t->mHeight == 0) {
        int w, h, n;
        stbi_set_flip_vertically_on_load(false);
        unsigned char* d = stbi_load_from_memory(
            reinterpret_cast<const unsigned char*>(t->pcData), (int)t->mWidth, &w, &h, &n, 0);
        if (!d) { std::cerr << "embed tex decode fail\n"; return 0; }
        GLenum fmt = (n == 1) ? GL_RED : (n == 3) ? GL_RGB : GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, d);
        stbi_image_free(d);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->mWidth, t->mHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, t->pcData);
    }
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return id;
}

// Normalize windows absolute paths (`C:\Users\..\tex.png`) to just basename.
static std::string basenameOf(const std::string& p) {
    auto pos = p.find_last_of("/\\");
    return pos == std::string::npos ? p : p.substr(pos + 1);
}

// Tiny value-noise so color-only materials don't look like flat paint.
// Lifted from procgen.cpp's hash; duplicated here to keep model.cpp standalone.
static float colorNoise(int x, int y) {
    uint32_t h = (uint32_t)(x * 374761393u + y * 668265263u);
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return (h & 0xFFFFFFu) / float(0xFFFFFFu);
}

// 64x64 RGB texture from material color + low-freq noise. Used when FBX material
// has no texture, only a color (Maya Lambert / aiStandardSurface).
// Noise amplitude scales with material brightness so dark mats stay dark.
static unsigned int makeColorTexture(float r, float g, float b) {
    constexpr int N = 64;
    std::vector<uint8_t> px(N * N * 3);
    float lum = 0.299f * r + 0.587f * g + 0.114f * b;
    float amp = 0.08f + 0.04f * lum; // brighter mats wear noise better
    for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
        // 2-octave value noise
        float n  = colorNoise(x / 4, y / 4) * 0.6f
                 + colorNoise(x / 2, y / 2) * 0.3f
                 + colorNoise(x, y)         * 0.1f;
        float d = (n - 0.5f) * amp;
        float rr = std::min(1.0f, std::max(0.0f, r + d));
        float gg = std::min(1.0f, std::max(0.0f, g + d));
        float bb = std::min(1.0f, std::max(0.0f, b + d));
        px[(y * N + x) * 3 + 0] = (uint8_t)(rr * 255);
        px[(y * N + x) * 3 + 1] = (uint8_t)(gg * 255);
        px[(y * N + x) * 3 + 2] = (uint8_t)(bb * 255);
    }
    unsigned int id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, N, N, 0, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return id;
}

static std::unordered_map<std::string, unsigned int> g_texCache;

unsigned int loadTexture(const std::string& path, bool srgb) {
    auto it = g_texCache.find(path);
    if (it != g_texCache.end()) return it->second;
    unsigned int id = 0; glGenTextures(1, &id);
    int w, h, n;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 0);
    if (!data) { std::cerr << "tex fail: " << path << "\n"; return 0; }
    GLenum fmt = (n == 1) ? GL_RED : (n == 3) ? GL_RGB : GL_RGBA;
    GLenum ifmt = srgb ? ((n == 4) ? GL_SRGB8_ALPHA8 : GL_SRGB8) : fmt;
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    g_texCache[path] = id;
    return id;
}

void Mesh::setup() {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
    glBindVertexArray(0);
}

void Mesh::draw(const Shader& s) const {
    unsigned int diffN = 0, specN = 0, normN = 0;
    for (unsigned int i = 0; i < textures.size(); ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        std::string name = "tex_" + textures[i].type;
        if (textures[i].type == "diffuse") name += std::to_string(diffN++);
        else if (textures[i].type == "specular") name += std::to_string(specN++);
        else if (textures[i].type == "normal") name += std::to_string(normN++);
        s.setInt(name, (int)i);
        glBindTexture(GL_TEXTURE_2D, textures[i].id);
    }
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
}

static std::vector<Texture> loadMaterialTex(aiMaterial* mat, aiTextureType type, const std::string& typeName, const std::string& dir, const aiScene* scene) {
    namespace fs = std::filesystem;
    std::vector<Texture> out;
    for (unsigned int i = 0; i < mat->GetTextureCount(type); ++i) {
        aiString s; mat->GetTexture(type, i, &s);
        Texture t; t.type = typeName;
        const char* c = s.C_Str();
        if (c[0] == '*') {
            // FBX/GLB embedded texture (binary glTF + FBX embedded media).
            int idx = std::atoi(c + 1);
            if (scene && idx >= 0 && (unsigned)idx < scene->mNumTextures) {
                t.path = std::string("embedded:") + c;
                t.id = loadEmbeddedTexture(scene->mTextures[idx]);
                std::cerr << "[embed:model] " << typeName << " idx=" << idx << " id=" << t.id << "\n";
            }
        } else {
            // Try as-stored, then dir+basename (handles windows abs paths like C:\...\tex.png).
            std::string p1 = dir + "/" + c;
            std::string p2 = dir + "/" + basenameOf(c);
            std::string p3 = c;
            std::string chosen;
            if (fs::exists(p1)) chosen = p1;
            else if (fs::exists(p2)) chosen = p2;
            else if (fs::exists(p3)) chosen = p3;
            if (!chosen.empty()) {
                t.path = chosen;
                t.id = loadTexture(chosen, typeName == "diffuse");
            } else {
                // Last resort: try assimp's embedded-by-filename lookup (FBX may store
                // path as a filename but embed bytes under that name).
                if (auto* emb = scene ? scene->GetEmbeddedTexture(c) : nullptr) {
                    t.path = std::string("embedded-byname:") + c;
                    t.id = loadEmbeddedTexture(emb);
                    std::cerr << "[embed-byname:model] " << typeName << " name=" << c << " id=" << t.id << "\n";
                } else {
                    std::cerr << "[tex:model] miss " << typeName << " name=" << c << " (tried " << p1 << ", " << p2 << ", " << p3 << ")\n";
                }
            }
        }
        out.push_back(t);
    }
    return out;
}

static Mesh processMesh(aiMesh* m, const aiScene* scene, const std::string& dir) {
    Mesh out;
    out.vertices.reserve(m->mNumVertices);
    for (unsigned int i = 0; i < m->mNumVertices; ++i) {
        Vertex v{};
        v.pos = { m->mVertices[i].x, m->mVertices[i].y, m->mVertices[i].z };
        if (m->HasNormals()) v.normal = { m->mNormals[i].x, m->mNormals[i].y, m->mNormals[i].z };
        if (m->mTextureCoords[0]) v.uv = { m->mTextureCoords[0][i].x, m->mTextureCoords[0][i].y };
        if (m->HasTangentsAndBitangents()) v.tangent = { m->mTangents[i].x, m->mTangents[i].y, m->mTangents[i].z };
        out.vertices.push_back(v);
    }
    for (unsigned int i = 0; i < m->mNumFaces; ++i)
        for (unsigned int j = 0; j < m->mFaces[i].mNumIndices; ++j)
            out.indices.push_back(m->mFaces[i].mIndices[j]);
    if (m->mMaterialIndex < scene->mNumMaterials) {
        aiMaterial* mat = scene->mMaterials[m->mMaterialIndex];
        aiString matName; mat->Get(AI_MATKEY_NAME, matName);
        // Dump which texture slots this material exposes (helps debug FBX/Maya exports).
        auto countTex = [&](aiTextureType t){ return mat->GetTextureCount(t); };
        std::cerr << "[mat] " << matName.C_Str()
                  << " D=" << countTex(aiTextureType_DIFFUSE)
                  << " BC=" << countTex(aiTextureType_BASE_COLOR)
                  << " S=" << countTex(aiTextureType_SPECULAR)
                  << " N=" << countTex(aiTextureType_NORMALS)
                  << " H=" << countTex(aiTextureType_HEIGHT)
                  << " A=" << countTex(aiTextureType_AMBIENT)
                  << " E=" << countTex(aiTextureType_EMISSIVE)
                  << " R=" << countTex(aiTextureType_REFLECTION)
                  << " M=" << countTex(aiTextureType_METALNESS)
                  << " DR=" << countTex(aiTextureType_DIFFUSE_ROUGHNESS)
                  << " UNK=" << countTex(aiTextureType_UNKNOWN)
                  << "\n";
        auto d  = loadMaterialTex(mat, aiTextureType_DIFFUSE,  "diffuse",  dir, scene);
        auto sp = loadMaterialTex(mat, aiTextureType_SPECULAR, "specular", dir, scene);
        // FBX exports often store normal map under NORMALS (not HEIGHT). Try both.
        auto nm = loadMaterialTex(mat, aiTextureType_NORMALS,  "normal",   dir, scene);
        if (nm.empty()) nm = loadMaterialTex(mat, aiTextureType_HEIGHT, "normal", dir, scene);
        // Some FBX use BASE_COLOR (PBR) instead of DIFFUSE
        if (d.empty()) d = loadMaterialTex(mat, aiTextureType_BASE_COLOR, "diffuse", dir, scene);
        // Maya/FBX color-only materials: no textures, just diffuse color. Bake a
        // tiny solid color texture so the shader path stays uniform.
        if (d.empty()) {
            aiColor3D color(0.8f, 0.8f, 0.8f);
            bool gotColor = (mat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
                         || (mat->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS);
            // Arnold/aiStandardSurface materials hide color in non-standard props that assimp
            // doesn't expose. Fall back to name-based heuristic so the house looks intentional
            // instead of uniformly gray.
            std::string n = matName.C_Str();
            std::string nl = n;
            for (auto& ch : nl) ch = (char)std::tolower((unsigned char)ch);
            auto has = [&](const char* k) { return nl.find(k) != std::string::npos; };
            if (!gotColor || (color.r > 0.79f && color.r < 0.81f && color.g > 0.79f && color.g < 0.81f)) {
                if      (has("pared") || has("wall") || has("estuco")) { color = {0.93f, 0.91f, 0.86f}; } // warm off-white wall
                else if (has("madera") || has("wood"))                  { color = {0.45f, 0.30f, 0.18f}; } // walnut
                else if (has("vidrio") || has("glass"))                 { color = {0.55f, 0.70f, 0.85f}; } // bluish glass
                else if (has("postes") || has("post") || has("metal"))  { color = {0.20f, 0.20f, 0.22f}; } // dark metal
                else if (has("aluminio") || has("aluminum") || has("alum")) { color = {0.75f, 0.76f, 0.78f}; } // brushed alum
                else if (has("negro") || has("black"))                  { color = {0.10f, 0.10f, 0.10f}; }
                else if (has("default") || has("lambert"))              { color = {0.85f, 0.82f, 0.78f}; } // neutral light
                // else leave whatever was read (or 0.8 gray)
            }
            Texture t; t.type = "diffuse";
            t.path = std::string("color:") + n;
            t.id = makeColorTexture(color.r, color.g, color.b);
            std::cerr << "[color] " << n
                      << " rgb=(" << color.r << "," << color.g << "," << color.b << ")"
                      << " id=" << t.id << "\n";
            d.push_back(t);
        }
        out.textures.insert(out.textures.end(), d.begin(), d.end());
        out.textures.insert(out.textures.end(), sp.begin(), sp.end());
        out.textures.insert(out.textures.end(), nm.begin(), nm.end());
    }
    out.setup();
    return out;
}

static void processNode(aiNode* node, const aiScene* scene, Model& mdl) {
    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
        mdl.meshes.push_back(processMesh(scene->mMeshes[node->mMeshes[i]], scene, mdl.dir));
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
        processNode(node->mChildren[i], scene, mdl);
}

void Model::load(const std::string& path) {
    Assimp::Importer imp;
    const aiScene* scene = imp.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
        aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality |
        aiProcess_RemoveRedundantMaterials | aiProcess_OptimizeMeshes |
        aiProcess_PreTransformVertices);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "assimp fail: " << imp.GetErrorString() << "\n"; return;
    }
    dir = path.substr(0, path.find_last_of('/'));
    processNode(scene->mRootNode, scene, *this);
}

void Model::draw(const Shader& s) const {
    for (const auto& m : meshes) m.draw(s);
}

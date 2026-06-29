#include "animated_model.h"
#include <glad/gl.h>
#include <stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <iostream>

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

static glm::mat4 toGlm(const aiMatrix4x4& m) {
    glm::mat4 r;
    r[0][0]=m.a1; r[1][0]=m.a2; r[2][0]=m.a3; r[3][0]=m.a4;
    r[0][1]=m.b1; r[1][1]=m.b2; r[2][1]=m.b3; r[3][1]=m.b4;
    r[0][2]=m.c1; r[1][2]=m.c2; r[2][2]=m.c3; r[3][2]=m.c4;
    r[0][3]=m.d1; r[1][3]=m.d2; r[2][3]=m.d3; r[3][3]=m.d4;
    return r;
}

void AnimMesh::setup() {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(AnimVertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(AnimVertex), (void*)offsetof(AnimVertex, pos));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(AnimVertex), (void*)offsetof(AnimVertex, normal));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(AnimVertex), (void*)offsetof(AnimVertex, uv));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(AnimVertex), (void*)offsetof(AnimVertex, tangent));
    glEnableVertexAttribArray(4); glVertexAttribIPointer(4, MAX_BONE_INFLUENCE, GL_INT, sizeof(AnimVertex), (void*)offsetof(AnimVertex, boneIDs));
    glEnableVertexAttribArray(5); glVertexAttribPointer(5, MAX_BONE_INFLUENCE, GL_FLOAT, GL_FALSE, sizeof(AnimVertex), (void*)offsetof(AnimVertex, weights));
    glBindVertexArray(0);
}

void AnimMesh::draw(const Shader& s) const {
    unsigned int diffN = 0, normN = 0;
    for (unsigned int i = 0; i < textures.size(); ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        std::string name = "tex_" + textures[i].type;
        if (textures[i].type == "diffuse") name += std::to_string(diffN++);
        else if (textures[i].type == "normal") name += std::to_string(normN++);
        s.setInt(name, (int)i);
        glBindTexture(GL_TEXTURE_2D, textures[i].id);
    }
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
}

static void setBoneSlot(AnimVertex& v, int id, float w) {
    for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
        if (v.boneIDs[i] < 0) {
            v.boneIDs[i] = id;
            v.weights[i] = w;
            return;
        }
    }

    int minI = 0;
    for (int i = 1; i < MAX_BONE_INFLUENCE; ++i)
        if (v.weights[i] < v.weights[minI]) minI = i;
    if (w > v.weights[minI]) { v.boneIDs[minI] = id; v.weights[minI] = w; }
}

static std::vector<Texture> loadMatTex(aiMaterial* mat, aiTextureType type, const std::string& typeName, const std::string& dir, const aiScene* scene) {
    std::vector<Texture> out;
    for (unsigned int i = 0; i < mat->GetTextureCount(type); ++i) {
        aiString s; mat->GetTexture(type, i, &s);
        Texture t; t.type = typeName;
        const char* c = s.C_Str();
        if (c[0] == '*') {
            // glb/fbx embedded texture: reference into scene->mTextures[N].
            int idx = std::atoi(c + 1);
            if (scene && idx >= 0 && (unsigned)idx < scene->mNumTextures) {
                t.path = std::string("embedded:") + c;
                t.id = loadEmbeddedTexture(scene->mTextures[idx]);
                std::cerr << "[embed] " << typeName << " idx=" << idx
                          << " id=" << t.id
                          << " (" << scene->mTextures[idx]->mWidth << "x"
                          << scene->mTextures[idx]->mHeight << ")\n";
            } else {
                std::cerr << "[embed] miss: scene=" << (scene?1:0)
                          << " idx=" << idx << " count=" << (scene?scene->mNumTextures:0) << "\n";
            }
        } else {
            t.path = dir + "/" + c;
            t.id = loadTexture(t.path, typeName == "diffuse");
        }
        out.push_back(t);
    }
    return out;
}

static AnimMesh processMesh(aiMesh* m, const aiScene* scene, AnimatedModel& mdl) {
    AnimMesh out;
    out.vertices.resize(m->mNumVertices);
    for (unsigned int i = 0; i < m->mNumVertices; ++i) {
        AnimVertex& v = out.vertices[i];
        v.pos = { m->mVertices[i].x, m->mVertices[i].y, m->mVertices[i].z };
        if (m->HasNormals()) v.normal = { m->mNormals[i].x, m->mNormals[i].y, m->mNormals[i].z };
        if (m->mTextureCoords[0]) v.uv = { m->mTextureCoords[0][i].x, m->mTextureCoords[0][i].y };
        if (m->HasTangentsAndBitangents()) v.tangent = { m->mTangents[i].x, m->mTangents[i].y, m->mTangents[i].z };
    }
    for (unsigned int i = 0; i < m->mNumFaces; ++i)
        for (unsigned int j = 0; j < m->mFaces[i].mNumIndices; ++j)
            out.indices.push_back(m->mFaces[i].mIndices[j]);

    // bone weights
    for (unsigned int b = 0; b < m->mNumBones; ++b) {
        aiBone* bone = m->mBones[b];
        std::string name = bone->mName.C_Str();
        int id;
        auto it = mdl.boneMap.find(name);
        if (it == mdl.boneMap.end()) {
            BoneInfo info; info.id = mdl.boneCounter++; info.offset = toGlm(bone->mOffsetMatrix);
            mdl.boneMap[name] = info;
            id = info.id;
        } else id = it->second.id;
        for (unsigned int w = 0; w < bone->mNumWeights; ++w) {
            auto& vw = bone->mWeights[w];
            if (vw.mVertexId < out.vertices.size())
                setBoneSlot(out.vertices[vw.mVertexId], id, vw.mWeight);
        }
    }

    if (m->mMaterialIndex >= 0) {
        aiMaterial* mat = scene->mMaterials[m->mMaterialIndex];
        auto d = loadMatTex(mat, aiTextureType_DIFFUSE, "diffuse", mdl.dir, scene);
        auto n = loadMatTex(mat, aiTextureType_NORMALS, "normal", mdl.dir, scene);
        out.textures.insert(out.textures.end(), d.begin(), d.end());
        out.textures.insert(out.textures.end(), n.begin(), n.end());
    }
    out.setup();
    return out;
}

static void processNode(aiNode* node, const aiScene* scene, AnimatedModel& mdl) {
    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
        mdl.meshes.push_back(processMesh(scene->mMeshes[node->mMeshes[i]], scene, mdl));
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
        processNode(node->mChildren[i], scene, mdl);
}

void AnimatedModel::load(const std::string& path) {
    Assimp::Importer* imp = new Assimp::Importer();
    importer = imp;
    scene = imp->ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights | aiProcess_FlipUVs);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "assimp anim fail: " << imp->GetErrorString() << "\n";
        return;
    }
    dir = path.substr(0, path.find_last_of('/'));
    processNode(scene->mRootNode, scene, *this);
}

void AnimatedModel::draw(const Shader& s) const {
    for (const auto& m : meshes) m.draw(s);
}

AnimatedModel::~AnimatedModel() {
    delete (Assimp::Importer*)importer;
}

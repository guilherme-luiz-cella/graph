#include "animator.h"
#include <assimp/scene.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <string>

static glm::mat4 toGlm(const aiMatrix4x4& m) {
    glm::mat4 r;
    r[0][0]=m.a1; r[1][0]=m.a2; r[2][0]=m.a3; r[3][0]=m.a4;
    r[0][1]=m.b1; r[1][1]=m.b2; r[2][1]=m.b3; r[3][1]=m.b4;
    r[0][2]=m.c1; r[1][2]=m.c2; r[2][2]=m.c3; r[3][2]=m.c4;
    r[0][3]=m.d1; r[1][3]=m.d2; r[2][3]=m.d3; r[3][3]=m.d4;
    return r;
}

static const aiNodeAnim* findChannel(const aiAnimation* anim, const std::string& name) {
    for (unsigned int i = 0; i < anim->mNumChannels; ++i)
        if (anim->mChannels[i]->mNodeName.C_Str() == name) return anim->mChannels[i];
    return nullptr;
}

static glm::vec3 lerpVec(const aiVectorKey* keys, unsigned int n, float t) {
    if (n == 1) return { keys[0].mValue.x, keys[0].mValue.y, keys[0].mValue.z };
    unsigned int i = 0;
    for (; i + 1 < n; ++i) if (t < (float)keys[i + 1].mTime) break;
    if (i + 1 >= n) i = n - 2;
    float dt = (float)(keys[i + 1].mTime - keys[i].mTime);
    float f = dt > 0.0f ? (t - (float)keys[i].mTime) / dt : 0.0f;
    auto a = keys[i].mValue, b = keys[i + 1].mValue;
    return glm::mix(glm::vec3(a.x, a.y, a.z), glm::vec3(b.x, b.y, b.z), f);
}

static glm::quat slerpQuat(const aiQuatKey* keys, unsigned int n, float t) {
    if (n == 1) return { keys[0].mValue.w, keys[0].mValue.x, keys[0].mValue.y, keys[0].mValue.z };
    unsigned int i = 0;
    for (; i + 1 < n; ++i) if (t < (float)keys[i + 1].mTime) break;
    if (i + 1 >= n) i = n - 2;
    float dt = (float)(keys[i + 1].mTime - keys[i].mTime);
    float f = dt > 0.0f ? (t - (float)keys[i].mTime) / dt : 0.0f;
    auto a = keys[i].mValue, b = keys[i + 1].mValue;
    glm::quat qa(a.w, a.x, a.y, a.z), qb(b.w, b.x, b.y, b.z);
    return glm::slerp(qa, qb, f);
}

static glm::mat4 nodeTransform(const aiNodeAnim* ch, float t) {
    if (!ch) return glm::mat4(1.0f);
    glm::vec3 p = lerpVec(ch->mPositionKeys, ch->mNumPositionKeys, t);
    glm::quat r = slerpQuat(ch->mRotationKeys, ch->mNumRotationKeys, t);
    glm::vec3 s = lerpVec(ch->mScalingKeys, ch->mNumScalingKeys, t);
    return glm::translate(glm::mat4(1.0f), p) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
}

static void readNode(const aiNode* node, const glm::mat4& parent,
                     const aiAnimation* anim, float t,
                     const glm::mat4& globalInv,
                     AnimatedModel& mdl,
                     std::vector<glm::mat4>& out) {
    std::string name = node->mName.C_Str();
    const aiNodeAnim* ch = findChannel(anim, name);
    glm::mat4 local = ch ? nodeTransform(ch, t) : toGlm(node->mTransformation);
    glm::mat4 global = parent * local;

    auto it = mdl.boneMap.find(name);
    if (it != mdl.boneMap.end()) {
        int id = it->second.id;
        if (id < (int)out.size())
            out[id] = globalInv * global * it->second.offset;
    }
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
        readNode(node->mChildren[i], global, anim, t, globalInv, mdl, out);
}

void Animator::update(float dt) {
    if (!model || !model->scene || model->scene->mNumAnimations == 0) return;
    int idx = animIndex < (int)model->scene->mNumAnimations ? animIndex : 0;
    const aiAnimation* anim = model->scene->mAnimations[idx];
    float tps = anim->mTicksPerSecond > 0.0 ? (float)anim->mTicksPerSecond : 25.0f;
    timeSec += dt;
    float t = std::fmod(timeSec * tps, (float)anim->mDuration);
    glm::mat4 globalInv = glm::inverse(toGlm(model->scene->mRootNode->mTransformation));
    readNode(model->scene->mRootNode, glm::mat4(1.0f), anim, t, globalInv, *model, finalBoneMatrices);
}

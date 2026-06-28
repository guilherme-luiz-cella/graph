#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in ivec4 aBoneIDs;
layout(location = 5) in vec4  aWeights;

const int MAX_BONES = 100;
uniform mat4 bones[MAX_BONES];

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
uniform mat4 lightSpaceMatrix;

out vec3 vFragPos;
out vec3 vNormal;
out vec2 vUV;
out vec3 vTangent;
out vec4 vFragPosLight;

void main() {
    mat4 skin = mat4(0.0);
    float wSum = 0.0;
    for (int i = 0; i < 4; ++i) {
        int id = aBoneIDs[i];
        float w = aWeights[i];
        if (id >= 0 && id < MAX_BONES && w > 0.0) {
            skin += bones[id] * w;
            wSum += w;
        }
    }
    if (wSum < 0.0001) skin = mat4(1.0);

    vec4 skinned = skin * vec4(aPos, 1.0);
    vec3 skinnedN = mat3(skin) * aNormal;
    vec3 skinnedT = mat3(skin) * aTangent;

    vFragPos = vec3(model * skinned);
    vNormal  = mat3(transpose(inverse(model))) * skinnedN;
    vTangent = mat3(model) * skinnedT;
    vUV = aUV;
    vFragPosLight = lightSpaceMatrix * vec4(vFragPos, 1.0);
    gl_Position = proj * view * vec4(vFragPos, 1.0);
}

#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 4) in ivec4 aBoneIDs;
layout(location = 5) in vec4  aWeights;

const int MAX_BONES = 450;
layout(std140) uniform Bones { mat4 bones[MAX_BONES]; };
uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main() {
    mat4 skin = mat4(0.0);
    float wSum = 0.0;
    for (int i = 0; i < 4; ++i) {
        int id = aBoneIDs[i];
        float w = aWeights[i];
        if (id >= 0 && id < MAX_BONES && w > 0.0) { skin += bones[id] * w; wSum += w; }
    }
    if (wSum < 0.0001) skin = mat4(1.0);
    gl_Position = lightSpaceMatrix * model * skin * vec4(aPos, 1.0);
}

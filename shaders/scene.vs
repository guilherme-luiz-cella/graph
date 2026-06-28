#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;

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
    vFragPos = vec3(model * vec4(aPos, 1.0));
    vNormal  = mat3(transpose(inverse(model))) * aNormal;
    // ponytail: mat3(model) distorts tangent under non-uniform scale (brick wall is 6x4x0.5).
    // Upgrade to inverse-transpose of model's upper-3x3 if normal-map artifacts appear.
    vTangent = mat3(model) * aTangent;
    vUV = aUV;
    vFragPosLight = lightSpaceMatrix * vec4(vFragPos, 1.0);
    gl_Position = proj * view * vec4(vFragPos, 1.0);
}

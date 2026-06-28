#version 330 core
layout(location = 0) in vec3 aPos;
out vec3 vDir;
uniform mat4 view;
uniform mat4 proj;
void main() {
    vDir = aPos;
    vec4 p = proj * view * vec4(aPos, 1.0);
    gl_Position = p.xyww;
}

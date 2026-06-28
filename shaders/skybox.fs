#version 330 core
in vec3 vDir;
out vec4 FragColor;
uniform samplerCube sky;
void main() { FragColor = texture(sky, vDir); }

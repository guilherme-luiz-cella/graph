#include "camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

glm::mat4 Camera::view() const {
    return glm::lookAt(pos, pos + front, up);
}

void Camera::mouse(float dx, float dy) {
    yaw += dx * sens;
    pitch = std::clamp(pitch - dy * sens, -89.0f, 89.0f);
    float ry = glm::radians(yaw), rp = glm::radians(pitch);
    front = glm::normalize(glm::vec3(std::cos(ry) * std::cos(rp), std::sin(rp), std::sin(ry) * std::cos(rp)));
}

void Camera::key(int dir, float dt) {
    float v = speed * dt;
    glm::vec3 right = glm::normalize(glm::cross(front, up));
    switch (dir) {
        case 0: pos += front * v; break;
        case 1: pos -= front * v; break;
        case 2: pos -= right * v; break;
        case 3: pos += right * v; break;
        case 4: pos += up * v; break;
        case 5: pos -= up * v; break;
    }
}

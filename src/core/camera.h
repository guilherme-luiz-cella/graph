#pragma once
#include <glm/glm.hpp>

struct Camera {
    glm::vec3 pos{0.0f, 1.7f, 5.0f};
    glm::vec3 front{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float yaw = -90.0f, pitch = 0.0f;
    float speed = 4.0f, sens = 0.1f, fov = 60.0f;

    glm::mat4 view() const;
    void mouse(float dx, float dy);
    void key(int dir, float dt); // 0=fwd 1=back 2=left 3=right 4=up 5=down
};

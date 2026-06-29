#pragma once
#include <glm/glm.hpp>

// Portal mechanics: placement geometry + teleport. No recursive see-through view.
// GL-free so it can be unit-tested without a context (see tests/test_portal.cpp).

struct Portal {
    bool active = false;
    glm::vec3 pos{0.0f}, normal{0.0f}, right{0.0f}, up{0.0f};
};

constexpr float PORTAL_HW = 0.8f;  // half width (meters)
constexpr float PORTAL_HH = 1.1f;  // half height (meters)

// Orthonormal tangent frame on a surface with the given normal.
void portalBasis(const glm::vec3& n, glm::vec3& right, glm::vec3& up);

// Ray vs horizontal plane at height y, clipped to a centered square. True on hit.
bool rayPlaneY(glm::vec3 ro, glm::vec3 rd, float y, float half,
               float& t, glm::vec3& n, glm::vec3& hit);

// Ray vs AABB (slab), returns entry distance + the face normal hit. True on hit.
bool rayAABBn(glm::vec3 ro, glm::vec3 rd, glm::vec3 lo, glm::vec3 hi,
              float& tOut, glm::vec3& nOut, glm::vec3& hit);

// World-space AABB of a local box transformed by xform (8 corners).
void worldAABB(glm::vec3 lo, glm::vec3 hi, const glm::mat4& x,
               glm::vec3& wlo, glm::vec3& whi);

// If the player is inside an active portal (both must be active and cooldown
// elapsed), teleport through to the linked portal: rewrites pos/front/yaw/pitch
// and arms the cooldown. Returns true if a teleport happened.
bool portalTeleport(const Portal& blue, const Portal& orange,
                    glm::vec3& pos, glm::vec3& front, float& yaw, float& pitch,
                    float now, float& cooldownUntil);

// Same pass-through for a physics body: moves pos and rotates vel through the
// portal pair (preserving speed). Returns true if it passed through.
bool portalTeleportBody(const Portal& blue, const Portal& orange,
                        glm::vec3& pos, glm::vec3& vel,
                        float now, float& cooldownUntil);

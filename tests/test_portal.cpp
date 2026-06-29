// Self-test for the GL-free portal math. No framework: asserts + exit code.
// Run: ctest  (or ./test_portal).  Covers success + failure cases per surface.
#include "portal.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static bool approx(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }

int main() {
    glm::vec3 n, hit; float t;

    // --- rayPlaneY ---
    // Hit: looking down onto the ground from above.
    assert(rayPlaneY({0, 2, 0}, glm::normalize(glm::vec3(0, -1, 0)), 0.0f, 40.0f, t, n, hit));
    assert(approx(hit.y, 0.0f) && approx(n.y, 1.0f));
    // Miss: ray parallel to plane.
    assert(!rayPlaneY({0, 2, 0}, {1, 0, 0}, 0.0f, 40.0f, t, n, hit));
    // Miss: hit point outside the clipped square.
    assert(!rayPlaneY({100, 2, 0}, {0, -1, 0}, 0.0f, 40.0f, t, n, hit));

    // --- rayAABBn ---
    // Hit a unit box centered at origin from -X; entry face normal points -X.
    glm::vec3 lo(-1, -1, -1), hi(1, 1, 1);
    assert(rayAABBn({-5, 0, 0}, {1, 0, 0}, lo, hi, t, n, hit));
    assert(approx(n.x, -1.0f) && approx(hit.x, -1.0f));
    // Miss: ray points away from the box.
    assert(!rayAABBn({-5, 0, 0}, {-1, 0, 0}, lo, hi, t, n, hit));

    // --- portalBasis: orthonormal frame on a +Z wall ---
    glm::vec3 r, u;
    portalBasis({0, 0, 1}, r, u);
    assert(approx(glm::dot(r, u), 0.0f) && approx(glm::length(r), 1.0f) && approx(glm::length(u), 1.0f));

    // --- portalTeleport ---
    Portal blue, orange;
    blue.active = orange.active = true;
    blue.pos = {0, 1, 0};   blue.normal = {0, 0, 1};   portalBasis(blue.normal, blue.right, blue.up);
    orange.pos = {10, 1, 0}; orange.normal = {0, 0, 1}; portalBasis(orange.normal, orange.right, orange.up);

    glm::vec3 pos{0, 1, 0.1f}, front{0, 0, 1}; // standing in the blue portal
    float yaw = 0, pitch = 0, cooldown = 0.0f;
    bool moved = portalTeleport(blue, orange, pos, front, yaw, pitch, 1.0f, cooldown);
    assert(moved);
    assert(approx(pos.x, 10.0f, 1.0f)); // popped out near orange
    assert(cooldown > 1.0f);            // cooldown armed

    // Body teleport: a cube falling into blue keeps its speed, pops out orange.
    orange.active = true;
    glm::vec3 bpos{0, 1, 0.1f}, bvel{0, -5, 0}; float bcd = 0.0f;
    float speedBefore = glm::length(bvel);
    assert(portalTeleportBody(blue, orange, bpos, bvel, 1.0f, bcd));
    assert(approx(bpos.x, 10.0f, 1.0f));
    assert(approx(glm::length(bvel), speedBefore));  // speed preserved

    // Failure: one portal inactive → no teleport.
    orange.active = false;
    glm::vec3 pos2{0, 1, 0.1f}; float cd2 = 0.0f;
    assert(!portalTeleport(blue, orange, pos2, front, yaw, pitch, 5.0f, cd2));

    std::puts("test_portal: all assertions passed");
    return 0;
}

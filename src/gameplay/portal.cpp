#include "portal.h"
#include <algorithm>
#include <cmath>

void portalBasis(const glm::vec3& n, glm::vec3& right, glm::vec3& up) {
    glm::vec3 ref = (std::fabs(n.y) > 0.9f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    right = glm::normalize(glm::cross(ref, n));
    up    = glm::normalize(glm::cross(n, right));
}

bool rayPlaneY(glm::vec3 ro, glm::vec3 rd, float y, float half,
               float& t, glm::vec3& n, glm::vec3& hit) {
    if (std::fabs(rd.y) < 1e-6f) return false;
    float tt = (y - ro.y) / rd.y;
    if (tt < 0.01f) return false;
    glm::vec3 p = ro + rd * tt;
    if (std::fabs(p.x) > half || std::fabs(p.z) > half) return false;
    t = tt; n = glm::vec3(0, 1, 0); hit = p;
    return true;
}

bool rayAABBn(glm::vec3 ro, glm::vec3 rd, glm::vec3 lo, glm::vec3 hi,
              float& tOut, glm::vec3& nOut, glm::vec3& hit) {
    float tmin = -1e9f, tmax = 1e9f; int axis = 0; float sign = -1.0f;
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(rd[i]) < 1e-6f) {
            if (ro[i] < lo[i] || ro[i] > hi[i]) return false;
            continue;
        }
        float inv = 1.0f / rd[i];
        float t1 = (lo[i] - ro[i]) * inv, t2 = (hi[i] - ro[i]) * inv;
        float s = -1.0f;
        if (t1 > t2) { std::swap(t1, t2); s = 1.0f; }
        if (t1 > tmin) { tmin = t1; axis = i; sign = s; }
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return false;
    }
    if (tmin < 0.01f) return false;
    tOut = tmin; nOut = glm::vec3(0); nOut[axis] = sign; hit = ro + rd * tmin;
    return true;
}

void worldAABB(glm::vec3 lo, glm::vec3 hi, const glm::mat4& x,
               glm::vec3& wlo, glm::vec3& whi) {
    wlo = glm::vec3(1e9f); whi = glm::vec3(-1e9f);
    for (int i = 0; i < 8; ++i) {
        glm::vec3 c((i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z);
        glm::vec3 w = glm::vec3(x * glm::vec4(c, 1.0f));
        wlo = glm::min(wlo, w); whi = glm::max(whi, w);
    }
}

// Shared core: if pos lies inside portal s's disc, move it (and optionally a
// direction vector dir, e.g. look or velocity) through to portal d, mapping the
// frame 180° about the up axis. Returns true on pass-through.
static bool through(const Portal& s, const Portal& d, glm::vec3& pos, glm::vec3* dir) {
    glm::vec3 rel = pos - s.pos;
    float dist = glm::dot(rel, s.normal);
    float u = glm::dot(rel, s.right), v = glm::dot(rel, s.up);
    if (std::fabs(dist) > 0.5f || std::fabs(u) > PORTAL_HW || std::fabs(v) > PORTAL_HH)
        return false;
    if (dir) {
        glm::vec3 g = *dir;
        float gr = glm::dot(g, s.right), gu = glm::dot(g, s.up), gn = glm::dot(g, s.normal);
        *dir = -d.right * gr + d.up * gu - d.normal * gn;
    }
    pos = d.pos + d.normal * 0.6f - d.right * u + d.up * v;
    return true;
}

bool portalTeleport(const Portal& blue, const Portal& orange,
                    glm::vec3& pos, glm::vec3& front, float& yaw, float& pitch,
                    float now, float& cooldownUntil) {
    if (!blue.active || !orange.active || now <= cooldownUntil) return false;
    glm::vec3 f = glm::normalize(front);
    if (through(blue, orange, pos, &f) || through(orange, blue, pos, &f)) {
        front = glm::normalize(f);
        yaw = glm::degrees(std::atan2(front.z, front.x));
        pitch = glm::degrees(std::asin(glm::clamp(front.y, -1.0f, 1.0f)));
        cooldownUntil = now + 0.6f;
        return true;
    }
    return false;
}

bool portalTeleportBody(const Portal& blue, const Portal& orange,
                        glm::vec3& pos, glm::vec3& vel,
                        float now, float& cooldownUntil) {
    if (!blue.active || !orange.active || now <= cooldownUntil) return false;
    if (through(blue, orange, pos, &vel) || through(orange, blue, pos, &vel)) {
        cooldownUntil = now + 0.4f;
        return true;
    }
    return false;
}

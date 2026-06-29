#version 330 core
in vec3 vFragPos;
in vec3 vNormal;
in vec2 vUV;
in vec3 vTangent;
in vec4 vFragPosLight;
out vec4 FragColor;

// textures
uniform sampler2D tex_diffuse0;   // base color
uniform sampler2D tex_diffuse1;   // second base for multi-texture blend (ground)
uniform sampler2D tex_normal0;    // normal map
uniform sampler2D tex_specular0;  // optional spec/gloss
uniform sampler2D shadowMap;      // spot light shadow map
uniform samplerCube envMap;       // skybox cubemap for env reflection

// toggles per object
uniform int  useMultiTex;
uniform int  useNormalMap;
uniform int  useEnvMap;
uniform int  useAlpha;            // blending pass (lantern glass)
uniform int  useShadows;
uniform int  useEmissive;         // unlit portal surface
uniform vec3 emissiveColor;
uniform float uTime;              // animates the portal swirl

// camera + lights
uniform vec3 viewPos;
uniform vec3 pointLightPos;
uniform vec3 pointLightColor;
uniform vec3 spotPos;
uniform vec3 spotDir;
uniform vec3 spotColor;
uniform float spotCutoff;
uniform float spotOuter;

// fog
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;

float shadowCalc(vec4 fragPosLight, vec3 N, vec3 L) {
    vec3 proj = fragPosLight.xyz / fragPosLight.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 0.0;
    float bias = max(0.005 * (1.0 - dot(N, L)), 0.0005);
    float shadow = 0.0;
    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y) {
            float pcf = texture(shadowMap, proj.xy + vec2(x, y) * texel).r;
            shadow += proj.z - bias > pcf ? 1.0 : 0.0;
        }
    return shadow / 9.0;
}

void main() {
    if (useEmissive == 1) {
        // Elliptical portal with swirling rings + glowing rim (Portal-style).
        vec2 p = (vUV - 0.5) * 2.0;          // -1..1 across the quad
        float r = length(p);
        if (r > 1.0) discard;                 // clip quad corners to an ellipse
        float ang = atan(p.y, p.x);
        float swirl = 0.6 + 0.4 * sin(r * 16.0 - uTime * 6.0 + ang * 2.0);
        float rim   = smoothstep(0.6, 1.0, r); // bright edge ring
        vec3 c = emissiveColor * swirl + emissiveColor * rim * 1.5;
        c += vec3(1.0) * rim * 0.3;           // white-hot rim highlight
        FragColor = vec4(c, 1.0);
        return;
    }

    vec3 N = normalize(vNormal);
    if (useNormalMap == 1) {
        vec3 T = normalize(vTangent);
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(N, T);
        vec3 nm = texture(tex_normal0, vUV).rgb * 2.0 - 1.0;
        N = normalize(mat3(T, B, N) * nm);
    }

    // Env-mapped objects (orb) have no diffuse texture — use neutral metallic
    // base so reflection doesn't pick up garbage from unit 0 bindings.
    vec4 baseTex = (useEnvMap == 1) ? vec4(0.6, 0.6, 0.65, 1.0) : texture(tex_diffuse0, vUV);
    // House glass (useAlpha==2): alpha cutout → transparent texels become holes you
    // can see the interior through, without transparency depth-ordering problems.
    if (useAlpha == 2 && baseTex.a < 0.5) discard;
    if (useMultiTex == 1) {
        vec4 second = texture(tex_diffuse1, vUV * 2.0);
        // radial mask from world origin: cobble inside courtyard, grass outside
        float r = length(vFragPos.xz);
        float blend = smoothstep(6.0, 12.0, r);
        baseTex = mix(baseTex, second, blend);
    }
    vec3 albedo = baseTex.rgb;

    // ambient — bright "showroom" baseline so even unlit faces read clearly.
    vec3 color = 0.55 * albedo;

    // point light
    vec3 Lp = normalize(pointLightPos - vFragPos);
    float dPoint = length(pointLightPos - vFragPos);
    float attP = 1.0 / (1.0 + 0.09 * dPoint + 0.032 * dPoint * dPoint);
    float diffP = max(dot(N, Lp), 0.0);
    color += attP * diffP * pointLightColor * albedo;

    // spot light w/ shadow
    vec3 Ls = normalize(spotPos - vFragPos);
    float theta = dot(Ls, normalize(-spotDir));
    float eps = spotCutoff - spotOuter;
    float intensity = clamp((theta - spotOuter) / eps, 0.0, 1.0);
    float diffS = max(dot(N, Ls), 0.0);
    float shadow = (useShadows == 1) ? shadowCalc(vFragPosLight, N, Ls) : 0.0;
    color += (1.0 - shadow) * intensity * diffS * spotColor * albedo;

    // env reflection
    if (useEnvMap == 1) {
        vec3 V = normalize(vFragPos - viewPos);
        vec3 R = reflect(V, N);
        vec3 envCol = texture(envMap, R).rgb;
        color = mix(color, envCol, 0.6);
    }

    // fog
    float dist = length(viewPos - vFragPos);
    float f = clamp((fogEnd - dist) / (fogEnd - fogStart), 0.0, 1.0);
    color = mix(fogColor, color, f);

    // 1 = fixed translucency (lantern); 2 = alpha-cutout (house glass, opaque survivors)
    float a = (useAlpha == 1) ? baseTex.a * 0.45 : 1.0;
    FragColor = vec4(color, a);
}

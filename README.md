# Trabalho Final de Computação Gráfica

Scene: **Medieval courtyard at dusk** with animated character.
Deadline: **2026-06-29**.
Author: Guilherme Luiz Cella.

## Stack
- C++17, OpenGL 3.3 Core, GLFW, GLAD, GLM
- ASSIMP (model + skeletal animation)
- stb_image (textures, single header)
- miniaudio (sound, single header)
- CMake + FetchContent (zero system installs except `cmake`)

## Build (macOS)
```bash
brew install cmake
cmake -S . -B build
cmake --build build -j
./build/cg_final
```
First configure downloads GLFW/GLAD/GLM/ASSIMP. Slow once, fast after.

**Demo runs with zero asset downloads.** All textures + skybox have
procedural fallbacks generated in C++. Drop real assets into `assets/`
later to upgrade visuals.

## Controls
| Key | Action |
|-----|--------|
| WASD | Move |
| Mouse | Look |
| Space / Shift | Up / Down |
| Esc | Quit |

### Technique toggles (apresentação)
| Key | Toggle | Demonstrates |
|-----|--------|--------------|
| 1 | Multi-texture | Spec § 2d (ground: stone center → grass edge) |
| 2 | Normal mapping | Spec § 2g (brick wall depth shift) |
| 3 | Environment mapping | Spec § 2f (orb reflects skybox) |
| 4 | Fog | Spec § 2h (distance haze) |
| 5 | Blending | Spec § 2i (lantern alpha) |
| 6 | Shadow map | Spec § 2j (spot shadows on/off) |
| 7 | Spot light | Spec § 2c (moving cone) |
| 8 | Point light | Spec § 2c (warm torch) |
| 9 | Skybox | Spec § 2e (cubemap sky) |
| C | Cinematic camera | Auto-orbit for hands-free demo |

## Spec § 2 coverage
| Req | Element | State |
|-----|---------|-------|
| a. `.3DS` model | Castle prop in scene origin | **Needs real asset** — drop `assets/models/castle/castle.3ds`. Demo runs without it (procedural geometry shown). |
| b. UVW Unwrap | UVs come embedded in the .3DS material | **Needs real asset** |
| c. Lighting | Spot (lantern) + Point (torch), Phong | ✅ |
| d. Multi-texture | Ground radial blend cobble→grass | ✅ (procedural or real) |
| e. SkyBox | 6-face cubemap | ✅ (procedural gradient fallback, real images upgrade) |
| f. EnvMap | Orb reflects skybox cubemap | ✅ |
| g. Normal map | Brick wall | ✅ (procedural normal map computed from height) |
| h. Fog | Linear distance | ✅ |
| i. Blending | Lantern alpha cube | ✅ |
| j. Shadow map | 2048² depth FBO, spot view, PCF 3×3 | ✅ |

## Spec § 3 bonus
- (b) **ASSIMP skeletal animation** — full bone-weight vertex format, skinning vs, `mat4[100]` uniform array, lerp/slerp channel sampling. ✅ Wired. Needs Mixamo FBX.
- (c) **Sound** — miniaudio ambient loop + footstep cadence on walk. ✅ Wired. Needs `.ogg` files.
- (a) PhysX — **skipped**, deadline cost too high. Mention in apresentação.

## Optional asset upgrade
Drop into `assets/` to replace procedural fallbacks. Each file is independent — partial drops still work.

- `assets/skybox/{px,nx,py,ny,pz,nz}.jpg` — Polyhaven HDRIs converted to cubemap, or OpenGameArt skybox packs.
- `assets/textures/cobble_diffuse.jpg` + `grass_diffuse.jpg` — https://polyhaven.com (CC0).
- `assets/textures/brick_diffuse.jpg` + `brick_normal.jpg` — Polyhaven brick wall set.
- `assets/textures/glass.png` — any translucent amber PNG.
- `assets/models/castle/castle.3ds` — https://free3d.com filter `.3ds`. If only OBJ/FBX available: import to 3DS MAX → File → Export → `.3DS`. **Required for spec § 2a/b**.
- `assets/models/girl/girl.fbx` — Mixamo. Settings: **FBX Binary, With Skin, FPS 30, Keyframe Reduction "none"**. Walking animation. **Required for spec § 3b**.
- `assets/audio/ambient.ogg` + `footstep.ogg` — Freesound.org CC0.

## Apresentação script (5 min)

1. **Intro (30s)** — "Cena: pátio medieval ao entardecer. Stack: OpenGL 3.3 + C++. Press C agora."
2. **Cinematic on, walk through toggles (3min)**:
   - Start with everything ON. State: "todas as técnicas ativas."
   - Press `9` then `9` → SkyBox off/on. "Cubemap de 6 faces."
   - Press `4` then `4` → Fog off/on. "Neblina linear baseada em distância."
   - Press `1` then `1` → Multi-tex off/on. "Mistura radial cobble + grama, sem if-tree."
   - Press `2` then `2` → Normal map off/on. "Bump per-pixel da textura tangente."
   - Press `3` then `3` → EnvMap off/on. "Esfera reflete cubemap. Reflect(view, N)."
   - Press `6` then `6` → Shadow map off/on. "FBO 2048², depth pass, PCF 3×3."
   - Press `7` `8` → light toggles para mostrar contribuição cada uma.
   - Press `5` then `5` → Blending off/on. "Lanterna translucida."
3. **Modelos (1min)**:
   - Press C (off), WASD perto do castelo. "Modelo .3DS exportado do 3DS MAX, UV unwrap, ASSIMP carrega."
   - Walk perto da menina. "Mixamo Mixamo, skeletal anim via ASSIMP, skinning no vertex shader, ossos lerp/slerp."
4. **Outras (30s)**:
   - "Áudio ambiente FMOD-like via miniaudio. Footstep síncrono."
   - "PhysX deixei de fora para focar em qualidade gráfica."

## Notes vs global CLAUDE.md
- DDD layering not applied: single-binary realtime renderer, no domain logic
  to separate. Layering would add files without separating real concerns.
- Tests deferred: graphics correctness verified visually per step. Golden-image
  fixtures overkill for a 24h demo.
- `ponytail:` markers in code call out deliberate simplifications and their
  upgrade paths:
  - `src/procgen.h` — value-noise hash instead of Perlin.
  - `shaders/scene.vs` — tangent uses `mat3(model)`, distorts under non-uniform scale.
  - `src/main.cpp` cinematic — fixed-radius orbit, no easing.
  - `src/main.cpp` footstep — fixed 0.45s period, no anim-phase coupling.

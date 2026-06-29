# Trabalho Final de Computação Gráfica

Cena interativa em **OpenGL 3.3 Core / C++17** com mecânicas inspiradas em *Portal*:
portal gun na mão, dois portais (azul/laranja) com teleporte, cubo companion com
física, turret animada, e as técnicas gráficas exigidas pela spec.

Autor: **Guilherme Luiz Cella (105491)**

## Stack
- C++17, OpenGL 3.3 Core, GLFW, GLAD, GLM
- ASSIMP (carrega `.3ds`, `.glb`, `.fbx` + animação esqueletal)
- stb_image (texturas) + stb_easy_font (HUD) — single-header
- miniaudio (áudio) — single-header
- CMake + FetchContent (zero instalação além de `cmake`)

## Build & Run (macOS)
```bash
# configurar (uma vez)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# compilar
cmake --build build -j$(sysctl -n hw.ncpu)
# rodar (shaders + assets são copiados para build/ no build)
cd build && ./cg_final
```
A primeira configuração baixa GLFW/GLAD/GLM/ASSIMP (lento uma vez, rápido depois).
Release + LTO + strip: binário fica ~2 MB.

Rodar os testes:
```bash
ctest --test-dir build --output-on-failure
```

> **Roda sem nenhum asset.** Texturas e skybox têm fallback procedural gerado em
> C++. Assets reais em `assets/` substituem os fallbacks automaticamente.

## Controles
| Tecla | Ação |
|-------|------|
| WASD | Mover |
| Mouse | Olhar |
| Espaço / Shift | Subir / Descer |
| **Botão esq.** | Mirar turret → serenata; senão **portal AZUL** |
| **Botão dir.** | **portal LARANJA** |
| **G** | Mostra/esconde a portal gun (escondida = não dispara) |
| **E** | Agarra / solta o cubo (arremessa pra frente) |
| **F5** | Câmera 1ª / 3ª pessoa (estilo Minecraft) |
| **P** | Para a música da turret |
| Esc | Sair |

Entre num portal ativo (com os dois colocados) → teleporta pro outro, levando a
direção do olhar. O cubo também atravessa portais carregando o momento.

### Toggles de técnica (para a apresentação)
| Tecla | Técnica | Spec |
|-------|---------|------|
| 1 | Multi-textura (chão pedra→grama) | § 2d |
| 2 | Normal mapping (parede tijolos) | § 2g |
| 3 | Environment mapping (esfera) | § 2f |
| 4 | Neblina / Fog | § 2h |
| 5 | Blending (lanterna) | § 2i |
| 6 | Shadow map (luz spot, PCF 3×3) | § 2j |
| 7 | Luz spot | § 2c |
| 8 | Luz point (lanterna POV) | § 2c |
| 9 | Skybox | § 2e |
| C | Câmera cinemática (auto-orbita) | — |

## Cena
| Elemento | Asset | Técnica destacada |
|----------|-------|-------------------|
| Chão | grass_with_rocks (PBR color) + cobble procedural | multi-textura radial |
| Parede | tijolo procedural + normal map | normal mapping |
| Esfera | — | environment mapping (reflete skybox) |
| Lanterna | vidro procedural | blending |
| Castelo | `castle.3ds` + textura stone_wall | modelo `.3DS` |
| Casa | `minecraft_house.glb` | modelo glTF |
| Turret | `turret.glb` | varredura procedural + serenata |
| Personagem | `girl.glb` | pipeline skinned (vertex shader de ossos) |
| Portal gun | `hd_portal_gun.glb` | viewmodel na mão |
| Cubo | `portal_cube.glb` | física (gravidade) + portais |
| Portais | quad emissivo | shader de redemoinho + teleporte |

## Estrutura do código
```
src/
  app/        main.cpp          — loop, input, montagem da cena
  core/       shader, camera
  scene/      model, animated_model, animator, primitives, procgen, skybox
  render/     render (DrawItem), hud
  gameplay/   portal             — matemática de portal (sem GL, testável)
tests/        test_portal.cpp    — ctest: raycast, teleporte, física do cubo
shaders/      scene/skybox/depth/text + skinned
```
Headers ficam planos (`#include "shader.h"`); o CMake adiciona cada pasta ao
include path. A matemática de portal é GL-free de propósito → coberta por testes.

## Cobertura da Spec § 2
| Req | Elemento | Estado |
|-----|----------|--------|
| a. modelo `.3DS` | Castelo | ✅ `castle.3ds` carregado |
| b. UVW unwrap | UVs do `.3ds` + textura stone_wall aplicada | ✅ |
| c. Iluminação | Spot + Point, Phong | ✅ |
| d. Multi-textura | Chão pedra→grama (radial smoothstep) | ✅ |
| e. SkyBox | Cubemap 6 faces (gradiente procedural) | ✅ |
| f. EnvMap | Esfera reflete cubemap, `reflect(view, N)` | ✅ |
| g. Normal map | Parede de tijolos | ✅ |
| h. Fog | Linear por distância | ✅ |
| i. Blending | Lanterna alpha | ✅ |
| j. Shadow map | FBO 2048², depth pass, PCF 3×3 | ✅ |

## Spec § 3 bônus
- **(c) Som** — miniaudio: serenata da turret (clique) + sfx do portal gun. ✅
- **(b) Animação esqueletal** — pipeline skinned completo (formato de vértice com
  pesos de osso, `mat4[100]`, sampling lerp/slerp). Wired; o `girl.glb` atual não
  traz clips (0 animações) → personagem estático. Drop de um FBX Mixamo anima.
- **(a) PhysX** — não usado; em vez disso, física simples própria no cubo
  (gravidade + atravessa portais com momento).

## Extra (além da spec)
- **Mecânica de portais** estilo *Portal*: place + teleporte (sem see-through
  recursivo — pendente). Baseado na técnica de stencil do LearnOpenGL.
- **Portal gun** como viewmodel na mão, com botão para esconder (G).
- **Cubo companion** com física e teleporte por portal.
- **Câmera 1ª/3ª pessoa** (F5).

## `ponytail:` — simplificações deliberadas (e como evoluir)
- `gameplay/portal` teleporte: rotação 180° simples, sem see-through recursivo.
- viewmodel da gun: offset/rotação calibrados na mão (`GUN_OFFSET`, `GUN_EULER`).
- física do cubo: gravidade + chão plano, sem colisão com props.
- cinemática/footstep: período fixo, sem easing/acoplamento de fase.

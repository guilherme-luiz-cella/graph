# TL;DR do código — guia para defender o projeto

Cola de bolso: **se o professor perguntar "onde / como você fez X"**, aqui está o
arquivo e a ideia. Tudo é OpenGL 3.3 Core + C++17.

---

## Como o programa roda (fluxo geral)
`src/app/main.cpp` é o ponto de entrada e contém o **loop principal**:
1. **Init** — GLFW cria a janela fullscreen, GLAD carrega o OpenGL, compila shaders.
2. **Carga** — texturas (com fallback procedural), modelos via ASSIMP, áudio.
3. **Loop por frame**:
   - `input()` lê teclado/mouse → move a câmera, dispara toggles e ações.
   - lógica: portais, física do cubo, teleporte, varredura da turret, avatar.
   - **depth pass** (shadow map) → **main pass** (cena) → portais → vidro → skybox
     → gun → HUD.
4. `glfwSwapBuffers` mostra o frame.

## Mapa de arquivos (separação por responsabilidade)
| Pasta | Arquivo | Responsabilidade |
|-------|---------|------------------|
| `app/` | `main.cpp` | loop, input, montagem da cena, passes de render |
| `core/` | `shader.*` | compila/linka GLSL, setters de uniform |
| `core/` | `camera.*` | posição/orientação, matriz `view`, controle FPS |
| `scene/` | `model.*` | carrega malhas+texturas via ASSIMP, `Mesh::draw` |
| `scene/` | `animated_model.*`, `animator.*` | pipeline de animação esqueletal (ossos) |
| `scene/` | `primitives.*` | gera plano/cubo/esfera proceduralmente |
| `scene/` | `procgen.*` | texturas e cubemap procedurais (fallback) |
| `scene/` | `skybox.*` | cubemap + desenho do céu |
| `render/` | `render.*` | `DrawItem` + `renderItem` (liga técnicas por objeto) |
| `render/` | `hud.*` | overlay 2D (legenda, tooltips) com stb_easy_font |
| `gameplay/`| `portal.*` | **matemática de portal sem OpenGL** (testável) |
| `tests/` | `test_portal.cpp` | testes automáticos (ctest) da lógica de portal |
| `shaders/` | `scene.vs/fs`, `skinned.vs`, `depth.*`, `skybox.*`, `text.*` | GLSL |

> Headers ficam planos (`#include "shader.h"`); o CMake adiciona cada pasta ao
> include path. A lógica de portal é separada de propósito para poder ser testada
> sem janela/contexto OpenGL.

---

## Onde está cada técnica da Spec (arquivo → como)
| Spec | Onde | Como funciona |
|------|------|---------------|
| **2a/b** modelo .3DS + UV | `scene/model.cpp` `Model::load` | ASSIMP lê `castle.3ds`; UVs vêm do material. Em `main.cpp` troco o material pela textura `stone_wall`. |
| **2c** iluminação | `shaders/scene.fs` | Phong: ambiente + difusa por luz **point** (lanterna) e **spot** (cone com `smoothstep` entre cutoff interno/externo). |
| **2d** multi-textura | `shaders/scene.fs` (`useMultiTex`) | `mix(pedra, grama, smoothstep(6,12,r))` onde `r = length(posXZ)`. Mistura radial, sem `if`. |
| **2e** skybox | `scene/skybox.cpp` + `skybox.fs` | cubemap 6 faces, desenhado por último com `depth = LEQUAL`. |
| **2f** env map | `shaders/scene.fs` (`useEnvMap`) | `reflect(view, N)` amostra o cubemap; `mix` 60% reflexão na esfera. |
| **2g** normal map | `shaders/scene.fs` (`useNormalMap`) | normal da textura em espaço tangente (TBN) perturba `N` por pixel. |
| **2h** fog | `shaders/scene.fs` | `f = clamp((fogEnd-dist)/(fogEnd-fogStart))`, `mix(fogColor, cor, f)`. |
| **2i** blending | `main.cpp` passe "transparent" + `useAlpha` | `glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)`; lanterna com alpha. |
| **2j** shadow map | `main.cpp` `setupShadow` + depth pass; `scene.fs` `shadowCalc` | FBO de profundidade 2048², render do ponto de vista da spot, comparação com **PCF 3×3** (9 amostras) para suavizar a borda. |
| **3b** skeletal | `scene/animated_model.*`, `animator.*`, `skinned.vs` | vértice tem pesos de osso; `mat4 bones[100]` no shader transforma o vértice. (girl.glb atual sem clip → estático). |
| **3c** som | `main.cpp` (miniaudio) | serenata da turret (stateful, liga/desliga no clique) + sfx do tiro do portal. |

---

## Mecânicas extra (se perguntarem do "Portal")
- **Disparo de portal** — `main.cpp` `castPortal`: lança um raio da câmera e testa
  contra chão (`rayPlaneY`) e caixas AABB (`rayAABBn`, método slab). O ponto + a
  normal da face viram a posição/orientação do portal.
- **Teleporte** — `gameplay/portal.cpp` `portalTeleport`: se o jogador está dentro
  do disco do portal, mapeia posição e direção do olhar pro portal ligado (rotação
  de 180° em torno do "up"). `portalTeleportBody` faz o mesmo pro **cubo**,
  preservando o módulo da velocidade.
- **Física do cubo** — `main.cpp`: gravidade (`v.y -= 9.8*dt`), repouso no chão, e
  passa pelos portais. Pegar/soltar com `E` (soltar arremessa pra frente).
- **Vidro see-through** — a casa tem material `alphaMode=BLEND`; desenho com
  blending e `alpha = textura.a` (parede α=1 opaca, vidro α<1 transparente).
- **Visual do portal** — `scene.fs` (`useEmissive`): elipse recortada do quad, com
  redemoinho animado (`sin(r*16 - tempo)`) e borda brilhante.
- **Câmera 1ª/3ª pessoa** (`F5`) — em 3ª, a `girl` vira avatar na posição do jogador
  com um "andar" procedural (bob/sway quando se move; o modelo não tem clip real).
- **Jiggle de bust/rear** — `main.cpp` (loop do avatar/NPC): o bob vertical do torso
  (`bounce`) vira velocidade (`bounceVel = (bounce-prevBounce)/dt`) que alimenta dois
  **springs amortecidos** (mola-massa: `vel += (-drive*bounceVel - k*ang - c*vel)*dt`).
  O **bust** usa os ossos `J_Sec_*_Bust1` com jiggle de 2 eixos (X = quica, Z = sway
  lateral espelhado → balanço mais redondo, `swing2`); o **rear** (`rumpAng`) não tem
  osso próprio, então é **fingido** dobrando o contra-balanço do quadril nas duas pernas
  (`+ rear` no swing de `lLeg`/`rLeg`). Ângulos com `clamp` pra não estourar.

---

## Perguntas prováveis e respostas curtas
- **"Por que não tem DDD/camadas clássicas?"** É um renderer realtime de binário
  único; separei por concern (app/core/scene/render/gameplay) que é o padrão pra
  engine, não por domínio de negócio.
- **"Tem teste?"** Sim — a matemática de portal é GL-free e tem `ctest`
  (`ctest --test-dir build`): cobre raycast (acerto/erro), ortonormalidade da base,
  teleporte (sucesso + portal inativo) e preservação de velocidade do cubo.
- **"E o PhysX?"** Troquei por física simples própria no cubo, pra focar no gráfico.
- **"See-through recursivo nos portais?"** Ainda não — só place+teleporte. A técnica
  (stencil + câmera virtual, LearnOpenGL) está mapeada como próximo passo.
- **"Como builda?"** `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` →
  `cmake --build build -j` → `cd build && ./cg_final`. Release+LTO+strip → ~2 MB.

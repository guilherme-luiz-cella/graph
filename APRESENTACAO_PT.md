# Apresentação — Trabalho Final de Computação Gráfica

**Autor:** Guilherme Luiz Cella (105491)
**Tema:** Cena interativa em OpenGL 3.3 / C++ com mecânicas de *Portal*.

---

## Roteiro de ~6 minutos

### 1. Introdução (30s)
> "Esta é uma cena 3D em tempo real feita em **OpenGL 3.3 Core com C++17**. Toda a
> stack é compilada via CMake com FetchContent — não precisa instalar nada além do
> cmake. Além de cobrir as técnicas gráficas exigidas, adicionei mecânicas
> inspiradas no jogo *Portal*: a portal gun, dois portais com teleporte, e um cubo
> com física."

Aperta **C** para ligar a câmera cinemática (orbita sozinha durante a fala).

### 2. Técnicas gráficas — toggles (3 min)
Começa com tudo ligado: *"todas as técnicas ativas."* Depois liga/desliga cada uma:

- **9** → Skybox. *"Cubemap de 6 faces, desenhado por último com depth LEQUAL."* (§2e)
- **4** → Fog. *"Neblina linear baseada em distância à câmera."* (§2h)
- **1** → Multi-textura. *"Chão mistura pedra no centro e grama na borda com
  smoothstep radial — sem if. A grama é uma textura PBR real."* (§2d)
- **2** → Normal map. *"Bump per-pixel na parede de tijolos, normal em espaço
  tangente."* (§2g)
- **3** → Environment map. *"A esfera reflete o cubemap via reflect(view, N)."* (§2f)
- **6** → Shadow map. *"FBO de profundidade 2048², render do ponto de vista da luz
  spot, filtro PCF 3×3 (9 amostras)."* (§2j)
- **7** e **8** → luzes spot e point, pra mostrar a contribuição de cada uma. (§2c)
- **5** → Blending. *"A lanterna usa alpha translúcido."* (§2i)

### 3. Modelos (1 min)
Aperta **C** de novo (desliga cinemática), anda com **WASD**:

- Perto do **castelo**: *"Modelo no formato .3DS carregado via ASSIMP, com a textura
  stone_wall aplicada por cima do material."* (§2a/2b)
- A **casa** (minecraft_house.glb) e o **personagem** (girl.glb): *"glTF e pipeline
  skinned — o vertex shader transforma os vértices por matrizes de ossos."* (§3b)

### 4. Mecânicas de Portal (1.5 min)
- *"Esta é a portal gun, renderizada como viewmodel na mão. **G** esconde/mostra."*
- Mira numa parede ou no chão → **botão esquerdo** dispara o **portal azul**,
  **botão direito** o **laranja**. *"O portal é um quad emissivo com um shader de
  redemoinho animado e borda brilhante."*
- Andar pra dentro de um portal → **teleporta** pro outro levando a direção do olhar.
- **E** agarra o **cubo**; soltar arremessa pra frente. *"O cubo tem gravidade e
  também atravessa os portais carregando o momento — igual no jogo."*
- **F5** alterna 1ª / 3ª pessoa (estilo Minecraft).

### 5. Som e fechamento (30s)
- Clicar na **turret** → toca a serenata (miniaudio). **P** para.
- *"PhysX eu deixei de fora pra focar na qualidade gráfica; em vez disso fiz uma
  física simples própria no cubo. A matemática dos portais é separada em um módulo
  sem OpenGL e coberta por testes automáticos (ctest)."*

---

## Mapa rápido spec → onde mostrar
| Spec | Como demonstrar |
|------|-----------------|
| §2a/b modelo .3DS + UV | andar até o castelo |
| §2c iluminação | toggles 7 e 8 |
| §2d multi-textura | toggle 1 (chão) |
| §2e skybox | toggle 9 |
| §2f env map | toggle 3 (esfera) |
| §2g normal map | toggle 2 (parede) |
| §2h fog | toggle 4 |
| §2i blending | toggle 5 (lanterna) |
| §2j shadow map | toggle 6 |
| §3b skeletal | pipeline skinned (girl) |
| §3c som | clique na turret + tiro do portal |

## Se perguntarem "o que é seu, além da spec?"
- Mecânica de portais (place + teleporte), portal gun, cubo com física e teleporte.
- Câmera 1ª/3ª pessoa, HUD com legenda e tooltips.
- Organização modular do código (app/core/scene/render/gameplay) + testes.
- Build slim: Release + LTO + strip, binário ~2 MB; ASSIMP só com os importers usados.

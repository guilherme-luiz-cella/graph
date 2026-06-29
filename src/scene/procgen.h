#pragma once
// Procedural texture/cubemap generators.
// Used as runtime fallback when asset files are missing — so demo
// still shows every required technique without needing downloads.
// ponytail: noise here is value-noise hash, not perlin; upgrade only if visuals demand.

unsigned int genCobble(int size = 256);
unsigned int genGrass(int size = 256);
unsigned int genBrick(int size = 256);
unsigned int genBrickNormal(int size = 256);
unsigned int genGlass(int size = 64);
unsigned int genSkyCubemap(int size = 256);

// Flat solid color 2x2 texture — for procedural house panels (wall, roof, door, trim).
unsigned int genFlat(unsigned char r, unsigned char g, unsigned char b);

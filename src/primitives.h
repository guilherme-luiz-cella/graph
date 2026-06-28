#pragma once
// Procedural geometry — keeps demo runnable before all real assets land.
// Each builder returns a Mesh ready to draw with scene.fs.
#include "model.h"

Mesh makePlane(float size, float uvTile);
Mesh makeCube(float size);
Mesh makeSphere(float radius, int segments);

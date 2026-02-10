#ifndef RENDER_H
#define RENDER_H

#include "structures.h"

const bool showStatistics = false;

void clearRenderBuffer();

void submitMesh(const Mesh &mesh);

void drawFrame(const CameraInfo &camera, uint *buffer);


#endif // RENDER_H

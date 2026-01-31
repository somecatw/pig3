#ifndef RENDER_H
#define RENDER_H

#include <vector>
#include "structures.h"

void drawFrame(const std::vector<Vertex> &vertices, const std::vector<Triangle> &triangles, const CameraInfo &camera, uint *buffer);


#endif // RENDER_H

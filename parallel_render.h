#ifndef PARALLEL_RENDER_H
#define PARALLEL_RENDER_H

#include <QObject>
#include "structures.h"
#include "utils.h"

const int threadCount = 4;
const uint tileBufferH = 20, tileBufferW = 30;

struct Tile{
    int tileX, tileY;
    int xlt, ylt;
    uint triangleID[tileSize][tileSize];
    uint color[tileSize][tileSize];
    float zInv[tileSize][tileSize], u_z[tileSize][tileSize], v_z[tileSize][tileSize];
    float zInvMin;
    float derivative[tileSize][tileSize];
    bool vis[tileSize][tileSize];
    int cpCount;
};

struct TiledFragment{
    int xlt, ylt, xrb, yrb;
    const Fragment *fragment;
};

struct RenderTask{
    Tile *tile;
    std::vector<const Fragment*> fragments;

    RenderTask() = default;
    RenderTask(const RenderTask&) = default;
    RenderTask(RenderTask &&other) noexcept{
        if(&other == this)return;
        tile = other.tile;
        swap(fragments, other.fragments);
    }
    RenderTask &operator=(const RenderTask&) = default;
    void operator()();
};

class RenderTaskDispatcher{
public:
    int tileH, tileW;
    RenderTask taskBuffer[tileBufferH][tileBufferW];
    uint *globalColorBuffer;
    TaskDispatcher<RenderTask> disp;

    RenderTaskDispatcher(int _threadCount):disp(_threadCount){}
    void init();
    void submitFragment(const Fragment &frag, int tileX, int tileY);
    void finish();
};

extern RenderTaskDispatcher taskDispatcher;

#endif // PARALLEL_RENDER_H

#ifndef PARALLEL_RENDER_H
#define PARALLEL_RENDER_H

#include <QObject>
#include <thread>
#include "structures.h"

const int threadCount = 4;
const uint tileBufferH = 20, tileBufferW = 30;

struct Tile{
    int tileX, tileY;
    int xlt, ylt;
    uint triangleID[tileSize][tileSize];
    uint materialID[tileSize][tileSize];
    uint color[tileSize][tileSize];
    float zInv[tileSize][tileSize], u_z[tileSize][tileSize], v_z[tileSize][tileSize];
    float zInvMin;
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

    uint workLoad()const{
        return std::accumulate(fragments.begin(), fragments.end(), 0, [](uint val, const Fragment *ptr){
            return val + (ptr->xrb - ptr->xlt) * (ptr->yrb - ptr->ylt);
        });
    }
};


struct WorkerControl {
    std::atomic<int> state{0}; // 0: 等待, 1: 执行, 2: 退出
    std::vector<RenderTask> bucket;
};

class RenderTaskDispatcher{
public:
    RenderTaskDispatcher(int threadCount);
    ~RenderTaskDispatcher();
    void init();
    void submitFragment(const Fragment &frag, int tileX, int tileY);
    void finish();
private:
    void runBatch(std::vector<std::vector<RenderTask>> &&buckets);

    int taskCount;
    bool finishFlag;

    RenderTask taskBuffer[tileBufferH][tileBufferW];
    int tileH, tileW;
    uint *globalColorBuffer;

    int threadCount;
    std::vector<std::thread> workers;
    std::vector<WorkerControl*> controls;
    std::atomic<int> finishedCount;

};
extern RenderTaskDispatcher taskDispatcher;

#endif // PARALLEL_RENDER_H

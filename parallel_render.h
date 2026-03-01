#ifndef PARALLEL_RENDER_H
#define PARALLEL_RENDER_H

#include <QObject>
#include <latch>
#include <thread>
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

template<std::semiregular TaskType> requires std::invocable<TaskType>
class TaskDispatcher{

    struct WorkerControl {
        std::atomic<int> state{0}; // 0: 等待, 1: 执行, 2: 退出
        std::vector<TaskType> bucket;
        std::atomic<int> head;
        int tail;
    };
public:
    TaskDispatcher(int _threadCount):finishedCount(0), threadCount(_threadCount){
        for(int i=0;i<threadCount;i++){
            controls.push_back(new WorkerControl());
            workers.emplace_back([this, i] {
                auto* ctrl = controls[i];
                while (true) {
                    ctrl->state.wait(0);
                    if (ctrl->state.load() == 2) break;
                    ctrl->head = 0;
                    ctrl->tail = ctrl->bucket.size()-1;

                    while(ctrl->head <= ctrl->tail) {
                        int chead = ctrl->head;
                        int ctail = ctrl->tail;
                        if(chead > ctail)continue;
                        int tmp = chead + 1;
                        if(ctrl->head.compare_exchange_strong(chead, tmp)){
                            // if(ctrl->head > ctail) continue;
                            ctrl->bucket[chead]();
                        }

                    }
                    ctrl->state.store(0);
                    finishedCount.fetch_add(1);
                    while(true){
                        if(this->finishedCount.load() == this->threadCount) break;
                        std::vector<int> ids;
                        for(auto [id, cxk]: enumerate(controls))
                            if(cxk->head <= cxk ->tail) ids.push_back(id);

                        if(!ids.size()) continue;
                        int id = ids[rand()%ids.size()];

                        int chead = controls[id]->head;
                        int ctail = controls[id]->tail;
                        if(chead > ctail)continue;
                        int tmp = chead + 1;
                        if(controls[id]->head.compare_exchange_strong(chead, tmp)){
                            // if(controls[id]->head > ctail) continue;
                            controls[id]->bucket[chead]();
                        }
                    }
                    workDone->count_down();
                }
            });

        }
    }
    ~TaskDispatcher(){
        for (auto* ctrl : controls) {
            ctrl->state.store(2);
            ctrl->state.notify_one();
        }
        for (auto& t : workers) t.join();
        for (auto* ctrl : controls) delete ctrl;
    }

    void runBatch(std::vector<std::vector<TaskType>> &&buckets){
        finishedCount.store(0, std::memory_order_relaxed);
        int taskCount = 0;

        workDone = make_unique<std::latch>(threadCount);

        for (int i = 0; i < threadCount; ++i) {
            taskCount += buckets[i].size();
            controls[i]->bucket = std::move(buckets[i]);
            controls[i]->state.store(1);
            controls[i]->state.notify_one();
        }
        workDone->wait();
    }

private:

    int threadCount;
    std::vector<std::thread> workers;
    std::vector<WorkerControl*> controls;
    std::atomic<int> finishedCount;

    std::unique_ptr<std::latch> workDone;

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

#include <QThreadPool>
#include <QObject>
#include <QDebug>
#include "parallel_render.h"
#include "shader_interface.h"
#include "utils.h"

using namespace std;

Tile tiles[tileBufferH][tileBufferW];
RenderTaskDispatcher taskDispatcher(threadCount);

using namespace ShaderInternal;

int tileLevelIterate(const Fragment &frag, int tileXlt, int tileYlt){
    int flags[4][3], innerFlag = true;
    for(int i=0;i<2;i++){
        for(int j=0;j<2;j++){
            EdgeIterator it = frag.edgeIterator;
            it.batchIterate(tileXlt + i*(tileSize-1), tileYlt + j*(tileSize-1));
            for(int k=0;k<3;k++){
                flags[i*2+j][k] = (it.e[k].val >= 0);
            }
            if(it.check() == EdgeIterator::OUTER) innerFlag = false;
        }
    }
    for(int i=0;i<3;i++){
        bool flag = true;
        for(int j=0;j<4;j++){
            if(flags[j][i] == 1){
                flag = false;
                break;
            }
        }
        if(flag) return TileLevelResult::OUTER;
    }
    if(innerFlag)
        return TileLevelResult::INNER;
    return TileLevelResult::UNKNOWN;
}

std::atomic<int> tmp1 = 0, tmp2=0, tmp3=0;

void runTask(RenderTask &task){
    Tile *tile = task.tile;
    tile->zInvMin = 1e9f;
    tile->cpCount = 0;

    for(int y=0;y<tileSize;y++)
        for(int x=0;x<tileSize;x++){
            tile->triangleID[y][x] = 0x80000000;
        }
    memset(tile->zInv, 0, sizeof(tile->zInv));
    memset(tile->vis, 0, sizeof(tile->vis));


    sort(task.fragments.begin(), task.fragments.end(), [](const Fragment *a, const Fragment *b){
        return maxZInv[a->triangleID] > maxZInv[b->triangleID];
    });

    for(const Fragment *ptr:task.fragments){
        int tileLevelResult = TileLevelResult::UNKNOWN;

        if(ptr->xlt <= tile->tileX*tileSize
            && ptr->ylt <= tile->tileY*tileSize
            && ptr->xrb > (tile->tileX+1)*tileSize
            && ptr->yrb > (tile->tileY+1)*tileSize)
            tileLevelResult = tileLevelIterate(*ptr, tile->tileX*tileSize, tile->tileY*tileSize);

        uint shaderConfig = ShaderInternal::triangles[ptr->triangleID].shaderConfig;

        if(shaderConfig & ShaderConfig::WireframeOnly)
            tileRasterization<WireframeShader>(*ptr, *tile, tileLevelResult);
        else
            tileRasterization<BaseShader>(*ptr, *tile, tileLevelResult);

    }

    int tileXlt = tile->tileX * tileSize;
    int tileYlt = tile->tileY * tileSize;

    for(int y=0;y<tileSize;y++){
        for(int x=0;x<tileSize;x++){
            if(tile->triangleID[y][x] < 0x80000000u){
                tile->u_z[y][x] /= tile->zInv[y][x];
                tile->v_z[y][x] /= tile->zInv[y][x];
            }
        }
    }
    for(int y=0;y<tileSize;y++){
        int globalY = tileYlt+y;
        int bufferBase = globalY * ShaderInternal::pixelW;
        for(int x=0;x<tileSize;x++){
            uint colorRef = 0xff000000;
            if(tile->triangleID[y][x] < 0x80000000u){

                float u = tile->u_z[y][x];
                float v = tile->v_z[y][x];

                uint shaderConfig = ShaderInternal::triangles[tile->triangleID[y][x]].shaderConfig;

                float d = 0.0f;
                if(!(shaderConfig & ShaderConfig::DisableMipmap)){
                    float ux = 0.0f, vx = 0.0f, uy = 0.0f, vy = 0.0f;
                    if(x+1 < tileSize){
                        ux = tile->u_z[y][x+1] - u;
                        vx = tile->v_z[y][x+1] - v;
                    }else if(x > 0){
                        ux = tile->u_z[y][x-1] - u;
                        vx = tile->v_z[y][x-1] - v;
                    }
                    if(y+1 < tileSize){
                        uy = tile->u_z[y+1][x] - u;
                        vy = tile->v_z[y+1][x] - v;
                    }else if(y > 0){
                        uy = tile->u_z[y-1][x] - u;
                        vy = tile->v_z[y-1][x] - v;
                    }
                    d = sqrt(max(ux*ux+vx*vx, uy*uy+vy*vy));
                    // tile->derivative[y][x] = d;

                }

                if(shaderConfig & ShaderConfig::WireframeOnly)
                    colorRef = colorDetermination<WireframeShader>(u, v, task.tile->triangleID[y][x], {1,0,0}, d);
                else
                    colorRef = colorDetermination<BaseShader>(u, v, task.tile->triangleID[y][x], {1,0,0}, d);
            }
            int globalX = tileXlt+x;


            // if(globalX%64==0 || globalY%64==0) colorRef = 0xffff0000;
            ShaderInternal::buffer[bufferBase + globalX] = colorRef;
        }
    }
}


RenderTaskDispatcher::RenderTaskDispatcher(int _threadCount)
    :finishedCount(0), threadCount(_threadCount)
{
    for(int i=0;i<threadCount;i++){
        controls.push_back(new WorkerControl());
        workers.emplace_back([this, i] {
            auto* ctrl = controls[i];
            while (true) {
                ctrl->state.wait(0, std::memory_order_acquire);
                if (ctrl->state.load(std::memory_order_relaxed) == 2) break;
                ctrl->head = 0;
                ctrl->tail = ctrl->bucket.size()-1;

                // head: 第一个没做的任务
                // tail: 最后一个没被抢的任务
                // 现在逻辑还有点小问题，可能有一个 task 被做了两次，概率非常低
                while(ctrl->head <= ctrl->tail) {
                    int h = ctrl->head;
                    ctrl->head ++;
                    runTask(ctrl->bucket[h]);
                }
                ctrl->state.store(0, std::memory_order_release);
                finishedCount.fetch_add(1, std::memory_order_release);
                while(true){
                    if(this->finishedCount.load() == this->threadCount) break;
                    for(int id=0;id<this->threadCount;id++){
                        int chead = controls[id]->head;
                        int ctail = controls[id]->tail;
                        if(chead > ctail)continue;
                        int tmp = ctail - 1;
                        if(controls[id]->tail.compare_exchange_strong(ctail, tmp)){
                            if(controls[id]->head > ctail) continue;
                            runTask(controls[id]->bucket[ctail]);
                        }
                    }
                }
                workDone->count_down();
            }
        });

    }
}
RenderTaskDispatcher::~RenderTaskDispatcher(){
    for (auto* ctrl : controls) {
        ctrl->state.store(2);
        ctrl->state.notify_one();
    }
    for (auto& t : workers) t.join();
    for (auto* ctrl : controls) delete ctrl;
}

void RenderTaskDispatcher::runBatch(vector<std::vector<RenderTask>> &&buckets){

    finishedCount.store(0, std::memory_order_relaxed);
    int taskCount = 0;

    for (int i = 0; i < threadCount; ++i) {
        taskCount += buckets[i].size();
        controls[i]->bucket = std::move(buckets[i]);
        controls[i]->state.store(1, std::memory_order_release);
        controls[i]->state.notify_one();
    }

    workDone = make_unique<latch>(threadCount);
    workDone->wait();

}
void RenderTaskDispatcher::init(){
    tileH = camera.height;
    tileW = camera.width;

    for(int i=0;i<tileH;i++){
        for(int j=0;j<tileW;j++){
            tiles[i][j].tileX = j;
            tiles[i][j].tileY = i;
            taskBuffer[i][j].tile = &tiles[i][j];
            taskBuffer[i][j].fragments.clear();
        }
    }
}

void RenderTaskDispatcher::submitFragment(const Fragment &frag, int tileX, int tileY){
    RenderTask &task = taskBuffer[tileY][tileX];
    task.fragments.push_back(&frag);
}



void RenderTaskDispatcher::finish(){

    static vector<RenderTask> dogTasks;
    dogTasks.clear();

    tmp1=0;
    for(int y=0;y<tileH;y++){
        for(int x=0;x<tileW;x++){
            RenderTask &task = taskBuffer[y][x];
            dogTasks.push_back(std::move(task));
            // task.fragments.clear();
        }
    }

    static vector<vector<RenderTask>> threadTasks(threadCount);
    for(int i=0;i<threadCount;i++)
        threadTasks[i].clear();

    // 排个序随便分一下
    // sort(dogTasks.begin(), dogTasks.end(), [](const pair<int, RenderTask> &a, const pair<int, RenderTask> &b){return a.first > b.first;});

    int ttfa = (dogTasks.size()-1) / threadCount+1;
    for(auto [id, task]:enumerate(dogTasks)){
        threadTasks[id%threadCount].push_back(std::move(task));
        // loads[id%threadCount] += pair.first;
    }

    runBatch(std::move(threadTasks));
    tmp1=tmp2=tmp3=0;
}


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

std::atomic<int> tmp1 = 0, tmp2=0;;

void runTask(const RenderTask &task){
    Tile *tile = task.tile;
    tile->zInvMin = 0.0f;
    for(int y=0;y<tileSize;y++)
        for(int x=0;x<tileSize;x++){
            tile->triangleID[y][x] = 0x80000000;
            tile->zInv[y][x] = 0;
        }
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

    for(int y=0;y<tileSize;y++)
        for(int x=0;x<tileSize;x++){
            uint colorRef = 0xff000000;
            if(tile->triangleID[y][x] < 0x80000000u){
                float z = 1.0 / tile->zInv[y][x];
                float u = tile->u_z[y][x] * z;
                float v = tile->v_z[y][x] * z;

                uint shaderConfig = ShaderInternal::triangles[tile->triangleID[y][x]].shaderConfig;
                if(shaderConfig & ShaderConfig::WireframeOnly)
                    colorRef = colorDetermination<WireframeShader>(u, v, task.tile->triangleID[y][x], {1,0,0});
                else
                    colorRef = colorDetermination<BaseShader>(u, v, task.tile->triangleID[y][x], {1,0,0});
            }
            int globalX = tileXlt+x;
            int globalY = tileYlt+y;

            if(globalX%64==0 || globalY%64==0) colorRef = 0xffff0000;
            ShaderInternal::buffer[globalY * ShaderInternal::pixelW + globalX] = colorRef;
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
                    taskFinishedCount ++;
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
                            taskFinishedCount ++;
                        }
                    }
                }
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
    taskFinishedCount = 0;

    for (int i = 0; i < threadCount; ++i) {
        taskCount += buckets[i].size();
        controls[i]->bucket = std::move(buckets[i]);
        controls[i]->state.store(1, std::memory_order_release);
        controls[i]->state.notify_one();
    }

    // 又有原子又有自旋，我看这是量子物理 ---somecat

    // 主线程自旋等待完成：4ms 级别不建议挂起，直接自旋
    auto t0 = chrono::system_clock::now();
    int curr = 0, tmp;
    while (1) {
        // 这里可以加一行针对特定平台的 pause 指令来稍微优化功耗
        tmp = finishedCount.load(std::memory_order_acquire);
        if(tmp > curr){
            curr = tmp;
            auto t = chrono::system_clock::now();
            qDebug()<<t-t0;
        }
        if(taskFinishedCount >= taskCount) break;
        auto t = chrono::system_clock::now();
        if(t-t0 > chrono::seconds(1)){
            qDebug()<<taskFinishedCount;
        }
    }

    // qDebug()<<t-t0;

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

    vector<pair<int, RenderTask>> dogTasks;

    for(int y=0;y<tileH;y++){
        for(int x=0;x<tileW;x++){
            RenderTask &task = taskBuffer[y][x];
            int load = 0;
            for(const Fragment *ptr:task.fragments){
                int xlt = max(ptr->xlt, x*tileSize);
                int xrb = min(ptr->xrb, x*tileSize+tileSize);
                int ylt = max(ptr->ylt, y*tileSize);
                int yrb = min(ptr->yrb, y*tileSize+tileSize);
                load += (yrb-ylt)*(xrb-xlt);
            }
            dogTasks.push_back({load, std::move(task)});
            // task.fragments.clear();
        }
    }

    vector<vector<RenderTask>> threadTasks(threadCount);

    // 排个序随便分一下
    sort(dogTasks.begin(), dogTasks.end(), [](const pair<int, RenderTask> &a, const pair<int, RenderTask> &b){return a.first > b.first;});
    int loads[4] = {0};
    for(auto [id, pair]:enumerate(dogTasks)){
        threadTasks[id%threadCount].push_back(std::move(pair.second));
    }

    runBatch(std::move(threadTasks));
    tmp1=tmp2=0;
}


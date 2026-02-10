#include "stage3d.h"
#include "render.h"
#include "raytest.h"
#include <QPaintEvent>
#include <QPainter>
using namespace std;

Stage3D::Stage3D(QWidget *parent)
    : QWidget{parent}
{
    activeCam = nullptr;
    root = new GameObject(this);
}

void Stage3D::updateFrame(){
    auto frameStart = chrono::system_clock::now();
    clearRenderBuffer();
    updateObjects(root, Transform());
    if(activeCam != nullptr){
        submitObjects(root);
        int pw = activeCam->camInfo.width * tileSize;
        int ph = activeCam->camInfo.height * tileSize;
        if(frameBuffer.isNull() || pw != frameBuffer.width() || ph != frameBuffer.height()){
            frameBuffer = QImage(pw, ph, QImage::Format_ARGB32);
        }
        drawFrame(activeCam->camInfo, (uint*)frameBuffer.bits());
        avgRenderTime = 1e6 / frameStat.fps;
    }
    QWidget::update();
    auto frameEnd = chrono::system_clock::now();
    double t = chrono::duration_cast<chrono::microseconds>(frameEnd - frameStart).count();
    frameTimes.push_back(t);
    if(frameTimes.size() > 100u) frameTimes.erase(frameTimes.begin());
    avgFrameTime = accumulate(frameTimes.begin(), frameTimes.end(), 0.0) / frameTimes.size();
}
void Stage3D::paintEvent(QPaintEvent *evt){
    if(activeCam == nullptr) return;
    if(frameBuffer.isNull()) return;
    QPainter painter(this);
    QImage img = frameBuffer.scaled(size());
    int x = (width() - img.width())/2;
    int y = (height() - img.height())/2;
    painter.drawImage(x, y, img);
}

void Stage3D::updateObjects(GameObject *rt, const Transform &curr, bool ignoreFlag) const{
    if(rt == nullptr) return;
    rt->modified = false;
    for(GameObject *child: rt->children()){
        assert(child != nullptr);
        Transform ttfa = child->transform * curr;
        if(child->modified || ignoreFlag)
            child->updatePosition(ttfa);
        updateObjects(child, ttfa, ignoreFlag || child->modified);
    }
}
void Stage3D::submitObjects(GameObject *rt) const{
    if(rt == nullptr) return;
    for(GameObject *child: rt->children()){
        child->submitForRender();
        submitObjects(child);
    }
}

SceneRayHit Stage3D::raytest(const Ray &ray)const{
    return recursiveRaytest(root, Transform(), ray);
}

// 过会给对象树也安排上 BVH
SceneRayHit Stage3D::recursiveRaytest(GameObject *rt, const Transform &globalTrans, const Ray &ray) const{
    MeshActor *actor = dynamic_cast<MeshActor*>(rt);
    SceneRayHit dog;
    assert(rt != nullptr);
    if(actor != nullptr){
        Transform inv = globalTrans.inverseTransform();
        Ray localRay = {ray.point * inv.rotation + inv.translation, ray.direction * inv.rotation};
        auto ttfa = raytestManager.meshIntersect(actor->meshID, localRay);
        if(ttfa.hit){
            dog.actor = actor;
            dog.dis   = ttfa.dis;
            dog.pos   = ttfa.pos * globalTrans.rotation + globalTrans.translation;
            dog.triangleID = ttfa.leaf.triangleID;
        }
    }
    for(GameObject *child:rt->children()){
        SceneRayHit ttfa = recursiveRaytest(child, child->transform * globalTrans, ray);
        if(ttfa.actor != nullptr && dog.actor != nullptr && ttfa.dis < dog.dis) dog = ttfa;
        if(ttfa.actor != nullptr && dog.actor == nullptr) dog = ttfa;
    }
    return dog;
}

Ray Stage3D::pixelToRay(int x, int y) const{
    if(activeCam == nullptr || this->frameBuffer.isNull()) return {};
    int x1 = (float)x/this->size().width() * this->frameBuffer.width();
    int y1 = (float)y/this->size().height() * this->frameBuffer.height();
    return activeCam->pixelToRay(x1, y1);
}

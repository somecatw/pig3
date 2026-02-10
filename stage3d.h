#ifndef STAGE3D_H
#define STAGE3D_H

#include <QWidget>
#include "gameobject.h"

struct SceneRayHit{
    MeshActor *actor = nullptr;
    uint triangleID;
    float dis;
    Vec3 pos;
};

class Stage3D : public QWidget
{
    Q_OBJECT
public:
    explicit Stage3D(QWidget *parent = nullptr);

    QImage frameBuffer;
    GameObject *root;
    Camera *activeCam;

    double avgFrameTime;
    double avgRenderTime;

    void updateFrame();
    Ray pixelToRay(int x, int y)const;
    SceneRayHit raytest(const Ray &ray)const;
protected:
    void paintEvent(QPaintEvent *evt) override;
private:
    std::vector<double> frameTimes;
    void updateObjects(GameObject *rt, const Transform &c, bool ignoreFlag=false) const;
    void submitObjects(GameObject *rt) const;
    SceneRayHit recursiveRaytest(GameObject *rt, const Transform &globalTrans, const Ray &ray)const;

signals:
};

#endif // STAGE3D_H

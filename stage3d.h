#ifndef STAGE3D_H
#define STAGE3D_H

#include <QWidget>
#include "assetmanager.h"
#include "gameobject.h"
#include "raytest.h"
#include <set>

struct SceneRayHit{
    MeshActor *actor = nullptr;
    uint triangleID;
    float dis;
    Vec3 pos;
    bool miss()const{
        return actor == nullptr;
    }
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
    void buildStaticBVH();
    GameObject *loadObj(const QString &path, bool isStatic=false);
    Ray pixelToRay(int x, int y)const;
    SceneRayHit raytest(const Ray &ray)const;
    std::set<BoxtestResult> boxtest(const BBox3D &box)const;

protected:
    void paintEvent(QPaintEvent *evt) override;
private:
    std::vector<double> frameTimes;
    void updateObjects(GameObject *rt, const Transform &c, bool ignoreFlag=false) const;
    void submitObjects(GameObject *rt) const;

    // AssetManager assetManager;
    RaytestManager raytestManager;

signals:
};

#endif // STAGE3D_H

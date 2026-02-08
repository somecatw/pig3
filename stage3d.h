#ifndef STAGE3D_H
#define STAGE3D_H

#include <QWidget>
#include "gameobject.h"

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
protected:
    void paintEvent(QPaintEvent *evt) override;
private:
    std::vector<double> frameTimes;
    void updateObjects(GameObject *rt, const Transform &c, bool ignoreFlag=false) const;
    void submitObjects(GameObject *rt) const;

signals:
};

#endif // STAGE3D_H

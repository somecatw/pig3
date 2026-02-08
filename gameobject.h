#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <QObject>
#include "transform.h"
#include "structures.h"


class GameObject : public QObject
{
    Q_OBJECT
public:

    explicit GameObject(QObject *parent = nullptr);

    // 两个函数都是相对于父对象的局部变换
    void translate(const Vec3 &v);
    void rotateAroundAxis(const Vec3 &axis, float rad);

    void setTransform(const Transform &t);
    const Transform &getTransform()const;
    virtual void updatePosition(const Transform &t);
    virtual void submitForRender();

    GameObject *parent() const;
    QList<GameObject*> children() const;

    friend class Stage3D;

private:
    bool modified;
    Transform transform;

signals:
};

class MeshActor : public GameObject{
    Q_OBJECT
public:
    uint meshID;
    Mesh mesh;
    explicit MeshActor(uint _meshID, QObject *parent = nullptr);
    void updatePosition(const Transform &t) override;
    void submitForRender() override;
};

class Camera : public GameObject{
    Q_OBJECT
public:
    CameraInfo camInfo;
    explicit Camera(const CameraInfo &info, QObject *parent = nullptr);
};


#endif // GAMEOBJECT_H

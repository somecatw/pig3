#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <QObject>
#include "transform.h"

class GameObject : public QObject
{
    Q_OBJECT
public:
    Transform transform;
    explicit GameObject(QObject *parent = nullptr);
    // virtual void applyTransform(const Transform &t);
    // virtual void submitForRender();

signals:
};

class MeshActor : public GameObject{
    Q_OBJECT
public:
    explicit MeshActor(QObject *parent = nullptr);
};

#endif // GAMEOBJECT_H

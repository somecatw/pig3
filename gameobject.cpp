#include "gameobject.h"

GameObject::GameObject(QObject *parent)
    : QObject{parent}
{}

MeshActor::MeshActor(QObject *parent) : GameObject(parent){}

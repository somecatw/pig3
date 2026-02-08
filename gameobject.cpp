#include "gameobject.h"
#include "assetmanager.h"
#include "render.h"
#include <QDebug>

GameObject::GameObject(QObject *parent)
    : QObject{parent}, transform()
{}

void GameObject::updatePosition(const Transform &t){}

void GameObject::submitForRender(){}

void GameObject::translate(const Vec3 &v){
    modified = true;
    transform = transform * Transform::translate(v);
}

void GameObject::setTransform(const Transform &t){
    modified = true;
    transform = t;
}
const Transform &GameObject::getTransform()const{
    return transform;
}

void GameObject::rotateAroundAxis(const Vec3 &v, float rad){
    modified = true;
    transform = transform * Transform::rotateAroundAxis(v, rad);
}

GameObject *GameObject::parent() const{
    return dynamic_cast<GameObject*>(QObject::parent());
}

QList<GameObject*> GameObject::children() const{
    QList<GameObject*> ret;
    for(QObject *obj: QObject::children()){
        GameObject *p = dynamic_cast<GameObject*>(obj);
        if(p != nullptr) ret.push_back(p);
    }
    return ret;
}

MeshActor::MeshActor(uint _meshID, QObject *_parent)
    : GameObject(_parent), meshID(_meshID){

    mesh = assetManager.getMeshes().at(meshID);
}

void MeshActor::updatePosition(const Transform &t){
    mesh = assetManager.getMeshes().at(meshID);
    mesh.applyTransform(t);
}

void MeshActor::submitForRender(){
    submitMesh(mesh);
}

Camera::Camera(const CameraInfo &info, QObject *parent): GameObject(parent), camInfo(info){}


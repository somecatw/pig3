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

void GameObject::localTranslate(const Vec3 &v){
    modified = true;
    transform = Transform::translate(v) * transform;
}

void GameObject::moveToLocalPos(const Vec3 &v){
    modified = true;
    transform.translation = v;
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
    Vec3 tmp = transform.translation;
    transform = transform * Transform::rotateAroundAxis(v, rad);
    transform.translation = tmp;
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

void Camera::updatePosition(const Transform &t){
    camInfo.pos = t.translation;
    camInfo.frame.axisX = t.rotation.row(0);
    camInfo.frame.axisY = t.rotation.row(1);
    camInfo.frame.axisZ = t.rotation.row(2);

}

Ray Camera::pixelToRay(int x, int y)const{
    int w = camInfo.width * tileSize;
    int h = camInfo.height * tileSize;
    float fx = ((float)x/w - 0.5f) * camInfo.screenSize.x;
    float fy = ((float)y/h - 0.5f) * camInfo.screenSize.y;
    Vec3 dir = camInfo.frame.axisZ * camInfo.focalLength + camInfo.frame.axisX * fx + camInfo.frame.axisY * fy;
    return {camInfo.pos, dir.normalized()};
}

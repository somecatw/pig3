#include "raytest.h"
#include <QDebug>
using namespace std;
using namespace Raytest;

int Raytest::intersectCallCnt;

std::vector<BVHLeaf<Triangle>> toLeafVec(const Mesh &mesh){
    std::vector<BVHLeaf<Triangle>> ret;
    for(auto [id, _]: enumerate(mesh.triangles)){
        ret.emplace_back(mesh, id);
    }
    return ret;
}

float BBox3D::intersectDis(const Ray &ray)const{
    if(ray.point.x >= x1 && ray.point.x <= x2
        && ray.point.y >= y1 && ray.point.y <= y2
        && ray.point.z >= z1 && ray.point.z <= z2) return 1e-5;
    if(std::abs(ray.direction.x) > 1e-5){
        float target = ray.direction.x > 0.0f? x1: x2;
        Vec3 hit = ray.point + ray.direction * (target-ray.point.x) / ray.direction.x;
        if(hit.y >= y1 && hit.y <= y2 && hit.z >= z1 && hit.z <= z2) return (hit-ray.point).dot(ray.direction);
    }
    if(std::abs(ray.direction.y) > 1e-5){
        float target = ray.direction.y > 0.0f? y1: y2;
        Vec3 hit = ray.point + ray.direction * (target-ray.point.y) / ray.direction.y;
        if(hit.x >= x1 && hit.x <= x2 && hit.z >= z1 && hit.z <= z2) return (hit-ray.point).dot(ray.direction);
    }
    if(std::abs(ray.direction.z) > 1e-5){
        float target = ray.direction.z > 0.0f? z1: z2;
        Vec3 hit = ray.point + ray.direction * (target-ray.point.z) / ray.direction.z;
        if(hit.x >= x1 && hit.x <= x2 && hit.y >= y1 && hit.y <= y2) return (hit-ray.point).dot(ray.direction);
    }
    return -1.0f;
}

void RaytestManager::appendMesh(const Mesh &mesh){
    meshTrees.push_back(BVHTree<Triangle>(toLeafVec(mesh)));
    qDebug()<<"built BVH tree for mesh"<<mesh.meshID<<"with"<<meshTrees.back().nodes.size()<<"nodes";
}

RaytestResult<Triangle> RaytestManager::meshIntersect(uint meshID, const Ray &ray)const{
    return meshTrees[meshID].intersect(ray);
}

RaytestManager raytestManager;

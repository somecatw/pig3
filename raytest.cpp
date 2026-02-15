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

// By gemini
// 针对单个平面裁剪多边形
// axis: 0-x, 1-y, 2-z
// isGreater: true 表示保留坐标大于 value 的部分，false 表示保留坐标小于 value 的部分
std::vector<Vec3> clipPolygon(const std::vector<Vec3>& poly, int axis, float value, bool isGreater) {
    std::vector<Vec3> result;
    if (poly.empty()) return result;

    for (size_t i = 0; i < poly.size(); ++i) {
        Vec3 p1 = poly[i];
        Vec3 p2 = poly[(i + 1) % poly.size()];

        float v1 = (axis == 0) ? p1.x : (axis == 1 ? p1.y : p1.z);
        float v2 = (axis == 0) ? p2.x : (axis == 1 ? p2.y : p2.z);

        bool in1 = isGreater ? (v1 >= value) : (v1 <= value);
        bool in2 = isGreater ? (v2 >= value) : (v2 <= value);

        if (in1 && in2) {
            result.push_back(p2);
        } else if (in1 && !in2) {
            float t = (value - v1) / (v2 - v1);
            result.push_back({
                p1.x + t * (p2.x - p1.x),
                p1.y + t * (p2.y - p1.y),
                p1.z + t * (p2.z - p1.z)
            });
        } else if (!in1 && in2) {
            float t = (value - v1) / (v2 - v1);
            result.push_back({
                p1.x + t * (p2.x - p1.x),
                p1.y + t * (p2.y - p1.y),
                p1.z + t * (p2.z - p1.z)
            });
            result.push_back(p2);
        }
    }
    return result;
}

float getYMin(Vec3 v0, Vec3 v1, Vec3 v2, const BBox3D &box) {
    // 1. 初始化多边形为三角形的三个顶点
    std::vector<Vec3> poly = {v0, v1, v2};

    // 2. 依次针对 X 和 Z 轴的 4 个边界平面进行裁剪
    // 裁剪掉落在 AABB 柱体之外的部分
    poly = clipPolygon(poly, 0, box.x1, true);  // x >= x1
    poly = clipPolygon(poly, 0, box.x2, false); // x <= x2
    poly = clipPolygon(poly, 2, box.z1, true);  // z >= z1
    poly = clipPolygon(poly, 2, box.z2, false); // z <= z2

    // 3. 在裁剪后的多边形中找 minY
    // 如果由于数值精度导致 poly 为空（理论上相交时不应为空），则退化处理
    float minY = 1e30f;
    for (const auto& p : poly) {
        if (p.y < minY) minY = p.y;
    }

    // 4. 与盒子的 y 范围求交
    // 因为三角形保证与 box 相交，所以结果一定不小于 box.y1
    // 如果计算出的 minY 比 box.y1 还小，说明三角形穿过了 box 的底面
    return std::max(minY, box.y1);
}

bool axisCheck(const BBox3D &box, const BVHLeaf<Triangle> &leaf, const Vec3 &axis){
    auto [bmin, bmax] = box.project(axis);
    float fmin = 1e18, fmax = -1e18;
    for(const Plane &p:leaf.edge){
        float curr = p.point.dot(axis);
        fmin = std::min(fmin, curr);
        fmax = std::max(fmax, curr);
    }
    return (fmax >= bmin && fmin <= bmax);
}
namespace Raytest{

std::optional<BoxtestResult> lowLevelBoxIntersect(const BVHLeaf<Triangle> &leaf, const BBox3D &box){
    if(!axisCheck(box, leaf, {1, 0, 0})) return std::nullopt;
    if(!axisCheck(box, leaf, {0, 1, 0})) return std::nullopt;
    if(!axisCheck(box, leaf, {0, 0, 1})) return std::nullopt;
    if(!axisCheck(box, leaf, leaf.plane.normal)) return std::nullopt;
    Vec3 v0 = leaf.edge[0].point;
    Vec3 v1 = leaf.edge[1].point;
    Vec3 v2 = leaf.edge[2].point;
    for(const Vec3 &v:{v1-v0, v2-v1, v0-v2}){
        if(!axisCheck(box, leaf, v.cross({1, 0, 0}).normalized())) return std::nullopt;
        if(!axisCheck(box, leaf, v.cross({0, 1, 0}).normalized())) return std::nullopt;
        if(!axisCheck(box, leaf, v.cross({0, 0, 1}).normalized())) return std::nullopt;
    }
    BoxtestResult res;
    res.normal = leaf.plane.normal;
    res.ymin = getYMin(v0, v1, v2, box);
    return res;
}

}


void RaytestManager::buildStaticBVH(const QList<Mesh> &lst){
    vector<BVHLeaf<Triangle>> v;
    for(const Mesh &mesh:lst){
        vector<BVHLeaf<Triangle>> dog = toLeafVec(mesh);
        for(const BVHLeaf<Triangle> &t:dog)v.push_back(t);
    }
    staticSceneBVH = BVHTree<Triangle>(v);
}

std::set<BoxtestResult> RaytestManager::sceneBoxIntersect(const BBox3D &box)const{
    return staticSceneBVH.boxIntersect(box);
}

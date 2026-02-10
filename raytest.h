#ifndef RAYTEST_H
#define RAYTEST_H

#include "structures.h"
#include "utils.h"
#include <QDebug>

struct BBox3D{
    float x1, x2, y1, y2, z1, z2;
    BBox3D(){x1=1e9,x2=-1e9,y1=1e9,y2=-1e9,z1=1e9,z2=-1e9;}
    BBox3D(const std::initializer_list<Vec3> &l){
        x1=1e9,x2=-1e9,y1=1e9,y2=-1e9,z1=1e9,z2=-1e9;
        for(const Vec3 &v:l){
            x1=std::min(x1,v.x),x2=std::max(x2,v.x),
            y1=std::min(y1,v.y),y2=std::max(y2,v.y),
            z1=std::min(z1,v.z),z2=std::max(z2,v.z);
        }
    }
    BBox3D merge(const BBox3D &other)const{
        BBox3D ret;
        ret.x1=std::min(x1,other.x1),ret.x2=std::max(x2,other.x2);
        ret.y1=std::min(y1,other.y1),ret.y2=std::max(y2,other.y2);
        ret.z1=std::min(z1,other.z1),ret.z2=std::max(z2,other.z2);
        return ret;
    }
    bool disjointCheck(const BBox3D &other)const{
        return x1>other.x2||x2<other.x1||y1>other.y2||y2<other.y1||z1>other.z2||z2<other.z1;
    }
    float intersectDis(const Ray &ray)const;
};

namespace Raytest{
    template<typename T> struct BVHLeaf{};

    template<> struct BVHLeaf<Triangle>{
        uint triangleID;
        BBox3D box;
        Plane plane;
        Plane edge[3];
        BVHLeaf() = default;
        BVHLeaf(const Mesh &mesh, uint _triangleID){
            triangleID = _triangleID;
            const Triangle &tr = mesh.triangles[_triangleID];
            Vec3 v0 = mesh.vertices[tr.vid[0]].pos;
            Vec3 v1 = mesh.vertices[tr.vid[1]].pos;
            Vec3 v2 = mesh.vertices[tr.vid[2]].pos;
            Vec3 normal = (v1-v0).cross(v2-v0).normalized();

            plane.point  = v0;
            plane.normal = normal;

            edge[0] = {v0, normal.cross(v1-v0)};
            edge[1] = {v1, normal.cross(v2-v1)};
            edge[2] = {v2, normal.cross(v0-v2)};

            box = BBox3D({v0, v1, v2});
        }
    };

    // 内部树遍历时使用这个数据结构
    struct RaytestHit{
        uint hitID = -1u; // -1 if miss
        Vec3 pos;
        float dis;
    };

    extern int intersectCallCnt;
    RaytestHit inline lowLevelIntersect(const BVHLeaf<Triangle> &leaf, const Ray &ray){
        intersectCallCnt ++;
        Vec3 hit = leaf.plane.intersect(ray);
        if(hit.isInvalid()) return {};
        float dis = (hit-ray.point).dot(ray.direction);
        if(dis < 0.0f) return {};
        for(int i=0;i<3;i++)
            if((hit-leaf.edge[i].point).dot(leaf.edge[i].normal) < 0.0f) return {};
        return {0, hit, dis};
    }

    template<typename T> concept BVHContainable = requires(BVHLeaf<T> obj, const Ray &ray){
        {obj.box} -> std::same_as<BBox3D&>;
        {lowLevelIntersect(obj, ray)} -> std::same_as<RaytestHit>;
    };
}


std::vector<Raytest::BVHLeaf<Triangle>> toLeafVec(const Mesh &mesh);


template<typename T> struct RaytestResult{
    bool hit = false;
    Vec3 pos;
    float dis;
    Raytest::BVHLeaf<T> leaf;
};
struct BVHNode{
    uint leafID; // -1 if not leaf
    BBox3D box;
    uint ls, rs;
};
template<typename T> requires Raytest::BVHContainable<T> class BVHTree{
private:
    using Leaf = Raytest::BVHLeaf<T>;
    using Hit  = Raytest::RaytestHit;

    std::vector<Leaf> leaves;
    std::vector<BVHNode> nodes;

    Hit recursiveIntersect(const BVHNode &rt, const Ray &ray)const{
        Raytest::intersectCallCnt ++;
        if(rt.leafID != -1){
            Hit ret = Raytest::lowLevelIntersect(leaves[rt.leafID], ray);
            if(ret.hitID != -1)
                ret.hitID = rt.leafID;
            return ret;
        }
        float disL = -1.0f, disR = -1.0f;
        if(rt.ls != -1) disL = nodes[rt.ls].box.intersectDis(ray);
        if(rt.rs != -1) disR = nodes[rt.rs].box.intersectDis(ray);
        if(disL < 0.0f && disR < 0.0f) return {};
        if(disL >= 0.0f && disR < 0.0f) return recursiveIntersect(nodes[rt.ls], ray);
        if(disL < 0.0f && disR >= 0.0f) return recursiveIntersect(nodes[rt.rs], ray);

        Hit hitL, hitR;
        if(disL > disR){
            hitR = recursiveIntersect(nodes[rt.rs], ray);
            if(hitR.hitID != -1 && hitR.dis <= disL) return hitR;
            hitL = recursiveIntersect(nodes[rt.ls], ray);
        }else{
            hitL = recursiveIntersect(nodes[rt.ls], ray);
            if(hitL.hitID != -1 && hitL.dis <= disR) return hitL;
            hitR = recursiveIntersect(nodes[rt.rs], ray);
        }
        if(hitL.hitID == -1) return hitR;
        if(hitR.hitID == -1) return hitL;
        if(hitL.dis < hitR.dis) return hitL;
        else return hitR;
    }
    float buildTest(std::vector<uint> &idx, int dim){
        std::sort(idx.begin(), idx.end(), [&](int a, int b){
            if(dim == 0) return leaves[a].box.x2 < leaves[b].box.x2;
            if(dim == 1) return leaves[a].box.y2 < leaves[b].box.y2;
            else         return leaves[a].box.z2 < leaves[b].box.z2;
        });
        BBox3D l, r;
        int mid = idx.size() / 2;
        for(auto [id, x]: enumerate(idx))
            if(id < mid) l = l.merge(leaves[x].box);
            else         r = r.merge(leaves[x].box);
        if(dim == 0) return (r.x1-l.x2) / (l.x2+r.x2-l.x1-r.x1);
        if(dim == 1) return (r.y1-l.z2) / (l.y2+r.y2-l.y1-r.y1);
        else         return (r.z1-l.z2) / (l.z2+r.z2-l.z1-r.z1);
    }
    void build(int rt, std::vector<uint> &idx){
        if(idx.size() == 1u){
            nodes[rt].box = leaves[idx[0]].box;
            nodes[rt].ls = nodes[rt].rs = -1;
            nodes[rt].leafID = idx[0];
            return;
        }
        nodes[rt].leafID = -1;
        float xScore = buildTest(idx, 0);
        float yScore = buildTest(idx, 1);
        float zScore = buildTest(idx, 2);
        int bestDim = 0;
        if(std::max({xScore, yScore, zScore}) == xScore) bestDim = 0;
        else if(std::max({xScore, yScore, zScore}) == yScore) bestDim = 1;
        else bestDim = 2;

        std::sort(idx.begin(), idx.end(), [&](int a, int b){
            if(bestDim == 0) return leaves[a].box.x2 < leaves[b].box.x2;
            if(bestDim == 1) return leaves[a].box.y2 < leaves[b].box.y2;
            else             return leaves[a].box.z2 < leaves[b].box.z2;
        });
        int mid = idx.size() / 2;
        std::vector<uint> rsIdx(idx.begin()+mid, idx.end());
        idx.resize(mid);

        if(idx.size()){
            nodes[rt].ls = nodes.size();
            nodes.push_back({});
            build(nodes[rt].ls, idx);
            nodes[rt].box = nodes[rt].box.merge(nodes[nodes[rt].ls].box);
        }else nodes[rt].ls = -1;
        if(rsIdx.size()){
            nodes[rt].rs = nodes.size();
            nodes.push_back({});
            build(nodes[rt].rs, rsIdx);
            nodes[rt].box = nodes[rt].box.merge(nodes[nodes[rt].rs].box);
        }else nodes[rt].rs = -1;
    }

public:
    BVHTree() = default;
    BVHTree(const BVHTree &other) = default;
    BVHTree(BVHTree &&other) noexcept{
        if(this == &other) return;
        swap(leaves, other.leaves);
        swap(nodes, other.nodes);
    }
    BVHTree(const std::vector<Leaf> &_data){
        leaves = _data;
        nodes.clear();
        std::vector<uint> idx;
        for(uint i=0;i<leaves.size();i++) idx.push_back(i);
        nodes.push_back({});
        build(0, idx);
    }

    // 外部接口，返回真正的元信息
    RaytestResult<T> intersect(const Ray &ray)const{
        assert(nodes.size());
        Hit hit = recursiveIntersect(nodes[0], {ray.point, ray.direction.normalized()});
        if(hit.hitID != -1){
            return {true, hit.pos, hit.dis, leaves[hit.hitID]};
        }else return {false};
    }
    friend class RaytestManager;
};

class RaytestManager{
public:
    std::vector<BVHTree<Triangle>> meshTrees;
    void appendMesh(const Mesh &mesh);
    RaytestResult<Triangle> meshIntersect(uint meshID, const Ray &ray)const;
};

extern RaytestManager raytestManager;

#endif // RAYTEST_H

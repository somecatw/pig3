#ifndef STRUCTURES
#define STRUCTURES

#include "mathbase.h"
#include <vector>
#include <QImage>
#include "transform.h"

const int tileSize = 64;

struct ShaderConfig{
    static constexpr ushort
        WireframeOnly  = 0x0001,
        UseAlphaBitmap = 0x0002,
        MonoChrome     = 0x0004,
        UseLightModel  = 0x0008;
};

struct Material{
    QImage img;
};

struct Vertex{
    Vec3 pos, uv;
};

// 3D 的三角形
struct Triangle{
    uint vid[3];
    ushort materialID;
    ushort shaderConfig = 0;
};

// 这里 triangle 的 vid 是在此 mesh 内部的 vertexs 中的下标
struct Mesh{
    std::vector<Triangle> triangles;
    std::vector<Vertex> vertices;
    ushort materialID;
    ushort shaderConfig = 0;

    void applyTransform(const Transform &t){
        for(Vertex &v:vertices){
            v.pos = v.pos*t.rotation;
            v.pos += t.translation;
        }
    }
};

struct Iterator2D{
    float val;
    float dv_dx;
    float dv_dy;
    void batchIterate(int x, int y){
        val = val + dv_dx*x + dv_dy*y;
    }
    void xIterate(){
        val += dv_dx;
    }
    void yIterate(){
        val += dv_dy;
    }
};
struct EdgeIterator{

    static constexpr int OUTER = 0, INNER = 1;

    Iterator2D e[3];

    void batchIterate(int x, int y){
        e[0].batchIterate(x, y);
        e[1].batchIterate(x, y);
        e[2].batchIterate(x, y);
    }
    void xIterate(){
        e[0].xIterate();
        e[1].xIterate();
        e[2].xIterate();
    }
    void yIterate(){
        e[0].yIterate();
        e[1].yIterate();
        e[2].yIterate();
    }
    int check()const{
        if(e[0].val < 0 || e[1].val < 0 || e[2].val < 0) return OUTER;
        return INNER;
    }
};

// 2D 的片段，包含光栅化阶段需要的信息
struct Fragment{
    uint triangleID;
    Vec3 v2d[3];
    EdgeIterator edgeIterator;
    Iterator2D zInv, u_z, v_z;
};

struct ShadingUnit{
    uint triangleID;
    float zInv;
    float u_z, v_z;
};

struct Tile{
    ShadingUnit buf[tileSize][tileSize];
};

struct CameraInfo{
    Vec3 pos;
    uint width, height;
    Vec3 screenSize;
    float focalLength;
    LocalFrame frame;
};

inline void loadEdgeEquation(Iterator2D &edgeIter, const Vec3 &point, const Vec3 &edge, bool direction){
    edgeIter.dv_dx = -edge.y;
    edgeIter.dv_dy = edge.x;
    if(direction){
        edgeIter.dv_dx *= -1;
        edgeIter.dv_dy *= -1;
    }
    edgeIter.val = -point.x*edgeIter.dv_dx - point.y*edgeIter.dv_dy;
}

inline void calcLinearCoefficient(Iterator2D &iter, const Vec3 &v0, const Vec3 &e1, const Vec3 &e2){
    Vec3 res = SolveLinearEq2(e1, e2);
    iter.dv_dx = res.x;
    iter.dv_dy = res.y;
    iter.val = v0.z - iter.dv_dx*v0.x - iter.dv_dy*v0.y;
}


#endif

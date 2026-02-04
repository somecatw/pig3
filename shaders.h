#ifndef SHADERS_H
#define SHADERS_H

#include "structures.h"
#include "assetmanager.h"

namespace ShaderInternal{
    extern std::vector<Vertex> vertices;
    extern std::vector<Triangle> triangles;
    extern CameraInfo camera;
    extern uint *buffer;
    extern uint pixelW, pixelH;
    extern std::vector<Vertex> projectedVertices;
    extern std::vector<Fragment> fragments;
}

struct ShadingBuffer{
    constexpr static int W=1500, H=1500;

    uint triangleID[H][W];
    uint materialID[H][W];
    float zInv[H][W], u_z[H][W], v_z[H][W];

};
extern ShadingBuffer shadingBuffer;

class BaseShader{
public:
    // 在光栅化阶段，判断当前像素是否需要显示
    bool static alphaTest(uint triangleID, const EdgeIterator &edgeIt, const Iterator2D &zInv, const Iterator2D &u_z, const Iterator2D &v_z){
        return edgeIt.check() == EdgeIterator::INNER;
    }
    // 在着色阶段，算出当前像素的颜色。输入的 x 和 y 是屏幕像素坐标；此函数只在 alphaTest 返回 true 的像素上执行
    uint static colorSample(float u, float v, uint triangleID, Vec3 view){
        uint materialID = ShaderInternal::triangles[triangleID].materialID;

        // uint materialID = shadingBuffer.materialID[y][x];

        const Material &material = assetManager.getMaterials()[materialID];

        int w = material.img.width();
        int h = material.img.height();

        // float u = shadingBuffer.u_z[y][x] / shadingBuffer.zInv[y][x];
        // float v = shadingBuffer.v_z[y][x] / shadingBuffer.zInv[y][x];

        int rx = int(u*w)%w;
        int ry = int(v*h)%h;

        if(rx<0) rx += w;
        if(ry<0) ry += h;

        return material.img.pixel(rx, ry);
    }
};

class WireframeShader: public BaseShader{
public:
    bool static alphaTest(uint triangleID, const EdgeIterator &edgeIt, const Iterator2D &zInv, const Iterator2D &u_z, const Iterator2D &v_z){
        int ttfa = 0;
        for(int i=0;i<3;i++){
            float threshold = std::max(std::abs(edgeIt.e[i].dv_dx), std::abs(edgeIt.e[i].dv_dy))/2;
            if(-threshold <= edgeIt.e[i].val && edgeIt.e[i].val <= threshold) ttfa += 1;
            else if(edgeIt.e[i].val >= 0) ttfa += 2;
            else return false;
        }
        if(ttfa == 4 || ttfa == 5) return true;
        else return false;
    }
    uint static colorSample(float u, float v, uint triangleID, const Vec3 &view){
        unsigned int h = triangleID;

        // 简单的位混合哈希，让相邻 ID 的颜色产生剧烈跳变
        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        h *= 0xc2b2ae35;
        h ^= h >> 16;

        // 将哈希值映射到 RGB 通道
        uint r = ((h & 0xFF0000) >> 16);
        uint g = ((h & 0x00FF00) >> 8);
        uint b = (h & 0x0000FF);

        return 0xff000000 | (r<<16) | (g<<8) | b;
    }
};

class MonoChromeShader : public BaseShader{
public:
    bool static alphaTest(uint triangleID, const EdgeIterator &edgeIt, const Iterator2D &zInv, const Iterator2D &u_z, const Iterator2D &v_z){
        return edgeIt.check() == EdgeIterator::INNER;
    }
    uint static colorSample(float u, float v, uint triangleID, const Vec3 &view) {
        uint materialID = ShaderInternal::triangles[triangleID].materialID;

        const Material &material = assetManager.getMaterials()[materialID];

        int w = material.img.width();
        int h = material.img.height();

        int rx = int(u*w);
        int ry = int(v*h);

        uint color = material.img.pixel(rx, ry);
        uint r = (color & 0x00ff0000) >> 16;
        uint g = (color & 0x0000ff00) >> 8;
        uint b = (color & 0x000000ff);
        uint gray = static_cast<uint>(r * 0.299 + g * 0.587 + b * 0.114);
        return (color & 0xff000000) | (gray << 16) | (gray << 8) | gray;
    }
};



#endif // SHADERS_H

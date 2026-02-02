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
    constexpr static int W=1000, H=1000;

    uint triangleID[H][W];
    uint materialID[H][W];
    float zInv[H][W], u_z[H][W], v_z[H][W];

}shadingBuffer;

std::tuple<float, float, float, float> fragmentBBox(const Fragment &fragment){
    return {
        std::min({fragment.v2d[0].x, fragment.v2d[1].x, fragment.v2d[2].x}),
        std::min({fragment.v2d[0].y, fragment.v2d[1].y, fragment.v2d[2].y}),
        std::max({fragment.v2d[0].x, fragment.v2d[1].x, fragment.v2d[2].x}),
        std::max({fragment.v2d[0].y, fragment.v2d[1].y, fragment.v2d[2].y})
    };
}
template<typename T> concept IsShader = requires(const EdgeIterator &edgeIt, const Iterator2D &zInv, const Iterator2D &u_z, const Iterator2D &v_z){
    {T::alphaTest(uint(), edgeIt, zInv, u_z, v_z)} -> std::same_as<bool>;
    {T::colorSample(int(), int(), Vec3())} -> std::same_as<uint>;
};

class BaseShader{
public:
    // 在光栅化阶段，判断当前像素是否需要显示
    bool static alphaTest(uint triangleID, const EdgeIterator &edgeIt, const Iterator2D &zInv, const Iterator2D &u_z, const Iterator2D &v_z){
        return edgeIt.check() == EdgeIterator::INNER;
    }
    // 在着色阶段，算出当前像素的颜色。输入的 x 和 y 是屏幕像素坐标；此函数只在 alphaTest 返回 true 的像素上执行
    uint static colorSample(int x, int y, Vec3 view){
        // uint materialID = ShaderInternal::triangles[shadingBuffer.triangleID[x][y]].materialID;

        uint materialID = shadingBuffer.materialID[y][x];

        const Material &material = assetManager.getMaterials()[materialID];

        int w = material.img.width();
        int h = material.img.height();

        float u = shadingBuffer.u_z[y][x] / shadingBuffer.zInv[y][x];
        float v = shadingBuffer.v_z[y][x] / shadingBuffer.zInv[y][x];

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
    uint static colorSample(int x, int y, const Vec3 &view){
        return 0xff00ff00;
    }
};

class MonoChromeShader : public BaseShader{
public:
    bool static alphaTest(uint triangleID, const EdgeIterator &edgeIt, const Iterator2D &zInv, const Iterator2D &u_z, const Iterator2D &v_z){
        return edgeIt.check() == EdgeIterator::INNER;
    }
    uint static colorSample(int x, int y, const Vec3 &view) {
        uint materialID = ShaderInternal::triangles[shadingBuffer.triangleID[x][y]].materialID;

        const Material &material = assetManager.getMaterials()[materialID];

        int w = material.img.width();
        int h = material.img.height();

        float u = shadingBuffer.u_z[x][y] / shadingBuffer.zInv[x][y];
        float v = shadingBuffer.v_z[x][y] / shadingBuffer.zInv[x][y];
        assert(0<=u && u<=1 && 0<=v && v<=1);
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

// 所有涉及部分透明面片的逻辑在这里实现，包括线框渲染，单片草之类的
template<typename FragmentShader>
requires IsShader<FragmentShader> void segmentRasterization(const Fragment &frag, int pixelW, int pixelH){
    auto [xmin, ymin, xmax, ymax] = fragmentBBox(frag);

    EdgeIterator edgeIt = frag.edgeIterator;
    Iterator2D zInv = frag.zInv;
    Iterator2D u_z = frag.u_z;
    Iterator2D v_z = frag.v_z;

    int xlt = std::max(0, (int)xmin);
    int ylt = std::max(0, (int)ymin);
    int xrb = std::min((int)pixelW-1, (int)xmax);
    int yrb = std::min((int)pixelH-1, (int)ymax);

    edgeIt.batchIterate(xlt, ylt);
    zInv.batchIterate(xlt, ylt);
    u_z.batchIterate(xlt, ylt);
    v_z.batchIterate(xlt, ylt);

    for(int y = ylt; y <= yrb; y++){
        EdgeIterator tempEdgeIt = edgeIt;
        Iterator2D tempZInv = zInv;
        Iterator2D tempUZ = u_z;
        Iterator2D tempVZ = v_z;
        bool passFlag = false;

        for(int x = xlt; x <= xrb; x++){
            bool result = FragmentShader::alphaTest(frag.triangleID, tempEdgeIt, tempZInv, tempUZ, tempVZ);

            if(result){
                passFlag = true;
                if(shadingBuffer.zInv[y][x] < tempZInv.val){
                    shadingBuffer.triangleID[y][x] = frag.triangleID;
                    shadingBuffer.zInv[y][x] = tempZInv.val;
                    shadingBuffer.u_z[y][x] = tempUZ.val;
                    shadingBuffer.v_z[y][x] = tempVZ.val;
                    shadingBuffer.materialID[y][x] = ShaderInternal::triangles[frag.triangleID].materialID;
                }
            }else if(passFlag)break;

            tempEdgeIt.xIterate();
            tempZInv.xIterate();
            tempUZ.xIterate();
            tempVZ.xIterate();
        }
        edgeIt.yIterate();
        zInv.yIterate();
        u_z.yIterate();
        v_z.yIterate();
    }
}

template<typename FragmentShader>
requires IsShader<FragmentShader> uint colorDetermination(int x, int y, const Vec3 &view){
    return FragmentShader::colorSample(x, y, view);
}

#endif // SHADERS_H

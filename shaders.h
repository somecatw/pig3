#ifndef SHADERS_H
#define SHADERS_H

#include "structures.h"
#include "assetmanager.h"
#include <QDebug>

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

struct IntColorRef{
    int r, g, b;
    IntColorRef(uint color){
        r = (color&(0xff0000))>>10;
        g = (color&(0x00ff00))>>2;
        b = (color&(0x0000ff))<<6;
    }
    // k 从 0 到 32768
    void blend(const IntColorRef &other, int k){
        r = r + ((k*(other.r-r)) >> 14);
        g = g + ((k*(other.g-g)) >> 14);
        b = b + ((k*(other.b-b)) >> 14);
    }
    uint toUintColor(){
        return 0xff000000 | (0xff0000 & (r<<10)) | (0x00ff00 & (g<<2)) | (0x0000ff & (b>>6));
    }
};

// 这是人写得出来的东西？
uint inline uintBlend(uint c1, uint c2, int alpha){
    uint32_t rb1 = c1 & 0xFF00FF;
    uint32_t g1  = c1 & 0x00FF00;

    uint32_t rb2 = c2 & 0xFF00FF;
    uint32_t g2  = c2 & 0x00FF00;

    // (c1 + (c2 - c1) * alpha) >> 8
    uint32_t rb = rb1 + (((rb2 - rb1) * alpha) >> 8);
    uint32_t g  = g1 + (((g2 - g1) * alpha) >> 8);

    return (rb & 0xFF00FF) | (g & 0x00FF00);
}
uint inline BilinearSample(float u, float v, int l, const Material &mtl){
    // const QImage *texture = &mtl.mipmaps.at(l);
    const TextureMap *texture = &mtl.mipmap2.at(l);

    int w = texture->w;
    int h = texture->h;

    float ru = u*w;
    float rv = v*h;

    uint x0 = int(ru)&(w-1);
    uint y0 = int(rv)&(h-1);
    // if(x0<0) x0 += w;
    // if(y0<0) y0 += h;

    uint c00 = texture->pixel(x0, y0) & 0xffffff;
    return c00;
}
class BaseShader{
public:
    static inline int lg2[256];
    static inline float lg2f[65536];
    // 在光栅化阶段，判断当前像素是否需要显示
    bool static alphaTest(uint triangleID, const EdgeIterator &edgeIt, const Iterator2D &zInv, const Iterator2D &u_z, const Iterator2D &v_z){
        return edgeIt.check() == EdgeIterator::INNER;
    }
    // 在着色阶段，算出当前像素的颜色。输入的 x 和 y 是屏幕像素坐标；此函数只在 alphaTest 返回 true 的像素上执行
    uint static colorSample(float u, float v, uint triangleID, const Vec3 &view, float d=0.0f){
        uint materialID = ShaderInternal::triangles[triangleID].materialID;

        // uint materialID = shadingBuffer.materialID[y][x];

        const Material &material = assetManager.getMaterials()[materialID];

        int w = material.mipmap2[0].w;
        int h = w; //material.mipmap2[0].h;

        float pxSpan = std::clamp(d * w, 0.999f, 255.0f);
        int level = lg2[int(pxSpan)];

        const TextureMap &texture = material.mipmap2[std::max(0, level-1)];

        w = texture.w;
        h = w;

        float ru = u*w;
        float rv = v*h;

        uint x0 = int(ru)&(w-1);
        uint y0 = int(rv)&(h-1);

        // uint *ptr0 = (uint*)texture->constScanLine(y0);
        uint c00 = texture.pixel(x0, y0) & 0xffffff;

        if(level > 1){
            float lgSpan = lg2f[int(pxSpan * 16.0f)];
            if(level - lgSpan > 0.2f && level > 0 ){
                uint c100 = BilinearSample(u, v, level-2, material);
                // int k = 256 * (level+1-std::min(8.0f, lgSpan));
                int k = 256 * (level - std::min(8.0f, lgSpan));
                c00 = uintBlend(c00, c100, k);
            }
        }

        // if(level > 4) c00 = 0xff000000;
        // if(level <= 4){
        //     int gray = 0xff & int((level-lgSpan) * 64);
        //     return 0xff000000 | (gray<<16) | (gray<<8) | gray;

        // }

        // c01.blend(c00, kx);
        // c11.blend(c10, kx);
        // c11.blend(c01, ky);


        // return 0xff000000 | (int(kk)<<16) | (int(kk)<<8) | 0;
        return 0xff000000 | c00;

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
    uint static colorSample(float u, float v, uint triangleID, const Vec3 &view, float d=0.0f){
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
    uint static colorSample(float u, float v, uint triangleID, const Vec3 &view, float d=0.0f) {
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

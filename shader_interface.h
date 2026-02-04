#ifndef SHADER_INTERFACE_H
#define SHADER_INTERFACE_H

#include "shaders.h"
#include "parallel_render.h"

template<typename T> concept IsShader = requires(const EdgeIterator &edgeIt, const Iterator2D &zInv, const Iterator2D &u_z, const Iterator2D &v_z){
    {T::alphaTest(uint(), edgeIt, zInv, u_z, v_z)} -> std::same_as<bool>;
} && requires(float u, float v, uint triangleID, Vec3 viewDirection){
    {T::colorSample(int(), int(), uint(), Vec3())} -> std::same_as<uint>;
};

// 所有涉及部分透明面片的逻辑在这里实现，包括线框渲染，单片草之类的
template<typename FragmentShader>
    requires IsShader<FragmentShader> void segmentRasterization(const Fragment &frag, int pixelW, int pixelH){

    EdgeIterator edgeIt = frag.edgeIterator;
    Iterator2D zInv = frag.zInv;
    Iterator2D u_z = frag.u_z;
    Iterator2D v_z = frag.v_z;

    int xlt = frag.xlt;
    int xrb = frag.xrb;
    int ylt = frag.ylt;
    int yrb = frag.yrb;

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

struct TileLevelResult{
    const static int
        OUTER   = 0,
        UNKNOWN = 1,
        INNER   = 2;
};

template<typename FragmentShader>
    requires IsShader<FragmentShader> void tileRasterization(const Fragment &frag, Tile &tile, int tileLevelResult){

    if(tileLevelResult == TileLevelResult::OUTER) return;

    EdgeIterator edgeIt = frag.edgeIterator;
    Iterator2D zInv = frag.zInv;
    Iterator2D u_z = frag.u_z;
    Iterator2D v_z = frag.v_z;

    int xlt = std::max(frag.xlt, tile.tileX * tileSize);
    int xrb = std::min(frag.xrb, tile.tileX * tileSize + tileSize-1);
    int ylt = std::max(frag.ylt, tile.tileY * tileSize);
    int yrb = std::min(frag.yrb, tile.tileY * tileSize + tileSize-1);

    frameStat.pixelIterated += (xrb-xlt+1)*(yrb-ylt+1);

    edgeIt.batchIterate(xlt, ylt);
    zInv.batchIterate(xlt, ylt);
    u_z.batchIterate(xlt, ylt);
    v_z.batchIterate(xlt, ylt);

    xlt -= tile.tileX * tileSize;
    xrb -= tile.tileX * tileSize;
    ylt -= tile.tileY * tileSize;
    yrb -= tile.tileY * tileSize;

    for(int y = ylt; y <= yrb; y++){

        EdgeIterator tempEdgeIt = edgeIt;
        Iterator2D tempZInv = zInv;
        Iterator2D tempUZ = u_z;
        Iterator2D tempVZ = v_z;
        bool passFlag = false;

        for(int x = xlt; x <= xrb; x++){

            if(tile.zInv[y][x] < tempZInv.val){
                bool result = FragmentShader::alphaTest(frag.triangleID, tempEdgeIt, tempZInv, tempUZ, tempVZ);
                if(result){
                    passFlag = true;
                    // tile.zInvMin = min(tile.zInvMin, tempZInv.val);
                    tile.triangleID[y][x] = frag.triangleID;
                    tile.zInv[y][x] = tempZInv.val;
                    tile.u_z[y][x] = tempUZ.val;
                    tile.v_z[y][x] = tempVZ.val;
                    tile.materialID[y][x] = ShaderInternal::triangles[frag.triangleID].materialID;
                }else if(passFlag)break;
            }

            if(tileLevelResult == TileLevelResult::UNKNOWN)
                tempEdgeIt.xIterate();
            tempZInv.xIterate();
            tempUZ.xIterate();
            tempVZ.xIterate();
        }
        if(tileLevelResult == TileLevelResult::UNKNOWN)
            edgeIt.yIterate();
        zInv.yIterate();
        u_z.yIterate();
        v_z.yIterate();

    }
}

template<typename FragmentShader>
    requires IsShader<FragmentShader> void tileRasterization2(const Fragment &frag, Tile &tile, int tileLevelResult){

    if(tileLevelResult == TileLevelResult::OUTER) return;

    EdgeIterator edgeIt = frag.edgeIterator;
    Iterator2D zInv = frag.zInv;
    Iterator2D u_z = frag.u_z;
    Iterator2D v_z = frag.v_z;

    int xlt = std::max(frag.xlt, tile.tileX * tileSize);
    int xrb = std::min(frag.xrb, tile.tileX * tileSize + tileSize-1);
    int ylt = std::max(frag.ylt, tile.tileY * tileSize);
    int yrb = std::min(frag.yrb, tile.tileY * tileSize + tileSize-1);

    frameStat.pixelIterated += (xrb-xlt+1)*(yrb-ylt+1);

    edgeIt.batchIterate(xlt, ylt);
    zInv.batchIterate(xlt, ylt);
    u_z.batchIterate(xlt, ylt);
    v_z.batchIterate(xlt, ylt);

    xlt -= tile.tileX * tileSize;
    xrb -= tile.tileX * tileSize;
    ylt -= tile.tileY * tileSize;
    yrb -= tile.tileY * tileSize;

    for(int y = ylt; y <= yrb; y++){
        bool passFlag = false;
        for(int x = xlt; x <= xrb; x++){

            if(tile.zInv[y][x] < zInv.val){
                bool result = FragmentShader::alphaTest(frag.triangleID, edgeIt, zInv, u_z, v_z);
                if(result){
                    passFlag = true;
                    // tile.zInvMin = min(tile.zInvMin, tempZInv.val);
                    tile.triangleID[y][x] = frag.triangleID;
                    tile.zInv[y][x] = zInv.val;
                    tile.u_z[y][x] = u_z.val;
                    tile.v_z[y][x] = v_z.val;
                    tile.materialID[y][x] = ShaderInternal::triangles[frag.triangleID].materialID;
                }// else if(passFlag)break;
            }

            if(x == xrb)break;

            if(tileLevelResult == TileLevelResult::UNKNOWN)
                edgeIt.xIterate();
            zInv.xIterate();
            u_z.xIterate();
            v_z.xIterate();
        }
        if(tileLevelResult == TileLevelResult::UNKNOWN)
            edgeIt.yIterate();
        zInv.yIterate();
        u_z.yIterate();
        v_z.yIterate();

        y++;
        if(y > yrb) break;
        passFlag = false;

        for(int x = xrb; x >= xlt; x--){

            if(tile.zInv[y][x] < zInv.val){
                bool result = FragmentShader::alphaTest(frag.triangleID, edgeIt, zInv, u_z, v_z);
                if(result){
                    passFlag = true;
                    // tile.zInvMin = min(tile.zInvMin, tempZInv.val);
                    tile.triangleID[y][x] = frag.triangleID;
                    tile.zInv[y][x] = zInv.val;
                    tile.u_z[y][x] = u_z.val;
                    tile.v_z[y][x] = v_z.val;
                    tile.materialID[y][x] = ShaderInternal::triangles[frag.triangleID].materialID;
                }// else if(passFlag)break;
            }

            if(x == xlt) break;

            if(tileLevelResult == TileLevelResult::UNKNOWN)
                edgeIt.xInversedIterate();
            zInv.xInversedIterate();
            u_z.xInversedIterate();
            v_z.xInversedIterate();
        }
        if(tileLevelResult == TileLevelResult::UNKNOWN)
            edgeIt.yIterate();
        zInv.yIterate();
        u_z.yIterate();
        v_z.yIterate();

    }
}

template<typename FragmentShader>
    requires IsShader<FragmentShader> uint colorDetermination(float u, float v, uint triangleID, const Vec3 &view){
    return FragmentShader::colorSample(u, v, triangleID, view);
}

#endif // SHADER_INTERFACE_H

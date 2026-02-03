#include "render.h"
#include "utils.h"
#include <QDebug>
#include "shader_interface.h"
using namespace std;

FrameStat frameStat;

namespace ShaderInternal{
    vector<Vertex> vertices;
    vector<Triangle> triangles;
    CameraInfo camera;
    uint *buffer;

    uint pixelW, pixelH;

    // (x, y, 4096.0f/z)
    vector<Vertex> projectedVertices;

    vector<Fragment> fragments;
}

using namespace ShaderInternal;

ShadingBuffer shadingBuffer;


Vertex vertexIntersect(const Vertex &a, const Vertex &b, const Plane &p){
    Vec3 intersection = p.intersect(link(a.pos, b.pos));
    float c = (intersection - a.pos).len() / (b.pos - a.pos).len();
    Vec3 interpolatedUV = a.uv * (1.0f-c) + b.uv * c;
    return {intersection, interpolatedUV};
}
uint simpleLightBlend(uint color, float intensity){
    // assert(0 <= intensity && intensity <= 1);
    uint r = (color & (0x00ff0000)) >> 16;
    uint g = (color & (0x0000ff00)) >> 8;
    uint b = color & 0x000000ff;
    return 0xff000000 | (uint(r*intensity) << 16) | (uint(g*intensity) << 8) | (uint(b*intensity));
}
class Renderer{
private:


public:
    void vertexProject(){
        projectedVertices.reserve(vertices.size());
        Vec3 screenCenter = camera.pos + camera.focalLength * camera.frame.axisZ;
        // qDebug()<<screenCenter.to_string();

        for(const Vertex& v:vertices){
            Vec3 ray = v.pos - camera.pos;
            Vec3 projection = camera.pos + ray / ray.dot(camera.frame.axisZ) * camera.focalLength;

            float zInv = 1024.0f / (ray.dot(camera.frame.axisZ));
            // if(zInv < 0) throw runtime_error("point behind screen!");

            float x2d = (projection - screenCenter).dot(camera.frame.axisX) / camera.screenSize.x * pixelW;
            float y2d = (projection - screenCenter).dot(camera.frame.axisY) / camera.screenSize.y * pixelH;

            x2d += pixelW/2;
            y2d += pixelH/2;

            Vec3 pos = {x2d, y2d, zInv};
            Vec3 uv = v.uv * zInv;

            projectedVertices.push_back({pos, uv});
        }
    }
    void frontClip(){
        // 可能在 triangles 末尾填充切分出的新三角形
        vector<pair<uint, Vertex>> front, back;

        Vec3 screenCenter = camera.pos + camera.focalLength * camera.frame.axisZ;
        Plane cameraPlane = {screenCenter, camera.frame.axisZ};
        vector<Triangle> tmpTriangles;

        for(auto [id, triangle]: enumerate(triangles)){

            front.clear();
            back.clear();

            for(uint i=0;i<3;i++){
                uint vid = triangle.vid[i];

                float dist = (vertices[vid].pos - screenCenter).dot(camera.frame.axisZ);
                if(dist >= 0)front.push_back({vid, vertices[vid]});
                else back.push_back({vid, vertices[vid]});
            }

            if(front.size() == 3u) continue;
            if(back.size() == 3u){
                triangle.vid[0] = -1;
                continue;
            }
            if(front.size() == 2u && back.size() == 1u){
                Vertex int0 = vertexIntersect(front[0].second, back[0].second, cameraPlane);
                Vertex int1 = vertexIntersect(front[1].second, back[0].second, cameraPlane);

                uint id0 = vertices.size();
                vertices.push_back(int0);
                uint id1 = vertices.size();
                vertices.push_back(int1);

                triangle.vid[0] = front[0].first;
                triangle.vid[1] = id0;
                triangle.vid[2] = front[1].first;

                Triangle tmp;
                tmp.vid[0] = id0;
                tmp.vid[1] = id1;
                tmp.vid[2] = front[1].first;
                tmpTriangles.push_back(tmp);
                continue;
            }
            if(front.size() == 1u && back.size() == 2u){
                Vertex int0 = vertexIntersect(front[0].second, back[0].second, cameraPlane);
                Vertex int1 = vertexIntersect(front[0].second, back[1].second, cameraPlane);

                uint id0 = vertices.size();
                vertices.push_back(int0);
                uint id1 = vertices.size();
                vertices.push_back(int1);

                triangle.vid[0] = front[0].first;
                triangle.vid[1] = id0;
                triangle.vid[2] = id1;

                continue;
            }
            throw runtime_error("how did we get here?");
        }

        triangles.erase(
            remove_if(triangles.begin(), triangles.end(), [](const Triangle &t){return t.vid[0]==-1;}),
            triangles.end()
            );
        for(const Triangle &t: tmpTriangles) triangles.push_back(t);
    }
    void getFragments(){
        fragments.reserve(triangles.size());
        for(auto [id, triangle]: enumerate(triangles)){

            triangle.hardNormal = (vertices[triangle.vid[2]].pos - vertices[triangle.vid[0]].pos).cross(vertices[triangle.vid[1]].pos-vertices[triangle.vid[0]].pos);
            triangle.hardNormal.normalize();

            if(!(triangle.shaderConfig & ShaderConfig::DisableBackCulling)){

                if(triangle.hardNormal.dot(vertices[triangle.vid[2]].pos - camera.pos) < 0) continue;
            }

            fragments.push_back(Fragment());
            Fragment &current = fragments.back();

            current.triangleID = id;

            Vertex v0 = projectedVertices[triangle.vid[0]];
            Vertex v1 = projectedVertices[triangle.vid[1]];
            Vertex v2 = projectedVertices[triangle.vid[2]];

            if(v0.pos.z < 0 || v1.pos.z < 0 || v2.pos.z < 0)
                throw runtime_error("point behind screen!");

            Vec3 e0 = v1.pos - v0.pos;
            Vec3 e1 = v2.pos - v1.pos;
            Vec3 e2 = v0.pos - v2.pos;

            int direction = 0;
            // 算一下直线方程的绕向
            if(e0.cross(e1).z < 0)
                direction = 1;

            loadEdgeEquation(current.edgeIterator.e[0], v0.pos, e0, direction);
            loadEdgeEquation(current.edgeIterator.e[1], v1.pos, e1, direction);
            loadEdgeEquation(current.edgeIterator.e[2], v2.pos, e2, direction);

            calcLinearCoefficient(current.zInv, v0.pos, e0, e2);
            // qDebug()<<v0.pos.to_string()<<v1.pos.to_string()<<v2.pos.to_string();


            current.xlt = max(0,             int(min({v0.pos.x, v1.pos.x, v2.pos.x})));
            current.xrb = min((int)pixelW-1, int(max({v0.pos.x, v1.pos.x, v2.pos.x})));
            current.ylt = max(0,             int(min({v0.pos.y, v1.pos.y, v2.pos.y})));
            current.yrb = min((int)pixelH-1, int(max({v0.pos.y, v1.pos.y, v2.pos.y})));

            // 算 uv
            e0.z = v1.uv.x - v0.uv.x;
            e2.z = v0.uv.x - v2.uv.x;
            calcLinearCoefficient(current.u_z, {v0.pos.x, v0.pos.y, v0.uv.x}, e0, e2);

            e0.z = v1.uv.y - v0.uv.y;
            e2.z = v0.uv.y - v2.uv.y;
            calcLinearCoefficient(current.v_z, {v0.pos.x, v0.pos.y, v0.uv.y}, e0, e2);
        }
    }
    void parallelRasterization(){

        taskDispatcher.init();
        for(const Fragment &frag:fragments){
            int tileXlt = frag.xlt / tileSize;
            int tileYlt = frag.ylt / tileSize;
            int tileXrb = frag.xrb / tileSize;
            int tileYrb = frag.yrb / tileSize;

            EdgeIterator edgeIt = frag.edgeIterator, tmp;

            for(int y = tileYlt; y <= tileYrb; y++){
                for(int x = tileXlt; x <= tileXrb; x++){
                    taskDispatcher.submitFragment(frag, x, y);
                    frameStat.tileFragmentSum ++;
                }
            }
        }
        taskDispatcher.finish();
    }

    void bfRasterization(){
        for(const Fragment& frag:fragments){
            ushort shaderConfig = triangles[frag.triangleID].shaderConfig;
            if(shaderConfig & ShaderConfig::WireframeOnly)
                segmentRasterization<WireframeShader>(frag, pixelW, pixelH);
            else if (shaderConfig & ShaderConfig::MonoChrome)
                segmentRasterization<MonoChromeShader>(frag, pixelW, pixelH);
            else
                segmentRasterization<BaseShader>(frag, pixelW, pixelH);
        }
    }
    void determineColor(){
        Vec3 view = camera.pos + camera.focalLength*camera.frame.axisZ
                    - camera.screenSize.x/2 * camera.frame.axisX
                    - camera.screenSize.y/2 * camera.frame.axisY;

        Vec3 dx = 1.0f / pixelW * camera.screenSize.x * camera.frame.axisX;
        Vec3 dy = 1.0f / pixelH * camera.screenSize.y * camera.frame.axisY;

        Vec3 sunLight = {1, -1, -1};
        sunLight.normalize();

        for(uint y = 0; y < pixelH; y++)
        {
            Vec3 tmpView = view;
            for(uint x = 0; x < pixelW; x++){
                uint colorRef = 0xff000000;

                if(shadingBuffer.triangleID[y][x] < 0x80000000){
                    ushort shaderConfig = triangles[shadingBuffer.triangleID[y][x]].shaderConfig;

                    float u = shadingBuffer.u_z[y][x] / shadingBuffer.zInv[y][x];
                    float v = shadingBuffer.v_z[y][x] / shadingBuffer.zInv[y][x];

                    // 静态转发逻辑
                    if(shaderConfig & ShaderConfig::WireframeOnly)
                        colorRef = colorDetermination<WireframeShader>(u, v, shadingBuffer.triangleID[y][x], tmpView);
                    else if (shaderConfig & ShaderConfig::MonoChrome)
                        colorRef = colorDetermination<MonoChromeShader>(u, v, shadingBuffer.triangleID[y][x], tmpView);
                    else
                        colorRef = colorDetermination<BaseShader>(u, v, shadingBuffer.triangleID[y][x], tmpView);

                    if(!(shaderConfig & ShaderConfig::DisableLightModel)){
                        // 对于简单光照，这一步可以提前；
                        // 如果是平滑光照，则需要逐像素的法线插值，但也不需要在这里算
                        float lightIntensity = -sunLight.dot(triangles[shadingBuffer.triangleID[y][x]].hardNormal) * 0.5f + 0.5f;
                        colorRef = simpleLightBlend(colorRef, lightIntensity);
                    }
                }
                tmpView += dy;

                // if(x%64==0 || y%64==0) colorRef = 0xffff0000;
                buffer[y*pixelW + x] = colorRef;
            }
            view += dx;
        }
    }
    void drawFrame(const CameraInfo &_camera, uint *_buffer){
        // vertices   = _vertices;
        // triangles = _triangles;
        camera = _camera;
        buffer = _buffer;
        projectedVertices.clear();
        fragments.clear();

        pixelW = camera.width * tileSize;
        pixelH = camera.height * tileSize;

        if(pixelW > ShadingBuffer::W)
            throw runtime_error("width "+to_string(pixelW)+" is too wide for buffer");
        if(pixelH > ShadingBuffer::H)
            throw runtime_error("height "+to_string(pixelH)+" is too high for buffer");

        for(uint j=0;j<pixelH;j++)
        {
            for(uint i=0;i<pixelW;i++){
                shadingBuffer.zInv[j][i] = 0.0f;
                shadingBuffer.triangleID[j][i] = 0x80000000u;
            }
        }

        auto t0 = std::chrono::system_clock::now();
        frontClip();
        auto t1 = std::chrono::system_clock::now();
        qDebug()<<"stage1: frontclip         |"<<t1-t0;
        vertexProject();
        auto t2 = std::chrono::system_clock::now();
        qDebug()<<"stage2: vertexProject     |"<<t2-t1;
        getFragments();
        auto t3 = std::chrono::system_clock::now();
        qDebug()<<"stage3: getFragments      |"<<t3-t2;

        decltype(t3-t2) total;
        static vector<decltype(total)> frametimes;

        frameStat.vcnt = vertices.size();
        frameStat.tcnt = triangles.size();
        frameStat.tileFragmentSum = 0;
        frameStat.pixelIterated = 0;

        if(1){
            parallelRasterization();
            auto t4 = std::chrono::system_clock::now();
            qDebug()<<"stage4&5: parallel render |"<<t4-t3;
            total = t4-t0;
        }else{
            //bfRasterization();
            auto t4 = std::chrono::system_clock::now();
            qDebug()<<"stage4: rasterization     |"<<t4-t3;
            determineColor();
            auto t5 = std::chrono::system_clock::now();
            qDebug()<<"stage5: color             |"<<t5-t4;
            total = t5-t0;
        }
        qDebug()<<"---------------------------------";
        qDebug()<<"total                     |"<<total;

        frametimes.push_back(total);
        if(frametimes.size() > 20u)
            frametimes.erase(frametimes.begin());

        decltype(total) avg = std::accumulate(frametimes.begin(), frametimes.end(), decltype(total)(0)) / frametimes.size();
        int64_t var = 0;
        for(auto x:frametimes){
            int64_t v = x.count();
            var += v*v;
        }
        var /= frametimes.size();
        var -= avg.count()*avg.count();
        var = sqrt(var);

        qDebug()<<"total(20 frames avg)      |"<<avg;
        qDebug()<<"sqrt variance             |"<<var;

        // 土法 profiling
        qDebug()<<"---------------------------------";
        qDebug()<<"statistics:";
        qDebug()<<"vertex                    |"<<frameStat.vcnt;
        qDebug()<<"triangle                  |"<<frameStat.tcnt;
        qDebug()<<"tiled triangle part       |"<<frameStat.tileFragmentSum;
        qDebug()<<"iterated pixel            |"<<frameStat.pixelIterated;
    }
    friend void clearRenderBuffer();
    friend void submitMesh(const Mesh &mesh);
}renderer;

void drawFrame(const CameraInfo &camera, uint *buffer){
    renderer.drawFrame(camera, buffer);
}

void clearRenderBuffer(){
    vertices.clear();
    triangles.clear();

}

void submitMesh(const Mesh &mesh){
    uint n = vertices.size();
    for(const Vertex &v:mesh.vertices){
        vertices.push_back(v);
    }
    for(const Triangle &t:mesh.triangles){
        triangles.push_back(t);
        Triangle &curr = triangles.back();

        curr.materialID = mesh.materialID;
        curr.shaderConfig = mesh.shaderConfig;
        curr.vid[0] += n;
        curr.vid[1] += n;
        curr.vid[2] += n;
    }
}

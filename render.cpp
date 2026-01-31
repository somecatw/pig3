#include "render.h"
#include "utils.h"
#include <QDebug>
#include "assetmanager.h"
using namespace std;


ShadingUnit shadingBuffer[640][480];

Vertex vertexIntersect(const Vertex &a, const Vertex &b, const Plane &p){
    Vec3 intersection = p.intersect(link(a.pos, b.pos));
    float c = (intersection - a.pos).len() / (b.pos - a.pos).len();
    Vec3 interpolatedUV = a.uv * (1.0f-c) + b.uv * c;
    return {intersection, interpolatedUV};
}
class Renderer{
private:
    vector<Vertex> vertices;
    vector<Triangle> triangles;
    CameraInfo camera;
    uint *buffer;

    uint pixelW, pixelH;

    // (x, y, 4096.0f/z)
    vector<Vertex> projectedVertices;

    vector<Fragment> fragments;

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

        uint n = triangles.size();
        Vec3 screenCenter = camera.pos + camera.focalLength * camera.frame.axisZ;
        Plane cameraPlane = {screenCenter, camera.frame.axisZ};

        for(auto [id, triangle]: enumerate(triangles)){
            if(id>=n) break;

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
                triangles.push_back(tmp);
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
    }
    void getFragments(){
        fragments.reserve(triangles.size());
        for(auto [id, triangle]: enumerate(triangles)){
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


            current.v2d[0] = v0.pos;
            current.v2d[1] = v1.pos;
            current.v2d[2] = v2.pos;

            // 算 uv
            e0.z = v1.uv.x - v0.uv.x;
            e2.z = v0.uv.x - v2.uv.x;
            calcLinearCoefficient(current.u_z, {v0.pos.x, v0.pos.y, v0.uv.x}, e0, e2);

            e0.z = v1.uv.y - v0.uv.y;
            e2.z = v0.uv.y - v2.uv.y;
            calcLinearCoefficient(current.v_z, {v0.pos.x, v0.pos.y, v0.uv.y}, e0, e2);
        }
    }
    tuple<float, float, float, float> fragmentBBox(const Fragment &fragment){
        return {
            min({fragment.v2d[0].x, fragment.v2d[1].x, fragment.v2d[2].x}),
            min({fragment.v2d[0].y, fragment.v2d[1].y, fragment.v2d[2].y}),
            max({fragment.v2d[0].x, fragment.v2d[1].x, fragment.v2d[2].x}),
            max({fragment.v2d[0].y, fragment.v2d[1].y, fragment.v2d[2].y})
        };
    }
    void bfRasterization(){
        for(const Fragment& frag:fragments){
            auto [xmin, ymin, xmax, ymax] = fragmentBBox(frag);

            EdgeIterator edgeIt = frag.edgeIterator;
            Iterator2D zInv = frag.zInv;
            Iterator2D u_z = frag.u_z;
            Iterator2D v_z = frag.v_z;

            int xlt = max(0, (int)xmin);
            int ylt = max(0, (int)ymin);
            int xrb = min((int)pixelW-1, (int)xmax);
            int yrb = min((int)pixelH-1, (int)ymax);

            edgeIt.batchIterate(xlt, ylt);
            zInv.batchIterate(xlt, ylt);
            u_z.batchIterate(xlt, ylt);
            v_z.batchIterate(xlt, ylt);

            for(int x = xlt; x <= xrb; x++){
                EdgeIterator tempEdgeIt = edgeIt;
                Iterator2D tempZInv = zInv;
                Iterator2D tempUZ = u_z;
                Iterator2D tempVZ = v_z;

                for(int y = ylt; y <= yrb; y++){
                    int edgeResult = tempEdgeIt.check();
                    if(edgeResult == EdgeIterator::INNER){
                        if(shadingBuffer[x][y].zInv < tempZInv.val){
                            shadingBuffer[x][y].triangleID = frag.triangleID;
                            shadingBuffer[x][y].zInv = tempZInv.val;
                            shadingBuffer[x][y].u_z = tempUZ.val;
                            shadingBuffer[x][y].v_z = tempVZ.val;
                        }
                    }
                    tempEdgeIt.yIterate();
                    tempZInv.yIterate();
                    tempUZ.yIterate();
                    tempVZ.yIterate();
                }
                edgeIt.xIterate();
                zInv.xIterate();
                u_z.xIterate();
                v_z.xIterate();
            }
        }
    }
    void determineColor(){
        const Material &material = assetManager.getMaterials()[0];

        int w = material.img.width();
        int h = material.img.height();

        for(uint y = 0; y < pixelH; y++){
            for(uint x = 0; x < pixelW; x++){
                uint colorRef = 0xff000000;


                if(shadingBuffer[x][y].triangleID <= 10000){
                    float u = shadingBuffer[x][y].u_z / shadingBuffer[x][y].zInv;
                    float v = shadingBuffer[x][y].v_z / shadingBuffer[x][y].zInv;
                    assert(0<=u && u<=1 && 0<=v && v<=1);
                    int rx = int(u*w);
                    int ry = int(v*h);
                    colorRef = material.img.pixel(rx, ry);
                    // colorRef = 0xff33f6d4;
                }
                buffer[y*pixelW + x] = colorRef;
            }
        }
    }
    void drawFrame(const std::vector<Vertex> &_vertices, const std::vector<Triangle> &_triangles, const CameraInfo &_camera, uint *_buffer){
        vertices   = _vertices;
        triangles = _triangles;
        camera = _camera;
        buffer = _buffer;
        projectedVertices.clear();
        fragments.clear();

        pixelW = camera.width * tileSize;
        pixelH = camera.height * tileSize;

        qDebug()<<pixelW<<pixelH;

        if(pixelW > 640)
            throw runtime_error("width "+to_string(pixelW)+" is too wide for buffer");
        if(pixelH > 480)
            throw runtime_error("height "+to_string(pixelH)+" is too high for buffer");

        for(uint i=0;i<640;i++){
            for(uint j=0;j<480;j++){
                shadingBuffer[i][j].zInv = 0.0f;
                shadingBuffer[i][j].triangleID = 0x80000000u;
            }
        }

        auto t0 = std::chrono::system_clock::now();
        qDebug()<<"stage1: frontclip";
        frontClip();
        auto t1 = std::chrono::system_clock::now();
        qDebug()<<"stage2: vertexProject";
        vertexProject();
        auto t2 = std::chrono::system_clock::now();
        qDebug()<<"stage3: getFragments";
        getFragments();
        auto t3 = std::chrono::system_clock::now();
        qDebug()<<"stage4: rasterization";
        bfRasterization();
        auto t4 = std::chrono::system_clock::now();
        qDebug()<<"stage5: color";
        determineColor();
        qDebug()<<"complete!";
        auto t5 = std::chrono::system_clock::now();
        qDebug()<<t1-t0<<t2-t1<<t3-t2<<t4-t3<<t5-t4;
        // 土法 profiling
    }
}renderer;

void drawFrame(const std::vector<Vertex> &vertices, const std::vector<Triangle> &triangles, const CameraInfo &camera, uint *buffer){
    renderer.drawFrame(vertices, triangles, camera, buffer);
}

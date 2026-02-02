#include "assetmanager.h"
// By doubao

#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDebug>
#include <map>
#include <sstream>
#include <string>

// 定义全局AssetManager实例
AssetManager assetManager;

// 分割字符串的辅助函数（处理OBJ/MTL的行解析，按空格分割）
static std::vector<std::string> splitString(const std::string& str, char delimiter = ' ')
{
    std::vector<std::string> parts;
    std::stringstream ss(str);
    std::string part;
    while (std::getline(ss, part, delimiter))
    {
        if (!part.empty())
            parts.push_back(part);
    }
    return parts;
}

std::map<QString, int> AssetManager::loadMTL(const QString& mtlPath, const QString& baseDir)
{
    std::map<QString, int> mtlMap;
    QFile mtlFile(mtlPath);
    if (!mtlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[MTL] Failed to open:" << mtlPath;
        return mtlMap;
    }

    qInfo() << "[MTL] Parsing file:" << mtlPath;

    QTextStream in(&mtlFile);
    QString currentMtlName;
    Material currentMat;
    bool hasMat = false;

    while (!in.atEnd()) {
        QString line = in.readLine().simplified();
        if (line.isEmpty() || line.startsWith('#')) continue;
        std::vector<std::string> parts = splitString(line.toStdString());

        if (parts[0] == "newmtl") {
            // 保存上一个解析完的材质
            if (hasMat) {
                m_materials.push_back(currentMat);
                mtlMap[currentMtlName] = m_materials.size() - 1;
                qInfo() << " - Material defined:" << currentMtlName << "ID:" << (m_materials.size() - 1);
            }
            currentMtlName = (parts.size() > 1) ? QString::fromStdString(parts[1]) : "default";
            currentMat = Material();
            hasMat = true;
        }
        else if (parts[0] == "map_Kd" && hasMat) {
            QString texRelPath = QString::fromStdString(parts[1]);
            QString texAbsPath = QDir(baseDir).absoluteFilePath(texRelPath);
            currentMat.img = QImage(texAbsPath);
            if (currentMat.img.isNull()) {
                qWarning() << "  [!] Failed to load texture:" << texAbsPath;
            } else {
                qInfo() << "  [+] Texture loaded:" << texAbsPath;
            }
        }
    }

    // 别忘了保存最后一个材质块
    if (hasMat) {
        m_materials.push_back(currentMat);
        mtlMap[currentMtlName] = m_materials.size() - 1;
        qInfo() << " - Material defined:" << currentMtlName << "ID:" << (m_materials.size() - 1);
    }

    mtlFile.close();
    return mtlMap;
}
bool AssetManager::loadOBJ(const QString& objPath)
{
    QFile objFile(objPath);
    if (!objFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[OBJ] Failed to open:" << objPath;
        return false;
    }

    qInfo() << "[OBJ] Loading:" << objPath;

    std::vector<Vec3> tempPos;
    std::vector<Vec3> tempUV;
    std::map<QString, int> mtlNameToId;

    Mesh currentMesh;
    currentMesh.materialID = -1;
    std::map<std::pair<uint, uint>, uint> vertexMap;

    QString objBaseDir = QFileInfo(objPath).absolutePath();
    QTextStream in(&objFile);

    while (!in.atEnd()) {
        QString line = in.readLine().simplified();
        if (line.isEmpty() || line.startsWith('#')) continue;
        std::vector<std::string> parts = splitString(line.toStdString());
        if (parts.empty()) continue;

        std::string cmd = parts[0];

        if (cmd == "v") {
            tempPos.emplace_back(std::stof(parts[1]), std::stof(parts[2]), std::stof(parts[3]));
        }
        else if (cmd == "vt") {
            tempUV.emplace_back(std::stof(parts[1]), 1.0f - std::stof(parts[2]), 0.0f);
        }
        else if (cmd == "mtllib") {
            QString mtlAbsPath = QDir(objBaseDir).absoluteFilePath(QString::fromStdString(parts[1]));
            auto newMtls = loadMTL(mtlAbsPath, objBaseDir);
            mtlNameToId.insert(newMtls.begin(), newMtls.end());
        }
        else if (cmd == "usemtl") {
            QString mtlName = QString::fromStdString(parts[1]);
            if (!currentMesh.triangles.empty()) {
                m_meshes.push_back(currentMesh);
                currentMesh = Mesh();
                vertexMap.clear();
            }
            if (mtlNameToId.count(mtlName)) {
                currentMesh.materialID = mtlNameToId[mtlName];
            } else {
                currentMesh.materialID = -1;
            }
        }
        else if (cmd == "f") {
            // parts[0] 是 "f"，后面可能有 3 个、4 个或更多顶点
            if (parts.size() < 4) continue;

            std::vector<uint> faceVertexIndices; // 存储当前面转换后的顶点索引
            bool faceValid = true;

            // 1. 先解析并存储该面所有的顶点
            for (size_t i = 1; i < parts.size(); ++i) {
                std::vector<std::string> vParts = splitString(parts[i], '/');
                if (vParts.empty()) { faceValid = false; break; }

                uint posIdx = std::stoull(vParts[0]) - 1;
                uint uvIdx = (vParts.size() > 1 && !vParts[1].empty()) ? (std::stoull(vParts[1]) - 1) : 0;

                if (posIdx >= tempPos.size()) { faceValid = false; break; }

                // 使用 Map 复用顶点，减少冗余
                std::pair<uint, uint> key = {posIdx, uvIdx};
                if (vertexMap.find(key) == vertexMap.end()) {
                    Vertex v;
                    v.pos = tempPos[posIdx];
                    v.uv = (uvIdx < tempUV.size()) ? tempUV[uvIdx] : Vec3(0, 0, 0);
                    vertexMap[key] = (uint)currentMesh.vertices.size();
                    currentMesh.vertices.push_back(v);
                }
                faceVertexIndices.push_back(vertexMap[key]);
            }

            if (!faceValid || faceVertexIndices.size() < 3) continue;

            // 2. 扇形剖分：将多边形拆分为 (n-2) 个三角形
            for (size_t i = 1; i < faceVertexIndices.size() - 1; ++i) {
                Triangle tri;
                tri.vid[0] = faceVertexIndices[0];
                tri.vid[1] = faceVertexIndices[i];
                tri.vid[2] = faceVertexIndices[i + 1];

                // 计算该三角形的面法线
                Vec3 v0 = currentMesh.vertices[tri.vid[0]].pos;
                Vec3 v1 = currentMesh.vertices[tri.vid[1]].pos;
                Vec3 v2 = currentMesh.vertices[tri.vid[2]].pos;
                tri.hardNormal = (v1 - v0).cross(v2 - v0);
                tri.hardNormal.normalize();

                currentMesh.triangles.push_back(tri);
            }
        }
    }

    if (!currentMesh.triangles.empty()) {
        m_meshes.push_back(currentMesh);
    }

    objFile.close();
    qInfo() << "[OBJ] Load finished. Total SubMeshes:" << m_meshes.size();
    return !m_meshes.empty();
}

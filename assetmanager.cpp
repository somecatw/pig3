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

// 加载MTL文件（仅解析漫反射贴图map_Kd）
bool AssetManager::loadMTL(const QString& mtlPath, const QString& baseDir)
{
    QFile mtlFile(mtlPath);
    if (!mtlFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Failed to open MTL file:" << mtlPath;
        return false;
    }

    Material newMat;
    QTextStream in(&mtlFile);
    while (!in.atEnd())
    {
        QString line = in.readLine().simplified();  // 去除首尾空格+合并中间空格
        if (line.isEmpty() || line.startsWith('#')) // 跳过空行和注释
            continue;

        std::vector<std::string> parts = splitString(line.toStdString());
        if (parts.empty())
            continue;

        // 解析漫反射贴图：map_Kd 贴图路径.png
        if (parts[0] == "map_Kd")
        {
            if (parts.size() < 2)
                continue;
            // 贴图路径是相对MTL的，转成绝对路径（基于OBJ所在目录）
            QString texRelPath = QString::fromStdString(parts[1]);
            QString texAbsPath = QDir(baseDir).absoluteFilePath(texRelPath);
            // 加载贴图（QImage支持png/jpg/bmp等常见格式）
            newMat.img = QImage(texAbsPath);
            // newMat.texPath = texAbsPath;
            if (newMat.img.isNull())
            {
                qWarning() << "Failed to load texture:" << texAbsPath;
                continue;
            }
            qInfo() << "Loaded texture:" << texAbsPath;
            break; // 暂时只加载一个漫反射贴图
        }
    }

    mtlFile.close();
    m_materials.push_back(newMat); // 加入材质库，ID为当前下标
    return true;
}

// 核心：加载OBJ文件
bool AssetManager::loadOBJ(const QString& objPath)
{
    QFile objFile(objPath);
    if (!objFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Failed to open OBJ file:" << objPath;
        return false;
    }

    // 初始化临时变量：存储OBJ原始的v/vt，当前网格，顶点映射表
    std::vector<Vec3> tempPos;    // 临时存储OBJ的v指令（顶点位置）
    std::vector<Vec3> tempUV;     // 临时存储OBJ的vt指令（UV坐标）
    Mesh currentMesh;             // 当前正在构建的网格
    // 顶点映射表：(pos索引, uv索引) -> currentMesh.vertexs的下标，避免重复顶点
    std::map<std::pair<uint, uint>, uint> vertexMap;
    QString objBaseDir = QFileInfo(objPath).absolutePath(); // OBJ所在目录（解决相对路径）

    QTextStream in(&objFile);
    while (!in.atEnd())
    {
        QString line = in.readLine().simplified();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        std::vector<std::string> parts = splitString(line.toStdString());
        if (parts.empty())
            continue;

        std::string cmd = parts[0];
        // 1. 解析顶点位置：v x y z
        if (cmd == "v")
        {
            if (parts.size() < 4)
                continue;
            float x = std::stof(parts[1]);
            float y = std::stof(parts[2]);
            float z = std::stof(parts[3]);
            tempPos.emplace_back(x, y, z);
        }
        // 2. 解析UV坐标：vt u v（OBJ的UV是u/v，范围通常0~1，v轴上下翻转）
        else if (cmd == "vt")
        {
            if (parts.size() < 3)
                continue;
            float u = std::stof(parts[1]);
            float v = std::stof(parts[2]);
            tempUV.emplace_back(u, 1.0-v, 0.0f); // UV用Vec3，z留0
        }
        // 3. 解析材质库引用：mtllib 材质文件.mtl
        else if (cmd == "mtllib")
        {
            if (parts.size() < 2)
                continue;
            QString mtlRelPath = QString::fromStdString(parts[1]);
            QString mtlAbsPath = QDir(objBaseDir).absoluteFilePath(mtlRelPath);
            // 加载MTL并关联材质ID（当前材质库最后一个下标）
            if (loadMTL(mtlAbsPath, objBaseDir))
                currentMesh.materialID = m_materials.size() - 1;
        }
        // 4. 解析面：f v1/vt1[/vn1] v2/vt2[/vn2] v3/vt3[/vn3]（忽略vn）
        else if (cmd == "f")
        {
            if (parts.size() < 4) // f需要至少3个顶点（三角形）
                continue;

            Triangle tri;
            // 处理三角形的3个顶点（parts[1], parts[2], parts[3]）
            for (int i = 0; i < 3; ++i)
            {
                std::string vertexStr = parts[i + 1];
                // 按/分割：提取pos索引和uv索引（跳过vn）
                std::vector<std::string> vParts = splitString(vertexStr, '/');
                if (vParts.size() < 2) // 必须有pos和uv，否则跳过
                    continue;

                // OBJ索引从1开始，转成0开始
                uint posIdx = std::stoull(vParts[0]) - 1;
                uint uvIdx = std::stoull(vParts[1]) - 1;

                // 检查索引是否越界
                if (posIdx >= tempPos.size() || uvIdx >= tempUV.size())
                    continue;

                // 检查是否已有该(pos,uv)组合，无则添加到顶点列表
                std::pair<uint, uint> key = {posIdx, uvIdx};
                if (vertexMap.find(key) == vertexMap.end())
                {
                    Vertex newVert;
                    newVert.pos = tempPos[posIdx];
                    newVert.uv = tempUV[uvIdx];
                    vertexMap[key] = currentMesh.vertices.size();
                    currentMesh.vertices.push_back(newVert);
                }

                // 给三角形赋值顶点下标
                tri.vid[i] = vertexMap[key];
            }

            currentMesh.triangles.push_back(tri);
        }
    }

    objFile.close();

    // 检查是否加载到有效网格，有则加入网格库
    if (!currentMesh.triangles.empty() || !currentMesh.vertices.empty())
    {
        m_meshes.push_back(currentMesh);
        qInfo() << "Loaded OBJ successfully:" << objPath;
        qInfo() << " - Vertices:" << currentMesh.vertices.size();
        qInfo() << " - Triangles:" << currentMesh.triangles.size();
        qInfo() << " - Material ID:" << currentMesh.materialID;
        return true;
    }
    else
    {
        qWarning() << "No valid mesh data in OBJ:" << objPath;
        return false;
    }
}

#ifndef ASSETMANAGER_H
#define ASSETMANAGER_H
#include <QString>
#include "structures.h"
#include <map>

class AssetManager
{
public:
    AssetManager() = default;
    ~AssetManager() = default;

    // 核心接口：加载OBJ文件，返回是否成功
    // objPath: OBJ文件绝对/相对路径（Qt的QString兼容跨平台）
    bool loadOBJ(const QString& objPath);

    // 公共获取接口（只读，避免外部修改内部数据）
    std::vector<Mesh>& getMeshes() { return m_meshes; }
    std::vector<Material>& getMaterials() { return m_materials; }

private:
    // 辅助方法：加载MTL材质文件（解析漫反射贴图map_Kd）
    // mtlPath: MTL文件路径，baseDir: OBJ所在目录（解决贴图相对路径问题）
    std::map<QString, int> loadMTL(const QString& mtlPath, const QString& baseDir);

    // 成员变量：存储所有加载的网格和材质
    std::vector<Mesh> m_meshes;
    std::vector<Material> m_materials;
};

class SceneManager{
public:

};

extern AssetManager assetManager;

#endif // ASSETMANAGER_H

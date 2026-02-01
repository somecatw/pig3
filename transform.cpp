#include "transform.h"

// by gemini
Transform Transform::rotateAroundAxis(Vec3 axis, float rad)
{
    // 1. 确保旋转轴是单位向量
    axis.normalize();

    float s = sin(rad);
    float c = cos(rad);
    float oc = 1.0f - c;

    // 2. 根据罗德里格旋转公式构建旋转矩阵
    // 这里假设 Mat3 的构造函数接受 9 个参数，或者你可以直接操作其成员
    Mat3 rot;

    // 第一行
    rot[0][0] = oc * axis.x * axis.x + c;
    rot[0][1] = oc * axis.x * axis.y - axis.z * s;
    rot[0][2] = oc * axis.z * axis.x + axis.y * s;

    // 第二行
    rot[1][0] = oc * axis.x * axis.y + axis.z * s;
    rot[1][1] = oc * axis.y * axis.y + c;
    rot[1][2] = oc * axis.y * axis.z - axis.x * s;

    // 第三行
    rot[2][0] = oc * axis.z * axis.x - axis.y * s;
    rot[2][1] = oc * axis.y * axis.z + axis.x * s;
    rot[2][2] = oc * axis.z * axis.z + c;

    // 3. 返回变换对象（纯旋转，位移通常设为 0）
    return {Vec3(0, 0, 0), rot};
}

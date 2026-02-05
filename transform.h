#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "mathbase.h"



struct Transform
{
    Vec3 translation = {0, 0, 0};
    Mat3 rotation = Mat3::eye();

    Transform inverseTransform() const{
        return {-1 * translation * rotation.transpose(), rotation.transpose()};
    }

    Transform operator *(const Transform &other) const{
        return {
            other.translation + translation * other.rotation,
            rotation * other.rotation
        };
    }
    static Transform rotateAroundAxis(Vec3 axis, float rad);

    static inline Transform translate(const Vec3 &v){
        return {v, Mat3::eye()};
    }
    static inline Transform rotateAroundPoint(const Vec3 &pos, const Vec3 &axis, float rad){
        return translate(-pos) * rotateAroundAxis(axis, rad) * translate(pos);
    }
};

struct LocalFrame{
    Vec3 axisX, axisY, axisZ;
    void applyTransform(const Transform &transform){
        Mat3 frame = {axisX, axisY, axisZ};
        frame *= transform.rotation;
        (*this) = {frame.row(0), frame.row(1), frame.row(2)};
    }
};

#endif // TRANSFORM_H

#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "mathbase.h"



struct Transform
{
    Vec3 translation;
    Mat3 rotation;

    Transform inverseTransform() const{
        return {-1 * translation * rotation, rotation.transpose()};
    }

    Transform operator *(const Transform &other) const{
        return {
            translation + other.translation * rotation,
            rotation * other.rotation
        };
    }
    static Transform rotateAroundAxis(Vec3 axis, float rad);

    static inline Transform translate(const Vec3 &v){
        return {v};
    }
    static inline Transform rotateAroundPoint(const Vec3 &pos, const Vec3 &axis, float rad){
        return translate(pos) * rotateAroundAxis(axis, rad) * translate(-pos);
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

#ifndef MATHBASE
#define MATHBASE

#include <cassert>
#include <cmath>
#include <array>
#include <algorithm>
#include <iostream>
#include <string>

class Vec3{
    public:
    float x,y,z;
    Vec3():x(0),y(0),z(0){}
    Vec3(float _x, float _y, float _z):x(_x),y(_y),z(_z){}  
    Vec3 operator +(const Vec3& other)const{
        return {x+other.x, y+other.y, z+other.z};
    }  
    Vec3 operator -(const Vec3& other)const{
        return {x-other.x, y-other.y, z-other.z};
    }
    Vec3 operator -()const{
        return {-x, -y, -z};
    }
    float dot(const Vec3& other)const{
        return x*other.x + y*other.y + z*other.z;
    }
    Vec3 cross(const Vec3& other)const{
        return {
            y*other.z-z*other.y,
            -x*other.z+z*other.x,
            x*other.y-y*other.x
        };
    }
    float len()const{
        return sqrt(x*x+y*y+z*z);
    }
    Vec3 operator *(float f)const{
        return {f*x, f*y, f*z};
    }
    friend inline Vec3 operator *(float f, const Vec3& v){
        return {f*v.x, f*v.y, f*v.z};
    }
    Vec3 operator /(float f)const{
        return {x/f, y/f, z/f};
    }
    Vec3& operator +=(const Vec3& other){
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
    Vec3& operator -=(const Vec3& other){
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }
    Vec3& operator *=(float f){
        x *= f;
        y *= f;
        z *= f;
        return *this;
    }
    Vec3& operator /=(float f){
        x /= f;
        y /= f;
        z /= f;
        return *this;
    }
    void normalize(){
        (*this)/=len();
    }
    friend std::ostream& operator <<(std::ostream& cout, const Vec3& v){
        cout<<"["<<v.x<<", "<<v.y<<", "<<v.z<<"]";
        return cout;
    }
    std::string to_string()const{
        return "["+std::to_string(x)+", "+std::to_string(y)+", "+std::to_string(z)+"]";
    }
};
class Mat3{
    private:
    std::array<std::array<float, 3>,3> val;
    public:
    Mat3():val(){}
    Mat3(const Vec3& a, const Vec3& b, const Vec3& c){
        val[0] = {a.x, a.y, a.z};
        val[1] = {b.x, b.y, b.z};
        val[2] = {c.x, c.y, c.z};
    }
    const std::array<float, 3>& operator [](int index)const{
        return val[index];
    }
    std::array<float, 3>& operator [](int index){
        return val[index];
    }
    Vec3 row(int r)const{
        return {val[r][0], val[r][1], val[r][2]};
    }
    Mat3 operator+ (const Mat3& other)const{
        Mat3 ret;
        for(int i=0;i<3;i++)
            for(int j=0;j<3;j++)
                ret.val[i][j]=val[i][j]+other.val[i][j];
        return ret;
    }
    Mat3 operator- (const Mat3& other)const{
        Mat3 ret;
        for(int i=0;i<3;i++)
            for(int j=0;j<3;j++)
                ret.val[i][j]=val[i][j]-other.val[i][j];
        return ret;
    }
    Mat3 operator* (const Mat3& other)const{
        Mat3 ret;
        for(int i=0;i<3;i++)
            for(int j=0;j<3;j++)
                for(int k=0;k<3;k++){
                    ret.val[i][j] += val[i][k]*other.val[k][j];
                }
        return ret;
    }
    Mat3 &operator *= (const Mat3 &other){
        *this = (*this) * other;
        return *this;
    }
    Mat3 transpose()const{
        Mat3 ret = *this;
        std::swap(ret.val[0][1], ret.val[1][0]);
        std::swap(ret.val[0][2], ret.val[2][0]);
        std::swap(ret.val[1][2], ret.val[2][1]);
        return ret;
    }
    Mat3 inverse()const{
        // 懒得写高斯消元了
        throw std::runtime_error("not implemented!");
        return *this;
    }
    Mat3 operator *(float f){
        Mat3 ret=*this;
        for(int i=0;i<3;i++)
            for(int j=0;j<3;j++)
                ret.val[i][j] *= f;
        return ret;
    }
    friend inline Mat3 operator *(float f, const Mat3& m){
        Mat3 ret = m;
        for(int i=0;i<3;i++)
            for(int j=0;j<3;j++)
                for(int k=0;k<3;k++){
                    ret.val[i][j] *= f;
                }
        return ret;
    }
    Mat3 operator /(float f){
        Mat3 ret=*this;
        for(int i=0;i<3;i++)
            for(int j=0;j<3;j++)
                ret.val[i][j] /= f;
        return ret;
    }
    friend inline Vec3 operator *(const Vec3& v, const Mat3& m){
        return {
            v.x*m.val[0][0]+v.y*m.val[1][0]+v.z*m.val[2][0],
            v.x*m.val[0][1]+v.y*m.val[1][1]+v.z*m.val[2][1],
            v.x*m.val[0][2]+v.y*m.val[1][2]+v.z*m.val[2][2],
        };
    }
    static Mat3 eye(){
        return {
            {1, 0, 0},
            {0, 1, 0},
            {0, 0, 1}
        };
    }
};

const static float eps=1e-6;

inline Vec3 SolveLinearEq2(Vec3 eq1, Vec3 eq2){
    float delta = eq1.x*eq2.y - eq1.y*eq2.x;
    if(abs(delta) <= eps)return {0.0f, 0.0f, 0.0f};
    return{
        (eq1.z*eq2.y-eq1.y*eq2.z)/delta,
        (eq1.x*eq2.z-eq1.z*eq2.x)/delta,
        0.0f
    };
}

struct Line{
    Vec3 point, direction;
};

inline Line link(const Vec3 &start, const Vec3& end){
    return Line({start, end-start});
}

struct Plane{
    Vec3 point, normal;
    Vec3 intersect(const Line& line)const{
        float p1 = line.direction.dot(normal);
        float p2 = (point-line.point).dot(normal);
        return line.point + line.direction / p1 * p2;
    }
};

#endif // mathbase.h

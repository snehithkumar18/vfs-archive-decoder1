#ifndef PIXELFORGE_MATH_UTILS_H
#define PIXELFORGE_MATH_UTILS_H

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace PixelForge {

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vector3() = default;
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vector3 operator+(const Vector3& o) const { return Vector3(x + o.x, y + o.y, z + o.z); }
    Vector3 operator-(const Vector3& o) const { return Vector3(x - o.x, y - o.y, z - o.z); }
    Vector3 operator*(float s) const { return Vector3(x * s, y * s, z * s); }
};

struct Matrix3x3 {
    float m[3][3] = {0};

    Matrix3x3() = default;
    Matrix3x3(float m00, float m01, float m02,
              float m10, float m11, float m12,
              float m20, float m21, float m22) {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m12;
        m[2][0] = m20; m[2][1] = m21; m[2][2] = m22;
    }

    Vector3 multiply(const Vector3& v) const {
        return Vector3(
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        );
    }
};

struct HSVColor {
    float h; // [0, 360)
    float s; // [0, 1]
    float v; // [0, 1]
};

struct HSLColor {
    float h; // [0, 360)
    float s; // [0, 1]
    float l; // [0, 1]
};

struct YCbCrColor {
    float y;  // [0, 255]
    float cb; // [0, 255]
    float cr; // [0, 255]
};

struct XYZColor {
    float x;
    float y;
    float z;
};

struct LabColor {
    float l;
    float a;
    float b;
};

// Conversions
HSVColor RGBToHSV(uint8_t r, uint8_t g, uint8_t b);
void HSVToRGB(const HSVColor& hsv, uint8_t& r, uint8_t& g, uint8_t& b);

HSLColor RGBToHSL(uint8_t r, uint8_t g, uint8_t b);
void HSLToRGB(const HSLColor& hsl, uint8_t& r, uint8_t& g, uint8_t& b);

YCbCrColor RGBToYCbCr(uint8_t r, uint8_t g, uint8_t b);
void YCbCrToRGB(const YCbCrColor& ycc, uint8_t& r, uint8_t& g, uint8_t& b);

XYZColor RGBToXYZ(uint8_t r, uint8_t g, uint8_t b);
void XYZToRGB(const XYZColor& xyz, uint8_t& r, uint8_t& g, uint8_t& b);

LabColor XYZToLab(const XYZColor& xyz);
XYZColor LabToXYZ(const LabColor& lab);

LabColor RGBToLab(uint8_t r, uint8_t g, uint8_t b);
void LabToRGB(const LabColor& lab, uint8_t& r, uint8_t& g, uint8_t& b);

} // namespace PixelForge

#endif // PIXELFORGE_MATH_UTILS_H

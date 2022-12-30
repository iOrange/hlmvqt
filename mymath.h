#pragma once
#include <cmath>
#include <cassert>

#define DebugAssert assert

//#define MYMATH_ALIGN_FOR_SSE    alignas(16)
#define MYMATH_ALIGN_FOR_SSE

constexpr float MM_Pi               = 3.14159265358979323846f;
constexpr float MM_InvPi            = 1.0f / MM_Pi;
constexpr float MM_TwoPi            = MM_Pi * 2.0f;
constexpr float MM_HalfPi           = MM_Pi * 0.5f;
constexpr float MM_Epsilon          = 1.192092896e-07f;
constexpr float MM_OneMinusEpsilon  = 0.9999999403953552f;


constexpr float Rad2Deg(const float rad) {
    return rad * (180.0f / MM_Pi);
}
constexpr float Deg2Rad(const float deg) {
    return deg * (MM_Pi / 180.0f);
}

template <typename T>
inline T NormalizeAngle(const T& angle) {
    T result = angle;
    while (result < T(0)) {
        result += T(360);
    }
    while (result > T(360)) {
        result -= T(360);
    }
    return result;
}

template <typename T>
inline T Min3(const T& a, const T& b, const T& c) {
    return std::min(a, std::min(b, c));
}
template <typename T>
inline T Max3(const T& a, const T& b, const T& c) {
    return std::max(a, std::max(b, c));
}

template <typename T>
inline void Swap(T& a, T& b) {
    T temp = a;
    a = b;
    b = temp;
}

template <typename T>
inline T Lerp(const T& a, const T& b, const float t) {
    //return a + (b - a) * t;
    return (a * (1.0f - t)) + (b * t);
}

template <typename T>
inline T Clamp(const T& x, const T& left, const T& right) {
    return (x < left) ? left : ((x > right) ? right : x);
}

inline float FAbs(const float x) {
    return std::fabsf(x);
}
inline float Sin(const float x) {
    return std::sinf(x);
}
inline float ASin(const float x) {
    return std::asinf(x);
}
inline float Cos(const float x) {
    return std::cosf(x);
}
inline float ACos(const float x) {
    return std::acosf(x);
}
inline float Sqrt(const float x) {
    return std::sqrtf(x);
}
inline int Floori(const float x) {
    return static_cast<int>(std::floorf(x));
}



struct vec2f {
    float x, y;

    vec2f() : x(0.0f), y(0.0f) {}
    vec2f(const float _x, const float _y) : x(_x), y(_y) {}
    vec2f(const vec2f& other) : x(other.x), y(other.y) {}

    inline float operator[](const size_t index) const {
        DebugAssert(index == 0 || index == 1);
        return (&x)[index];
    }
    inline float& operator[](const size_t index) {
        DebugAssert(index == 0 || index == 1);
        return (&x)[index];
    }

    inline bool operator==(const vec2f& other) const {
        return x == other.x && y == other.y;
    }
    inline bool operator!=(const vec2f& other) const {
        return x != other.x || y != other.y;
    }

    inline vec2f operator-() const { return vec2f(-x, -y); }
    inline vec2f operator*(const float f) const { return vec2f(x * f, y * f); }
    inline vec2f operator/(const float f) const { return vec2f(x / f, y / f); }
    inline vec2f operator+(const vec2f& other) const { return vec2f(x + other.x, y + other.y); }
    inline vec2f operator-(const vec2f& other) const { return vec2f(x - other.x, y - other.y); }
    inline vec2f operator*(const vec2f& other) const { return vec2f(x * other.x, y * other.y); }
    inline vec2f operator/(const vec2f& other) const { return vec2f(x / other.x, y / other.y); }

    inline vec2f& operator+=(const vec2f& other) {
        x += other.x;
        y += other.y;
        return *this;
    }
    inline vec2f& operator-=(const vec2f& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    inline vec2f& operator*=(const vec2f& other) {
        x *= other.x;
        y *= other.y;
        return *this;
    }
    inline vec2f& operator/=(const vec2f& other) {
        x /= other.x;
        y /= other.y;
        return *this;
    }
    inline vec2f& operator*=(const float f) {
        x *= f;
        y *= f;
        return *this;
    }
    inline vec2f& operator/=(const float f) {
        x /= f;
        y /= f;
        return *this;
    }

    friend inline vec2f operator*(const float f, const vec2f& v) {
        return vec2f(f * v.x, f * v.y);
    }

    inline float length() const {
        return Sqrt(vec2f::dot(*this, *this));
    }

    static float dot(const vec2f& a, const vec2f& b) {
        return a.x * b.x + a.y * b.y;
    }
    static vec2f normalize(const vec2f& v) {
        return v / v.length();
    }
};

struct vec3f {
    float x, y, z;

    vec3f() : x(0.0f), y(0.0f), z(0.0f) {}
    vec3f(const float _x, const float _y, const float _z) : x(_x), y(_y), z(_z) {}
    vec3f(const vec3f& other) : x(other.x), y(other.y), z(other.z) {}

    inline float operator[](const size_t index) const {
        DebugAssert(index == 0 || index == 1 || index == 2);
        return (&x)[index];
    }
    inline float& operator[](const size_t index) {
        DebugAssert(index == 0 || index == 1 || index == 2);
        return (&x)[index];
    }

    inline bool operator==(const vec3f& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
    inline bool operator!=(const vec3f& other) const {
        return x != other.x || y != other.y || z != other.z;
    }

    inline vec3f operator-() const { return vec3f(-x, -y, -z); }
    inline vec3f operator*(const float f) const { return vec3f(x * f, y * f, z * f); }
    inline vec3f operator/(const float f) const { return vec3f(x / f, y / f, z / f); }
    inline vec3f operator+(const vec3f& other) const { return vec3f(x + other.x, y + other.y, z + other.z); }
    inline vec3f operator-(const vec3f& other) const { return vec3f(x - other.x, y - other.y, z - other.z); }
    inline vec3f operator*(const vec3f& other) const { return vec3f(x * other.x, y * other.y, z * other.z); }
    inline vec3f operator/(const vec3f& other) const { return vec3f(x / other.x, y / other.y, z / other.z); }

    inline vec3f& operator+=(const vec3f& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
    inline vec3f& operator-=(const vec3f& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }
    inline vec3f& operator*=(const vec3f& other) {
        x *= other.x;
        y *= other.y;
        z *= other.z;
        return *this;
    }
    inline vec3f& operator/=(const vec3f& other) {
        x /= other.x;
        y /= other.y;
        z /= other.z;
        return *this;
    }
    inline vec3f& operator*=(const float f) {
        x *= f;
        y *= f;
        z *= f;
        return *this;
    }
    inline vec3f& operator/=(const float f) {
        x /= f;
        y /= f;
        z /= f;
        return *this;
    }

    friend inline vec3f operator*(const float f, const vec3f& v) {
        return vec3f(f * v.x, f * v.y, f * v.z);
    }

    inline float length() const {
        return Sqrt(vec3f::dot(*this, *this));
    }

    static float dot(const vec3f& a, const vec3f& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    static vec3f cross(const vec3f& a, const vec3f& b) {
        return vec3f(a.y * b.z - a.z * b.y,
                     a.z * b.x - a.x * b.z,
                     a.x * b.y - a.y * b.x);
    }
    static vec3f normalize(const vec3f& v) {
        return v / v.length();
    }
};

struct MYMATH_ALIGN_FOR_SSE vec4f {
    float x, y, z, w;

    vec4f() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    vec4f(const float _x, const float _y, const float _z, const float _w) : x(_x), y(_y), z(_z), w(_w) {}
    vec4f(const vec4f& other) : x(other.x), y(other.y), z(other.z), w(other.w) {}

    inline float operator[](const size_t index) const {
        DebugAssert(index == 0 || index == 1 || index == 2 || index == 3);
        return (&x)[index];
    }
    inline float& operator[](const size_t index) {
        DebugAssert(index == 0 || index == 1 || index == 2 || index == 3);
        return (&x)[index];
    }

    inline bool operator==(const vec4f& other) const {
        return x == other.x && y == other.y && z == other.z && w == other.w;
    }
    inline bool operator!=(const vec4f& other) const {
        return x != other.x || y != other.y || z != other.z || w != other.w;
    }

    inline vec4f operator-() const { return vec4f(-x, -y, -z, -w); }
    inline vec4f operator*(const float f) const { return vec4f(x * f, y * f, z * f, w * f); }
    inline vec4f operator/(const float f) const { return vec4f(x / f, y / f, z / f, w / f); }
    inline vec4f operator+(const vec4f& other) const { return vec4f(x + other.x, y + other.y, z + other.z, w + other.w); }
    inline vec4f operator-(const vec4f& other) const { return vec4f(x - other.x, y - other.y, z - other.z, w - other.w); }
    inline vec4f operator*(const vec4f& other) const { return vec4f(x * other.x, y * other.y, z * other.z, w * other.w); }
    inline vec4f operator/(const vec4f& other) const { return vec4f(x / other.x, y / other.y, z / other.z, w / other.w); }

    inline vec4f& operator+=(const vec4f& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        w += other.w;
        return *this;
    }
    inline vec4f& operator-=(const vec4f& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;
        return *this;
    }
    inline vec4f& operator*=(const vec4f& other) {
        x *= other.x;
        y *= other.y;
        z *= other.z;
        w *= other.w;
        return *this;
    }
    inline vec4f& operator/=(const vec4f& other) {
        x /= other.x;
        y /= other.y;
        z /= other.z;
        w /= other.w;
        return *this;
    }
    inline vec4f& operator*=(const float f) {
        x *= f;
        y *= f;
        z *= f;
        w *= f;
        return *this;
    }
    inline vec4f& operator/=(const float f) {
        x /= f;
        y /= f;
        z /= f;
        w /= f;
        return *this;
    }

    friend inline vec4f operator*(const float f, const vec4f& v) {
        return vec4f(f * v.x, f * v.y, f * v.z, f * v.w);
    }

    inline float length() const {
        return Sqrt(vec4f::dot(*this, *this));
    }

    static float dot(const vec4f& a, const vec4f& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }
    static vec4f normalize(const vec4f& v) {
        return v / v.length();
    }
};

struct MYMATH_ALIGN_FOR_SSE quatf {
    float x, y, z, w;

    quatf() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    quatf(const float _x, const float _y, const float _z, const float _w) : x(_x), y(_y), z(_z), w(_w) {}
    quatf(const quatf& other) : x(other.x), y(other.y), z(other.z), w(other.w) {}

    inline float operator[](const size_t index) const {
        DebugAssert(index == 0 || index == 1 || index == 2 || index == 3);
        return (&x)[index];
    }
    inline float& operator[](const size_t index) {
        DebugAssert(index == 0 || index == 1 || index == 2 || index == 3);
        return (&x)[index];
    }

    inline bool operator==(const quatf& other) const {
        return x == other.x && y == other.y && z == other.z && w == other.w;
    }
    inline bool operator!=(const quatf& other) const {
        return x != other.x || y != other.y || z != other.z || w != other.w;
    }

    // this is not a conjugate !!!
    inline quatf operator-() const { return quatf(-x, -y, -z, -w); }
    inline quatf operator*(const float f) const { return quatf(x * f, y * f, z * f, w * f); }
    inline quatf operator/(const float f) const { return quatf(x / f, y / f, z / f, w / f); }
    inline quatf operator+(const quatf& other) const { return quatf(x + other.x, y + other.y, z + other.z, w + other.w); }
    inline quatf operator-(const quatf& other) const { return quatf(x - other.x, y - other.y, z - other.z, w - other.w); }
    inline quatf operator*(const quatf& other) const {
        return quatf(w * other.x + x * other.w + y * other.z - z * other.y,
                     w * other.y + y * other.w + z * other.x - x * other.z,
                     w * other.z + z * other.w + x * other.y - y * other.x,
                     w * other.w - x * other.x - y * other.y - z * other.z);
    }
    inline vec3f operator*(const vec3f& v) const {
        // original code from DooM 3 SDK
        const float xxzz = x * x - z * z;
        const float wwyy = w * w - y * y;

        const float xw2 = x * w * 2.0f;
        const float xy2 = x * y * 2.0f;
        const float xz2 = x * z * 2.0f;
        const float yw2 = y * w * 2.0f;
        const float yz2 = y * z * 2.0f;
        const float zw2 = z * w * 2.0f;

        return vec3f((xxzz + wwyy) * v.x + (xy2 + zw2) * v.y + (xz2 - yw2) * v.z,
                     (xy2 - zw2) * v.x + (y * y + w * w - x * x - z * z) * v.y + (yz2 + xw2) * v.z,
                     (xz2 + yw2) * v.x + (yz2 - xw2) * v.y + (wwyy - xxzz) * v.z
        );
    }

    inline quatf& operator+=(const quatf& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        w += other.w;
        return *this;
    }
    inline quatf& operator-=(const quatf& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;
        return *this;
    }
    inline quatf& operator*=(const quatf& other) {
        *this = *this * other;
        return *this;
    }
    inline quatf& operator*=(const float f) {
        x *= f;
        y *= f;
        z *= f;
        w *= f;
        return *this;
    }
    inline quatf& operator/=(const float f) {
        x /= f;
        y /= f;
        z /= f;
        w /= f;
        return *this;
    }

    friend inline quatf operator*(const float f, const quatf& q) {
        return quatf(f * q.x, f * q.y, f * q.z, f * q.w);
    }
    friend inline vec3f operator*(const vec3f& v, const quatf& q) {
        return q * v;
    }

    inline float length() const {
        return Sqrt(quatf::dot(*this, *this));
    }

    static float dot(const quatf& a, const quatf& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }
    static quatf normalize(const quatf& q) {
        return q / q.length();
    }
    static quatf conjugate(const quatf& q) {
        return quatf(-q.x, -q.y, -q.z, q.w);
    }

    // ZYX order
    // https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Euler_Angles_to_Quaternion_Conversion
    static quatf fromEuler(const vec3f& euler) {
        const float hx = euler.x * 0.5f;
        const float hy = euler.y * 0.5f;
        const float hz = euler.z * 0.5f;

        const float sx = Sin(hx);
        const float sy = Sin(hy);
        const float sz = Sin(hz);
        const float cx = Cos(hx);
        const float cy = Cos(hy);
        const float cz = Cos(hz);

        return quatf(sx * cy * cz - cx * sy * sz,
                     cx * sy * cz + sx * cy * sz,
                     cx * cy * sz - sx * sy * cz,
                     cx * cy * cz + sx * sy * sz);
    }

    // stolen from Doom3 SDK
    static quatf slerp(const quatf& from, const quatf& to, const float t) {
        quatf temp;
        float cosom = from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w;
        if (cosom < 0.0f) {
            temp = -to;
            cosom = -cosom;
        } else {
            temp = to;
        }

        float scale0, scale1;
        if ((1.0f - cosom) > 1e-6f) {
            const float omega = ACos(cosom);
            const float sinom = 1.0f / Sin(omega);
            scale0 = Sin((1.0f - t) * omega) * sinom;
            scale1 = Sin(t * omega) * sinom;
        } else {
            scale0 = 1.0f - t;
            scale1 = t;
        }

        return (from * scale0) + (temp * scale1);
    }
};

// row-major
struct MYMATH_ALIGN_FOR_SSE mat4f {
    vec4f m[4];

    mat4f() = default;
    mat4f(const vec4f& row0, const vec4f& row1, const vec4f& row2, const vec4f& row3) : m{row0, row1, row2, row3} {}

    inline const vec4f& operator[](const size_t index) const {
        DebugAssert(index >= 0 && index <= 3);
        return m[index];
    }
    inline vec4f& operator[](const size_t index) {
        DebugAssert(index >= 0 && index <= 3);
        return m[index];
    }

    inline mat4f operator*(const float f) const {
        return mat4f(m[0] * f, m[1] * f, m[2] * f, m[3] * f);
    }
    inline mat4f operator+(const mat4f& other) const {
        return mat4f(m[0] + other.m[0], m[1] + other.m[1], m[2] + other.m[2], m[3] + other.m[3]);
    }
    inline mat4f operator-(const mat4f& other) const {
        return mat4f(m[0] - other.m[0], m[1] - other.m[1], m[2] - other.m[2], m[3] - other.m[3]);
    }
    inline mat4f operator*(const mat4f& other) const {
        mat4f result;
        result.m[0][0] = m[0][0] * other.m[0][0] + m[0][1] * other.m[1][0] + m[0][2] * other.m[2][0] + m[0][3] * other.m[3][0];
        result.m[0][1] = m[0][0] * other.m[0][1] + m[0][1] * other.m[1][1] + m[0][2] * other.m[2][1] + m[0][3] * other.m[3][1];
        result.m[0][2] = m[0][0] * other.m[0][2] + m[0][1] * other.m[1][2] + m[0][2] * other.m[2][2] + m[0][3] * other.m[3][2];
        result.m[0][3] = m[0][0] * other.m[0][3] + m[0][1] * other.m[1][3] + m[0][2] * other.m[2][3] + m[0][3] * other.m[3][3];
        result.m[1][0] = m[1][0] * other.m[0][0] + m[1][1] * other.m[1][0] + m[1][2] * other.m[2][0] + m[1][3] * other.m[3][0];
        result.m[1][1] = m[1][0] * other.m[0][1] + m[1][1] * other.m[1][1] + m[1][2] * other.m[2][1] + m[1][3] * other.m[3][1];
        result.m[1][2] = m[1][0] * other.m[0][2] + m[1][1] * other.m[1][2] + m[1][2] * other.m[2][2] + m[1][3] * other.m[3][2];
        result.m[1][3] = m[1][0] * other.m[0][3] + m[1][1] * other.m[1][3] + m[1][2] * other.m[2][3] + m[1][3] * other.m[3][3];
        result.m[2][0] = m[2][0] * other.m[0][0] + m[2][1] * other.m[1][0] + m[2][2] * other.m[2][0] + m[2][3] * other.m[3][0];
        result.m[2][1] = m[2][0] * other.m[0][1] + m[2][1] * other.m[1][1] + m[2][2] * other.m[2][1] + m[2][3] * other.m[3][1];
        result.m[2][2] = m[2][0] * other.m[0][2] + m[2][1] * other.m[1][2] + m[2][2] * other.m[2][2] + m[2][3] * other.m[3][2];
        result.m[2][3] = m[2][0] * other.m[0][3] + m[2][1] * other.m[1][3] + m[2][2] * other.m[2][3] + m[2][3] * other.m[3][3];
        result.m[3][0] = m[3][0] * other.m[0][0] + m[3][1] * other.m[1][0] + m[3][2] * other.m[2][0] + m[3][3] * other.m[3][0];
        result.m[3][1] = m[3][0] * other.m[0][1] + m[3][1] * other.m[1][1] + m[3][2] * other.m[2][1] + m[3][3] * other.m[3][1];
        result.m[3][2] = m[3][0] * other.m[0][2] + m[3][1] * other.m[1][2] + m[3][2] * other.m[2][2] + m[3][3] * other.m[3][2];
        result.m[3][3] = m[3][0] * other.m[0][3] + m[3][1] * other.m[1][3] + m[3][2] * other.m[2][3] + m[3][3] * other.m[3][3];
        return result;
    }
    inline vec4f operator*(const vec4f& v) const {
        return vec4f(vec4f::dot(v, m[0]),
                     vec4f::dot(v, m[1]),
                     vec4f::dot(v, m[2]),
                     vec4f::dot(v, m[3]));
    }

    inline mat4f& operator*=(const float f) {
        m[0] *= f;
        m[1] *= f;
        m[2] *= f;
        m[3] *= f;
        return *this;
    }
    inline mat4f& operator+(const mat4f& other) {
        m[0] += other.m[0];
        m[1] += other.m[1];
        m[2] += other.m[2];
        m[3] += other.m[3];
        return *this;
    }
    inline mat4f& operator-(const mat4f& other) {
        m[0] -= other.m[0];
        m[1] -= other.m[1];
        m[2] -= other.m[2];
        m[3] -= other.m[3];
        return *this;
    }
    inline mat4f& operator*=(const mat4f& other) {
        *this = *this * other;
        return *this;
    }

    inline vec3f transformPos(const vec3f& pos) const {
        vec4f v4(pos.x, pos.y, pos.z, 1.0f);
        return vec3f(vec4f::dot(v4, m[0]),
                     vec4f::dot(v4, m[1]),
                     vec4f::dot(v4, m[2]));
    }

    inline vec3f transformDir(const vec3f& pos) const {
        vec4f v4(pos.x, pos.y, pos.z, 0.0f);
        return vec3f(vec4f::dot(v4, m[0]),
                     vec4f::dot(v4, m[1]),
                     vec4f::dot(v4, m[2]));
    }


    static mat4f transpose(const mat4f& mat) {
        return mat4f(vec4f(mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0]),
                     vec4f(mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1]),
                     vec4f(mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2]),
                     vec4f(mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]));
    }

    // https://datasheet.datasheetarchive.com/originals/distributors/Datasheets-14/DSA-277306.pdf
    // I'm trusting Intel that this is a fast way to do this ;)
    static mat4f inverse(const mat4f& matToInvert) {
        float tmp[12];  // temp array for pairs
        float src[16];  // array of transpose source matrix

        mat4f result;
        float* dst = reinterpret_cast<float*>(&result);

        // transpose matrix
        const float* mat = reinterpret_cast<const float*>(&matToInvert);
        for (int i = 0; i < 4; ++i) {
            src[i +  0] = mat[i * 4 + 0];
            src[i +  4] = mat[i * 4 + 1];
            src[i +  8] = mat[i * 4 + 2];
            src[i + 12] = mat[i * 4 + 3];
        }
        // calculate pairs for first 8 elements (cofactors)
        tmp[ 0] = src[10] * src[15];
        tmp[ 1] = src[11] * src[14];
        tmp[ 2] = src[ 9] * src[15];
        tmp[ 3] = src[11] * src[13];
        tmp[ 4] = src[ 9] * src[14];
        tmp[ 5] = src[10] * src[13];
        tmp[ 6] = src[ 8] * src[15];
        tmp[ 7] = src[11] * src[12];
        tmp[ 8] = src[ 8] * src[14];
        tmp[ 9] = src[10] * src[12];
        tmp[10] = src[ 8] * src[13];
        tmp[11] = src[ 9] * src[12];

        // calculate first 8 elements (cofactors)
        dst[0]  = tmp[0] * src[5] + tmp[3] * src[6] + tmp[ 4] * src[7];
        dst[0] -= tmp[1] * src[5] + tmp[2] * src[6] + tmp[ 5] * src[7];
        dst[1]  = tmp[1] * src[4] + tmp[6] * src[6] + tmp[ 9] * src[7];
        dst[1] -= tmp[0] * src[4] + tmp[7] * src[6] + tmp[ 8] * src[7];
        dst[2]  = tmp[2] * src[4] + tmp[7] * src[5] + tmp[10] * src[7];
        dst[2] -= tmp[3] * src[4] + tmp[6] * src[5] + tmp[11] * src[7];
        dst[3]  = tmp[5] * src[4] + tmp[8] * src[5] + tmp[11] * src[6];
        dst[3] -= tmp[4] * src[4] + tmp[9] * src[5] + tmp[10] * src[6];
        dst[4]  = tmp[1] * src[1] + tmp[2] * src[2] + tmp[ 5] * src[3];
        dst[4] -= tmp[0] * src[1] + tmp[3] * src[2] + tmp[ 4] * src[3];
        dst[5]  = tmp[0] * src[0] + tmp[7] * src[2] + tmp[ 8] * src[3];
        dst[5] -= tmp[1] * src[0] + tmp[6] * src[2] + tmp[ 9] * src[3];
        dst[6]  = tmp[3] * src[0] + tmp[6] * src[1] + tmp[11] * src[3];
        dst[6] -= tmp[2] * src[0] + tmp[7] * src[1] + tmp[10] * src[3];
        dst[7]  = tmp[4] * src[0] + tmp[9] * src[1] + tmp[10] * src[2];
        dst[7] -= tmp[5] * src[0] + tmp[8] * src[1] + tmp[11] * src[2];

        // calculate pairs for second 8 elements (cofactors)
        tmp[ 0] = src[2] * src[7];
        tmp[ 1] = src[3] * src[6];
        tmp[ 2] = src[1] * src[7];
        tmp[ 3] = src[3] * src[5];
        tmp[ 4] = src[1] * src[6];
        tmp[ 5] = src[2] * src[5];
        tmp[ 6] = src[0] * src[7];
        tmp[ 7] = src[3] * src[4];
        tmp[ 8] = src[0] * src[6];
        tmp[ 9] = src[2] * src[4];
        tmp[10] = src[0] * src[5];
        tmp[11] = src[1] * src[4];

        // calculate second 8 elements (cofactors)
        dst[ 8]  = tmp[ 0] * src[13] + tmp[ 3] * src[14] + tmp[ 4] * src[15];
        dst[ 8] -= tmp[ 1] * src[13] + tmp[ 2] * src[14] + tmp[ 5] * src[15];
        dst[ 9]  = tmp[ 1] * src[12] + tmp[ 6] * src[14] + tmp[ 9] * src[15];
        dst[ 9] -= tmp[ 0] * src[12] + tmp[ 7] * src[14] + tmp[ 8] * src[15];
        dst[10]  = tmp[ 2] * src[12] + tmp[ 7] * src[13] + tmp[10] * src[15];
        dst[10] -= tmp[ 3] * src[12] + tmp[ 6] * src[13] + tmp[11] * src[15];
        dst[11]  = tmp[ 5] * src[12] + tmp[ 8] * src[13] + tmp[11] * src[14];
        dst[11] -= tmp[ 4] * src[12] + tmp[ 9] * src[13] + tmp[10] * src[14];
        dst[12]  = tmp[ 2] * src[10] + tmp[ 5] * src[11] + tmp[ 1] * src[ 9];
        dst[12] -= tmp[ 4] * src[11] + tmp[ 0] * src[ 9] + tmp[ 3] * src[10];
        dst[13]  = tmp[ 8] * src[11] + tmp[ 0] * src[ 8] + tmp[ 7] * src[10];
        dst[13] -= tmp[ 6] * src[10] + tmp[ 9] * src[11] + tmp[ 1] * src[ 8];
        dst[14]  = tmp[ 6] * src[ 9] + tmp[11] * src[11] + tmp[ 3] * src[ 8];
        dst[14] -= tmp[10] * src[11] + tmp[ 2] * src[ 8] + tmp[ 7] * src[ 9];
        dst[15]  = tmp[10] * src[10] + tmp[ 4] * src[ 8] + tmp[ 9] * src[ 9];
        dst[15] -= tmp[ 8] * src[ 9] + tmp[11] * src[10] + tmp[ 5] * src[ 8];

        // calculate determinant
        float det = src[0] * dst[0] + src[1] * dst[1] + src[2] * dst[2] + src[3] * dst[3];
        // calculate matrix inverse
        det = 1.0f / det;
        for (int i = 0; i < 16; ++i) {
            dst[i] *= det;
        }

        return result;
    }

    static mat4f fromQuatAndPos(const quatf& q, const vec3f& p) {
        const float x2 = q.x + q.x;
        const float y2 = q.y + q.y;
        const float z2 = q.z + q.z;
        const float xx = q.x * x2;
        const float xy = q.x * y2;
        const float xz = q.x * z2;
        const float yy = q.y * y2;
        const float yz = q.y * z2;
        const float zz = q.z * z2;
        const float wx = q.w * x2;
        const float wy = q.w * y2;
        const float wz = q.w * z2;

        return mat4f(vec4f(1.0f - (yy + zz),          xy - wz,          xz + wy,   p.x),
                     vec4f(         xy + wz, 1.0f - (xx + zz),          yz - wx,   p.y),
                     vec4f(         xz - wy,          yz + wx, 1.0f - (xx + yy),   p.z),
                     vec4f(            0.0f,             0.0f,             0.0f,  1.0f));
    }

    static mat4f fromQuat(const quatf& q) {
        return mat4f::fromQuatAndPos(q, vec3f(0.0f, 0.0f, 0.0f));
    }
};


struct AABBox {
    vec3f minimum, maximum;

    AABBox() : minimum{}, maximum{} {}
    AABBox(const AABBox& other) : minimum(other.minimum), maximum(other.maximum) {}
    AABBox(const vec3f& _minimum, const vec3f& _maximum) : minimum(_minimum), maximum(_maximum) {}

    inline bool Valid() const {
        return this->Extent().length() > MM_Epsilon;
    }

    inline void Reset(const bool makeEmpty = false) {
        minimum = makeEmpty ? vec3f(0.0f, 0.0f, 0.0f) : vec3f(FLT_MAX, FLT_MAX, FLT_MAX);
        maximum = makeEmpty ? vec3f(0.0f, 0.0f, 0.0f) : vec3f(FLT_MIN, FLT_MIN, FLT_MIN);
    }

    inline void Absorb(const vec3f& p) {
        minimum.x = std::min(minimum.x, p.x);
        minimum.y = std::min(minimum.y, p.y);
        minimum.z = std::min(minimum.z, p.z);

        maximum.x = std::max(maximum.x, p.x);
        maximum.y = std::max(maximum.y, p.y);
        maximum.z = std::max(maximum.z, p.z);
    }

    inline void Absorb(const AABBox& other) {
        this->Absorb(other.minimum);
        this->Absorb(other.maximum);
    }

    inline vec3f Center() const {
        return (minimum + maximum) * 0.5f;
    }

    inline vec3f Extent() const {
        return (maximum - minimum) * 0.5f;
    }

    inline float MaximumValue() const {
        return std::max(Max3(std::fabsf(minimum.x), std::fabsf(minimum.y), std::fabsf(minimum.z)),
                        Max3(std::fabsf(maximum.x), std::fabsf(maximum.y), std::fabsf(maximum.z)));
    }
};

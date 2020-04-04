struct Vec3f
{
    float x, y, z;
    Vec3f() : x(0), y(0), z(0) {};
    Vec3f(const float x, const float y,const  float z) : x(x), y(y), z(z) {
    }
    Vec3f(const Vec3f &v) : x(v.x), y(v.y), z(v.z) {}
    Vec3f operator + (const Vec3f &v) const
    {
        return Vec3f(x + v.x, y + v.y, z + v.z);
    }
    Vec3f operator - (const Vec3f &v) const
    {
        return Vec3f(x - v.x, y - v.y, z- v.z);
    }
    Vec3f operator * (const float &r) const
    {
        return Vec3f(r * x, r * y, r * z);
    }
    Vec3f & operator +=(const Vec3f &v)
    {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }
    Vec3f & operator -=(const Vec3f &v)
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }
    Vec3f operator -() const
    {
        return Vec3f(-x, -y, -z);
    }
    friend float dot(const Vec3f &a, const Vec3f &b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    friend Vec3f cross(const Vec3f &a, const Vec3f &b)
    {
        return Vec3f(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
    }
    friend Vec3f operator * (const float a, const Vec3f &v) 
    {
        return Vec3f(a * v.x, a * v.y, a* v.z);
    }

    Vec3f &  normalize()
    {
        float t = sqrtf(x * x + y * y + z* z);
        if (t){
            float tt = 1 / t;
            x *= tt;
            y *= tt;
            z *= tt;
        }
        return *this;
    }

}; 

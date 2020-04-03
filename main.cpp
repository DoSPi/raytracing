#include <iostream>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include<cmath>
#include <algorithm>
#include "Bitmap.h"
#include "vector.h"
#include <omp.h>
Vec3f reflect(const Vec3f v,const Vec3f normal)
{
    return Vec3f(v - normal *2 * dot(v, normal) );
}
inline float limit(const float lo, const float hi, const float value)
{
    return std::max(std::min(hi,value),lo);
}
Vec3f refract(const Vec3f &v, Vec3f  normal, float n2)
{
    float n1 = 1;
    float cosa = limit(-1.f, 1.f, dot(v,normal));
    if (cosa < 0){
        cosa = -cosa;
    } else { // inside
        normal = -normal;
        std::swap(n1, n2);    
    }
    float n = n1/ n2;
    float cosb2 =  1 - n * n *(1 - cosa * cosa);
    if (cosb2 < 0){ // total reflection
        return Vec3f(0,0,0);
    }
    return v * n + normal * (n * cosa - sqrtf(cosb2));
}  
struct Light
{
    Vec3f position;
    float intensity;
    Light(const Vec3f &pos, const float &i) : position(pos), intensity(i) {}
};
struct Material
{
    float n; // refractive_index
    Vec3f diffuse_color;
    float specular_exponent;
    float k_diffuse;
    float k_specular;
    float k_reflection;
    float k_refraction;
    Material() {}
    Material(const float n, const Vec3f &v, const float s, const float kd, const float ks,
            const float k_refl, const float k_refr) : n(n), diffuse_color(v) ,
            specular_exponent(s), k_diffuse(kd), 
            k_specular(ks), k_reflection(k_refl), k_refraction(k_refr) {}

};
struct Object
{
    Material material;
    virtual ~Object() {}
    virtual bool intersect(const Vec3f &orig, const Vec3f &dir, float &alpha) const = 0;
    virtual Vec3f get_normal( const Vec3f &pos) const = 0;
    virtual Vec3f get_color(const Vec3f &pos) const = 0;

};
struct Sphere : Object
{
    Vec3f center;
    float radius;
    Sphere(const Vec3f &pos, const float &r, const Material &m) : center(pos), radius(r)
    {
        material = m;
    }
    bool intersect(const Vec3f &orig, const Vec3f &dir, float &alpha) const
    {
        // quadratic equation from the cosine theorem
        Vec3f t = orig - center; 
        float a = dot(dir, dir);
        float b = 2 * dot(dir, t);
        float c = dot(t,t) - radius * radius;
        float d = b * b - 4 * a * c;
        if (d < 0){
            return false;
        }
        d = sqrtf(d);
        float x1 = -0.5 * (b + d) / a;
        float x2 = -0.5 * (b - d) / a;
        if (x1 >= 0 && x1 <= x2){ // return smaller  non-negative root 
            alpha = x1;
            return true;
        }
        if (x2 >= 0){
            alpha = x2;
            return true;
        }
        return false;
    }
    Vec3f get_normal(const Vec3f &v) const
    {
        return (v - center).normalize();
    }
    Vec3f get_color(const Vec3f &pos) const
    {
        return material.diffuse_color;
    }

};
struct Plane : Object
{
    Vec3f position;
    Vec3f normal;
    Plane (const Vec3f &orig, const Vec3f &n, const Material &m) : position(orig), normal(n)
    {
        material = m;
        normal.normalize();
    }
    bool intersect(const Vec3f &orig, const Vec3f &dir, float &alpha) const
    {
        float d = dot(dir, normal);
        if (d == 0){
            return false;
        }
        float a = dot(position - orig, normal);
        if (a == 0){ // when ray orig in the plane 
            return false;
        }
        alpha = a/ d;
        if (alpha <= 0){
            return false;
        }
        return true;
    }
    Vec3f get_normal(const Vec3f &v) const
    {
        if (dot(v, normal) < 0){
            return normal;
        }
        return -normal;
    }
    Vec3f get_color(const Vec3f &pos) const
    {
        Vec3f r =  pos - position;
        Vec3f normal = get_normal(pos);
        int a1 = r.x / 2 + 1000; //hide simmetric part
        int a2 = r.z / 2;
        if ((a1 + a2) % 2 == 0){
            return Vec3f(0.7, 0.7, 0.7);
        }
        return Vec3f(0.05, 0.05, 0.05);

    }
};
/*
struct Triangle : Object
{
    Vec3f  vert[3];
    Triangle() {}
    Triangle(const Vec3f &x1, const Vec3f &x2, const Vec3f &x3)
    {
        vert[0] = x1;
        vert[1] = x2;
        vert[2] = x3;
    }
    bool intersect(const Ray &ray, float &alpha) const
    {
        Vec3f e1 = vert[1] - vert[0];
        Vec3f e2 = vert[2] - vert[0];
        Vec3f pvec = ray.dir.vector_product(e2);
        float det = e1.scalar_product(pvec);
        if (det == 0 || det < 0){
            return false;
        }
        Vec3f tvec = ray.pos - vert[0];
        float u = tvec.scalar_product(pvec);
        if (u < 0 || u > det){
            return false;
        }
        Vec3f qvec = tvec.vector_product(e1);
        float v = ray.dir.scalar_product(qvec);
        if (v < 0 || u + v > det){
            return false;
        }
        alpha = e2.scalar_product(qvec) / det;
        return true;
    }
    
    Vec3f get_normal(const Vec3f &v) const
    {
        Vec3f a = vert[1] - vert[0];
        Vec3f b = vert[2] - vert[0];
        return a.vector_product(b).normalize();
    } 

}; */

namespace Options
{
    constexpr float fov =  M_PI / 2;
    constexpr uint32_t width = 800;
    constexpr uint32_t height = 512;
    constexpr uint32_t max_depth = 4;
    constexpr float max_distance = 10e9;
    constexpr float offset = 0.01;
    const Vec3f background(0.08, 0.08, 0.08);
    const Vec3f camera(0,0,0);
};
void scene_intersect(const Vec3f &orig, const Vec3f &dir, std::vector<Object *> object, 
        Object **first, Vec3f &hit)
{
    *first = nullptr;
    float min_dist = Options::max_distance;
    for (uint32_t i = 0; i < object.size(); i++){
        float dist;
        if (object[i]->intersect(orig, dir, dist) && dist < min_dist){
            min_dist = dist;
            *first = object[i]; 
        }
    }
    if (*first){
        hit =orig + dir * min_dist;
    }

}
Vec3f cast(const Vec3f &orig, const Vec3f &dir, const std::vector<Object *> &object,
        const std::vector<Light*> &light, unsigned depth)
{
    if (depth > Options::max_depth){
        return Options::background;
    }
    Object *first = nullptr;
    Vec3f hit;
    scene_intersect(orig, dir, object, &first, hit);
    if (first  == nullptr){
        return Options::background;
    }
   // return Vec3f(0.2, 0.5, 0.1);
    Vec3f normal = first->get_normal(hit);
    Vec3f reflect_dir =reflect(dir, normal).normalize();
    Vec3f reflect_orig = dot(reflect_dir, normal) < 0 ? hit - Options::offset * normal : 
            hit + Options::offset * normal;
    Vec3f reflect_color =cast(reflect_orig, reflect_dir, object, light, depth + 1);
    Vec3f refract_dir = refract(dir, normal, first->material.n).normalize();
    Vec3f refract_orig = dot(refract_dir, normal) < 0 ? hit - Options::offset * normal :
            hit + Options::offset * normal;
    Vec3f refract_color = cast(refract_orig, refract_dir, object, light, depth + 1);
    //Phong illumination model
    float light_diffuse =0;
    float light_specular = 0;
    for (uint32_t i = 0; i < light.size(); i++){
        Vec3f light_dir = light[i]->position - hit;
        float light_distance2 = dot(light_dir, light_dir);
        light_dir.normalize();
        Vec3f shadow_orig = dot(light_dir, normal) < 0 ? hit - Options:: offset * normal :
                hit + Options::offset * normal;
        Object *first_tmp = nullptr;
        Vec3f hit_tmp;
        scene_intersect(shadow_orig, light_dir, object, &first_tmp, hit_tmp);
        Vec3f dist = hit_tmp - shadow_orig;
        if (first_tmp != nullptr && (dot(dist,dist) < light_distance2)) { //shadow skip
            continue;
        }
        light_diffuse += light[i]->intensity * std::max(0.f, dot(light_dir, normal));
        Vec3f light_reflect = reflect(-light_dir, normal);
        light_specular += light[i]->intensity * 
                std::pow(std::max(0.f, -dot(light_reflect, dir)), first->material.specular_exponent);
    }
    light_diffuse *= first->material.k_diffuse;
    light_specular *= first->material.k_specular;
    return first->get_color(hit) * light_diffuse + Vec3f(1, 1, 1) * light_specular +
            first->material.k_reflection * reflect_color +
            first->material.k_refraction * refract_color;
}
Vec3f  *render(const std::vector<Object *> &object,
        const std::vector<Light *> &light)
{
    Vec3f *buf = new Vec3f[Options::width * Options::height];
    const float z = -1 / (2.0 * tan(0.5 * Options::fov)) * Options::height;
        #pragma omp parallel for
        for (uint32_t i = 0; i < Options::height; i++){
            for (uint32_t j = 0 ; j < Options::width; j++){
                float x = (j + 0.5) -  Options::width / 2.0;
                float y = -(i + 0.5) + Options::height / 2.0;
                buf[Options::width * i + j] = cast(Options::camera,
                        Vec3f(x, y, z).normalize(),
                        object, light, 0);
            }
        }
    return buf;


}
uint32_t *convert(const Vec3f *buf)
{
    struct Pixel { unsigned char r, g, b; } px;
    uint32_t *ans = new uint32_t[Options::width * Options::height];
    for (uint64_t i = 0; i < Options::height * Options::width; i++){
        ans[i] = 0;
        ans[i] |= (unsigned char)(limit(0,1,buf[i].x) * 255);
        ans[i] |= (unsigned char)(limit(0,1,buf[i].y) * 255) << 8;
        ans[i] |= (unsigned char)(limit(0, 1, buf[i].z) * 255) << 16;
    }
    return ans;
}
int main(int argc, const char** argv)
{
    std::unordered_map<std::string, std::string> cmdLineParams;
    for(int i=0; i<argc; i++)
    {
        std::string key(argv[i]);
        if(key.size() > 0 && key[0]=='-')
        {
        if(i != argc-1) // not last argument
        {
            cmdLineParams[key] = argv[i+1];
            i++;
        }
        else
            cmdLineParams[key] = "";
        }
    }
    std::string outFilePath = "zout.bmp";
    if(cmdLineParams.find("-out") != cmdLineParams.end())
        outFilePath = cmdLineParams["-out"];
    int sceneId = 0;
    if(cmdLineParams.find("-scene") != cmdLineParams.end()){
        sceneId = atoi(cmdLineParams["-scene"].c_str());
    }
    if (sceneId != 1){
        return 0;
    }
        if(cmdLineParams.find("-threads") != cmdLineParams.end()){
            uint32_t threads =atoi(cmdLineParams["-threads"].c_str());
            if (threads == 0){
                threads = 1;
            }
            omp_set_num_threads(threads);
        }

    std::vector<Object *> object;
    std::vector<Light *> light;
    // n, diff_color , specular, kd, ks, refl. refr
    Material glass(1.5, Vec3f(0.6, 0.7, 0.8), 100 ,0.1, 0.4, 0.2, 0.8);
    Material mirror(1.5, Vec3f(1.0, 1.0, 1.0), 500, 0, 2 , 0.9, 0);
    Material plastic(1.5, Vec3f(0.7,0,0), 125, 0.8, 0.2, 0.1, 0);
    Material reflective(1.5, Vec3f(0.7,0,0), 125, 0.9, 0.2, 0.8, 0);
    Vec3f *buf = nullptr;
    uint32_t *bmp = nullptr;
    if(sceneId == 1){
        object.push_back(new Sphere(Vec3f(-9, 0.5,-10),3, glass));
        object.push_back(new Sphere(Vec3f(0,0.5,-10),3, plastic));
        object.push_back(new Sphere(Vec3f(0, 3,-5),1, plastic));
        object.push_back(new Sphere(Vec3f(9 , 0.5, -10),3, mirror));
        object.push_back(new Plane(Vec3f(0,4,0),Vec3f(0, -1, 0), reflective));
        light.push_back(new Light(Vec3f(-2, -2, 5), 0.5));
        light.push_back(new Light(Vec3f(3, -10, -5), 0.5));
        light.push_back(new Light(Vec3f(10, -50, -28), 0.5));
        buf = render(object, light);
    }
    if (sceneId == 2){

    }
    bmp = convert(buf);
    SaveBMP(outFilePath.c_str(), bmp, Options::width, Options::height);
    delete[] buf;
    delete[] bmp;
    for (uint32_t i = 0; i < object.size(); i++){
        delete object[i];
    }
    for (uint i = 0; i < light.size(); i++){
        delete light[i];
    }
    std::cout << "end." << std::endl;
    return 0;
}
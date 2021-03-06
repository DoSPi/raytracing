#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include<cmath>
#include <algorithm>
#include "vector.h"
#include <omp.h>
#include <cstdio>
#include <iostream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image_write.h"
#include "stb_image.h"
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
    Vec3f diffuse_color2; // for plane
    Material() {}
    Material(const float n, const Vec3f &v, const float s, const float kd, const float ks,
            const float k_refl, const float k_refr, const Vec3f &v2 = Vec3f(0,0,0)) :
            n(n), diffuse_color(v) , specular_exponent(s), k_diffuse(kd), 
            k_specular(ks), k_reflection(k_refl), k_refraction(k_refr), diffuse_color2(v2) {}

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
        int a1 = r.x / 2 + 1000; //hide simmetric part
        int a2 = r.z / 2;
        if ((a1 + a2) % 2 == 0){
            return material.diffuse_color;
        }
        return material.diffuse_color2;

    }
};
struct Circle : Object
{
    Vec3f position;
    Vec3f normal;
    float radius;
    Circle( const Vec3f &position,  Vec3f n, const float radius, const Material &m) :
            position(position), normal(n), radius(radius)
    {
        n.normalize();
        material = m;
    }
    Vec3f get_normal(const Vec3f &v) const
    {
        if (dot(v, normal) < 0){
            return normal;
        }
        return -normal;
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
        Vec3f hit = orig + alpha * dir;
        Vec3f r = hit - position;
        float norm2 = r.x * r.x + r.y * r.y + r.z * r.z;
        if (norm2 > radius * radius){
            return false;
        }
        return true;
    }
    Vec3f get_color(const Vec3f &pos) const
    {
        return material.diffuse_color;
    }
};

struct Triangle : Object
{
    Vec3f  vert[3];
    Triangle(const Vec3f &x1, const Vec3f &x2, const Vec3f &x3, const Material &m)
    {
        vert[0] = x1;
        vert[1] = x2;
        vert[2] = x3;
        material = m;
    }
    bool intersect(const Vec3f &orig, const Vec3f &dir, float &alpha) const
    {
        Vec3f e1 = vert[1] - vert[0];
        Vec3f e2 = vert[2] - vert[0];
        Vec3f t = orig -  vert[0];
        Vec3f p = cross(dir, e2);
        Vec3f q = cross(t, e1);
        float det = dot(p, e1);
        if (det ==0 ){
            return false;
        }
        float u = dot(p, t) / det;
        if (u < 0 ){
            return false;
        }
        float v = dot(q, dir) / det;
        if (v < 0){
            return false;
        }
        if (u + v > 1){
            return false;
        }
        alpha = dot(q, e2) / det;
        if (alpha < 0 ){
            return false;
        }
        return true;

    }
    Vec3f get_normal(const Vec3f &v) const
    {
        Vec3f a = vert[1] - vert[0];
        Vec3f b = vert[2] - vert[0];
        Vec3f normal = cross(a,b).normalize();
        if (dot(normal, v) > 0){
            normal = - normal;
        }
        return normal;
    }
    Vec3f get_color(const Vec3f &pos) const
    {
        return material.diffuse_color;
    }

}; 
void read_obj(const char *s, std::vector<Object*> &object, const Vec3f &offset, const Material &m)
{   
    char type;
    std::vector<Vec3f> v;
    std::vector<Triangle> triangles;
    FILE *f = fopen(s, "r");
    char buf[100];
    while (fgets( buf, 98, f)){
        if (buf[0]== 'v'){
            float x, y ,z;
            if ( sscanf(buf,"%c %f %f %f\n",&type, &x, &y ,&z) != 4){
                continue;
            }
            v.push_back(Vec3f(x + offset.x , y + offset.y, z+ offset.z));
        }
        if (buf[0] == 'f'){
            uint32_t v1, t1, n1;
            uint32_t v2, t2, n2;
            uint32_t v3, t3, n3;
            if ( sscanf(buf,"%c %u/%u/%u %u/%u/%u %u/%u/%u\n",&type, &v1, &t1, &n1, &v2, &t2, &n2, 
                    &v3, &t3, &n3) != 10){
                continue;
            }
            if (v1 > v.size() || v2 > v.size() || v3 > v.size()){
                continue;
            }
            object.push_back( new Triangle(v[v1 -1], v[v2 -1], v[v3 - 1], m));
        }
    }
    fclose(f);
}
namespace Options
{
    constexpr float fov =  M_PI / 2;
    constexpr uint32_t width = 800;
    constexpr uint32_t height = 512;
    constexpr uint32_t max_depth = 5;
    constexpr float max_distance = 10e9;
    constexpr float offset = 0.01;
    uint32_t scene_id;
    Vec3f background(0.1, 0.1, 0.1);
    const Vec3f camera(0,0,0);
    unsigned char *image;
    int  image_width;
    int image_height;
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
        if (Options::scene_id == 2){
                int x =  (1.0 + dir.x) * Options::image_width / 2.0;
                int y = ( 1.0 - dir.y) * Options::image_height /2.0 ;
                unsigned char *p = Options::image;
                size_t index = x + Options::image_width * y;
                return Vec3f(p[3 * index] / 255.0, p[3 * index + 1] / 255.0, p[3 * index + 2] / 255.0);
        }
        return Options::background;
    }
    //return Vec3f(0.2, 0.5, 0.1);
    Vec3f normal = first->get_normal(hit);
    Vec3f reflect_dir =reflect(dir, normal).normalize();
    Vec3f reflect_orig = dot(reflect_dir, normal) < 0 ? hit - Options::offset * normal : 
            hit + Options::offset * normal;
    Vec3f reflect_color = first->material.k_reflection ?
            cast(reflect_orig, reflect_dir, object, light, depth + 1) : Vec3f(0,0,0);
    Vec3f refract_dir = first->material.k_refraction ?
            refract(dir, normal, first->material.n).normalize() : Vec3f(0,0,0);
    Vec3f refract_orig = dot(refract_dir, normal) < 0 ? hit - Options::offset * normal :
            hit + Options::offset * normal;
    Vec3f refract_color = cast(refract_orig, refract_dir, object, light, depth + 1);
    //Phong illumination model, no ambient
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
Vec3f  *render(const std::vector<Object *> &object, const std::vector<Light *> &light)
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
Vec3f  *render_4(const std::vector<Object *> &object, const std::vector<Light *> &light)
{
    Vec3f *buf = new Vec3f[Options::width * Options::height];
    const float z = -1 / (2.0 * tan(0.5 * Options::fov)) * Options::height;
    #pragma omp parallel for
    for (uint32_t i = 0; i < Options::height; i++) {
        for (uint32_t j = 0 ; j < Options::width; j++) {
            for (uint32_t a = 0; a < 2; a++) {
                for (uint32_t b = 0; b < 2; b++) {
                    float x = (j + 0.25 + 0.5 *a ) -  Options::width / 2.0;
                    float y = -(i + 0.25 + 0.5 *b) + Options::height / 2.0;
                    buf[Options::width * i + j] += cast(Options::camera,
                            Vec3f(x, y, z).normalize(),
                            object, light, 0) * 0.25;
                }
            }
        }
    }
    return buf;
}
Vec3f* tone_mapping(Vec3f *buf, size_t size)
{
    for (size_t i = 0; i < size; i++){
        buf[i].x = buf[i].x/ (buf[i].x + 1);
        buf[i].y = buf[i].y/ (buf[i].y + 1);
        buf[i].z = buf[i].z/ (buf[i].z + 1);

    }
    return buf;
}
uint8_t *convert(const Vec3f *buf)
{
    uint8_t *ans = new uint8_t[3 * Options::width * Options::height];
    for (uint64_t i = 0; i < Options::height * Options::width; i++){
        ans[3 *i] = (uint8_t)(limit(0,1,buf[i].x) * 255);
        ans[3 * i + 1] = (uint8_t)(limit(0,1,buf[i].y) * 255);
        ans[ 3 *i + 2] = (uint8_t)(limit(0, 1, buf[i].z) * 255);
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
    if (sceneId == 3){
        return 0;
    }
    if(cmdLineParams.find("-threads") != cmdLineParams.end()){
        uint32_t threads =atoi(cmdLineParams["-threads"].c_str());
        if (threads == 0){
            threads = 1;
        }
        omp_set_num_threads(threads);
    }
    Options::scene_id = sceneId;
    std::vector<Object *> object;
    std::vector<Light *> light;
    // n, diff_color , specular, kd, ks, refl. refr
    Material refract(1.5, Vec3f(0.6, 0.7, 0.8), 100 ,0.05, 0.4, 0.2, 0.8);
    Material mirror(1.5, Vec3f(1.0, 1.0, 1.0), 500, 0, 2 , 0.9, 0);
    Material plastic(1.5, Vec3f(0.7,0,0), 125, 0.8, 0.2, 0.01, 0);
    Material reflective(1.5, Vec3f(0.7, 0.7, 0.7), 125, 0.9, 0.2, 0.8, 0, Vec3f(0.05, 0.05, 0.05));
    Material marble(1.5, Vec3f(0.5,0.6,0.6), 10, 0.9, 0.2, 0, 0); // no refraction for mesh (no  true normal vectors)
    Material reflective2(1.5, Vec3f(0.8886,0.153,0.7882), 125, 0.9, 0.2, 0.2, 0, Vec3f(0.05,0.05,0.05));
    Vec3f *buf = nullptr;
    uint8_t *bytes = nullptr;
    if(sceneId == 1){
        object.push_back(new Sphere(Vec3f(-9, -0.5,-10),3, refract));
        object.push_back(new Sphere(Vec3f(0,-0.5,-10),3, plastic));
        object.push_back(new Sphere(Vec3f(0, -3,-5),1, plastic));
        object.push_back(new Sphere(Vec3f(9 , -0.5, -10),3, mirror));
        object.push_back(new Plane(Vec3f(0,-4,0),Vec3f(0, 1, 0), reflective));
        light.push_back(new Light(Vec3f(-2, 2, 5), 2.5));
        light.push_back(new Light(Vec3f(3, 10, -5), 3.5));
        light.push_back(new Light(Vec3f(10, 50, -28), 2.5));
        light.push_back(new Light(Vec3f(-8, -0.5, -10), 2.5));
        buf = render(object, light);
        tone_mapping(buf, Options::width * Options::height);
    }
    if (sceneId == 2){
        Options::background = Vec3f(0.0, 1.0, 1.0);
        int n;
        Options::image = stbi_load("envmap.jpg", &Options::image_width, &Options::image_height, &n, 3);
        if (Options::image == NULL){
            throw 1;
        };
        Vec3f offset(0, -3 , -26);
        read_obj("heliosbust.obj", object, offset, marble);
        //object.resize(100);
        object.push_back(new Sphere(Vec3f(6 , -0.5, -10),3, glass));
        object.push_back(new Sphere(Vec3f(-6 , -0.5, -10),3, marble));
        std::cout <<"Objects:"<<object.size() <<std::endl << std::flush;
        light.push_back(new Light(Vec3f(0, 2, 1), 0.5));
        light.push_back(new Light(Vec3f(-2, 2, 5), 0.5));
        light.push_back(new Light(Vec3f(3, 10, -5), 0.5));
        light.push_back(new Light(Vec3f(10, 50, -28), 0.5));
         object.push_back(new  Circle(Vec3f(0 , -4, -10),Vec3f(0,1,0),12, marble));
        //object.push_back(new Plane(Vec3f(0,-4,0),Vec3f(0, 1, 0), reflective2));
        buf = render(object, light);
        stbi_image_free(Options::image);
    }
    bytes = convert(buf);
    stbi_write_bmp(outFilePath.c_str(), Options::width, Options::height,3,bytes);
    delete[] buf;
    delete[] bytes;
    for (uint32_t i = 0; i < object.size(); i++){
        delete object[i];
    }
    for (uint i = 0; i < light.size(); i++){
        delete light[i];
    }
    std::cout << "end." << std::endl;
    return 0;
}
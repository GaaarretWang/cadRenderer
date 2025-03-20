#pragma once

#include <vector>
#include <vsg/all.h>
#include <random>
#include <cmath>

struct PlaneData {
    std::vector<std::vector<double>> origin;
    std::vector<std::vector<double>> normals;
    std::vector<std::vector<double>> u;
    std::vector<std::vector<double>> v;
};

struct Vertex {
    vsg::vec3 position;
    vsg::vec3 normal;
};

struct MeshData {
    vsg::ref_ptr<vsg::vec3Array> vertices;
    vsg::ref_ptr<vsg::vec3Array> normals;
    vsg::ref_ptr<vsg::uintArray> indices;
};

// 辅助函数：计算向量叉积
std::vector<double> cross(const std::vector<double>& a, const std::vector<double>& b);

// 辅助函数：向量单位化
void normalize(std::vector<double>& v);

MeshData convertPlaneDataToMesh(const PlaneData& planeData);

PlaneData createTestPlanes();

PlaneData subdividePlanes(const PlaneData& originalPlaneData, float subdivisions_length);
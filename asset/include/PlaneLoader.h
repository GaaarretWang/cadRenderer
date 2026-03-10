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

// 将平面数据转换为线框（合并后的长线段），使用 LINE_LIST 拓扑
// 对每个平面生成: (M+1)条水平线 + (N+1)条垂直线 + (N+M-1)条对角线
// 其中 N、M 为 U、V 方向的细分数，保证小格近似正方形
MeshData convertPlaneDataToWireframe(const PlaneData& planeData, float subdivision_length);

PlaneData createTestPlanes();
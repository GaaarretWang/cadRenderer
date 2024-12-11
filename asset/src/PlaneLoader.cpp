#include "PlaneLoader.h"

// 辅助函数：计算向量叉积
std::vector<double> cross(const std::vector<double>& a, const std::vector<double>& b) {
    return std::vector<double>{
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]
    };
}

// 辅助函数：向量单位化
void normalize(std::vector<double>& v) {
    double length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length > 0) {
        v[0] /= length;
        v[1] /= length;
        v[2] /= length;
    }
}

MeshData convertPlaneDataToMesh(const PlaneData& planeData) {
    MeshData meshData;

    size_t totalVertices = planeData.origin.size() * 4;
    size_t totalIndices = planeData.origin.size() * 6;

    meshData.vertices = vsg::vec3Array::create(totalVertices);
    meshData.normals = vsg::vec3Array::create(totalVertices);
    meshData.indices = vsg::uintArray::create(totalIndices);


    for (size_t i = 0; i < planeData.origin.size(); ++i) {
        const auto& origin = planeData.origin[i];
        const auto& normal = planeData.normals[i];
        const auto& u = planeData.u[i];
        const auto& v = planeData.v[i];

        // 创建四个顶点
        std::array<vsg::vec3, 4> vertices;

        // 左下角
        vertices[0] = vsg::vec3(origin[0], origin[1], origin[2]);

        // 右下角
        vertices[1] = vsg::vec3(origin[0] + u[0], origin[1] + u[1], origin[2] + u[2]);

        // 右上角
        vertices[2] = vsg::vec3(origin[0] + u[0] + v[0], origin[1] + u[1] + v[1], origin[2] + u[2] + v[2]);

        // 左上角
        vertices[3] = vsg::vec3(origin[0] + v[0], origin[1] + v[1], origin[2] + v[2]);

        // 添加顶点到meshData
        size_t baseIndex = i * 4;
        for (size_t j = 0; j < 4; ++j) {
            meshData.vertices->set(baseIndex + j, vertices[j]);
            meshData.normals->set(baseIndex + j, vsg::vec3(normal[0], normal[1], normal[2]));
        }

        // 添加索引（两个三角形）
        size_t indexBase = i * 6;
        meshData.indices->set(indexBase, baseIndex);
        meshData.indices->set(indexBase + 1, baseIndex + 1);
        meshData.indices->set(indexBase + 2, baseIndex + 2);
        meshData.indices->set(indexBase + 3, baseIndex);
        meshData.indices->set(indexBase + 4, baseIndex + 2);
        meshData.indices->set(indexBase + 5, baseIndex + 3);
    }

    std::cout << "MeshData: vertices size = " << meshData.vertices->size() << "---------------------------------------------------------------"<< std::endl;

    return meshData;
}

PlaneData createTestPlanes() {
    PlaneData planeData;

    // 定义立方体的尺寸
    double size = 1.0;

    // 1. 前面 (facing -Z)
    planeData.origin.push_back({-size/2, -size/2, -size/2});
    planeData.normals.push_back({0.0, 0.0, -1.0});
    planeData.u.push_back({size, 0.0, 0.0});
    planeData.v.push_back({0.0, size, 0.0});

    // 2. 右面 (facing +X)
    planeData.origin.push_back({size/2, -size/2, -size/2});
    planeData.normals.push_back({1.0, 0.0, 0.0});
    planeData.u.push_back({0.0, 0.0, size});
    planeData.v.push_back({0.0, size, 0.0});

    // 3. 后面 (facing +Z)
    planeData.origin.push_back({size/2, -size/2, size/2});
    planeData.normals.push_back({0.0, 0.0, 1.0});
    planeData.u.push_back({-size, 0.0, 0.0});
    planeData.v.push_back({0.0, size, 0.0});

    // 4. 左面 (facing -X)
    planeData.origin.push_back({-size/2, -size/2, size/2});
    planeData.normals.push_back({-1.0, 0.0, 0.0});
    planeData.u.push_back({0.0, 0.0, -size});
    planeData.v.push_back({0.0, size, 0.0});

    return planeData;
}

PlaneData subdividePlanes(const PlaneData& originalPlaneData, int subdivisions) {
    PlaneData subdividedPlaneData;

    for (size_t i = 0; i < originalPlaneData.origin.size(); ++i) {
        const auto& originalOrigin = originalPlaneData.origin[i];
        const auto& originalNormal = originalPlaneData.normals[i];
        const auto& originalU = originalPlaneData.u[i];
        const auto& originalV = originalPlaneData.v[i];

        // 计算单位子平面的 u 和 v 向量
        std::vector<double> subU = {
            originalU[0] / subdivisions,
            originalU[1] / subdivisions,
            originalU[2] / subdivisions
        };
        std::vector<double> subV = {
            originalV[0] / subdivisions,
            originalV[1] / subdivisions,
            originalV[2] / subdivisions
        };

        for (int row = 0; row < subdivisions; ++row) {
            for (int col = 0; col < subdivisions; ++col) {
                // 计算子平面的原点
                std::vector<double> subOrigin = {
                    originalOrigin[0] + col * subU[0] + row * subV[0],
                    originalOrigin[1] + col * subU[1] + row * subV[1],
                    originalOrigin[2] + col * subU[2] + row * subV[2]
                };

                // 添加子平面数据
                subdividedPlaneData.origin.push_back(subOrigin);
                subdividedPlaneData.normals.push_back(originalNormal);
                subdividedPlaneData.u.push_back(subU);
                subdividedPlaneData.v.push_back(subV);
            }
        }
    }

    return subdividedPlaneData;
}
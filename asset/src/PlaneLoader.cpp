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

MeshData convertPlaneDataToWireframe(const PlaneData& planeData, float subdivision_length) {
    // 第一遍：计算所有平面的总顶点数和总线段数
    size_t totalVertices = 0;
    size_t totalLineSegments = 0;

    struct PlaneGrid {
        int N; // U 方向细分数
        int M; // V 方向细分数
    };
    std::vector<PlaneGrid> grids(planeData.origin.size());

    for (size_t p = 0; p < planeData.origin.size(); ++p) {
        const auto& u = planeData.u[p];
        const auto& v = planeData.v[p];
        double lenU = std::sqrt(u[0]*u[0] + u[1]*u[1] + u[2]*u[2]);
        double lenV = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);

        // 选择 N, M 使得 ||U||/N ≈ ||V||/M（小格近似正方形）
        int N = std::max(1, static_cast<int>(std::round(lenU / subdivision_length)));
        int M = std::max(1, static_cast<int>(std::round(lenV / subdivision_length)));
        grids[p] = {N, M};

        totalVertices += (N + 1) * (M + 1);
        // 水平线 (M+1) + 垂直线 (N+1) + 对角线 (N+M-1) = 2N + 2M + 1
        totalLineSegments += 2 * N + 2 * M + 1;
    }

    MeshData meshData;
    meshData.vertices = vsg::vec3Array::create(totalVertices);
    meshData.normals = vsg::vec3Array::create(totalVertices);
    meshData.indices = vsg::uintArray::create(totalLineSegments * 2); // 每条线段 2 个索引

    size_t vertexOffset = 0;
    size_t indexOffset = 0;

    for (size_t p = 0; p < planeData.origin.size(); ++p) {
        const auto& origin = planeData.origin[p];
        const auto& normal = planeData.normals[p];
        const auto& u = planeData.u[p];
        const auto& v = planeData.v[p];
        int N = grids[p].N;
        int M = grids[p].M;

        // 生成 (N+1)*(M+1) 个顶点网格
        // vertex(i,j) = origin + i*(U/N) + j*(V/M)
        auto vertexIdx = [&](int i, int j) -> uint32_t {
            return static_cast<uint32_t>(vertexOffset + i * (M + 1) + j);
        };

        for (int i = 0; i <= N; ++i) {
            for (int j = 0; j <= M; ++j) {
                double fi = static_cast<double>(i) / N;
                double fj = static_cast<double>(j) / M;
                vsg::vec3 pos(
                    origin[0] + fi * u[0] + fj * v[0],
                    origin[1] + fi * u[1] + fj * v[1],
                    origin[2] + fi * u[2] + fj * v[2]
                );
                meshData.vertices->set(vertexIdx(i, j), pos);
                meshData.normals->set(vertexIdx(i, j), vsg::vec3(normal[0], normal[1], normal[2]));
            }
        }

        // 水平线 (M+1 条): 每条从 vertex(0,j) 到 vertex(N,j)
        for (int j = 0; j <= M; ++j) {
            meshData.indices->set(indexOffset++, vertexIdx(0, j));
            meshData.indices->set(indexOffset++, vertexIdx(N, j));
        }

        // 垂直线 (N+1 条): 每条从 vertex(i,0) 到 vertex(i,M)
        for (int i = 0; i <= N; ++i) {
            meshData.indices->set(indexOffset++, vertexIdx(i, 0));
            meshData.indices->set(indexOffset++, vertexIdx(i, M));
        }

        // 对角线 (N+M-1 条): 沿 d=i-j 方向的共线对角线
        // d 从 -(M-1) 到 (N-1)
        // 每条对角线从 vertex(max(0,d), max(0,-d)) 到 vertex(min(N,M+d), min(M,N-d))
        for (int d = -(M - 1); d <= (N - 1); ++d) {
            int i_start = std::max(0, d);
            int j_start = std::max(0, -d);
            int i_end = std::min(N, M + d);
            int j_end = std::min(M, N - d);
            meshData.indices->set(indexOffset++, vertexIdx(i_start, j_start));
            meshData.indices->set(indexOffset++, vertexIdx(i_end, j_end));
        }

        vertexOffset += (N + 1) * (M + 1);
    }

    std::cout << "Wireframe: vertices=" << meshData.vertices->size()
              << " lines=" << (meshData.indices->size() / 2)
              << " indices=" << meshData.indices->size() << std::endl;

    return meshData;
}

PlaneData createTestPlanes() {
    PlaneData planeData;

    // 定义立方体的尺寸
    double size0 = 4.0;

    // 1. 前面 (facing -Z)
    planeData.origin.push_back({-size0/2, -size0/2, -1.2});
    planeData.normals.push_back({0.0, 0.0, 1.0});
    planeData.u.push_back({size0, 0.0, 0.0});
    planeData.v.push_back({0.0, size0, 0.0});


    // 1. 前面 (facing -Z)
    planeData.origin.push_back({-0.0, 0.78, -0.72});
    planeData.normals.push_back({0.0, 0.0, 1.0});
    planeData.u.push_back({0.4, 0.0, 0.0});
    planeData.v.push_back({0.0, 0.4, 0.0});

    // // 2. 右面 (facing +X)
    // planeData.origin.push_back({size/2, -size/2, -size/2});
    // planeData.normals.push_back({1.0, 0.0, 0.0});
    // planeData.u.push_back({0.0, 0.0, size});
    // planeData.v.push_back({0.0, size, 0.0});

    // // 3. 后面 (facing +Z)
    // planeData.origin.push_back({size/2, -size/2, size/2});
    // planeData.normals.push_back({0.0, 0.0, 1.0});
    // planeData.u.push_back({-size, 0.0, 0.0});
    // planeData.v.push_back({0.0, size, 0.0});

    // // 4. 左面 (facing -X)
    // planeData.origin.push_back({-size/2, -size/2, size/2});
    // planeData.normals.push_back({-1.0, 0.0, 0.0});
    // planeData.u.push_back({0.0, 0.0, -size});
    // planeData.v.push_back({0.0, size, 0.0});

    return planeData;
}
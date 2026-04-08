#include "PlaneLoader.h"

// Helper: compute the cross product of two vectors.
std::vector<double> cross(const std::vector<double>& a, const std::vector<double>& b) {
    return std::vector<double>{
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]
    };
}

// Helper: normalize a vector.
void normalize(std::vector<double>& v) {
    double length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length > 0) {
        v[0] /= length;
        v[1] /= length;
        v[2] /= length;
    }
}

MeshData convertPlaneDataToWireframe(const PlaneData& planeData, float subdivision_length) {
    // First pass: compute the total vertex count and line-segment count for all planes.
    size_t totalVertices = 0;
    size_t totalLineSegments = 0;

    struct PlaneGrid {
        int N; // Subdivision count along U.
        int M; // Subdivision count along V.
    };
    std::vector<PlaneGrid> grids(planeData.origin.size());

    for (size_t p = 0; p < planeData.origin.size(); ++p) {
        const auto& u = planeData.u[p];
        const auto& v = planeData.v[p];
        double lenU = std::sqrt(u[0]*u[0] + u[1]*u[1] + u[2]*u[2]);
        double lenV = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);

        // Choose N and M so that ||U|| / N ~= ||V|| / M and the cells remain close to square.
        int N = std::max(1, static_cast<int>(std::round(lenU / subdivision_length)));
        int M = std::max(1, static_cast<int>(std::round(lenV / subdivision_length)));
        grids[p] = {N, M};

        totalVertices += (N + 1) * (M + 1);
        // Horizontal (M+1) + vertical (N+1) + diagonal (N+M-1) lines = 2N + 2M + 1.
        totalLineSegments += 2 * N + 2 * M + 1;
    }

    MeshData meshData;
    meshData.vertices = vsg::vec3Array::create(totalVertices);
    meshData.normals = vsg::vec3Array::create(totalVertices);
    meshData.indices = vsg::uintArray::create(totalLineSegments * 2); // Two indices per line segment.

    size_t vertexOffset = 0;
    size_t indexOffset = 0;

    for (size_t p = 0; p < planeData.origin.size(); ++p) {
        const auto& origin = planeData.origin[p];
        const auto& normal = planeData.normals[p];
        const auto& u = planeData.u[p];
        const auto& v = planeData.v[p];
        int N = grids[p].N;
        int M = grids[p].M;

        // Generate the (N+1) * (M+1) vertex grid.
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

        // Horizontal lines (M+1): each one goes from vertex(0, j) to vertex(N, j).
        for (int j = 0; j <= M; ++j) {
            meshData.indices->set(indexOffset++, vertexIdx(0, j));
            meshData.indices->set(indexOffset++, vertexIdx(N, j));
        }

        // Vertical lines (N+1): each one goes from vertex(i, 0) to vertex(i, M).
        for (int i = 0; i <= N; ++i) {
            meshData.indices->set(indexOffset++, vertexIdx(i, 0));
            meshData.indices->set(indexOffset++, vertexIdx(i, M));
        }

        // Diagonal lines (N+M-1): collinear diagonals along the d = i - j direction.
        // d ranges from -(M-1) to (N-1).
        // Each diagonal runs from vertex(max(0, d), max(0, -d)) to vertex(min(N, M + d), min(M, N - d)).
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

    // Define the cube dimensions.
    double size0 = 4.0;

    // 1. Front face (facing -Z).
    planeData.origin.push_back({-size0/2, -size0/2, -1.2});
    planeData.normals.push_back({0.0, 0.0, 1.0});
    planeData.u.push_back({size0, 0.0, 0.0});
    planeData.v.push_back({0.0, size0, 0.0});


    // 1. Front face (facing -Z).
    planeData.origin.push_back({-0.0, 0.78, -0.72});
    planeData.normals.push_back({0.0, 0.0, 1.0});
    planeData.u.push_back({0.4, 0.0, 0.0});
    planeData.v.push_back({0.0, 0.4, 0.0});

    // // 2. Right face (facing +X).
    // planeData.origin.push_back({size/2, -size/2, -size/2});
    // planeData.normals.push_back({1.0, 0.0, 0.0});
    // planeData.u.push_back({0.0, 0.0, size});
    // planeData.v.push_back({0.0, size, 0.0});

    // // 3. Back face (facing +Z).
    // planeData.origin.push_back({size/2, -size/2, size/2});
    // planeData.normals.push_back({0.0, 0.0, 1.0});
    // planeData.u.push_back({-size, 0.0, 0.0});
    // planeData.v.push_back({0.0, size, 0.0});

    // // 4. Left face (facing -X).
    // planeData.origin.push_back({-size/2, -size/2, size/2});
    // planeData.normals.push_back({-1.0, 0.0, 0.0});
    // planeData.u.push_back({0.0, 0.0, -size});
    // planeData.v.push_back({0.0, size, 0.0});

    return planeData;
}

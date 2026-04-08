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

// Helper: compute the cross product of two vectors.
std::vector<double> cross(const std::vector<double>& a, const std::vector<double>& b);

// Helper: normalize a vector.
void normalize(std::vector<double>& v);

// Convert plane data into a wireframe made of merged long segments using LINE_LIST topology.
// Each plane generates (M+1) horizontal lines, (N+1) vertical lines, and (N+M-1) diagonal lines.
// N and M are the subdivision counts in the U and V directions and keep each cell close to square.
MeshData convertPlaneDataToWireframe(const PlaneData& planeData, float subdivision_length);

PlaneData createTestPlanes();

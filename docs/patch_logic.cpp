#include <CGAL/Simple_cartesian.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/Polygon_mesh_processing/connected_components.h>
#include <CGAL/IO/polygon_mesh_io.h> // read_polygon_mesh
#include <unordered_map>
#include <vector>
#include <queue>
#include <cmath>
#include <iostream>

namespace PMP = CGAL::Polygon_mesh_processing;

using Kernel = CGAL::Simple_cartesian<double>;
using Point  = Kernel::Point_3;
using Vector = Kernel::Vector_3;
using Mesh   = CGAL::Surface_mesh<Point>;
using face_descriptor = Mesh::Face_index;
using halfedge_descriptor = Mesh::Halfedge_index;

static inline double clamp01(double x) { return std::max(-1.0, std::min(1.0, x)); }

static inline Vector normalize_vec(const Vector& v) {
    double len = std::sqrt(v.squared_length());
    if (len < 1e-12) return Vector(0,0,0);
    return v / len;
}

static inline double triangle_area(const Mesh& m, face_descriptor f) {
    return PMP::face_area(f, m); // CGAL 自带
}

// 计算 face 的单位法向（方向由三角形顶点绕序决定，STL可能不一致）
// 我们后面用面积加权平均再做“朝上”判断，通常够用；若你担心绕序乱，后面可取 |dot| 或做一致化朝向。
static inline Vector face_unit_normal(const Mesh& m, face_descriptor f) {
    Vector n = PMP::compute_face_normal(f, m);
    return normalize_vec(n);
}

// 相邻面二面角（返回 0~pi）
// 如果两个三角形几乎共面，二面角接近 0。
static double dihedral_angle_between_faces(const Mesh& m, face_descriptor f1, face_descriptor f2) {
    Vector n1 = face_unit_normal(m, f1);
    Vector n2 = face_unit_normal(m, f2);
    double c = clamp01( (n1 * n2) / std::sqrt(n1.squared_length() * n2.squared_length() + 1e-30) );
    // 如果绕序不一致，n2 可能反向；“共面”仍然应视为同一面片，所以取 abs(dot)
    c = std::abs(c);
    return std::acos(c);
}

// 用“共面阈值”做 patch 聚类：在共享边的相邻三角形之间 BFS
struct PatchInfo {
    std::vector<face_descriptor> faces;
    double area = 0.0;
    Vector areaWeightedNormal = Vector(0,0,0); // sum(A_f * n_f)
};

int main(int argc, char** argv) {
    const char* filename = (argc > 1) ? argv[1] : "model.stl";

    Mesh mesh;
    if (!CGAL::IO::read_polygon_mesh(filename, mesh) || CGAL::is_empty(mesh)) {
        std::cerr << "Failed to read mesh: " << filename << "\n";
        return 1;
    }
    if (!CGAL::is_triangle_mesh(mesh)) {
        std::cerr << "Mesh is not pure triangle mesh.\n";
        // 你也可以先三角化，但 STL 一般都是三角形
    }

    // ---------- 参数 ----------
    const double coplanar_angle_deg = 3.0;     // 共面阈值：相邻三角形二面角 < 3° 认为同一大面
    const double coplanar_angle_rad = coplanar_angle_deg * M_PI / 180.0;

    const double up_angle_deg = 60.0;          // “上表面”阈值：法向与 +Z 夹角 < 60°
    const double up_cos = std::cos(up_angle_deg * M_PI / 180.0);

    const Vector Zp(0,0,1);

    // ---------- BFS 聚类 ----------
    std::unordered_map<std::size_t, bool> visited; // face idx -> visited
    visited.reserve(mesh.number_of_faces() * 2);

    std::vector<PatchInfo> patches;
    patches.reserve(64);

    auto face_id = [&](face_descriptor f)->std::size_t { return (std::size_t)f; };

    for (face_descriptor f : mesh.faces()) {
        if (visited[face_id(f)]) continue;

        PatchInfo patch;
        std::queue<face_descriptor> q;
        q.push(f);
        visited[face_id(f)] = true;

        while (!q.empty()) {
            face_descriptor cur = q.front(); q.pop();
            patch.faces.push_back(cur);

            double A = triangle_area(mesh, cur);
            Vector n = face_unit_normal(mesh, cur);
            patch.area += A;
            patch.areaWeightedNormal = patch.areaWeightedNormal + (A * n);

            // 遍历 cur 的三条边（halfedges），找相邻 face
            halfedge_descriptor h = mesh.halfedge(cur);
            halfedge_descriptor h0 = h;
            do {
                halfedge_descriptor opp = mesh.opposite(h);
                if (!mesh.is_border(opp)) {
                    face_descriptor nb = mesh.face(opp);
                    if (!visited[face_id(nb)]) {
                        double ang = dihedral_angle_between_faces(mesh, cur, nb);
                        if (ang < coplanar_angle_rad) {
                            visited[face_id(nb)] = true;
                            q.push(nb);
                        }
                    }
                }
                h = mesh.next(h);
            } while (h != h0);
        }

        patches.push_back(std::move(patch));
    }

    // ---------- 在上表面 patch 中选面积最大 ----------
    int bestIdx = -1;
    double bestArea = -1.0;

    for (int i = 0; i < (int)patches.size(); ++i) {
        const auto& p = patches[i];
        Vector N = normalize_vec(p.areaWeightedNormal);
        double cosUp = (N * Zp) / std::sqrt(N.squared_length() * Zp.squared_length() + 1e-30);

        // 允许绕序乱：如果你希望“朝上”必须是 +Z 而不是 -Z，就不要 abs
        // 这里我们取 cosUp（不 abs），更符合“上表面=朝+Z”
        if (cosUp > up_cos) {
            if (p.area > bestArea) {
                bestArea = p.area;
                bestIdx = i;
            }
        }
    }

    if (bestIdx < 0) {
        std::cout << "No upward patch found. Try relaxing up_angle_deg or fix mesh orientation.\n";
        return 0;
    }

    const auto& best = patches[bestIdx];
    Vector bestN = normalize_vec(best.areaWeightedNormal);

    std::cout << "Best upward patch index: " << bestIdx << "\n";
    std::cout << "Patch area: " << best.area << "\n";
    std::cout << "Patch normal (avg): (" << bestN.x() << ", " << bestN.y() << ", " << bestN.z() << ")\n";
    std::cout << "Num triangles in patch: " << best.faces.size() << "\n";

    return 0;
}
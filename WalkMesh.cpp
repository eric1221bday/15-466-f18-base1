#include "WalkMesh.hpp"

// adapted from here:
// https://www.gamedev.net/forums/topic/552906-closest-point-on-triangle/
static glm::vec3 closest_point_on_triangle(
    const std::array<glm::vec3, 3> &triangle, const glm::vec3 &pos) {
  glm::vec3 edge0 = triangle[1] - triangle[0];
  glm::vec3 edge1 = triangle[2] - triangle[0];
  glm::vec3 v0 = triangle[0] - pos;

  float a = glm::dot(edge0, edge0);
  float b = glm::dot(edge0, edge1);
  float c = glm::dot(edge1, edge1);
  float d = glm::dot(edge0, v0);
  float e = glm::dot(edge1, v0);

  float det = a * c - b * b;
  float s = b * e - c * d;
  float t = b * d - a * e;

  if (s + t < det) {
    if (s < 0.f) {
      if (t < 0.f) {
        if (d < 0.f) {
          s = glm::clamp(-d / a, 0.f, 1.f);
          t = 0.f;
        } else {
          s = 0.f;
          t = glm::clamp(-e / c, 0.f, 1.f);
        }
      } else {
        s = 0.f;
        t = glm::clamp(-e / c, 0.f, 1.f);
      }
    } else if (t < 0.f) {
      s = glm::clamp(-d / a, 0.f, 1.f);
      t = 0.f;
    } else {
      float invDet = 1.f / det;
      s *= invDet;
      t *= invDet;
    }
  } else {
    if (s < 0.f) {
      float tmp0 = b + d;
      float tmp1 = c + e;
      if (tmp1 > tmp0) {
        float numer = tmp1 - tmp0;
        float denom = a - 2 * b + c;
        s = glm::clamp(numer / denom, 0.f, 1.f);
        t = 1 - s;
      } else {
        t = glm::clamp(-e / c, 0.f, 1.f);
        s = 0.f;
      }
    } else if (t < 0.f) {
      if (a + d > b + e) {
        float numer = c + e - b - d;
        float denom = a - 2 * b + c;
        s = glm::clamp(numer / denom, 0.f, 1.f);
        t = 1 - s;
      } else {
        s = glm::clamp(-e / c, 0.f, 1.f);
        t = 0.f;
      }
    } else {
      float numer = c + e - b - d;
      float denom = a - 2 * b + c;
      s = glm::clamp(numer / denom, 0.f, 1.f);
      t = 1.f - s;
    }
  }

  return triangle[0] + s * edge0 + t * edge1;
}

// Compute barycentric coordinates (u, v, w) for
// point p with respect to triangle (a, b, c)
// adapted from
// https://gamedev.stackexchange.com/questions/23743/whats-the-most-efficient-way-to-find-barycentric-coordinates
static glm::vec3 barycentric(glm::vec3 p, glm::vec3 a, glm::vec3 b,
                             glm::vec3 c) {
  float u, v, w;
  glm::vec3 v0 = b - a, v1 = c - a, v2 = p - a;
  float d00 = glm::dot(v0, v0);
  float d01 = glm::dot(v0, v1);
  float d11 = glm::dot(v1, v1);
  float d20 = glm::dot(v2, v0);
  float d21 = glm::dot(v2, v1);
  float denom = d00 * d11 - d01 * d01;
  v = (d11 * d20 - d01 * d21) / denom;
  w = (d00 * d21 - d01 * d20) / denom;
  u = 1.0f - v - w;

  return glm::vec3(u, v, w);
}

WalkMesh::WalkMesh(std::string filename) {
  std::ifstream file(filename, std::ios::binary);

  static_assert(sizeof(glm::vec3) == 3 * 4, "vec3 is packed.");
  static_assert(sizeof(glm::uvec3) == 3 * 4, "uvec3 is packed.");

  read_chunk(file, "vtx0", &vertices);
  read_chunk(file, "tri0", &triangles);
  read_chunk(file, "nom0", &vertex_normals);

  std::cout << vertices.size() << " number of vertices" << std::endl;
  std::cout << triangles.size() << " number of triangles" << std::endl;

  for (auto const &triangle : triangles) {
    next_vertex[glm::uvec2(triangle.x, triangle.y)] = triangle.z;
    next_vertex[glm::uvec2(triangle.y, triangle.z)] = triangle.x;
    next_vertex[glm::uvec2(triangle.z, triangle.x)] = triangle.y;
  }
}

WalkMesh::WalkPoint WalkMesh::start(glm::vec3 const &world_point) const {
  WalkPoint closest;
  // TODO: iterate through triangles
  // TODO: for each triangle, find closest point on triangle to world_point
  // TODO: if point is closest, closest.triangle gets the current triangle,
  // closest.weights gets the barycentric coordinates

  float min_dist = std::numeric_limits<float>::max();

  for (const glm::uvec3 &tri_indices : triangles) {
    std::array<glm::vec3, 3> tri_verts = {{vertices[tri_indices[0]],
                                           vertices[tri_indices[1]],
                                           vertices[tri_indices[2]]}};
    glm::vec3 point = closest_point_on_triangle(tri_verts, world_point);
    if (glm::distance(point, world_point) < min_dist) {
      min_dist = glm::distance(point, world_point);
      closest.triangle = tri_indices;
      closest.weights =
          barycentric(point, vertices[tri_indices[0]], vertices[tri_indices[1]],
                      vertices[tri_indices[2]]);
    }
  }

  return closest;
}

void WalkMesh::walk(WalkPoint &wp, glm::vec3 const &step,
                    uint32_t depth) const {
  // TODO: project step to barycentric coordinates to get weights_step
  //  glm::vec3 weights_step;
  glm::vec3 world_projected = world_point(wp) + step;
  glm::vec3 projected_weights =
      barycentric(world_projected, vertices[wp.triangle[0]],
                  vertices[wp.triangle[1]], vertices[wp.triangle[2]]);
  glm::vec3 weights_step = projected_weights - wp.weights;

  //  std::cout << "depth: " << depth << std::endl;
  //  std::cout << "current triangle: " << glm::to_string(wp.triangle) <<
  //  std::endl; std::cout << "current barycentric loc: " <<
  //  glm::to_string(wp.weights)
  //            << std::endl;
  //  std::cout << "projected to barycentric go: "
  //            << glm::to_string(projected_weights) << std::endl;
  //  std::cout << "barycentric step: " << glm::to_string(weights_step)
  //            << std::endl;
  //  std::cout << "current world step: " << glm::to_string(step) << std::endl;

  // super hacky, just kill the recursion if encountering numerical instability
  if (depth > 10) {
    return;
  }

  if (projected_weights.x >= 0 && projected_weights.y >= 0 &&
      projected_weights.z >= 0) {  // if a triangle edge is not crossed
    // TODO: wp.weights gets moved by weights_step, nothing else needs to be
    // done.
    wp.weights = projected_weights;

  } else {  // if a triangle edge is crossed
    // TODO: wp.weights gets moved to triangle edge, and step gets reduced
    glm::uvec2 crossed_edge;
    float reduced_coeff = 0.0f;

    if (projected_weights.x < 0) {
      reduced_coeff = wp.weights.x / -weights_step.x;
      crossed_edge = glm::uvec2(wp.triangle[1], wp.triangle[2]);
    } else if (projected_weights.y < 0) {
      reduced_coeff = wp.weights.y / -weights_step.y;
      crossed_edge = glm::uvec2(wp.triangle[2], wp.triangle[0]);
    } else if (projected_weights.z < 0) {
      reduced_coeff = wp.weights.z / -weights_step.z;
      crossed_edge = glm::uvec2(wp.triangle[0], wp.triangle[1]);
    } else {
      std::cout << "this is really not supposed to happen!" << std::endl;
    }

    wp.weights += weights_step * reduced_coeff;
    glm::vec3 world_point_edge = world_point(wp);
    glm::vec3 reduced_step = world_projected - world_point_edge;

    // if there is another triangle over the edge:
    // TODO: wp.triangle gets updated to adjacent triangle
    // TODO: step gets rotated over the edge
    if (next_vertex.find(glm::uvec2(crossed_edge.y, crossed_edge.x)) !=
        next_vertex.end()) {
      auto third_vertex =
          next_vertex.at(glm::uvec2(crossed_edge.y, crossed_edge.x));

      auto result = std::find_if(
          triangles.begin(), triangles.end(), [&](glm::uvec3 triangle) {
            return (crossed_edge.x == triangle.x ||
                    crossed_edge.x == triangle.y ||
                    crossed_edge.x == triangle.z) &&
                   (crossed_edge.y == triangle.x ||
                    crossed_edge.y == triangle.y ||
                    crossed_edge.y == triangle.z) &&
                   (third_vertex == triangle.x || third_vertex == triangle.y ||
                    third_vertex == triangle.z);
          });

      if (result != triangles.end()) {
        wp.triangle = *result;
        wp.weights =
            barycentric(world_point_edge, vertices[wp.triangle[0]],
                        vertices[wp.triangle[1]], vertices[wp.triangle[2]]);
        walk(wp, reduced_step, depth + 1);
      } else {
        std::cout << "should be able to find triangle!!" << std::endl;
      }
    } else {
      //      if (projected_weights.x < 0) {
      //        weights_step.x = 0.0f;
      //      }
      //      if (projected_weights.y < 0) {
      //        weights_step.y = 0.0f;
      //      }
      //      if (projected_weights.z < 0) {
      //        weights_step.z = 0.0f;
      //      }

      //      walk(wp, weights_step);
    }

    // else if there is no other triangle over the edge:
    // TODO: wp.triangle stays the same.
    // TODO: step gets updated to slide along the edge
  }
}

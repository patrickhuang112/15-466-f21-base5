#include "WalkMesh.hpp"

#include "read_write_chunk.hpp"

#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>

WalkMesh::WalkMesh(std::vector< glm::vec3 > const &vertices_, std::vector< glm::vec3 > const &normals_, std::vector< glm::uvec3 > const &triangles_)
	: vertices(vertices_), normals(normals_), triangles(triangles_) {

	//construct next_vertex map (maps each edge to the next vertex in the triangle):
	next_vertex.reserve(triangles.size()*3);
	auto do_next = [this](uint32_t a, uint32_t b, uint32_t c) {
		auto ret = next_vertex.insert(std::make_pair(glm::uvec2(a,b), c));
		assert(ret.second);
	};
	for (auto const &tri : triangles) {
		do_next(tri.x, tri.y, tri.z);
		do_next(tri.y, tri.z, tri.x);
		do_next(tri.z, tri.x, tri.y);
	}

	//DEBUG: are vertex normals consistent with geometric normals?
	for (auto const &tri : triangles) {
		glm::vec3 const &a = vertices[tri.x];
		glm::vec3 const &b = vertices[tri.y];
		glm::vec3 const &c = vertices[tri.z];
		glm::vec3 out = glm::normalize(glm::cross(b-a, c-a));

		float da = glm::dot(out, normals[tri.x]);
		float db = glm::dot(out, normals[tri.y]);
		float dc = glm::dot(out, normals[tri.z]);

		assert(da > 0.1f && db > 0.1f && dc > 0.1f);
	}
}

//project pt to the plane of triangle a,b,c and return the barycentric weights of the projected point:
glm::vec3 barycentric_weights(glm::vec3 const &a, glm::vec3 const &b, glm::vec3 const &c, glm::vec3 const &pt) {

	// From lecture in class
	glm::vec3 norm = glm::cross(b-a, c-a);
	glm::vec3 ab_perp = glm::cross(norm, b-a);
	glm::vec3 bc_perp = glm::cross(norm, c-b);
	glm::vec3 ca_perp = glm::cross(norm, a-c);

	float abp_area = glm::dot(ab_perp, pt-a);
	float bcp_area = glm::dot(bc_perp, pt-b);
	float cap_area = glm::dot(ca_perp, pt-c);
	float total = abp_area + bcp_area + cap_area;
	return glm::vec3(bcp_area / total, cap_area / total, abp_area / total);

	// Tried doing it myself first	
	// Using this equation: https://www.baeldung.com/cs/3d-point-2d-plane
	/*
	glm::vec3 line1 = glm::vec3(b.x - a.x, b.y - a.y, b.z - a.z);
	glm::vec3 line2 = glm::vec3(c.x - b.x, c.y - b.y, c.z - b.z);
	float lenl1 = glm::length(line1);
	float lenl2 = glm::length(line2);
	glm::vec3 normal = glm::cross(line1, line2);	
	float d = glm::dot(normal, line1); 
	float len = glm::length(normal);
	float k = (d - normal.x * pt.x, - normal.y * pt.y - normal.z * pt.z) / len;	
	float newx = pt.x + k * normal.x;
	float newy = pt.y + k * normal.y;
	float newz = pt.z + k * normal.z;

	// line1 opposite point c
	glm::vec3 topoint1 = glm::vec3(newx - a.x, newy - a.y, newy - a.z);
	float len1 = glm::length(topoint1);
	float cos1 = glm::dot(topoint1, line1) / (len1 * lenl1);
	float sin1 = 1.f - (cos1 * cos1);
	float area_c = 0.5f * len1 * lenl1 * sin1; 

	// line2, opposite point b
	glm::vec3 topoint2 = glm::vec3(newx - b.x, newy - b.y, newy - b.z);
	float len2 = glm::length(topoint2);
	float cos2 = glm::dot(topoint2, line2) / (len2 * lenl2);
	float sin2 = 1.f - (cos2 * cos2);
	float area_b = 0.5f * len2 * lenl2 * sin2; 
	float area_a = 1.f - area_b - area_c;

	// printf("BARY: %f %f %f\n", area_a, area_b, area_c);
	return glm::vec3(area_a, area_b, area_c);
	*/
}

WalkPoint WalkMesh::nearest_walk_point(glm::vec3 const &world_point) const {
	assert(!triangles.empty() && "Cannot start on an empty walkmesh");

	WalkPoint closest;
	float closest_dis2 = std::numeric_limits< float >::infinity();

	for (auto const &tri : triangles) {
		//find closest point on triangle:

		glm::vec3 const &a = vertices[tri.x];
		glm::vec3 const &b = vertices[tri.y];
		glm::vec3 const &c = vertices[tri.z];

		//get barycentric coordinates of closest point in the plane of (a,b,c):
		glm::vec3 coords = barycentric_weights(a,b,c, world_point);

		//is that point inside the triangle?
		if (coords.x >= 0.0f && coords.y >= 0.0f && coords.z >= 0.0f) {
			//yes, point is inside triangle.
			printf("COORDS: %f %f %f\n", coords.x, coords.y, coords.z);
			float dis2 = glm::length2(world_point - to_world_point(WalkPoint(tri, coords)));
			if (dis2 < closest_dis2) {
				closest_dis2 = dis2;
				closest.indices = tri;
				closest.weights = coords;
			}
		} else {
			//check triangle vertices and edges:
			auto check_edge = [&world_point, &closest, &closest_dis2, this](uint32_t ai, uint32_t bi, uint32_t ci) {
				glm::vec3 const &a = vertices[ai];
				glm::vec3 const &b = vertices[bi];

				//find closest point on line segment ab:
				float along = glm::dot(world_point-a, b-a);
				float max = glm::dot(b-a, b-a);
				glm::vec3 pt;
				glm::vec3 coords;
				if (along < 0.0f) {
					pt = a;
					coords = glm::vec3(1.0f, 0.0f, 0.0f);
				} else if (along > max) {
					pt = b;
					coords = glm::vec3(0.0f, 1.0f, 0.0f);
				} else {
					float amt = along / max;
					pt = glm::mix(a, b, amt);
					coords = glm::vec3(1.0f - amt, amt, 0.0f);
				}

				float dis2 = glm::length2(world_point - pt);
				if (dis2 < closest_dis2) {
					closest_dis2 = dis2;
					closest.indices = glm::uvec3(ai, bi, ci);
					closest.weights = coords;
				}
			};
			check_edge(tri.x, tri.y, tri.z);
			check_edge(tri.y, tri.z, tri.x);
			check_edge(tri.z, tri.x, tri.y);
		}
	}
	assert(closest.indices.x < vertices.size());
	assert(closest.indices.y < vertices.size());
	assert(closest.indices.z < vertices.size());
	return closest;
}


void WalkMesh::walk_in_triangle(WalkPoint const &start, glm::vec3 const &step, WalkPoint *end_, float *time_) const {
	assert(end_);
	auto &end = *end_;

	assert(time_);
	auto &time = *time_;

	glm::vec3 const &a = vertices[start.indices.x];
	glm::vec3 const &b = vertices[start.indices.y];
	glm::vec3 const &c = vertices[start.indices.z];


	glm::vec3 end_world = to_world_point(start) + step;
	glm::vec3 end_bary = barycentric_weights(a,b,c, end_world);
	glm::vec3 delta_bary = end_bary - start.weights;

	// Went outside of triangle
	if (end_bary.x < 0.f || end_bary.y < 0.f || end_bary.z < 0.f) {
		float time_to_reach_x = start.weights.x / -delta_bary.x;
		float time_to_reach_y = start.weights.y / -delta_bary.y;
		float time_to_reach_z = start.weights.z / -delta_bary.z;
		float timepassed = 2.f;
		// 0 = x, 1 = y, 2 = z
		int xyorz = 3;
		if (time_to_reach_x >= 0.f && time_to_reach_x < timepassed) {
			timepassed = time_to_reach_x;
			xyorz = 0;
		}
		if (time_to_reach_y >= 0.f && time_to_reach_y < timepassed) {
			timepassed = time_to_reach_y;
			xyorz = 1;
		}
		if (time_to_reach_z >= 0.f && time_to_reach_z < timepassed) {
			timepassed = time_to_reach_z;
			xyorz = 2;
		}
		assert(xyorz != 3);
		assert(timepassed >= 0.f && timepassed <= 1.f);
		time = timepassed;

		end.indices = start.indices;
		end.weights = start.weights + delta_bary * timepassed;

		if (xyorz == 0) {
			// y becomes x, z becomes y, x becomes z
			int oldind = end.indices.x;
			end.indices.x = end.indices.y;
			end.indices.y = end.indices.z;
			end.indices.z = oldind;

			end.weights.x = end.weights.y;
			end.weights.y = end.weights.z;
			end.weights.z = 0.f;
		}
		else if (xyorz == 1) {
			// y becomes z, x becomes y, z becomes x
			int oldind = end.indices.y;
			end.indices.y = end.indices.x;
			end.indices.x = end.indices.z;
			end.indices.z = oldind;
			
			end.weights.y = end.weights.x;
			end.weights.x = end.weights.z;
			end.weights.z = 0.f;
		}
		else {
			end.weights.z = 0.f;
		}

			
	}	
	else {
		end.indices = start.indices;
		end.weights = end_bary;
		time = 1.f;
	}
}


bool WalkMesh::cross_edge(WalkPoint const &start, WalkPoint *end_, glm::quat *rotation_) const {
	assert(end_);
	auto &end = *end_;

	assert(rotation_);
	auto &rotation = *rotation_;

	assert(start.weights.z == 0.0f); //*must* be on an edge.
	glm::uvec2 edge = glm::uvec2(start.indices);

	auto it = next_vertex.find(edge);
	end = start;
	rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

	if (it == next_vertex.end()) {
		return false;
	} else {
		int newz = (*it).second;
		
		end.indices = glm::uvec3(start.indices.y, start.indices.x, newz);
		end.weights = glm::vec3(start.weights.y, start.weights.x, 0.0f);

		rotation = glm::rotation(to_world_triangle_normal(start), to_world_triangle_normal(end));
		return true;
	}
}


WalkMeshes::WalkMeshes(std::string const &filename) {
	std::ifstream file(filename, std::ios::binary);

	std::vector< glm::vec3 > vertices;
	read_chunk(file, "p...", &vertices);

	std::vector< glm::vec3 > normals;
	read_chunk(file, "n...", &normals);

	std::vector< glm::uvec3 > triangles;
	read_chunk(file, "tri0", &triangles);

	std::vector< char > names;
	read_chunk(file, "str0", &names);

	struct IndexEntry {
		uint32_t name_begin, name_end;
		uint32_t vertex_begin, vertex_end;
		uint32_t triangle_begin, triangle_end;
	};

	std::vector< IndexEntry > index;
	read_chunk(file, "idxA", &index);

	if (file.peek() != EOF) {
		std::cerr << "WARNING: trailing data in walkmesh file '" << filename << "'" << std::endl;
	}

	//-----------------

	if (vertices.size() != normals.size()) {
		throw std::runtime_error("Mis-matched position and normal sizes in '" + filename + "'");
	}

	for (auto const &e : index) {
		if (!(e.name_begin <= e.name_end && e.name_end <= names.size())) {
			throw std::runtime_error("Invalid name indices in index of '" + filename + "'");
		}
		if (!(e.vertex_begin <= e.vertex_end && e.vertex_end <= vertices.size())) {
			throw std::runtime_error("Invalid vertex indices in index of '" + filename + "'");
		}
		if (!(e.triangle_begin <= e.triangle_end && e.triangle_end <= triangles.size())) {
			throw std::runtime_error("Invalid triangle indices in index of '" + filename + "'");
		}

		//copy vertices/normals:
		std::vector< glm::vec3 > wm_vertices(vertices.begin() + e.vertex_begin, vertices.begin() + e.vertex_end);
		std::vector< glm::vec3 > wm_normals(normals.begin() + e.vertex_begin, normals.begin() + e.vertex_end);

		//remap triangles:
		std::vector< glm::uvec3 > wm_triangles; wm_triangles.reserve(e.triangle_end - e.triangle_begin);
		for (uint32_t ti = e.triangle_begin; ti != e.triangle_end; ++ti) {
			if (!( (e.vertex_begin <= triangles[ti].x && triangles[ti].x < e.vertex_end)
			    && (e.vertex_begin <= triangles[ti].y && triangles[ti].y < e.vertex_end)
			    && (e.vertex_begin <= triangles[ti].z && triangles[ti].z < e.vertex_end) )) {
				throw std::runtime_error("Invalid triangle in '" + filename + "'");
			}
			wm_triangles.emplace_back(
				triangles[ti].x - e.vertex_begin,
				triangles[ti].y - e.vertex_begin,
				triangles[ti].z - e.vertex_begin
			);
		}
		
		std::string name(names.begin() + e.name_begin, names.begin() + e.name_end);

		auto ret = meshes.emplace(name, WalkMesh(wm_vertices, wm_normals, wm_triangles));
		if (!ret.second) {
			throw std::runtime_error("WalkMesh with duplicated name '" + name + "' in '" + filename + "'");
		}

	}
}

WalkMesh const &WalkMeshes::lookup(std::string const &name) const {
	auto f = meshes.find(name);
	if (f == meshes.end()) {
		throw std::runtime_error("WalkMesh with name '" + name + "' not found.");
	}
	return f->second;
}

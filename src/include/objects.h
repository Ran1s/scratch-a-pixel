/*
 * Copyright (C) 2012  www.scratchapixel.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstring>
#include <random>

#include "geometry_utils.h"

enum MaterialType {
	kDiffuse,
	kDiffuseAndGlossy,
	kReflectionAndRefraction,
	kReflection,
	kPhong
};

class Ray
{
public:
	Ray(const Vec3f &orig, const Vec3f &dir) : orig(orig), dir(dir)
	{
		invdir = 1 / dir;
		sign[0] = (invdir.x < 0);
		sign[1] = (invdir.y < 0);
		sign[2] = (invdir.z < 0);
	}
	Vec3f orig, dir; // ray orig and dir
	Vec3f invdir;
	int sign[3];
};

class Object
{
 public:
	Object()
	{
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<> dis(0.f, 1.f);
		diffuseColor = Vec3f(dis(gen), dis(gen), dis(gen));
	}

	Object(const Matrix44f &o2w) : objectToWorld(o2w), worldToObject(o2w.inverse()) {}

	virtual ~Object() {}
	virtual bool intersect(const Vec3f &, const Vec3f &, float &, uint32_t &, Vec2f &) const = 0;
	virtual void getSurfaceProperties(const Vec3f &, const Vec3f &, const uint32_t &, const Vec2f &, Vec3f &, Vec2f &) const = 0;
	virtual Vec3f evalDiffuseColor(const Vec2f &) const { return diffuseColor; }

	// material properties
	MaterialType materialType = kDiffuseAndGlossy;
	Vec3f diffuseColor; // albedo
	float ior = 1.3;
	float Kd = 0.8; // phong model diffuse weight
	float Ks = 0.2; // phong model specular weight
	float specularExponent = 25; // phong specular exponent

	// transforms
	Matrix44f objectToWorld;
	Matrix44f worldToObject;

	// shading
	bool smoothShading = false;
};

class AABBox
{
public:
	AABBox(const Vec3f &b0, const Vec3f &b1) { bounds[0] = b0, bounds[1] = b1; }

	bool intersect(const Ray &r, float &t) const
	{
		float tmin, tmax, tymin, tymax, tzmin, tzmax;

		tmin = (bounds[r.sign[0]].x - r.orig.x) * r.invdir.x;
		tmax = (bounds[1-r.sign[0]].x - r.orig.x) * r.invdir.x;
		tymin = (bounds[r.sign[1]].y - r.orig.y) * r.invdir.y;
		tymax = (bounds[1-r.sign[1]].y - r.orig.y) * r.invdir.y;

		if ((tmin > tymax) || (tymin > tmax)) {
			return false;
		}

		if (tymin > tmin) {
			tmin = tymin;
		}
		if (tymax < tmax) {
			tmax = tymax;
		}

		tzmin = (bounds[r.sign[2]].z - r.orig.z) * r.invdir.z;
		tzmax = (bounds[1-r.sign[2]].z - r.orig.z) * r.invdir.z;

		if ((tmin > tzmax) || (tzmin > tmax)) {
			return false;
		}

		if (tzmin > tmin) {
			tmin = tzmin;
		}
		if (tzmax < tmax) {
			tmax = tzmax;
		}

		t = tmin;

		if (t < 0) {
			t = tmax;
			if (t < 0) {
				return false;
			}
		}

		return true;
	}
	Vec3f bounds[2];
};

class Sphere : public Object
{
public:

	Sphere(const Matrix44f &o2w, const float &r) : Object(o2w), radius(r), radius2(r * r)
	{
		o2w.multVecMatrix(Vec3f(0), center);
	}

	Sphere(const Vec3f &c,
		   const float &r,
		   const Vec3f &sc = 0,
		   const float &refl = 0,
		   const float &transp = 0,
		   const Vec3f &ec = 0) :
		   center(c),
		   radius(r),
		   radius2(r * r),
		   surfaceColor(sc),
		   emissionColor(ec),
		   transparency(transp),
		   reflection(refl)
	{}

	/*
	//[comment]
	// Compute a ray-sphere intersection using the geometric solution
	//[/comment]
	bool intersect(const Vec3f &rayorig, const Vec3f &raydir, float &t0, float &t1) const
	{
			Vec3f l = center - rayorig;
			float tca = l.dotProduct(raydir);
			if (tca < 0) return false;
			float d2 = l.dotProduct(l) - tca * tca;
			if (d2 > radius2) return false;
			float thc = sqrt(radius2 - d2);
			t0 = tca - thc;
			t1 = tca + thc;

			return true;
	}
	*/

	//[comment]
	// Compute a ray-sphere intersection using the analytic solution
	//[/comment]
	bool intersect(const Vec3f &orig, const Vec3f &dir, float &tnear, uint32_t &index, Vec2f &uv) const
	{
		// analytic solution
		float t0, t1;
		Vec3f L = orig - center;
		float a = dir.dotProduct(dir);
		float b = 2 * dir.dotProduct(L);
		float c = L.dotProduct(L) - radius2;
		if (!scratch::utils::solveQuadratic(a, b, c, t0, t1)) return false;

		if (t0 < 0) {
			t0 = t1;
			if (t0 < 0) {
				return false;
			}
		}
		tnear = t0;

		return true;
	}

	void getSurfaceProperties(
		const Vec3f &P,
		const Vec3f &I,
		const uint32_t &index,
		const Vec2f &uv,
		Vec3f &N,
		Vec2f &st) const
	{
		N = (P - center).normalize();
		// In this particular case, the normal is simular to a point on a unit sphere
		// centred around the origin. We can thus use the normal coordinates to compute
		// the spherical coordinates of Phit.
		// atan2 returns a value in the range [-pi, pi] and we need to remap it to range [0, 1]
		// acosf returns a value in the range [0, pi] and we also need to remap it to the range [0, 1]
		st.x = (1 + atan2(N.z, N.x) / M_PI) * 0.5f;
		st.y = acosf(N.y) / M_PI;
	}

	Vec3f center;                           /// position of the sphere
	float radius, radius2;                  /// sphere radius and radius^2
	Vec3f surfaceColor, emissionColor;      /// surface color and emission (light)
	float transparency, reflection;         /// surface transparency and reflectivity
};

class TriangleMesh : public Object
{
public:
	// Build a triangle mesh from a face index array and a vertex index array
	TriangleMesh(
		const Vec3f *verts,
		const uint32_t *vertsIndex,
		const uint32_t &numTris,
		const Vec2f *st)
	{
		uint32_t maxIndex = 0;
		for (uint32_t i = 0; i < numTris * 3; ++i) {
			if (vertsIndex[i] > maxIndex) maxIndex = vertsIndex[i];
		}
		maxIndex += 1;
		P = std::unique_ptr<Vec3f[]>(new Vec3f[maxIndex]);
		memcpy(P.get(), verts, sizeof(Vec3f) * maxIndex);
		trisIndex = std::unique_ptr<uint32_t[]>(new uint32_t[numTris * 3]);
		memcpy(trisIndex.get(), vertsIndex, sizeof(uint32_t) * numTris * 3);
		this->numTris = numTris;
		texCoordinates = std::unique_ptr<Vec2f[]>(new Vec2f[maxIndex]);
		memcpy(texCoordinates.get(), st, sizeof(Vec2f) * maxIndex);
	}

	// Build a triangle mesh from a face index array and a vertex index array
	TriangleMesh(
		const uint32_t nfaces,
		const std::unique_ptr<uint32_t []> &faceIndex,
		const std::unique_ptr<uint32_t []> &vertsIndex,
		const std::unique_ptr<Vec3f []> &verts,
		std::unique_ptr<Vec3f []> &normals,
		std::unique_ptr<Vec2f []> &st) :
			numTris(0)
	{
		uint32_t k = 0, maxVertIndex = 0;
		// find out how many triangles we need to create for this mesh
		for (uint32_t i = 0; i < nfaces; ++i) {
			numTris += faceIndex[i] - 2;
			for (uint32_t j = 0; j < faceIndex[i]; ++j) {
				if (vertsIndex[k + j] > maxVertIndex)
					maxVertIndex = vertsIndex[k + j];
			}
			k += faceIndex[i];
		}
		maxVertIndex += 1;

		// allocate memory to store the position of the mesh vertices
		P = std::unique_ptr<Vec3f []>(new Vec3f[maxVertIndex]);
		for (uint32_t i = 0; i < maxVertIndex; ++i) {
			P[i] = verts[i];
		}

		// allocate memory to store triangle indices
		trisIndex = std::unique_ptr<uint32_t []>(new uint32_t [numTris * 3]);

		// Generate the triangle index array Keep in mind that there is generally 1 vertex attribute for each vertex of each face.
		// So for example if you have 2 quads, you only have 6 vertices but you have 2 * 4 vertex attributes (that is 8 normals,
		// 8 texture coordinates, etc.). So the easiest lazziest method in our triangle mesh, is to create a new array for each
		// supported vertex attribute (st, normals, etc.) whose size is equal to the number of triangles multiplied by 3, and then
		// set the value of the vertex attribute at each vertex of each triangle using the input array (normals[], st[], etc.)
		uint32_t l = 0;
		N = std::unique_ptr<Vec3f []>(new Vec3f[numTris * 3]);
		texCoordinates = std::unique_ptr<Vec2f []>(new Vec2f[numTris * 3]);
		for (uint32_t i = 0, k = 0; i < nfaces; ++i) { // for each face
			for (uint32_t j = 0; j < faceIndex[i] - 2; ++j) { // for each triangle in the face
				trisIndex[l] = vertsIndex[k];
				trisIndex[l + 1] = vertsIndex[k + j + 1];
				trisIndex[l + 2] = vertsIndex[k + j + 2];
				N[l] = normals[k];
				N[l + 1] = normals[k + j + 1];
				N[l + 2] = normals[k + j + 2];
				texCoordinates[l] = st[k];
				texCoordinates[l + 1] = st[k + j + 1];
				texCoordinates[l + 2] = st[k + j + 2];
				l += 3;
			}
			k += faceIndex[i];
		}

		// you can use move if the input geometry is already triangulated
		//N = std::move(normals); // transfer ownership
		//texCoordinates = std::move(st); // transfer ownership
	}

	TriangleMesh(
		const Matrix44f &o2w,
		const uint32_t nfaces,
		const std::unique_ptr<uint32_t []> &faceIndex,
		const std::unique_ptr<uint32_t []> &vertsIndex,
		const std::unique_ptr<Vec3f []> &verts,
		std::unique_ptr<Vec3f []> &normals,
		std::unique_ptr<Vec2f []> &st) :
			Object(o2w),
			numTris(0)
	{
		uint32_t k = 0, maxVertIndex = 0;
		// find out how many triangles we need to create for this mesh
		for (uint32_t i = 0; i < nfaces; ++i) {
			numTris += faceIndex[i] - 2;
			for (uint32_t j = 0; j < faceIndex[i]; ++j) {
				if (vertsIndex[k + j] > maxVertIndex)
					maxVertIndex = vertsIndex[k + j];
			}
			k += faceIndex[i];
		}
		maxVertIndex += 1;

		// allocate memory to store the position of the mesh vertices
		P = std::unique_ptr<Vec3f []>(new Vec3f[maxVertIndex]);
		for (uint32_t i = 0; i < maxVertIndex; ++i) {
			objectToWorld.multVecMatrix(verts[i], P[i]);
		}

		// allocate memory to store triangle indices
		trisIndex = std::unique_ptr<uint32_t []>(new uint32_t [numTris * 3]);
		uint32_t l = 0;
		N = std::unique_ptr<Vec3f []>(new Vec3f[numTris * 3]);
		texCoordinates = std::unique_ptr<Vec2f []>(new Vec2f[numTris * 3]);

		Matrix44f transformNormals = worldToObject.transpose();
		// generate the triangle index array and set normals and st coordinates
		for (uint32_t i = 0, k = 0; i < nfaces; ++i) { // for each face
			for (uint32_t j = 0; j < faceIndex[i] - 2; ++j) { // for each triangle in the face
				trisIndex[l] = vertsIndex[k];
				trisIndex[l + 1] = vertsIndex[k + j + 1];
				trisIndex[l + 2] = vertsIndex[k + j + 2];
				transformNormals.multDirMatrix(normals[k], N[l]);
				transformNormals.multDirMatrix(normals[k + j + 1], N[l + 1]);
				transformNormals.multDirMatrix(normals[k + j + 2], N[l + 2]);
				N[l].normalize();
				N[l + 1].normalize();
				N[l + 2].normalize();
				texCoordinates[l] = st[k];
				texCoordinates[l + 1] = st[k + j + 1];
				texCoordinates[l + 2] = st[k + j + 2];
				l += 3;
			}
			k += faceIndex[i];
		}
	}

	// Test if the ray intersects this triangle mesh with cross products
	bool intersect(const Vec3f &orig, const Vec3f &dir, float &tnear, uint32_t &triIndex, Vec2f &uv) const
	{
		uint32_t j = 0;
		bool intersect = false;
		for (uint32_t k = 0; k < numTris; ++k) {
			const Vec3f & v0 = P[trisIndex[j]];
			const Vec3f & v1 = P[trisIndex[j + 1]];
			const Vec3f & v2 = P[trisIndex[j + 2]];
			float t, u, v;
			if (scratch::geometry_utils::rayTriangleIntersect(orig, dir, v0, v1, v2, t, u, v) && t < tnear) {
				tnear = t;
				uv.x = u;
				uv.y = v;
				triIndex = k;
				intersect |= true;
			}
			j += 3;
		}

		return intersect;
	}

	void getSurfaceProperties(
		const Vec3f &hitPoint,
		const Vec3f &viewDirection,
		const uint32_t &triIndex,
		const Vec2f &uv,
		Vec3f &hitNormal,
		Vec2f &hitTextureCoordinates) const
	{
		if (smoothShading) {
			// vertex normal
			const Vec3f &n0 = N[triIndex * 3];
			const Vec3f &n1 = N[triIndex * 3 + 1];
			const Vec3f &n2 = N[triIndex * 3 + 2];
			hitNormal = (1 - uv.x - uv.y) * n0 + uv.x * n1 + uv.y * n2;
		}
		else {
			// face normal
			const Vec3f &v0 = P[trisIndex[triIndex * 3]];
			const Vec3f &v1 = P[trisIndex[triIndex * 3 + 1]];
			const Vec3f &v2 = P[trisIndex[triIndex * 3 + 2]];
			hitNormal = (v1 - v0).crossProduct(v2 - v0);
		}

		hitNormal.normalize();

		// texture coordinates
		const Vec2f &st0 = texCoordinates[triIndex * 3];
		const Vec2f &st1 = texCoordinates[triIndex * 3 + 1];
		const Vec2f &st2 = texCoordinates[triIndex * 3 + 2];
		hitTextureCoordinates = (1 - uv.x - uv.y) * st0 + uv.x * st1 + uv.y * st2;
	}

	Vec3f evalDiffuseColor(const Vec2f &st) const
	{
		float scale = 5.f;
		float pattern = (fmodf(st.x * scale, 1.f) > 0.5f) ^ (fmodf(st.y * scale, 1.f) > 0.5f);
		return mix(Vec3f(0.815f, 0.235f, 0.031f), Vec3f(0.937f, 0.937f, 0.231f), pattern);
	}


	// member variables
	uint32_t numTris; // number of triangles
	std::unique_ptr<Vec3f []> P; // triangles vertex position
	std::unique_ptr<uint32_t []> trisIndex; // vertex index array
	std::unique_ptr<Vec3f []> N; // triangles vertex normals
	std::unique_ptr<Vec2f []> texCoordinates; // triangles texture coordinates
};

//   Copyright 2013 Henry Schäfer
//
//   Licensed under the Apache License, Version 2.0 (the "Apache License")
//
//   You may obtain a copy of the Apache License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the Apache License  is  distributed on an 
//   "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//	 either express or implied. See the Apache License for the specific
//   language governing permissions and limitations under the Apache License.
//

cbuffer cbVoxelGrid : register(b9) {
	float4x4 g_matNormal;
	float4x4 g_matModelToVoxel;
	uint2 g_stride;
	uint2 g_vgpadding;
	uint3 g_gridSize;
};



bool IsVoxelSet(int3 pos){
	// CHECKME quick fix for y flip
	int p = pos.x * g_stride.x + (g_gridSize.y-pos.y-1) * g_stride.y + (pos.z >> 5);
	
	int bit = pos.z & 31;
	uint voxels = g_bufVoxels[p];
	return (voxels & (1u << uint(bit))) != 0u;
}

float copysign(float x, float y) {
	return y < 0.0 ? -x : x;
	// todo use sign
}

#define PINF asfloat(0x7f800000)
#define MINF asfloat(0xff800000)
#define INTMAX 0x7fffffff

// Implementation of the SIGGRAPH ray-box intersection test
// http://www.siggraph.org/education/materials/HyperGraph/raytrace/rtinter3.htm
bool intersectRayBoxSafe(float3 o, float3 d, float3 minCorner, float3 maxCorner, float tMinOff, out float tNear, out float tFar, bool useOffset)
{
	tNear = MINF; tFar = PINF; // Initialize

	[unroll(3)]
	for(uint i = 0; i<3; i++) // for all slabs
	{
		//if(d[i] != 0.0f) // ray is parallel to plane
		{						
			// if the ray is NOT parallel to the plane
			float t1 = (minCorner[i] - o[i]) / d[i]; // compute the intersection distance to the planes
			float t2 = (maxCorner[i] - o[i]) / d[i];

			if(t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; } // by definition t1 is first intersection

			if(t1 > tNear) tNear = t1; // we want largest tNear
			if(t2 < tFar) tFar = t2; // we want smallest tFar
			
			if(tNear > tFar) return false; // box is missed
			if(tFar < 0.0f) return false; // box is behind ray
		}
	}

	if(useOffset) tNear = max(tMinOff, tNear);

	return tNear <= tFar;
}

bool intersectRayBoxFast(in float3 p, in float3 d, in float3 bmin, in float3  bmax, out float tn, out float tf) {
	float3 id = 1.0f / d;
	float3 ol = (bmin - p) * id;
	float3 oh = (bmax - p) * id;
	float3 l = min(ol, oh);
	float3 h = max(ol, oh);
	tf = min(h.x,min(h.y, h.z));
	tn = max(l.x,max(l.y, l.z));
	//return (fmaxf(tn, 0.0f) <= tf) ? ((tn < 0.0f) ? tf : tn) : PINF;
	//if (max(tn, 0.0f) <= tf)
	if (max(tn, 0.0f) <= tf)
	{
		//if (tn < 0) {
		//	float tmp = tn;
		//	tn = tf;
		//	tf = tmp;
		//}
		return true;
	}
	else
		return false;// : PINF;
}

bool pointInAABBTest(in float3 p)
{
	if (all(p >= 0) && all(p < (float3)g_gridSize))
		return true;
	else
		return false;
}

// ray origin in voxel space, ray direction in voxel space
// returns traveled dist in voxel space
bool VoxelDDA(in float3 origin, in float3 dir, out float dist)
{
	dist = 0;
	//const float eps = exp2(-50.0);
	const float eps = 2 * MINF;
	
	if (abs(dir.x) < eps) dir.x = copysign(eps, dir.x);
	if (abs(dir.y) < eps) dir.y = copysign(eps, dir.y);
	if (abs(dir.z) < eps) dir.z = copysign(eps, dir.z);


	float3 deltaT = abs(1.0 / dir);	
	//deltaT = abs(deltaT);

	// determine intersection points with voxel grid box
	float tEnter= MINF;
	float tExit = PINF;
#if 0
	if(!intersectRayBoxSafe(origin,dir, 0, g_gridSize, 0.f, tEnter, tExit, false))			
		return false;
#else
#if 0
	if (!intersectRayBoxFast(origin, dir, 0, g_gridSize, tEnter, tExit))
		return false;
#else
	if (!pointInAABBTest(origin))
		return false;
	tEnter = 0;
#endif
#endif
	//if (sign(tEnter) < 0) return false;
	
	tEnter = max(tEnter - 0.5 * min(deltaT.x, min(deltaT.y, deltaT.z)), 0.0);		// start outside grid unless origin is inside
	//tEnter = 0;
	float3 p = origin;// +tEnter * dir;
	int3 currentVoxel = floor(p);
		
	//if (any(currentVoxel.xyz < 0) || any(currentVoxel.xyz >= int3(g_gridSize)) || !IsVoxelSet(currentVoxel)) 
	//	return false;

	if (any(currentVoxel.xyz < 0) || any(currentVoxel.xyz >= int3(g_gridSize)))
		return false;
	
	//float3 tMax = PINF;
	//if(dir.x > 0.0) tMax.x = (float(currentVoxel.x + 1) - p.x) * deltaT.x;
	//if(dir.x < 0.0) tMax.x = (p.x - float(currentVoxel.x))	 * deltaT.x;
	////
	//if(dir.y > 0.0) tMax.y = (float(currentVoxel.y + 1) - p.y) * deltaT.y;
	//if(dir.y < 0.0) tMax.y = (p.y - float(currentVoxel.y))	 * deltaT.y;
	////	
	//if(dir.z > 0.0) tMax.z = (float(currentVoxel.z + 1) - p.z) * deltaT.z;
	//if(dir.z < 0.0) tMax.z = (p.z - float(currentVoxel.z))	 * deltaT.z;
	////

	//float3 tMax = PINF;
	float3 s = sign(dir);
	float3 tMax = deltaT * s * (float3(currentVoxel + max(s, 0.f)) - p);
	//tMax *= s*deltaT;



	//float t = min(tMax.x, min(tMax.y, tMax.z));

	//float3 tMax = currentVoxel *deltaT;
	float t;
	//int3 cellStep = 1;
	int3 cellStep = sign(dir);  
	//if (dir.x <= 0.0) cellStep.x = -1;
	//if (dir.y <= 0.0) cellStep.y = -1;
	//if (dir.z <= 0.0) cellStep.z = -1;
		
	uint maxSteps =  g_gridSize.x + g_gridSize.y + g_gridSize.z + 1;	
	maxSteps = min(8, maxSteps);	// HENRY CHECKME, we dont have to go too deep

	[allow_uav_condition]		
	for(uint i = 0; i < maxSteps; i++) {
				
		t = min(tMax.x, min(tMax.y, tMax.z));
				
		//if(tEnter + t >= tExit)	 
		//	break;

		if (IsVoxelSet(currentVoxel))
			dist = t;
		
		if(tMax.x <= t) { tMax.x += deltaT.x; currentVoxel.x += cellStep.x; }
		if(tMax.y <= t) { tMax.y += deltaT.y; currentVoxel.y += cellStep.y; }
		if(tMax.z <= t) { tMax.z += deltaT.z; currentVoxel.z += cellStep.z; }		
		
		if(any(currentVoxel.xyz < 0) || any(currentVoxel.xyz >= int3(g_gridSize))) 
			break;
				
	}	

	if (dist > 0)	return true;
	else			return false;
}
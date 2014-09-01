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

cbuffer cbRaycasting : register(b10) {
	float4x4 g_matQuadToVoxel;
	float4x4 g_matVoxelToScreen;

	float3 g_rayOrigin;
    float padding1;

	float3 g_voxLightPos;
    float padding2;

	uint2 g_stride;
    uint2 padding3;

	uint3 g_gridSize;
	bool g_showLines;
};


//==============================================================================================================================================================

Buffer<uint> g_bufVoxels : register(t0);

//==============================================================================================================================================================

struct VSInput_Quad {
	float4 pos			: POSITION;
	float2 texcoord		: TEXCOORD0;
};

struct PSInput_Quad {
    float4 pos			: SV_Position;
	float4 start		: RAY_START_POS;
    float2 uv           : UV;
};

struct PSOutput_Color {
	float4 color		: SV_Target;
	float depth			: SV_Depth;
};

//==============================================================================================================================================================

bool IsVoxelSet(int3 pos){
	//int p = pos.x * g_stride.x + pos.y * g_stride.y + (pos.z >> 5);
	
	// CHECKME quick fix for y flip
	int p = pos.x * g_stride.x + (g_gridSize.y-pos.y-1) * g_stride.y + (pos.z >> 5);
	
	
	int bit = pos.z & 31;
	uint voxels = g_bufVoxels[p];
	return (voxels & (1u << uint(bit))) != 0u;
}

float copysign(float x, float y) {
	return y < 0.0 ? -x : x;
}

float3 ShadeVoxel(int3 cell, float3 p, float3 n) {
	float3 l = g_voxLightPos;
	float dotNV = dot(n, normalize(l-p));

	float a = float(cell.x) / float(g_gridSize.x);

	const float3 color0 = float3(1.0, 0.0, 0.0);
	const float3 color1 = float3(1.0, 1.0, 0.0);
	const float3 color2 = float3(0.0, 1.0, 0.0);
	float3 color;
	if(a < 0.5)
		color = lerp(color0, color1, 2.0 * a);
	else
		color = lerp(color1, color2, 2.0 * a - 1.0);

	return dotNV * color;
}

static float DEPTH = 0.0f;
bool CastRay(float3 o, float3 d, out float3 color, bool lines) {
	const float fltMax = 3.402823466e+38;
	const float eps = exp2(-50.0);

	color = 0.0;

	//DEPTH = 1.0 - d.x * 0.01;

	if(abs(d.x) < eps) d.x = copysign(eps, d.x);
	if(abs(d.y) < eps) d.y = copysign(eps, d.y);
	if(abs(d.z) < eps) d.z = copysign(eps, d.z);

	float3 deltaT = 1.0 / d;

	// determine intersection points with voxel grid box
	float3 tBox0 = (0.0 - o) * deltaT;
	float3 tBox1 = (float3(g_gridSize) - o) * deltaT; 

	float3 tBoxMax = max(tBox0, tBox1);
	float3 tBoxMin = min(tBox0, tBox1);

	float tEnter = max(tBoxMin.x, max(tBoxMin.y, tBoxMin.z));
	float tExit  = min(tBoxMax.x, min(tBoxMax.y, tBoxMax.z));

	if(tEnter > tExit || tExit < 0.0)
		return false;
		
	deltaT = abs(deltaT);
	float t0 = max(tEnter - 0.5 * min(deltaT.x, min(deltaT.y, deltaT.z)), 0.0);		// start outside grid unless origin is inside

	float3 p = o + t0 * d;

	int3 cellStep = 1;
	if(d.x < 0.0) cellStep.x = -1;
	if(d.y < 0.0) cellStep.y = -1;
	if(d.z < 0.0) cellStep.z = -1;

	int3 cell;
	cell.x = int(floor(p.x));
	cell.y = int(floor(p.y));
	cell.z = int(floor(p.z));

	if(d.x < 0.0 && frac(p.x) == 0.0) cell.x--;
	if(d.y < 0.0 && frac(p.y) == 0.0) cell.y--;
	if(d.z < 0.0 && frac(p.z) == 0.0) cell.z--;

	float3 tMax = fltMax;
	if(d.x > 0.0) tMax.x = (float(cell.x + 1) - p.x) * deltaT.x;
	if(d.x < 0.0) tMax.x = (p.x - float(cell.x)) * deltaT.x;
	if(d.y > 0.0) tMax.y = (float(cell.y + 1) - p.y) * deltaT.y;
	if(d.y < 0.0) tMax.y = (p.y - float(cell.y)) * deltaT.y;
	if(d.z > 0.0) tMax.z = (float(cell.z + 1) - p.z) * deltaT.z;
	if(d.z < 0.0) tMax.z = (p.z - float(cell.z)) * deltaT.z;

	// traverse voxel grid until ray hits a voxel or grid is left
	int maxSteps = g_gridSize.x + g_gridSize.y + g_gridSize.z + 1;
	float t;
	float3 tMaxPrev;
	for(int i = 0; i < maxSteps; i++) {
		t = min(tMax.x, min(tMax.y, tMax.z));
		if(t0 + t >= tExit)
			return false;

		tMaxPrev = tMax;
		if(tMax.x <= t) { tMax.x += deltaT.x; cell.x += cellStep.x; }
		if(tMax.y <= t) { tMax.y += deltaT.y; cell.y += cellStep.y; }
		if(tMax.z <= t) { tMax.z += deltaT.z; cell.z += cellStep.z; }

		if(any(cell.xyz < 0))
			continue;
		if(any(cell.xyz >= int3(g_gridSize)))
			continue;

		if(IsVoxelSet(cell))
			break;
	}

	// process hit point
	float3 n = 0.0;
	if(tMaxPrev.x <= t) n.x = d.x > 0.0 ? -1.0 : 1.0;
	if(tMaxPrev.y <= t) n.y = d.y > 0.0 ? -1.0 : 1.0;
	if(tMaxPrev.z <= t) n.z = d.z > 0.0 ? -1.0 : 1.0;
	n = normalize(n);

	p = o + (t0 + t) * d;

	color = ShadeVoxel(cell, p, n);
	if(lines) {
		float3 sP = mul(g_matVoxelToScreen, float4(p, 1.0)).xyw;
		sP.xy /= sP.z;
		//DEPTH = sP.z;
		// determine closest voxel edge
		float minDist = 10.0;
		float4 sPn;
		if(tMaxPrev.x > t) {
			float dist = 0.5 - abs(0.5 - frac(abs(p.x)));
			sPn = mul(g_matVoxelToScreen, float4(p.x + dist, p.y, p.z, 1.0)).xyzw;
			minDist = min(minDist, distance(sP.xy, sPn.xy / sPn.w));
		}

		if(tMaxPrev.y > t) {
			float dist = 0.5 - abs(0.5 - frac(abs(p.y)));
			sPn = mul(g_matVoxelToScreen, float4(p.x, p.y + dist, p.z, 1.0)).xyzw;
			minDist = min(minDist, distance(sP.xy, sPn.xy / sPn.w));
		}

		if(tMaxPrev.z > t) {
			float dist = 0.5 - abs(0.5 - frac(abs(p.z)));
			sPn = mul(g_matVoxelToScreen, float4(p.x, p.y, p.z + dist, 1.0)).xyzw;
			minDist = min(minDist, distance(sP.xy, sPn.xy / sPn.w));
		}

		DEPTH = sPn.z/sPn.w;

		// blend in line if closest edge overlaps pixel
		if(minDist < 1.0) {
			float a = exp2(-2.0 * pow(minDist * 1.8, 4.0));
			const float3 lineColor = 0.2;
			color = lerp(color, lineColor, a);
		}
	}

	return true;
}

#define PINF asfloat(0x7f800000)
#define MINF asfloat(0xff800000)
#define INTMAX 0x7fffffff

// Implementation of the SIGGRAPH ray-box intersection test
// http://www.siggraph.org/education/materials/HyperGraph/raytrace/rtinter3.htm
bool intersectRayBoxSafe(float3 o, float3 d, float3 minCorner, float3 maxCorner, float tMinOff, out float tNear, out float tFar, bool useOffset)
{
	tNear = MINF; tFar = PINF; // Initialize

	[unroll]
	for(uint i = 0; i<3; i++) // for all slabs
	{
		if(d[i] != 0.0f) // ray is parallel to plane
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

//bool VoxelDDA2(in float3 o, in float3 d, out float dist)
//{
//	const float fltMax = 3.402823466e+38;
//	const float eps = exp2(-50.0);
//	
//
//	if(abs(d.x) < eps) d.x = copysign(eps, d.x);
//	if(abs(d.y) < eps) d.y = copysign(eps, d.y);
//	if(abs(d.z) < eps) d.z = copysign(eps, d.z);
//
//	float3 deltaT = 1.0 / d;
//
//	// determine intersection points with voxel grid box
//	float3 tBox0 = (0.0 - o) * deltaT;
//	float3 tBox1 = (float3(g_gridSize) - o) * deltaT; 
//
//	float3 tBoxMax = max(tBox0, tBox1);
//	float3 tBoxMin = min(tBox0, tBox1);
//
//	float tEnter = max(tBoxMin.x, max(tBoxMin.y, tBoxMin.z));
//	float tExit  = min(tBoxMax.x, min(tBoxMax.y, tBoxMax.z));
//
//	if(tEnter > tExit || tExit < 0.0)
//		return false;
//		
//	deltaT = abs(deltaT);
//	float t0 = max(tEnter - 0.5 * min(deltaT.x, min(deltaT.y, deltaT.z)), 0.0);		// start outside grid unless origin is inside
//
//	float3 p = o + t0 * d;
//
//	int3 cellStep = 1;
//	if(d.x < 0.0) cellStep.x = -1;
//	if(d.y < 0.0) cellStep.y = -1;
//	if(d.z < 0.0) cellStep.z = -1;
//
//	int3 cell;
//	cell.x = int(floor(p.x));
//	cell.y = int(floor(p.y));
//	cell.z = int(floor(p.z));
//
//	if(d.x < 0.0 && frac(p.x) == 0.0) cell.x--;
//	if(d.y < 0.0 && frac(p.y) == 0.0) cell.y--;
//	if(d.z < 0.0 && frac(p.z) == 0.0) cell.z--;
//
//	float3 tMax = fltMax;
//	if(d.x > 0.0) tMax.x = (float(cell.x + 1) - p.x) * deltaT.x;
//	if(d.x < 0.0) tMax.x = (p.x - float(cell.x)) * deltaT.x;
//	if(d.y > 0.0) tMax.y = (float(cell.y + 1) - p.y) * deltaT.y;
//	if(d.y < 0.0) tMax.y = (p.y - float(cell.y)) * deltaT.y;
//	if(d.z > 0.0) tMax.z = (float(cell.z + 1) - p.z) * deltaT.z;
//	if(d.z < 0.0) tMax.z = (p.z - float(cell.z)) * deltaT.z;
//
//	//color = float3(1,1,1);//float3(cell)/g_gridSize;
//	dist = 0;
//	float alpha = 1.f/70;//max(g_gridSize.x, max(g_gridSize.y, g_gridSize.z));
//
//	
//	int maxSteps = g_gridSize.x + g_gridSize.y + g_gridSize.z + 1;
//	float t = min(tMax.x, min(tMax.y, tMax.z));
//	float tPrev;
//	float3 tMaxPrev;
//	for(int i = 0; i < maxSteps; i++) {
//		tPrev = t;
//		t = min(tMax.x, min(tMax.y, tMax.z));
//		if(t0 + t >= tExit)
//		{
//			break;			
//		}
//
//		tMaxPrev = tMax;
//		if(tMax.x <= t) { tMax.x += deltaT.x; cell.x += cellStep.x; }
//		if(tMax.y <= t) { tMax.y += deltaT.y; cell.y += cellStep.y; }
//		if(tMax.z <= t) { tMax.z += deltaT.z; cell.z += cellStep.z; }
//
//		if(any(cell.xyz <= 0))			continue;
//		if(any(cell.xyz >= int3(g_gridSize)+1))			break;
//
//		if(IsVoxelSet(cell))
//		{
//			dist += (t-tPrev)/64.0f;
//			//dist += alpha; 
//			//dist = t/128.0;
//			//dist+=1;
//			//break;
//		}
//	}
//	//dist /= 128.0;
//	return true;
//}

bool VoxelDDA(in float3 o, in float3 d, out float dist)
{
	const float fltMax = PINF;//3.402823466e+38;
	const float eps = exp2(-50.0);
	

	if(abs(d.x) < eps) d.x = copysign(eps, d.x);
	if(abs(d.y) < eps) d.y = copysign(eps, d.y);
	if(abs(d.z) < eps) d.z = copysign(eps, d.z);

	float3 deltaT = 1.f / d;
	deltaT = abs(deltaT);

	//// determine intersection points with voxel grid box
	float tEnter= MINF;
	float tExit = PINF;
	bool isctGrid = intersectRayBoxSafe(o,d, 0, g_gridSize, 0.f, tEnter, tExit, false);
	if(isctGrid == false)	return false;
	//dist = 1.0f;
	//return true;
	tEnter = max(tEnter - 0.5 * min(deltaT.x, min(deltaT.y, deltaT.z)), 0.0);		// start outside grid unless origin is inside
	//tEnter += 0;
	float3 p = o + tEnter * d;

	int3 cellStep = 1;
	if(d.x < 0.0) cellStep.x = -1;
	if(d.y < 0.0) cellStep.y = -1;
	if(d.z < 0.0) cellStep.z = -1;

	int3 currentVoxel = floor(p);

	if(d.x < 0.0 && frac(p.x) == 0.0) currentVoxel.x--;
	if(d.y < 0.0 && frac(p.y) == 0.0) currentVoxel.y--;
	if(d.z < 0.0 && frac(p.z) == 0.0) currentVoxel.z--;

	float3 tMax = fltMax;
	if(d.x > 0.0) tMax.x = (float(currentVoxel.x + 1) - p.x) * deltaT.x;
	if(d.x < 0.0) tMax.x = (p.x - float(currentVoxel.x)) * deltaT.x;

	if(d.y > 0.0) tMax.y = (float(currentVoxel.y + 1) - p.y) * deltaT.y;
	if(d.y < 0.0) tMax.y = (p.y - float(currentVoxel.y)) * deltaT.y;

	if(d.z > 0.0) tMax.z = (float(currentVoxel.z + 1) - p.z) * deltaT.z;
	if(d.z < 0.0) tMax.z = (p.z - float(currentVoxel.z)) * deltaT.z;

	dist = 0;
		
	int maxSteps = g_gridSize.x + g_gridSize.y + g_gridSize.z + 1;
	float t = min(tMax.x, min(tMax.y, tMax.z));

	float tPrev;
	float3 tMaxPrev;

	// alpha incr for vis
	const float incr = 1.f/96;


	bool firstVoxelSet= false;
	//[unroll]
	for(int i = 0; i < maxSteps; i++) {
		tPrev = t;
		t = min(tMax.x, min(tMax.y, tMax.z));
		if(tEnter + t >= tExit)			break;					

		tMaxPrev = tMax;

		if(tMax.x <= t) { tMax.x += deltaT.x; currentVoxel.x += cellStep.x; }
		if(tMax.y <= t) { tMax.y += deltaT.y; currentVoxel.y += cellStep.y; }
		if(tMax.z <= t) { tMax.z += deltaT.z; currentVoxel.z += cellStep.z; }

		
		if(any(currentVoxel.xyz < 0))							break;
		if(any(currentVoxel.xyz >= int3(g_gridSize+1)))			break;

		if(IsVoxelSet(currentVoxel))
		{			
			dist += incr * (t-tPrev) ;
			if(!firstVoxelSet) firstVoxelSet = true;
		}
		else
		{
			// comment next line for traveled dist for all voxels on path
			if(firstVoxelSet)		break;
		}
	}
	
	return true;
}


bool computeDist(in float3 origin, in float3 dir, out float4 color)
{	
	float dist = 0.f;
	
	bool ret = VoxelDDA(origin, dir, dist);

	dist = 1-exp(-dist);
	color = float4(1,0,1,dist);
	return ret;

}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------

struct VSIn
{
    uint Pos	: SV_VertexID ;
};


PSInput_Quad VS_RenderVoxelizationRaycasting(VSIn inn)
{
    PSInput_Quad output;

    output.pos.y  = -1.0f + (inn.Pos%2) * 2.0f ;
    output.pos.x  = -1.0f + (inn.Pos/2) * 2.0f;
    output.pos.z = .5;
    output.pos.w = 1;
	
    output.uv.x = inn.Pos/2;
    output.uv.y = 1.0f - inn.Pos%2;

	float2 ndcxy = float2(output.uv.x, 1.0f - output.uv.y);
    output.start = mul(g_matQuadToVoxel, float4(ndcxy * 2.0f - 1.0f, 0.0, 1.0));	
	return output;
}



//PSInput_Quad VS_RenderVoxelizationRaycasting(VSInput_Quad input) {
//    PSInput_Quad output;
//
//    output.pos = input.pos;
//    float2 ndcxy = float2(input.texcoord.x, 1.0f - input.texcoord.y);
//    output.start = mul(g_matQuadToVoxel, float4(ndcxy * 2.0f - 1.0f, 0.0, 1.0));	
//    output.uv = float2(input.texcoord.x,input.texcoord.y);
//    return output;
//}

PSOutput_Color PS_RenderVoxelizationRaycasting(PSInput_Quad input) {
	PSOutput_Color output;
	
	//output.color = 1;
	//output.depth = 0;
	//return output;

	// PROJ
	float3 o = g_rayOrigin;
	float3 d = normalize((input.start.xyz / input.start.w) - o);
	
	// ORTHO
	//float3 o = input.start.xyz / input.start.w;
	//float3 d = normalize(mul(g_matQuadToVoxel, float3(0,0,1)).xyz);

		
	if(!CastRay(o, d, output.color.xyz, g_showLines))
	{		
		discard;
		output.color.w = 0.0;
		output.depth = 0.1;
		return output;
	}
	else
	{
		output.color.w = 1.0f;
	}
	output.color.z = 1.0 - output.color.z;

	// origin and dist are in voxel space
	//if(!computeDist(o, d, output.color))		discard;
	output.depth = DEPTH;
	//output.color = 1;
	//output.depth = 1;
	return output;
}

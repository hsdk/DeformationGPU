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

#include "Intersect.h.hlsl"

//#define WITH_CON

cbuffer ConfigCB : register(b6) {
	int g_GregoryQuadOffsetBase;
	int g_PrimitiveIdBase;			// patch id of first patch to access ptex, far patch table
	int g_IndexStart;				// index of the first vertex in global index array
	uint g_NumVertexComponents;		// num float elements per vertex, for stride in vertex buffer, the first 3 are always pos
	uint g_NumIndicesPerPatch;		// num indices per patch, TODO better use shader define
	uint g_NumPatches;
};

// readables (SRVs)

Buffer<float>		g_VertexBuffer			: register(t0);	// t0 vertex buffer, float, numcomponents set in cb
Buffer<uint>		g_IndexBuffer			: register(t1);	// t1 index buffer			
Buffer<uint2>		g_OsdPatchParamBuffer	: register(t2); // t2 tile rotation info and patch to tile mapping, CHECKME
Buffer<int>			g_OsdValenceBuffer		: register(t3); // t3 valence buffer for gregory patch eval
Buffer<int>			g_OsdQuadOffsetBuffer	: register(t4);
Buffer<float>		g_maxPatchDisplacement	: register(t5);

#ifndef BATCH_SIZE
#define BATCH_SIZE 4u
#endif
// writables (UAVs)
RWBuffer<uint>					g_ptexFaceVisibleUAV		: register(u0);
RWBuffer<uint>					g_ptexFaceVisibleAllUAV		: register(u1);
#ifdef TYPE_GREGORY
AppendStructuredBuffer<uint3>	g_patchDataGregoryAppend0	: register(u2);
AppendStructuredBuffer<uint3>	g_patchDataGregoryAppend1	: register(u3);
AppendStructuredBuffer<uint3>	g_patchDataGregoryAppend2	: register(u4);
AppendStructuredBuffer<uint3>	g_patchDataGregoryAppend3	: register(u5);
AppendStructuredBuffer<uint3>	g_patchDataGregoryAppend4	: register(u6);
AppendStructuredBuffer<uint3>	g_patchDataGregoryAppend5	: register(u7);
//AppendStructuredBuffer<uint3>	g_patchDataGregoryAppend6	: register(u8);
//AppendStructuredBuffer<uint3>	g_patchDataGregoryAppend7	: register(u9);

static const AppendStructuredBuffer<uint3> g_patchDataGregory[6] = { g_patchDataGregoryAppend0, g_patchDataGregoryAppend1, g_patchDataGregoryAppend2, g_patchDataGregoryAppend3, g_patchDataGregoryAppend4, g_patchDataGregoryAppend5 };// , g_patchDataGregoryAppend6, g_patchDataGregoryAppend7 };
#else
AppendStructuredBuffer<uint2>	g_patchDataRegularAppend0	: register(u2);
AppendStructuredBuffer<uint2>	g_patchDataRegularAppend1	: register(u3);
AppendStructuredBuffer<uint2>	g_patchDataRegularAppend2	: register(u4);
AppendStructuredBuffer<uint2>	g_patchDataRegularAppend3	: register(u5);
AppendStructuredBuffer<uint2>	g_patchDataRegularAppend4	: register(u6);
AppendStructuredBuffer<uint2>	g_patchDataRegularAppend5	: register(u7);
//AppendStructuredBuffer<uint2>	g_patchDataRegularAppend6	: register(u8);
//AppendStructuredBuffer<uint2>	g_patchDataRegularAppend7	: register(u9);

static const AppendStructuredBuffer<uint2> g_patchDataRegular[6] = { g_patchDataRegularAppend0, g_patchDataRegularAppend1, g_patchDataRegularAppend2, g_patchDataRegularAppend3, g_patchDataRegularAppend4, g_patchDataRegularAppend5 };// , g_patchDataRegularAppend6, g_patchDataRegularAppend7 };
#endif



static const matrix Q = matrix(	0.1666666666667f, 0.6666666666667f, 0.1666666666667f, 0.0f,
								0.0f, 0.6666666666667f, 0.3333333333333f, 0.0f,
								0.0f, 0.3333333333333f, 0.6666666666667f, 0.0f,
								0.0f, 0.1666666666667f, 0.6666666666667f, 0.1666666666667f);

groupshared float3 controlPoints[2][16];
groupshared float3 controlPointsBezier[2][16];
groupshared float3 controlPointsBezierScan[2][16];

float3 GetVertex(in uint localPatchID, in uint vertexIndex)
{
	int vIdx = g_IndexBuffer[g_IndexStart + g_NumIndicesPerPatch * localPatchID + vertexIndex];

	return float3(	g_VertexBuffer[vIdx*g_NumVertexComponents + 0],
					g_VertexBuffer[vIdx*g_NumVertexComponents + 1],
					g_VertexBuffer[vIdx*g_NumVertexComponents + 2]);

}

float3 GetVertexShared(in uint blockPatchID, in uint vertexIndex) { // blockPatchID = [0 or 1]
	return controlPoints[blockPatchID][vertexIndex];
}

void ConApprox(in uint localPatchID, out float alpha, out float3 axis)
{
	uint i, j, k;

	//u direction	
	float3 dirU = normalize((GetVertex(localPatchID, 4 * 0 + 3).xyz - GetVertex(localPatchID, 4 * 0 + 0).xyz) + (GetVertex(localPatchID, 4 * 3 + 3).xyz - GetVertex(localPatchID, 4 * 3 + 0).xyz));

		float cosAngleU = 1.0f;
	[unroll(4)]
	for (i = 0; i < 4; i++) {
		[unroll]
		for (j = 0; j < 3; j++){
			float3 t = normalize(GetVertex(localPatchID, 4 * i + j + 1).xyz - GetVertex(localPatchID, 4 * i + j).xyz);
				cosAngleU = min(cosAngleU, dot(t, dirU));
		}
	}
	cosAngleU = clamp(cosAngleU, -1.0f, 1.0f);

	float3 dirV = normalize((GetVertex(localPatchID, 4 * 3 + 0).xyz - GetVertex(localPatchID, 4 * 0 + 0).xyz) + (GetVertex(localPatchID, 4 * 3 + 3).xyz - GetVertex(localPatchID, 4 * 0 + 3).xyz));

		float cosAngleV = 1.0f;
	//[unroll(4)]
	for (i = 0; i < 4; i++) {
		[unroll]
		for (j = 0; j < 3; j++)
		{
			float3 t = normalize(GetVertex(localPatchID, 4 * (j + 1) + i).xyz - GetVertex(localPatchID, 4 * j + i).xyz);
				cosAngleV = min(cosAngleV, dot(t, dirV));
		}
	}
	cosAngleV = clamp(cosAngleV, -1.0f, 1.0f);

	//float sinAngleU = sin(acos(cosAngleU));
	//float sinAngleV = sin(acos(cosAngleV));
	float sinAngleU = sqrt(1.0f - cosAngleU * cosAngleU);
	float sinAngleV = sqrt(1.0f - cosAngleV * cosAngleV);

	float cosBeta = clamp(dot(dirU, dirV), -1.0f, 1.0f);
	//float sinBeta = sin(acos(cosBeta));
	float sinBeta = sqrt(1.0f - cosBeta * cosBeta);

	float sinAlpha = sqrt(max(0.0f, sinAngleU * sinAngleU + 2.0f * sinAngleU * sinAngleV * cosBeta + sinAngleV * sinAngleV)) / sinBeta;
	sinAlpha = clamp(sinAlpha, -1.0f, 1.0f);
	//float alpha = asin(sinAlpha);
	//float cosAlpha = cos(alpha);
	//float3 a = -normalize(cross(dirU,dirV));
	alpha = asin(sinAlpha);
	axis = -normalize(cross(dirU, dirV));

}

void ConApproxShared(in uint blockPatchID, out float alpha, out float3 axis)
{
	//u direction	
	float3 dirU = normalize((GetVertexShared(blockPatchID, 4 * 0 + 3).xyz - GetVertexShared(blockPatchID, 4 * 0 + 0).xyz) + (GetVertexShared(blockPatchID, 4 * 3 + 3).xyz - GetVertexShared(blockPatchID, 4 * 3 + 0).xyz));
	float cosAngleU = 1.0f;
	//[unroll(4)]
	for (uint i = 0; i < 4; i++) {
		//[unroll]
		for (uint j = 0; j < 3; j++) // why not to 4?
		{
			float3  t = normalize(GetVertexShared(blockPatchID, 4 * i + j + 1).xyz - GetVertexShared(blockPatchID, 4 * i + j).xyz);
			cosAngleU = min(cosAngleU, dot(t, dirU));
		}
	}
	cosAngleU = clamp(cosAngleU, -1.0f, 1.0f);


	float3 dirV = normalize((GetVertexShared(blockPatchID, 4 * 3 + 0).xyz - GetVertexShared(blockPatchID, 4 * 0 + 0).xyz) + (GetVertexShared(blockPatchID, 4 * 3 + 3).xyz - GetVertexShared(blockPatchID, 4 * 0 + 3).xyz));
	float cosAngleV = 1.0f;
	//[unroll(4)]
	for (i = 0; i < 4; i++) {
		//[unroll]
		for (uint j = 0; j < 3; j++) // why not to 4?
		{
			float3  t = normalize(GetVertexShared(blockPatchID, 4 * (j + 1) + i).xyz - GetVertexShared(blockPatchID, 4 * j + i).xyz);
			cosAngleV = min(cosAngleV, dot(t, dirV));
		}
	}
	cosAngleV = clamp(cosAngleV, -1.0f, 1.0f);

	//float sinAngleU = sin(acos(cosAngleU));
	//float sinAngleV = sin(acos(cosAngleV));

	axis = -normalize(cross(dirU, dirV));
	float cosBeta = clamp(dot(dirU, dirV), -1.0f, 1.0f);
	//float sinBeta = sin(acos(cosBeta));
	float sinBeta = max(sqrt(1.0f - cosBeta * cosBeta), 0.000001f); // CHECKME HENRY
	float sinAngleU = sqrt(1.0f - cosAngleU * cosAngleU);
	float sinAngleV = sqrt(1.0f - cosAngleV * cosAngleV);

	float sinAlpha = sqrt(max(0.0f, sinAngleU * sinAngleU + 2.0f * sinAngleU * sinAngleV * cosBeta + sinAngleV * sinAngleV)) / sinBeta;
	sinAlpha = clamp(sinAlpha, -1.0f, 1.0f);
	alpha = asin(sinAlpha);

}


void BoundCone(in float alpha, in float3 axis, inout float3 extP, inout float3 extM) {

#if 0
	float cosAlpha = cos(alpha);
	float3 acosAxis = acos(axis);
	
	float3 extPMax = max(cos(acosAxis + alpha), cos(acosAxis - alpha));		
	
	if (axis.x < cosAlpha) extP.x = extPMax.x;
	if (axis.y < cosAlpha) extP.y = extPMax.y;
	if (axis.z < cosAlpha) extP.z = extPMax.z;

	float3 extMMax = max(cos(M_PI - acosAxis + alpha), cos(M_PI - acosAxis - alpha));
	if (-axis.x < cosAlpha)	extM.x = extMMax.x;
	if (-axis.y < cosAlpha)	extM.y = extMMax.y;
	if (-axis.z < cosAlpha)	extM.z = extMMax.z;

	extP = saturate(extP);
	extM = saturate(extM);
#else
	// should be safe to enable // CHECKME
	float sinAlpha;
	float cosAlpha;
	sincos(alpha, sinAlpha, cosAlpha);
	float3 sqAxis = sqrt(1.f - axis*axis);
	float3 acosPlusAlpha = axis*cosAlpha - sqAxis * sinAlpha;
	float3 acosMinusAlpha = sqAxis * sinAlpha + axis * cosAlpha;
	
	float3 maxAcos = float3(max((acosPlusAlpha), (acosMinusAlpha))) - 1; // -1 for fast check to make sure extP, extM is 1 otherwise 	// hmm, cos(PI-x) is x
	extP = 1.0;
	extM = 1.0;
	extP += (axis<cosAlpha)  * maxAcos;
	extM += (-axis>cosAlpha) * maxAcos;

	extP = saturate(extP);
	extM = saturate(extM);
#endif


}

float3 vecMin(float3 vec1, float3 vec2)
{
	float3 ret;
	ret.x = min(vec1.x, vec2.x);
	ret.y = min(vec1.y, vec2.y);
	ret.z = min(vec1.z, vec2.z);
	return ret;
}

float3 vecMax(float3 vec1, float3 vec2)
{
	float3 ret;
	ret.x = max(vec1.x, vec2.x);
	ret.y = max(vec1.y, vec2.y);
	ret.z = max(vec1.z, vec2.z);
	return ret;
}

float3 scanInclusiveIntraWarpMin(uint i, uint patch) {
	uint4 X = i - uint4(1, 2, 4, 8);

		if (i >= 0x01) controlPointsBezierScan[patch][i] = vecMin(controlPointsBezierScan[patch][i], controlPointsBezierScan[patch][X.x]);
	if (i >= 0x02) controlPointsBezierScan[patch][i] = vecMin(controlPointsBezierScan[patch][i], controlPointsBezierScan[patch][X.y]);
	if (i >= 0x04) controlPointsBezierScan[patch][i] = vecMin(controlPointsBezierScan[patch][i], controlPointsBezierScan[patch][X.z]);
	if (i >= 0x08) controlPointsBezierScan[patch][i] = vecMin(controlPointsBezierScan[patch][i], controlPointsBezierScan[patch][X.w]);
	if (i >= 0x10) controlPointsBezierScan[patch][i] = vecMin(controlPointsBezierScan[patch][i], controlPointsBezierScan[patch][i - 16]);

	// checkme last needed?

	return controlPointsBezierScan[patch][i];
}

float3 scanInclusiveIntraWarpMax(uint i, uint patch) {
	uint4 X = i - uint4(1, 2, 4, 8);

		if (i >= 0x01) controlPointsBezierScan[patch][i] = vecMax(controlPointsBezierScan[patch][i], controlPointsBezierScan[patch][X.x]);
	if (i >= 0x02) controlPointsBezierScan[patch][i] = vecMax(controlPointsBezierScan[patch][i], controlPointsBezierScan[patch][X.y]);
	if (i >= 0x04) controlPointsBezierScan[patch][i] = vecMax(controlPointsBezierScan[patch][i], controlPointsBezierScan[patch][X.z]);
	if (i >= 0x08) controlPointsBezierScan[patch][i] = vecMax(controlPointsBezierScan[patch][i], controlPointsBezierScan[patch][X.w]);
	if (i >= 0x10) controlPointsBezierScan[patch][i] = vecMax(controlPointsBezierScan[patch][i], controlPointsBezierScan[patch][i - 16]);

	// checkme last needed?
	return controlPointsBezierScan[patch][i];
}

struct fl32
{
	float3 bmin;
	float3 bmax;
};


groupshared fl32 controlPointsBezierMultiScan[2][16];

fl32 vecMinMax(in fl32 minmax1, in fl32 minmax2)
{
	fl32 ret;
	ret.bmin.x = min(minmax1.bmin.x, minmax2.bmin.x);
	ret.bmin.y = min(minmax1.bmin.y, minmax2.bmin.y);
	ret.bmin.z = min(minmax1.bmin.z, minmax2.bmin.z);

	ret.bmax.x = max(minmax1.bmax.x, minmax2.bmax.x);
	ret.bmax.y = max(minmax1.bmax.y, minmax2.bmax.y);
	ret.bmax.z = max(minmax1.bmax.z, minmax2.bmax.z);

	return ret;
}



fl32 scanInclusiveIntraWarpMulti(uint i, uint patch) {
	uint4 X = i - uint4(1, 2, 4, 8);

		if (i >= 0x01) controlPointsBezierMultiScan[patch][i] = vecMinMax(controlPointsBezierMultiScan[patch][i], controlPointsBezierMultiScan[patch][X.x]);
	if (i >= 0x02) controlPointsBezierMultiScan[patch][i] = vecMinMax(controlPointsBezierMultiScan[patch][i], controlPointsBezierMultiScan[patch][X.y]);
	if (i >= 0x04) controlPointsBezierMultiScan[patch][i] = vecMinMax(controlPointsBezierMultiScan[patch][i], controlPointsBezierMultiScan[patch][X.z]);
	if (i >= 0x08) controlPointsBezierMultiScan[patch][i] = vecMinMax(controlPointsBezierMultiScan[patch][i], controlPointsBezierMultiScan[patch][X.w]);
	if (i >= 0x10) controlPointsBezierMultiScan[patch][i] = vecMinMax(controlPointsBezierMultiScan[patch][i], controlPointsBezierMultiScan[patch][i - 16]);

	return controlPointsBezierMultiScan[patch][i];
}




int GetPtexTileID(uint localPatchID)
{
	return g_OsdPatchParamBuffer[localPatchID + g_PrimitiveIdBase].x;
}

[numthreads(512, 1, 1)]
void IntersectClearCS(uint3 blockIdx : SV_GroupID,
	uint3 DTid : SV_DispatchThreadID,
	uint3 threadIdx : SV_GroupThreadID,
	uint GI : SV_GroupIndex)
{
	g_ptexFaceVisibleUAV[DTid.x] = INTERSECT_FALSE;
}

#ifndef TYPE_GREGORY
[numthreads(2, 4, 4)]
void IntersectRegularCS
(
uint3 blockIdx : SV_GroupID,
uint3 DTid : SV_DispatchThreadID,
uint3 threadIdx : SV_GroupThreadID,
uint GI : SV_GroupIndex
)
{
	//uint localPatchID = DTid.x;	// 1 patch per block
	uint localPatchID = blockIdx.x * 2 + threadIdx.x;	// 2 patches per block

	if (localPatchID >= g_NumPatches)
		return;

	
	int ptexTileID = GetPtexTileID(localPatchID);

//#ifdef SET_ALL_ACTIVE
//	g_ptexFaceVisibleUAV[ptexTileID] = INTERSECT_TRUE;
//	g_ptexFaceVisibleAllUAV[ptexTileID] = INTERSECT_TRUE;
//	uint2 patchData = uint2(localPatchID + g_PrimitiveIdBase, g_IndexStart + g_NumIndicesPerPatch * localPatchID);//, g_NumVertexComponents);
//		g_patchDataRegularAppend.Append(patchData);
//	return;
//#endif

	// Read 16 control points to shared mem
	uint i = threadIdx.y;
	uint j = threadIdx.z;
	uint localID = j + i * 4;
	int vIdx = g_IndexBuffer[g_IndexStart + g_NumIndicesPerPatch * localPatchID + localID];

	controlPoints[threadIdx.x][localID] = float3(g_VertexBuffer[vIdx*g_NumVertexComponents + 0],
												 g_VertexBuffer[vIdx*g_NumVertexComponents + 1],
												 g_VertexBuffer[vIdx*g_NumVertexComponents + 2]);

		
	// transform to Bezier
#if 0 // slower
	float3 H[4];
	for (int l = 0; l < 4; l++){
		H[l] = float3(0.0f, 0.0f, 0.0f);
		//[unroll(4)]
		for (int k = 0; k < 4; k++){
			H[l] += Q[i][k] * controlPoints[threadIdx.x][l + 4 * k].xyz;
		}
	}

	controlPointsBezier[threadIdx.x][localID] = float3(0.0f, 0.0f, 0.0f);
	for (int k = 0; k < 4; k++){
		controlPointsBezier[threadIdx.x][localID].xyz += Q[j][k] * H[k];
	}
#else
	float3 H[4];
	float3 cpBez = float3(0.0f, 0.0f, 0.0f);
	for (int l = 0; l < 4; l++){
		H[l] = float3(0.0f, 0.0f, 0.0f);
		[unroll(4)]
		for (int k = 0; k < 4; k++){
			H[l] += Q[i][k] * controlPoints[threadIdx.x][l + 4 * k].xyz;
			//cpBez += Q[j][l] * Q[i][k] * controlPoints[threadIdx.x][l + 4 * k].xyz;
		}
		cpBez += Q[j][l] * H[l];
	}
	controlPointsBezier[threadIdx.x][localID].xyz = cpBez;
#endif

	// transform into brush space
	// checkme: transform bbox to brush space should be more efficient than transforming each vertex to brush space
	// Slower variant
	//matrix modelToBrush = mul(g_mWorldToBrush, g_matModelToWorld);
	//controlPointsBezier[threadIdx.x][localID] = mul(modelToBrush, float4(cp, 1)).xyz;

	float3 cp = controlPointsBezier[threadIdx.x][localID].xyz;

		//[unroll(BATCH_SIZE)]
		//[fastopt]

		bool tileActive = false;


	for (uint batchIdx = 0; batchIdx < BATCH_SIZE; ++batchIdx)
	{
		matrix modelToOBBTranspose = transpose(g_matModelToOBB[batchIdx]);
		controlPointsBezier[threadIdx.x][localID].xyz = cp.x * modelToOBBTranspose[0].xyz + (cp.y * modelToOBBTranspose[1].xyz + (cp.z * modelToOBBTranspose[2].xyz + modelToOBBTranspose[3].xyz));

//#define SHARED_MEM_MINMAX
#ifdef SHARED_MEM_MINMAX
		//compute AABB - shared mem scan works, but overall its slower than explicit loop
		controlPointsBezierScan[threadIdx.x][localID] = controlPointsBezier[threadIdx.x][localID];
		float3 bbMax = scanInclusiveIntraWarpMax(localID, threadIdx.x);
		controlPointsBezierScan[threadIdx.x][localID] = controlPointsBezier[threadIdx.x][localID];
		float3 bbMin = scanInclusiveIntraWarpMin(localID, threadIdx.x);
#endif
		if (localID == 4 * 4 - 1)
		{
#ifndef SHARED_MEM_MINMAX
			// compute aabb 
			float3 bbMax = -FLT_MAX;
			float3 bbMin = FLT_MAX;
			//[unroll(16)]
			for (int i = 0; i < 16; ++i)
			{
				float3 cp = controlPointsBezier[threadIdx.x][i].xyz;
				bbMax = max(cp, bbMax);
				bbMin = min(cp, bbMin);
			}
#endif
#ifdef WITH_CON


			// todo make it faster
			float alpha;
			float3 axis;
						
			//ConApprox(localPatchID, alpha, axis);
			ConApproxShared(threadIdx.x, alpha, axis);
			
			//cone of normals
			float3 extP = 1.f;//float3(1.0f, 1.0f, 1.0f);
			float3 extM = 1.f;//float3(1.0f, 1.0f, 1.0f);
			BoundCone(alpha, axis, extP, extM);

			float maxDisplacement = 0;// g_displacementScaler  * 0.2;//0.2; // default value set in memorymanage::inittiledisplacement		// g_maxDisplacement* g_PerPatchDisplacementInfo[patchID*2+0];
			float minDisplacement = -g_displacementScaler * 0.2;//0.2; //-g_maxDisplacement;// 0;	//g_maxDisplacement * g_PerPatchDisplacementInfo[patchID*2+1];

#ifdef 		WITH_DYNAMIC_MAX_DISP
			minDisplacement = -g_displacementScaler * g_maxPatchDisplacement[localPatchID];
#endif
			//extend by cone of normals
			float3 coneExt = max(maxDisplacement * extP, minDisplacement * extM);
			bbMax += coneExt;
			bbMin += coneExt;

#else

#ifdef 		WITH_DYNAMIC_MAX_DISP
			bbMax = bbMax + g_displacementScaler * g_maxPatchDisplacement[localPatchID];
			bbMin = bbMin - g_displacementScaler * g_maxPatchDisplacement[localPatchID];//- maxDisplacement;
#else
			bbMin = bbMin - 0.1* g_displacementScaler;//- maxDisplacement;
			bbMax = bbMax + 0.1* g_displacementScaler;//+ minDisplacement;
#endif


#endif
			// slightly cheaper than using all intrinisc below (2 less instructions)			
			//if ((bbMin.x > 1.0f || bbMax.x < 0.0)  || (bbMin.y > 1.0f || bbMax.y < 0.0)  || (bbMin.z > 1.0f || bbMax.z < 0.0))	{ } else			
			// slightly more expensive bbox test
			if (all(bbMin <= 1) && all(bbMax >=0))
			{			
				tileActive = true;
				g_ptexFaceVisibleUAV[ptexTileID] = INTERSECT_TRUE;
				g_ptexFaceVisibleAllUAV[ptexTileID] = INTERSECT_TRUE;

				uint2 patchData = uint2(localPatchID + g_PrimitiveIdBase, g_IndexStart + g_NumIndicesPerPatch * localPatchID);//, g_NumVertexComponents);
				{
					if (batchIdx == 0)	g_patchDataRegularAppend0.Append(patchData);
#if BATCH_SIZE > 1
					if (batchIdx == 1)	g_patchDataRegularAppend1.Append(patchData);
#endif
#if BATCH_SIZE > 2
					if (batchIdx == 2)	g_patchDataRegularAppend2.Append(patchData);
#endif
#if BATCH_SIZE > 3
					if (batchIdx == 3)	g_patchDataRegularAppend3.Append(patchData);
#endif
#if BATCH_SIZE > 4
					if (batchIdx == 4)	g_patchDataRegularAppend4.Append(patchData);
#endif
#if BATCH_SIZE > 5
					if (batchIdx == 5)	g_patchDataRegularAppend5.Append(patchData);
#endif					
				}
			}
			}
	}
}
#endif


#ifdef TYPE_GREGORY
// GREGORY PATCH ISCT

//----------------------------------------------------------
// Patches.Coefficients
//----------------------------------------------------------

#if OSD_MAX_VALENCE<=10
static float ef[7] = {
	0.813008, 0.500000, 0.363636, 0.287505,
	0.238692, 0.204549, 0.179211
};
#else
static float ef[27] = {
	0.812816, 0.500000, 0.363644, 0.287514,
	0.238688, 0.204544, 0.179229, 0.159657,
	0.144042, 0.131276, 0.120632, 0.111614,
	0.103872, 0.09715, 0.0912559, 0.0860444,
	0.0814022, 0.0772401, 0.0734867, 0.0700842,
	0.0669851, 0.0641504, 0.0615475, 0.0591488,
	0.0569311, 0.0548745, 0.0529621
};
#endif

float csf(uint n, uint j)
{
	if (j % 2 == 0) {
		return cos((2.0f * M_PI * float(float(j - 0) / 2.0f)) / (float(n) + 3.0f));
	}
	else {
		return sin((2.0f * M_PI * float(float(j - 1) / 2.0f)) / (float(n) + 3.0f));
	}
}




struct SGregory
{
	float3 position;
	int valence;
#if OSD_MAX_VALENCE > 0
	float3 r[OSD_MAX_VALENCE];
#endif
	float3 e0;
	float3 e1;

	float3 Ep;
	float3 Em;
	float3 Fp;
	float3 Fm;
};

groupshared float3 controlPointsGregory[2][4];

groupshared SGregory dgreg[2][4];

void Univar4x4(in float u, out float B[4], out float D[4])
{
	float t = u;
	float s = 1.0f - u;

	float A0 = s * s;
	float A1 = 2 * s * t;
	float A2 = t * t;

	B[0] = s * A0;
	B[1] = t * A0 + s * A1;
	B[2] = t * A1 + s * A2;
	B[3] = t * A2;

	D[0] = -A0;
	D[1] = A0 - A1;
	D[2] = A1 - A2;
	D[3] = A2;
}

void
Univar4x4(in float u, out float B[4], out float D[4], out float C[4])
{
	float t = u;
	float s = 1.0f - u;

	float A0 = s * s;
	float A1 = 2 * s * t;
	float A2 = t * t;

	B[0] = s * A0;
	B[1] = t * A0 + s * A1;
	B[2] = t * A1 + s * A2;
	B[3] = t * A2;

	D[0] = -A0;
	D[1] = A0 - A1;
	D[2] = A1 - A2;
	D[3] = A2;

	A0 = -s;
	A1 = s - t;
	A2 = t;

	C[0] = -A0;
	C[1] = A0 - A1;
	C[2] = A1 - A2;
	C[3] = A2;
}


[numthreads(2, 2, 2)]
void IntersectGregoryCS
(
uint3 blockIdx : SV_GroupID,
uint3 DTid : SV_DispatchThreadID,
uint3 threadIdx : SV_GroupThreadID,
uint GI : SV_GroupIndex
)
{
	
	uint localPatchID = blockIdx.x * 2 + threadIdx.x;	// 2 patches per block
	
	if (localPatchID >= g_NumPatches)
		return;

	int ptexTileID = GetPtexTileID(localPatchID);
	//#define SET_ALL_ACTIVE
#ifdef SET_ALL_ACTIVE
	g_ptexFaceVisibleUAV[ptexTileID] = INTERSECT_TRUE;
	g_ptexFaceVisibleAllUAV[ptexTileID] = INTERSECT_TRUE;
	uint3 patchData = uint3(localPatchID + g_PrimitiveIdBase,
		g_IndexStart + g_NumIndicesPerPatch * localPatchID,
		4 * localPatchID + g_GregoryQuadOffsetBase);

	g_patchDataGregoryAppend.Append(patchData);
	return;
#endif
	//g_ptexFaceVisibleUAV[ptexTileID] = INTERSECT_TRUE;
	//return;

	//if (g_ptexFaceVisibleUAV[ptexTileID] == INTERSECT_TRUE)
	//{
	//	uint3 patchData = uint3(localPatchID + g_PrimitiveIdBase, g_IndexStart + g_NumIndicesPerPatch * localPatchID, g_NumVertexComponents);
	//	g_patchDataAppend.Append(patchData);
	//	return;
	//}

	//float3 bbMin = float3( FLT_MAX,  FLT_MAX,  FLT_MAX);
	//float3 bbMax = float3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	// Read 4 control points

	//uint i = threadIdx.y;
	//uint j = threadIdx.z;
	uint localID = threadIdx.z + threadIdx.y * 2;

	int vIdx = g_IndexBuffer[g_IndexStart + g_NumIndicesPerPatch * localPatchID + localID];


	// currentlty we only check if control points are in obb/brush volume
	// TODO eval gregory
#ifndef EVAL_GREGORY
#define EVAL_GREGORY
#endif
#ifdef EVAL_GREGORY


	SGregory greg;
	float3 position = float3(g_VertexBuffer[vIdx*g_NumVertexComponents + 0],
		g_VertexBuffer[vIdx*g_NumVertexComponents + 1],
		g_VertexBuffer[vIdx*g_NumVertexComponents + 2]);

	/*dgreg[threadIdx.x][localID].pos = pos;*/
	/////////////////////////////////////////////////////////////////////////////////////////
	// vertex shader
	//output.hullPosition = mul(ModelViewMatrix, input.position).xyz;
	//OSD_PATCH_CULL_COMPUTE_CLIPFLAGS(input.position);

	//int ivalence = g_OsdValenceBuffer[int(vID * (2 * OSD_MAX_VALENCE + 1))]; 
	int ivalence = g_OsdValenceBuffer[int(vIdx * (2 * OSD_MAX_VALENCE + 1))]; // checkme is SV_VertexID == vertex buffer index?
	greg.valence = ivalence;
	uint valence = uint(abs(ivalence));

	float3 f[OSD_MAX_VALENCE];
	/*float3 pos = input.position.xyz;*/
	float3 opos = float3(0, 0, 0);

		float3 r[OSD_MAX_VALENCE];

	for (uint vl = 0; vl < valence; ++vl)
	{
		uint im = (vl + valence - 1) % valence;
		uint ip = (vl + 1) % valence;

		uint idx_neighbor = uint(g_OsdValenceBuffer[int(vIdx * (2 * OSD_MAX_VALENCE + 1) + 2 * vl + 0 + 1)]);


		float3 neighbor =
			float3(g_VertexBuffer[int(g_NumVertexComponents*idx_neighbor)],
			g_VertexBuffer[int(g_NumVertexComponents*idx_neighbor + 1)],
			g_VertexBuffer[int(g_NumVertexComponents*idx_neighbor + 2)]);

		uint idx_diagonal = uint(g_OsdValenceBuffer[int(vIdx * (2 * OSD_MAX_VALENCE + 1) + 2 * vl + 1 + 1)]);

		float3 diagonal =
			float3(g_VertexBuffer[int(g_NumVertexComponents*idx_diagonal)],
			g_VertexBuffer[int(g_NumVertexComponents*idx_diagonal + 1)],
			g_VertexBuffer[int(g_NumVertexComponents*idx_diagonal + 2)]);

		uint idx_neighbor_p = uint(g_OsdValenceBuffer[int(vIdx * (2 * OSD_MAX_VALENCE + 1) + 2 * ip + 0 + 1)]);

		float3 neighbor_p =
			float3(g_VertexBuffer[int(g_NumVertexComponents*idx_neighbor_p)],
			g_VertexBuffer[int(g_NumVertexComponents*idx_neighbor_p + 1)],
			g_VertexBuffer[int(g_NumVertexComponents*idx_neighbor_p + 2)]);

		uint idx_neighbor_m = uint(g_OsdValenceBuffer[int(vIdx * (2 * OSD_MAX_VALENCE + 1) + 2 * im + 0 + 1)]);

		float3 neighbor_m =
			float3(g_VertexBuffer[int(g_NumVertexComponents*idx_neighbor_m)],
			g_VertexBuffer[int(g_NumVertexComponents*idx_neighbor_m + 1)],
			g_VertexBuffer[int(g_NumVertexComponents*idx_neighbor_m + 2)]);

		uint idx_diagonal_m = uint(g_OsdValenceBuffer[int(vIdx * (2 * OSD_MAX_VALENCE + 1) + 2 * im + 1 + 1)]);

		float3 diagonal_m =
			float3(g_VertexBuffer[int(g_NumVertexComponents*idx_diagonal_m)],
			g_VertexBuffer[int(g_NumVertexComponents*idx_diagonal_m + 1)],
			g_VertexBuffer[int(g_NumVertexComponents*idx_diagonal_m + 2)]);

		f[vl] = (position * float(valence) + (neighbor_p + neighbor)*2.0f + diagonal) / (float(valence) + 5.0f);

		opos += f[vl];
		r[vl] = (neighbor_p - neighbor_m) / 3.0f + (diagonal - diagonal_m) / 6.0f;
	}
	greg.r = r;
	opos /= valence;
	//output.position = float4(opos, 1.0f).xyz;
	greg.position = position;

	//CHECKME shouldn position be opos?

	float3 e;
	greg.e0 = float3(0, 0, 0);
	greg.e1 = float3(0, 0, 0);

	for (uint vs = 0; vs < valence; ++vs) {
		uint im = (vs + valence - 1) % valence;
		e = 0.5f * (f[vs] + f[im]);
		greg.e0 += csf(valence - 3, 2 * vs) *e;
		greg.e1 += csf(valence - 3, 2 * vs + 1)*e;
	}
	greg.e0 *= ef[valence - 3];
	greg.e1 *= ef[valence - 3];

	greg.Ep = 0;
	greg.Em = 0;
	greg.Fp = 0;
	greg.Fm = 0;
	dgreg[threadIdx.x][localID] = greg;

	// sync?



	//  HULL 

	uint i = localID;
	uint ip = (i + 1) % 4;			// todo check order of localID above
	uint im = (i + 3) % 4;			// todo check order of localID above
	//uint valence = abs(patch[i].valence); // defined above
	uint n = valence;
	int base = g_GregoryQuadOffsetBase; // to appendbuffer:  4*primitiveID+base 

	//GregDomainVertex output;
	//output.position = patch[ID].position;

	//uint start = uint(g_OsdQuadOffsetBuffer[int(4*primitiveID+base + i)]) & 0x00ffu;
	//uint prev  = uint(g_OsdQuadOffsetBuffer[int(4*primitiveID+base + i)]) & 0xff00u;
	uint start = uint(g_OsdQuadOffsetBuffer[int(4 * localPatchID + base + i)]) & 0x00ffu;
	uint prev = uint(g_OsdQuadOffsetBuffer[int(4 * localPatchID + base + i)]) & 0xff00u;
	prev = uint(prev / 256);

	//uint start_m = uint(OsdQuadOffsetBuffer[int(4*primitiveID+base + im)]) & 0x00ffu;
	//uint prev_p  = uint(OsdQuadOffsetBuffer[int(4*primitiveID+base + ip)]) & 0xff00u;
	uint start_m = uint(g_OsdQuadOffsetBuffer[int(4 * localPatchID + base + im)]) & 0x00ffu;
	uint prev_p = uint(g_OsdQuadOffsetBuffer[int(4 * localPatchID + base + ip)]) & 0xff00u;
	prev_p = uint(prev_p / 256);

	//uint np = abs(patch[ip].valence);
	//uint nm = abs(patch[im].valence);

	uint np = abs(dgreg[threadIdx.x][ip].valence);
	uint nm = abs(dgreg[threadIdx.x][im].valence);

	// Control Vertices based on : 
	// "Approximating Subdivision Surfaces with Gregory Patches for Hardware Tessellation" 
	// Loop, Schaefer, Ni, Castafio (ACM ToG Siggraph Asia 2009)
	//
	//  P3         e3-      e2+         E2
	//     O--------O--------O--------O
	//     |        |        |        |
	//     |        |        |        |
	//     |        | f3-    | f2+    |
	//     |        O        O        |
	// e3+ O------O            O------O e2-
	//     |     f3+          f2-     |
	//     |                          |
	//     |                          |
	//     |      f0-         f1+     |
	// e0- O------O            O------O e1+
	//     |        O        O        |
	//     |        | f0+    | f1-    |
	//     |        |        |        |
	//     |        |        |        |
	//     O--------O--------O--------O
	//  P0         e0+      e1-         E1
	//

	//float3 Ep = patch[i].position + patch[i].e0 * csf(n-3, 2*start) + patch[i].e1*csf(n-3, 2*start + 1);
	//   float3 Em = patch[i].position + patch[i].e0 * csf(n-3, 2*prev ) + patch[i].e1*csf(n-3, 2*prev + 1);

	//   float3 Em_ip = patch[ip].position + patch[ip].e0*csf(np-3, 2*prev_p) + patch[ip].e1*csf(np-3, 2*prev_p + 1);
	//   float3 Ep_im = patch[im].position + patch[im].e0*csf(nm-3, 2*start_m) + patch[im].e1*csf(nm-3, 2*start_m + 1);

	float3 Ep = position + dgreg[threadIdx.x][i].e0 * csf(n - 3, 2 * start) + dgreg[threadIdx.x][i].e1*csf(n - 3, 2 * start + 1);
		float3 Em = position + dgreg[threadIdx.x][i].e0 * csf(n - 3, 2 * prev) + dgreg[threadIdx.x][i].e1*csf(n - 3, 2 * prev + 1);

		float3 Em_ip = dgreg[threadIdx.x][ip].position + dgreg[threadIdx.x][ip].e0*csf(np - 3, 2 * prev_p) + dgreg[threadIdx.x][ip].e1*csf(np - 3, 2 * prev_p + 1);
		float3 Ep_im = dgreg[threadIdx.x][im].position + dgreg[threadIdx.x][im].e0*csf(nm - 3, 2 * start_m) + dgreg[threadIdx.x][im].e1*csf(nm - 3, 2 * start_m + 1);

		float s1 = 3 - 2 * csf(n - 3, 2) - csf(np - 3, 2);
	float s2 = 2 * csf(n - 3, 2);

	float3 Fp = (csf(np - 3, 2)*position + s1*Ep + s2*Em_ip + dgreg[threadIdx.x][i].r[start]) / 3.0f;
		s1 = 3.0f - 2.0f*cos(2.0f*M_PI / float(n)) - cos(2.0f*M_PI / float(nm));
	float3 Fm = (csf(nm - 3, 2)*position + s1*Em + s2*Ep_im - dgreg[threadIdx.x][i].r[prev]) / 3.0f;

		dgreg[threadIdx.x][i].Ep = Ep;
	dgreg[threadIdx.x][i].Em = Em;
	dgreg[threadIdx.x][i].Fp = Fp;
	dgreg[threadIdx.x][i].Fm = Fm;

	//int patchLevel = GetPatchLevel(primitiveID);
	//output.patchCoord = float4(0, 0,
	//                           patchLevel+0.5f,
	//                           primitiveID+PrimitiveIdBase+0.5f);



	// domain
	//float u = uv.x,
	//      v = uv.y;

	float2 uv = 0;

		// todo check order
		if (i == 0) { uv = float2(0, 0); }
	if (i == 1) { uv = float2(1, 0); }
	if (i == 2) { uv = float2(1, 1); }
	if (i == 3) { uv = float2(0, 1); }




	float3 p[20];

	p[0] = dgreg[threadIdx.x][0].position;
	p[1] = dgreg[threadIdx.x][0].Ep;
	p[2] = dgreg[threadIdx.x][0].Em;
	p[3] = dgreg[threadIdx.x][0].Fp;
	p[4] = dgreg[threadIdx.x][0].Fm;

	p[5] = dgreg[threadIdx.x][1].position;
	p[6] = dgreg[threadIdx.x][1].Ep;
	p[7] = dgreg[threadIdx.x][1].Em;
	p[8] = dgreg[threadIdx.x][1].Fp;
	p[9] = dgreg[threadIdx.x][1].Fm;

	p[10] = dgreg[threadIdx.x][2].position;
	p[11] = dgreg[threadIdx.x][2].Ep;
	p[12] = dgreg[threadIdx.x][2].Em;
	p[13] = dgreg[threadIdx.x][2].Fp;
	p[14] = dgreg[threadIdx.x][2].Fm;

	p[15] = dgreg[threadIdx.x][3].position;
	p[16] = dgreg[threadIdx.x][3].Ep;
	p[17] = dgreg[threadIdx.x][3].Em;
	p[18] = dgreg[threadIdx.x][3].Fp;
	p[19] = dgreg[threadIdx.x][3].Fm;

	float3 q[16];

	float U = 1 - uv.x, V = 1 - uv.y;

	float d11 = uv.x + uv.y; if (uv.x + uv.y == 0.0f) d11 = 1.0f;
	float d12 = U + uv.y; if (U + uv.y == 0.0f) d12 = 1.0f;
	float d21 = uv.x + V; if (uv.x + V == 0.0f) d21 = 1.0f;
	float d22 = U + V; if (U + V == 0.0f) d22 = 1.0f;

	q[5] = (uv.x*p[3] + uv.y*p[4]) / d11;
	q[6] = (U*p[9] + uv.y*p[8]) / d12;
	q[9] = (uv.x*p[19] + V*p[18]) / d21;
	q[10] = (U*p[13] + V*p[14]) / d22;

	q[0] = p[0];
	q[1] = p[1];
	q[2] = p[7];
	q[3] = p[5];
	q[4] = p[2];
	q[7] = p[6];
	q[8] = p[16];
	q[11] = p[12];
	q[12] = p[15];
	q[13] = p[17];
	q[14] = p[11];
	q[15] = p[10];

	float3 WorldPos = float3(0, 0, 0);
		float3 Tangent = float3(0, 0, 0);
		float3 BiTangent = float3(0, 0, 0);

		// henry: new dx compiler wont compile without static here
		static float B[4], D[4];
	static float3 BUCP[4], DUCP[4];

	Univar4x4(uv.x, B, D);

	for (uint k = 0; k < 4; ++k) {
		BUCP[k] = float3(0, 0, 0);
		DUCP[k] = float3(0, 0, 0);

		[unroll]
		for (uint j = 0; j < 4; ++j) 
		{
			// reverse face front
			float3 A = q[k + 4 * j];

			BUCP[k] += A * B[j];
			DUCP[k] += A * D[j];
		}
	}

	Univar4x4(uv.y, B, D);

	for (uint m = 0; m < 4; ++m) 
	{
		WorldPos	+= B[m] * BUCP[m];
		Tangent		+= B[m] * DUCP[m];
		BiTangent	+= D[m] * BUCP[m];
	}
	//int level = int(patch[0].ptexInfo.z);
	//BiTangent *= 3 * level;
	//Tangent *= 3 * level;

	//BiTangent = mul(ModelViewMatrix, float4(BiTangent, 0)).xyz;
	//Tangent = mul(ModelViewMatrix, float4(Tangent, 0)).xyz;

	float3 normal = normalize(cross(BiTangent, Tangent));

		position = WorldPos;
	controlPointsGregory[threadIdx.x][localID] = position;
	//output.normal = normal;
	//output.tangent = BiTangent;
	//output.bitangent = Tangent;

	//output.patchCoord = patch[0].patchCoord;
	//output.patchCoord.xy = float2(v, u);

	//OSD_COMPUTE_PTEX_COORD_DOMAIN_SHADER;

	//OSD_DISPLACEMENT_CALLBACK;

	//#if USE_SHADOW	
	//	float4 posWorld = mul(mInverseView, float4(output.position.xyz, 1));
	//	output.vTexShadow = mul(g_lightViewMatrix, posWorld); // transform to light space
	//#endif

	//output.positionOut = mul(ProjectionMatrix,
	//                         float4(output.position.xyz, 1.0f));





	///////////////////////////////////////////////////////////////////////////////////////////
#else
	controlPointsGregory[threadIdx.x][localID] = float3(g_VertexBuffer[vIdx*g_NumVertexComponents + 0],
		g_VertexBuffer[vIdx*g_NumVertexComponents + 1],
		g_VertexBuffer[vIdx*g_NumVertexComponents + 2]);
#endif



	//float3 cp = controlPointsBezier[threadIdx.x][localID].xyz;
	float3 cp = controlPointsGregory[threadIdx.x][localID].xyz;
	//[unroll(BATCH_SIZE)]
	//for (int batchIdx = 0; batchIdx < BATCH_SIZE; ++batchIdx)
	for (uint batchIdx = 0; batchIdx < BATCH_SIZE; ++batchIdx)
	{
		matrix modelToOBBTranspose = transpose(g_matModelToOBB[batchIdx]);
		controlPointsGregory[threadIdx.x][localID].xyz = cp.x * modelToOBBTranspose[0].xyz +
			(cp.y * modelToOBBTranspose[1].xyz
			+ (cp.z * modelToOBBTranspose[2].xyz + modelToOBBTranspose[3].xyz)
			);
		//GroupMemoryBarrierWithGroupSync();

		//compute AABB
		float3 bbMax = float3(-1000, -1000, -1000);
		float3 bbMin = float3(1000, 1000, 1000);
		for (int m = 0; m < 4; ++m)
		{
			bbMax = max(controlPointsGregory[threadIdx.x][m], bbMax);
			bbMin = min(controlPointsGregory[threadIdx.x][m], bbMin);
		}

		//controlPointsBezierScan[threadIdx.x][localID] = controlPointsBezier[threadIdx.x][localID];
		//float3 bbMax = scanInclusiveIntraWarpMax(localID, threadIdx.x);	
		//controlPointsBezierScan[threadIdx.x][localID] = controlPointsBezier[threadIdx.x][localID];
		//float3 bbMin = scanInclusiveIntraWarpMin(localID, threadIdx.x);


		// multiscan min max variant
		//controlPointsBezierMultiScan[threadIdx.x][localID].bmin = controlPointsBezier[threadIdx.x][localID];
		//controlPointsBezierMultiScan[threadIdx.x][localID].bmax = controlPointsBezier[threadIdx.x][localID];
		//float3 bbMinMax[2] = scanInclusiveIntraWarpMulti(localID, threadIdx.x);

		//float3 bbMin = bbMinMax[0];
		//float3 bbMax = bbMinMax[1];

		//float3 bmin[2];
		//bmin[threadIdx.x] = bbMin;
		//float3 bmax[2];
		//bmax[threadIdx.x] = bbMax;
		// TODO use box extends
		//GroupMemoryBarrierWithGroupSync();
		if (localID == 4 - 1)
		{
			// todo write determine min/max displacement per tile in sampling or update overlap shader
			//maxDisplacement = minDisplacement = 0.0f;

			// displacement in brush space; should not matter 
			//maxDisplacement = mul((float3x3)g_mWorldToBrush, float3(0,0,maxDisplacement)).z;
			//float maxDisplacement = mul((float3x3)g_mWorldToBrush, float3(0,0,2)).z;
			//float minDisplacement = 0;//-maxDisplacement;

#ifdef WITH_CON
			// todo make it faster
			float alpha;
			float3 axis;

			// henry: TODO with cone of normals, disable for first tests
			//ConApprox(localPatchID, alpha, axis);
			ConApproxShared(threadIdx.x, alpha, axis);

			//cone of normals
			float3 extP = float3(1.0f, 1.0f, 1.0f);
			float3 extM = float3(1.0f, 1.0f, 1.0f);
			BoundCone(alpha, axis, extP, extM);

			float maxDisplacement = g_displacementScaler *0.2;//0.2; // default value set in memorymanage::inittiledisplacement		// g_maxDisplacement* g_PerPatchDisplacementInfo[patchID*2+0];
			float minDisplacement = -g_displacementScaler*0.2;//0.2; //-g_maxDisplacement;// 0;	//g_maxDisplacement * g_PerPatchDisplacementInfo[patchID*2+1];
			//maxDisplacement = mul((float3x3)g_mWorldToBrush, float3(0,0,g_displacementScaler/2)).z;
			//minDisplacement = maxDisplacement;

			//extend by cone of normals
			float3 coneExt = max(maxDisplacement * extP, minDisplacement * extM);
			bbMax += coneExt;
			bbMin += coneExt;

#else
			float maxDisplacement = -g_displacementScaler;
			float minDisplacement = g_displacementScaler;

			//extend with maxDisplacement; // CHECKME
			bbMin = bbMin - maxDisplacement;
			bbMax = bbMax + minDisplacement;

			//bmin[threadIdx.x] -= maxDisplacement;
			//bmax[threadIdx.x] += minDisplacement;
#endif

			if ((1.0f < bbMin.x || 0.0f > bbMax.x) || (1.0f < bbMin.y || 0.0f > bbMax.y) || (1.0f < bbMin.z || 0.0f > bbMax.z))
			{
			}
			else
			{
				g_ptexFaceVisibleUAV[ptexTileID] = INTERSECT_TRUE;
				g_ptexFaceVisibleAllUAV[ptexTileID] = INTERSECT_TRUE;
				//uint3 patchData = uint3(localPatchID + g_PrimitiveIdBase, g_IndexStart + g_NumIndicesPerPatch * localPatchID, g_NumVertexComponents);
				//g_patchDataRegularAppend.Append(patchData);
				uint3 patchData = uint3(localPatchID + g_PrimitiveIdBase,
					g_IndexStart + g_NumIndicesPerPatch * localPatchID,
					//4*g_PrimitiveIdBase + g_GregoryQuadOffsetBase);									
					4 * localPatchID + g_GregoryQuadOffsetBase);

				g_patchDataGregory[batchIdx].Append(patchData);
			}
		}
		}
}
#endif

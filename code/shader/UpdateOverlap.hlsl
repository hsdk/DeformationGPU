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

#ifndef TILE_SIZE
#define TILE_SIZE 16
#endif

// read buffers (SRVs)
Buffer<uint>	g_TileDescriptors	: register(t0);	//tile layout info, packed textureDisplace_Packing
Buffer<int>		g_TileNeighData		: register(t1);
StructuredBuffer<uint>	g_compactedVisibility	: register(t4);

// writable buffers
RWTexture2DArray<float>		g_displacementDataUAV	: register(u0);

int IsAllocated(uint ptexFaceID)
{
	return (g_TileDescriptors[ptexFaceID * 4 + 0] != 0xffff) ? 1 : 0;
	//	return 0;	// not allocated
	//else
	//	return 1;	// allocated
	
}

int GetPage(uint ptexFaceID)
{
	return g_TileDescriptors[ptexFaceID*4+0];
}

int2 TileStartUV(uint ptexFaceID)
{
	return int2(g_TileDescriptors[ptexFaceID*4+1], g_TileDescriptors[ptexFaceID*4+2]);
}


int4 GetNeighPtexInfo(uint ptexFaceID)
{
	int4 neighPtex;
	neighPtex.x = g_TileNeighData[ptexFaceID*8+0];
	neighPtex.y = g_TileNeighData[ptexFaceID*8+1];
	neighPtex.z = g_TileNeighData[ptexFaceID*8+2];
	neighPtex.w = g_TileNeighData[ptexFaceID*8+3];
	return neighPtex;
}

int4 GetNeighEdgeInfo(uint ptexFaceID)
{
	int4 neighEdgeID;
	neighEdgeID.x = g_TileNeighData[ptexFaceID*8+4];
	neighEdgeID.y = g_TileNeighData[ptexFaceID*8+5];
	neighEdgeID.z = g_TileNeighData[ptexFaceID*8+6];
	neighEdgeID.w = g_TileNeighData[ptexFaceID*8+7];
	return neighEdgeID;
}


[numthreads(TILE_SIZE, 4, 1)]
void OverlapEdgesCS(
	uint3 blockIdx	: SV_GroupID, 
	uint3 DTid		: SV_DispatchThreadID, 
	uint3 threadIdx : SV_GroupThreadID,
	uint  GI		: SV_GroupIndex )
{
#ifdef COMPACTED_VISIBILITY	
	uint ptexID = g_compactedVisibility[blockIdx.x];
#else
	uint ptexID = blockIdx.x;
#endif
	
	if(!IsAllocated(ptexID))
		return;

	int4 neighPtexID = GetNeighPtexInfo(ptexID);		
	uint edge = threadIdx.y; // asume edge 0 is XY 0,0 to TILE_SIZE, 0

	uint neighPtex = neighPtexID[edge];
	if (!IsAllocated(neighPtex))
		return;

	// where do we start in texture
	const int overlap = 1;
	uint2 targetStart = TileStartUV(ptexID);// - uint2(overlapSize,overlapSize); // CHECKME we want to move to boundary but not in x and y dir


	int offset =  threadIdx.x;
	uint ts = uint(TILE_SIZE-1); // we are already at start texel so -1
	uint myPage = GetPage(ptexID);

	int2 edgeOffsetsDst[4] = {
		int2(ts - offset, -overlap),	// blau
		int2(-overlap, offset),			// rot	PASST
		int2(+offset, ts + overlap),	// gelb PASST
		int2(ts + overlap, ts - offset) // gruen
	};

	// Opposite winding, offset by one in non read direction
	int2 edgeOffsetsSrc[4] = {
		int2(offset, 0),
		int2(0, ts - offset), 
		int2(ts - offset, ts),
		int2(ts, offset) // 
	};

	uint3 coordTarget = uint3(targetStart.x, targetStart.y, GetPage(ptexID));
	coordTarget.xy += edgeOffsetsDst[edge];
	
	uint neighPage = GetPage(neighPtex);
	uint2 sourceStart = TileStartUV(neighPtex); 
	uint3 coordSource =  uint3(sourceStart.x, sourceStart.y, neighPage);

	int4 neighEdges = GetNeighEdgeInfo(ptexID);
	int neighborEdge = neighEdges[edge];
	coordSource.xy += edgeOffsetsSrc[neighborEdge];

	g_displacementDataUAV[coordTarget] = g_displacementDataUAV[coordSource];
}

[numthreads(1, 4, 1)]
void OverlapCornersCS(
	uint3 blockIdx	: SV_GroupID, 
	uint3 DTid		: SV_DispatchThreadID, 
	uint3 threadIdx : SV_GroupThreadID,
	uint  GI		: SV_GroupIndex )
{

#ifdef COMPACTED_VISIBILITY	
	uint ptexID = g_compactedVisibility[blockIdx.x];
#else
	uint ptexID = blockIdx.x;
#endif
	
	//if(IsAllocated(ptexID) == 0)
	//	return;

	int4 neighPtexID = GetNeighPtexInfo(ptexID);
	int4 neighEdges  = GetNeighEdgeInfo(ptexID);
		
	uint edge = (threadIdx.y / 2) * 2; // e0 or e2 as start
	uint neighPtex = neighPtexID[edge];
	uint neighPage = GetPage(neighPtex);
	if (!IsAllocated(neighPtex))
		return;

	// where do we start in texture
	const int overlap = 1;
	int offset = (threadIdx.y % 2) * (TILE_SIZE + overlap) - overlap;
	uint ts = uint(TILE_SIZE - 1);

	int2 edgeOffsetsDst[4] = { int2(ts - offset, -overlap),	// blau
		int2(-overlap, offset),			// rot	
		int2(+offset, ts + overlap),	// gelb
		int2(ts + overlap, ts - offset) // gruen
	};

	// Opposite winding, offset by one in non read direction
	int2 edgeOffsetsSrc[4] = { int2(offset, 0),
		int2(0, ts - offset),
		int2(ts - offset, ts),
		int2(ts, offset) };

	uint2 targetStart = TileStartUV(ptexID);
	uint3 coordTarget = uint3(targetStart.x, targetStart.y, GetPage(ptexID));
	coordTarget.xy += edgeOffsetsDst[edge];
		
	uint2 sourceStart = TileStartUV(neighPtex); 
	uint3 coordSource =  uint3(sourceStart.x, sourceStart.y, neighPage);
	int neighborEdge = neighEdges[edge];
	coordSource.xy += edgeOffsetsSrc[neighborEdge];

	g_displacementDataUAV[coordTarget] = g_displacementDataUAV[coordSource];
}


#ifndef WORK_GROUP_SIZE_EXTRAORDINARY
#define WORK_GROUP_SIZE_EXTRAORDINARY 128
#endif


#ifndef MAX_VALENCE
#define MAX_VALENCE 12
#endif

cbuffer cbExtraordinary : register( b12 ) 
{
	uint  g_numExtraordinary;
	uint3 g_paddingExtraordinary;
};

Buffer<uint>	g_ExtraordinaryInfo			: register(t2);	
Buffer<uint>	g_ExtraordinaryPTexAndVID	: register(t3);


#define EXTRAORDINARY_STARTINDEX 0
#define EXTRAORDINARY_VALENCE 1

#define EXTRAORDINARY_PTEX_ID   0
#define EXTRAORDINARY_VERTEX_IDX 1

uint2 GetExtraordinaryInfo(uint index)
{
	int idx = index*2;
	return uint2(g_ExtraordinaryInfo[idx], g_ExtraordinaryInfo[idx+1]);
}

uint GetStartIndex(uint2 info)
{
	return info[EXTRAORDINARY_STARTINDEX];
}

uint GetValence(uint2 info)
{
	return info[EXTRAORDINARY_VALENCE];
}


int2 GetFaceAndVertexID(uint offset, uint i)
{
	// checkme
	int idx = (offset+i)*2;
	return int2(g_ExtraordinaryPTexAndVID[idx], g_ExtraordinaryPTexAndVID[idx+1]); 
}

uint GetPTexID(uint2 ptexAndVertex)
{
	return ptexAndVertex[EXTRAORDINARY_PTEX_ID];
}

// should return the vertex number in face: 0, 1, 2, 3 that we use to lookup corner
uint GetVertexIdx(uint2 ptexAndVertex)
{
	return ptexAndVertex[EXTRAORDINARY_VERTEX_IDX];
}


// one thread per extraordinary vertex,  equalize corners and push results
[numthreads(WORK_GROUP_SIZE_EXTRAORDINARY, 1, 1)]
void OverlapEqualizeExtraordinaryCS(
	uint3 blockIdx	: SV_GroupID, 
	uint3 DTid		: SV_DispatchThreadID, 
	uint3 threadIdx : SV_GroupThreadID,
	uint  GI		: SV_GroupIndex )
{

	if(DTid.x >= g_numExtraordinary)
		return;

	int extraordinaryID = DTid.x;

	uint2 info = GetExtraordinaryInfo(extraordinaryID);

	uint offset  = GetStartIndex(info);
	uint valence = GetValence(info);

	uint2 ptex2faceVertex[MAX_VALENCE];

	for(uint i = 0; i < MAX_VALENCE; ++i)
	{
		if(i >= valence) break;
		ptex2faceVertex[i] = GetFaceAndVertexID(offset, i);			
	}	

	const int ts = int(TILE_SIZE) - 1; // we are already at start texel so -1

	int overlap = 1;	// checkme set from outside

	// ccw
	int2 cornerOffsetsDstBase [4] = {			
		  int2(    -overlap,    -overlap)		// rot/blaue ecke uv: 0, 0
		, int2(ts + overlap,    -overlap)		// gruen/blaue ecke: 1, 0
		, int2(ts + overlap,   ts +overlap)		// gruen/gelbe ecke 1, 1
		, int2(	   -overlap,   ts +overlap)		// gelb/rot uv: 0, 1 		
	};

	int2 cornerOffsetsDstA [4] = {			
		  int2( 1, 0)			// rot/blaue ecke uv: 0, 0
		, int2( 0, 1)			// gruen/blaue ecke: 1, 0
		, int2(-1, 0)		// gruen/gelbe ecke 1, 1
		, int2( 0,-1)		// gelb/rot uv: 0, 1 		
	};

	int2 cornerOffsetsDstB [4] = {			
		  int2( 0, 1)			// rot/blaue ecke uv: 0, 0
		, int2(-1, 0)		// gruen/blaue ecke: 1, 0
		, int2( 0,-1)		// gruen/gelbe ecke 1, 1
		, int2( 1, 0)			// gelb/rot uv: 0, 1 		
	};


	int2 cornerOffsetsSrc [4] = {			
		  int2( 0,  0)		// rot/blaue ecke uv: 0, 0
		, int2(ts,  0)		// gruen/blaue ecke: 1, 0
		, int2(ts, ts)		// gruen/gelbe ecke 1, 1
		, int2( 0, ts)		// gelb/rot uv: 0, 1 		
	};


	float avgCorner = 0;
	uint cntValence = 0;
	//[unroll(MAX_VALENCE)]
	for(uint k = 0; k < MAX_VALENCE; ++k)
	{
		if(k >= valence) break;
		
		uint ptexID = GetPTexID(ptex2faceVertex[k]);

		if(IsAllocated(ptexID) !=0)
		{
			cntValence++;
			int3 coordCorner = int3(TileStartUV(ptexID), GetPage(ptexID));
			uint vID = GetVertexIdx(ptex2faceVertex[k]);
			coordCorner.xy += cornerOffsetsSrc[vID];

			avgCorner += g_displacementDataUAV[coordCorner];
		}
	}

	avgCorner /= cntValence;

	// scatter avg corner value
	//[unroll(MAX_VALENCE)]
	for(uint m = 0; m < MAX_VALENCE; ++m)	
	{
		if(m >= valence) break;
		
		uint ptexID = GetPTexID(ptex2faceVertex[m]);		
		if(IsAllocated(ptexID) !=0)
		{
			
			uint3 coordCorner = uint3(TileStartUV(ptexID), GetPage(ptexID));
			uint vID = GetVertexIdx(ptex2faceVertex[m]);

			uint3 coordCornerSrc =  coordCorner + uint3(cornerOffsetsSrc[vID],0);

			coordCorner.xy += cornerOffsetsDstBase[vID] ;
			uint3 coordCornerA = coordCorner;
			coordCornerA.xy += cornerOffsetsDstA[vID];
			uint3 coordCornerB = coordCorner;
			coordCornerB.xy += cornerOffsetsDstB[vID];

			{
				g_displacementDataUAV[coordCorner]		= avgCorner;
				g_displacementDataUAV[coordCornerA]		= avgCorner;
				g_displacementDataUAV[coordCornerB]		= avgCorner;
				g_displacementDataUAV[coordCornerSrc]	= avgCorner;
			}
		}
	}




}



// IMPROVE ME
Buffer<uint>					g_ptexFaceVisibleAllSRV		: register(t0);
AppendStructuredBuffer<uint>	g_compactedVisibilityAppend	: register(u0);

[numthreads(32, 1, 1)]
void CompactIntersectAll(	uint3 DTid	: SV_DispatchThreadID )
{
	uint ptexID = DTid.x;
	if(g_ptexFaceVisibleAllSRV[ptexID])
	{
		g_compactedVisibilityAppend.Append(ptexID);
	}
}
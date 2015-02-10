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

cbuffer ConfigCB : register( b6 ) {    
    int g_GregoryQuadOffsetBase;			
    int g_PrimitiveIdBase;			// patch id of first patch to access ptex, far patch table
	int g_IndexStart;				// index of the first vertex in global index array
	uint g_NumVertexComponents;		// num float elements per vertex, for stride in vertex buffer, the first 3 are always pos
	uint g_NumIndicesPerPatch;		// num indices per patch, TODO better use shader define
	uint g_NumPatches;				// num patches to process
};

cbuffer DisplacementCB : register( b7 ) 
{
    float g_displacementScale;
    float g_mipmapBias;
};

cbuffer cbDeformation : register( b13 ) 
{
	uint g_NumOverlapElements;
	uint3 dummy_cbDeformation;
};

// read buffers (SRVs)
Buffer<uint>		g_ptexFaceVisibleSRV	: register(t0);	// t0 penetrator voxelization uint griddims
Buffer<float>		g_VertexBuffer			: register(t1);	// t1 vertex buffer, float, numcomponents set in cb
Buffer<uint>		g_IndexBuffer			: register(t2);	// t2 index buffer			
Buffer<uint2>		g_OsdPatchParamBuffer	: register(t3); // t3 tile rotation info and patch to tile mapping, CHECKME
Buffer<int>			g_OsdValenceBuffer		: register(t4); // new t4 valence buffer for gregory patch eval
Buffer<int>			g_OsdQuadOffsetBuffer	: register(t5); // new t5
StructuredBuffer<uint2>	g_patchDataRegular	: register(t6); // t6 patchIdx: localPatchID + g_PrimitiveIdBase,	vIdx: g_IndexStart + g_NumIndicesPerPatch * localPatchID, numvertexcomponents (obsolete, use OSD_NUM_ELEMENTS)
StructuredBuffer<uint3>	g_patchDataGregory	: register(t7); // t7 patchIdx: localPatchID + g_PrimitiveIdBase,	vIdx: g_IndexStart + g_NumIndicesPerPatch * localPatchID,	g_PrimitiveIdBase + g_GregoryQuadOffsetBase
Buffer<uint>			g_TileInfo			: register(t8);	// t8 tile layout info displacement, packed textureDisplace_Packing
Buffer<uint>			g_TileInfoColor		: register(t9);	// t9 tile layout info color, packed textureDisplace_Packing
Texture2D				g_txBrush			: register(t10);

sampler		g_sampler : register (s0);

// writable buffers
RWTexture2DArray<float>		g_displacementUAV	: register(u0);
RWTexture2DArray<float>		g_ConstraintsUAV	: register(u0);
RWTexture2DArray<uint>		g_ColorUAV			: register(u1);
RWBuffer<uint>				g_maxPatchDisplacement : register (u2);
//RWTexture2DArray<float>		g_ColorUAVX	: register(u1);

#define MINF asfloat(0xff800000)

//#include "VoxelDDA.hlsl"
#include "PTexLookup.hlsl"
#include "OSDPatchCommon.hlsl"

#ifndef OSD_NUM_ELEMENTS
#define OSD_NUM_ELEMENTS 3
#endif

#ifndef OSD_MAX_VALENCE
#define OSD_MAX_VALENCE 7
#endif


uint GetLevel(uint patchDataX)
{
	return (g_OsdPatchParamBuffer[patchDataX].y & 0xf);
}


void GetPatchInfo(float2 UV, uint patchDataX, inout float4 patchCoord, inout int4 ptexInfo)
{
	int2 ptexIndex = g_OsdPatchParamBuffer[patchDataX].xy; 
    int ptexFaceID = ptexIndex.x;                                          
    int lv = 1 << ((ptexIndex.y & 0xf) - ((ptexIndex.y >> 4) & 1));    
    int u = (ptexIndex.y >> 17) & 0x3ff;				                           
    int v = (ptexIndex.y >> 7) & 0x3ff;                                
    int rotation = (ptexIndex.y >> 5) & 0x3;            // ptex rotation               
    patchCoord.w = ptexFaceID;                                  
    ptexInfo = int4(u, v, lv, rotation); 

	/////////////////////////////////////

	#if OSD_TRANSITION_ROTATE == 1
		patchCoord.xy = float2(UV.y, 1.0-UV.x);
	#elif OSD_TRANSITION_ROTATE == 3
		patchCoord.xy = float2(1.0-UV.x, 1.0-UV.y);
	#elif OSD_TRANSITION_ROTATE == 2
		patchCoord.xy = float2(1.0-UV.y, UV.x);
	#elif OSD_TRANSITION_ROTATE == 0 //, or non-transition patch
		patchCoord.xy = float2(UV.x, UV.y);
	#endif

	float2 uv = patchCoord.xy;
	int2 p = ptexInfo.xy;                                  
    int rot = ptexInfo.w;                                  

	patchCoord.xy = (uv * float2(1.0,1.0)/lv) + float2(p.x, p.y)/lv; 

}


//groupshared float3 CP[16];

void WriteColor(float3 color, uint faceID, float4 patchCoord, int patchLevel)
{
	PtexPacking ppackColor = getPtexPacking(g_TileInfoColor, faceID);
	if(ppackColor.page == -1)	// early exit if tile data is not allocated
		return; 

	float2 coordsColor = float2(patchCoord.x * ppackColor.tileSize + ppackColor.uOffset,
		patchCoord.y * ppackColor.tileSize + ppackColor.vOffset);

	uint2 ucoordColor = coordsColor;

	
	if (g_ptexFaceVisibleSRV[faceID])	
		g_ColorUAV[int3(ucoordColor.x, ucoordColor.y, ppackColor.page)] = uint(color.x*255) | uint(color.y * 255) << 8 | uint(color.z * 255) << 16 | 255 << 24;
}

void DebugColor(uint faceID, float4 patchCoord, int patchLevel)
{
	PtexPacking ppackColor = getPtexPacking(g_TileInfoColor, faceID);
	if(ppackColor.page == -1)		return ; // early exit if tile data is not allocated

	float2 coordsColor = float2(patchCoord.x * ppackColor.tileSize + ppackColor.uOffset,
		patchCoord.y * ppackColor.tileSize + ppackColor.vOffset);

	uint2 ucoordColor = coordsColor;

	float4 color = float4(0.3,0.3,0.3,1);					// l0 gray
	if(patchLevel == 1)		color.rgb = float3(0,0,0);		// l1 black
	if(patchLevel == 2)		color.rgb = float3(0,0,1);		// l2 green
	if(patchLevel == 3)		color.rgb = float3(0,1,0);		// l3 blue
	if(patchLevel == 4)		color.rgb = float3(0,1,1);		// l4 cyan
	if(patchLevel == 5)		color.rgb = float3(1,0,0);		// l5 red
	if(patchLevel == 6)		color.rgb = float3(1,0,1);		// l6 magenta
	if(patchLevel == 7)		color.rgb = float3(1,1,0);
	if(patchLevel == 8)		color.rgb = float3(1,1,1);
	if(patchLevel == 9)		color.rgb = float3(0,0.5,0);
	if(patchLevel == 10)	color.rgb = float3(0,0.5,0.5);
	
	// write patch level 
	float4 c = float4(patchCoord.xy, 0, 1);
	float2 pc = patchCoord.xy*2-1;
	float2 pca = abs(pc);
	if (pca.x > 0.8) {
		c = float4(1,0,0,1);
		if (pc.x > 0)
			c = float4(0,1,0,1);
	} else if (pca.y > 0.8) {
		c = float4(0,0,1,1);
		if (pc.y > 0)
			c = float4(1,1,0,1);
	}
	c=color;
	if (g_ptexFaceVisibleSRV[faceID])	
		g_ColorUAV[int3(ucoordColor.x, ucoordColor.y, ppackColor.page)] = uint(c.x*255) | uint(c.y * 255) << 8 | uint(c.z * 255) << 16 | uint(c.w * 255) << 24;
}

float cubicPulse(float w, float x) 
{
	x = min(abs(x),w)/w;
	return 1.0 - x*x*(3.0-2.0*x);
}

float2 mod_(float2 x, float y)
{
  return x - y * floor(x/y);
}

#define TILE_FACTOR 4 

float2 hash3(float2 p)
{
	float tileFactor = TILE_FACTOR;
	p = mod_(p, tileFactor);
	float3 q = float3( dot(p,float2(127.1,311.7)), 
				   dot(p,float2(269.5,183.3)), 
				   dot(p,float2(419.2,371.9)) );
	return frac(sin(q.xy)*43758.5453);
}


float voronoi(float2 x)
{
    float2 p = floor(x);
    float2 f = frac(x);
	float falloff = 15.0;

	float res = 0.0;
    for( int j=-1; j<=1; j++ )
    for( int i=-1; i<=1; i++ )
    {
        float2 b = float2( i, j );
        float2 r = float2( b ) - f + hash3(p + b );
        float d = dot( r, r );
		d = sqrt(d);
		
		res +=  exp(-falloff * d);
    }
	return smoothstep(0.0,1.,-(1.0/falloff)*log(res));
}

float lookup(float2 x) {
	return 1. - voronoi(TILE_FACTOR*x+voronoi(x*TILE_FACTOR));
}


static const float fOneThird = 1.0f/3.0f;
static const float fTwoThird = 2.0f/3.0f;

// B-spline basis evaluation via deBoor pyramid... 
void EvalCubicBSpline(in float u, out float B[4], out float BU[4])
{
    float T = u;
    float S = 1.0 - u;

	float C0 =					   S * (0.5 * S);
	float C1 = T * (S + 0.5 * T) + S * (0.5 * S + T);
	float C2 = T * (    0.5 * T);

    B[0] =										   fOneThird * S                  * C0;
    B[1] = (fTwoThird * S +			    T) * C0 + (fTwoThird * S + fOneThird * T) * C1;
    B[2] = (fOneThird * S + fTwoThird * T) * C1 + (			   S + fTwoThird * T) * C2;
    B[3] =					fOneThird * T  * C2;

    BU[0] =    - C0;
    BU[1] = C0 - C1;
    BU[2] = C1 - C2;
    BU[3] = C2;
}


void Eval(in float2 UV, out float3 WorldPos, out float3 Normal, in float3 CP[16])
{
	static float B[4], D[4];
	EvalCubicBSpline(UV.x, B, D);
	static float3 BUCP[4], DUCP[4];
	for (int i = 0; i < 4; i++) {
		BUCP[i] =  float3(0.0f, 0.0f, 0.0f);
		DUCP[i] =  float3(0.0f, 0.0f, 0.0f);
		for (int j = 0; j < 4; j++) 
		{
#if OSD_TRANSITION_ROTATE == 1
            float3 A = CP[4*(3-j) + i].xyz;
#elif OSD_TRANSITION_ROTATE == 2
            float3 A = CP[4*(3-i) + (3-j)].xyz;
#elif OSD_TRANSITION_ROTATE == 3
            float3 A = CP[4*j + (3-i)].xyz;
#else // OSD_TRANSITION_ROTATE == 0, or non-transition patch
            float3 A = CP[4*i + j].xyz;
#endif
			BUCP[i] +=  A * B[j];
			DUCP[i] +=  A * D[j];		 
		}
	}
	
	WorldPos = float3(0.f,0.f,0.f);
    float3 Tangent		=  float3(0.0f, 0.0f, 0.0f);
    float3 BiTangent	=  float3(0.0f, 0.0f, 0.0f);

	EvalCubicBSpline(UV.y, B, D);

	for (i = 0; i < 4; i++) {
		WorldPos	+= B[i] * BUCP[i];
		Tangent		+= B[i] * DUCP[i];
		BiTangent	+= D[i] * BUCP[i];
	}

	// checkme level

	Normal = normalize(cross(Tangent, BiTangent));
}

void EvalRegular(in uint2 patchData, in float2 UV, inout float3 WorldPos, inout float3 Normal, float disp, in int4 ptexInfo)
{
	static float3 CP[16];	
	[unroll(16)]
	for(int v = 0; v < 16; ++v)
	{
		int vIdx = g_IndexBuffer[patchData.y + v];
		CP[v] = float3(	g_VertexBuffer[vIdx*OSD_NUM_ELEMENTS + 0],
						g_VertexBuffer[vIdx*OSD_NUM_ELEMENTS + 1],
						g_VertexBuffer[vIdx*OSD_NUM_ELEMENTS + 2]);
	}


	int rotation = ptexInfo.w;
	if(rotation == 0) UV.xy = UV.xy;
	if(rotation == 1) UV.xy = float2(UV.y, 1.0-UV.x);
	if(rotation == 2) UV.xy = float2(1.0-UV.x, 1.0-UV.y);
	if(rotation == 3) UV.xy = float2(1.0-UV.y, UV.x);

	Eval(UV, WorldPos, Normal, CP); 
	WorldPos += disp * Normal;
}

// GREGORY
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
    if (j%2 == 0) {
        return cos((2.0f * M_PI * float(float(j-0)/2.0f))/(float(n)+3.0f));
    } else {
        return sin((2.0f * M_PI * float(float(j-1)/2.0f))/(float(n)+3.0f));
    }
}


void EvalGregory(in uint3 patchData, inout float2 UV, inout float3 WorldPos, inout float3 Normal, in float disp, in int4 ptexInfo)
{
	//static float3 g_CP[4];
	static float3 g_position[4];
	static int    g_valence[4];
	static float3 g_r[4][OSD_MAX_VALENCE];
	static float3 g_e0[4];
    static float3 g_e1[4];
	
	static float3 g_Ep[4];
    static float3 g_Em[4];
    static float3 g_Fp[4];
    static float3 g_Fm[4];

	//load to shared mem
	for(int v = 0; v < 4; ++v)
	{
		//int vIdx = g_IndexBuffer[g_IndexStart + g_NumIndicesPerPatch * localPatchID + localID];
		int vIdx = g_IndexBuffer[patchData.y + v];
		float3 pos = float3(	g_VertexBuffer[vIdx*OSD_NUM_ELEMENTS + 0],
						g_VertexBuffer[vIdx*OSD_NUM_ELEMENTS + 1],
						g_VertexBuffer[vIdx*OSD_NUM_ELEMENTS + 2]);
		    
		int ivalence = g_OsdValenceBuffer[int(vIdx * (2 * OSD_MAX_VALENCE + 1))]; // checkme is SV_VertexID == vertex buffer index?
		g_valence[v] = ivalence;
		uint valence = uint(abs(ivalence));

		float3 f[OSD_MAX_VALENCE]; 
		float3 opos = float3(0,0,0);
	
		for (uint vl=0; vl<valence; ++vl) 
		{
			uint im=(vl+valence-1)%valence; 
			uint ip=(vl+1)%valence; 

			uint idx_neighbor = uint(g_OsdValenceBuffer[int(vIdx * (2*OSD_MAX_VALENCE+1) + 2*vl + 0 + 1)]);


			float3 neighbor = float3(	g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor+1)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor+2)]);

			uint idx_diagonal = uint(g_OsdValenceBuffer[int(vIdx * (2*OSD_MAX_VALENCE+1) + 2*vl + 1 + 1)]);

			float3 diagonal = float3(	g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+1)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+2)]);

			uint idx_neighbor_p = uint(g_OsdValenceBuffer[int(vIdx * (2*OSD_MAX_VALENCE+1) + 2*ip + 0 + 1)]);

			float3 neighbor_p =  float3(g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_p)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_p+1)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_p+2)]);

			uint idx_neighbor_m = uint(g_OsdValenceBuffer[int(vIdx * (2*OSD_MAX_VALENCE+1) + 2*im + 0 + 1)]);

			float3 neighbor_m =	float3(	g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_m)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_m+1)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_m+2)]);

			uint idx_diagonal_m = uint(g_OsdValenceBuffer[int(vIdx * (2*OSD_MAX_VALENCE+1) + 2*im + 1 + 1)]);

			float3 diagonal_m =	float3(	g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal_m)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal_m+1)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal_m+2)]);

			f[vl] = (pos * float(valence) + (neighbor_p + neighbor)*2.0f + diagonal) / (float(valence)+5.0f);

			opos += f[vl];
			g_r[v][vl] = (neighbor_p-neighbor_m)/3.0f + (diagonal - diagonal_m)/6.0f;
		}
		
		opos /= valence;
		g_position[v] = opos;

		float3 e;
		g_e0[v] = float3(0,0,0);
		g_e1[v] = float3(0,0,0);

		for(uint vs=0; vs<valence; ++vs) {
		    uint im = (vs + valence -1) % valence;
		    e = 0.5f * (f[vs] + f[im]);
		    g_e0[v] += csf(valence-3, 2*vs) *e;
		    g_e1[v] += csf(valence-3, 2*vs + 1)*e;
		}
		g_e0[v] *= ef[valence - 3];
		g_e1[v] *= ef[valence - 3];

		g_Ep[v]=0;
		g_Em[v]=0;
		g_Fp[v]=0;
		g_Fm[v]=0;		
	}

	for(int v2 = 0; v2< 4; ++v2)
	{
		uint i = v2;	//localID;
		uint ip = (i+1)%4;	
		uint im = (i+3)%4;	
		uint valence = abs(g_valence[v2]); 
		uint n = valence;

		uint start = uint(g_OsdQuadOffsetBuffer[int(patchData.z + i)]) & 0x00ffu;
		uint prev  = uint(g_OsdQuadOffsetBuffer[int(patchData.z + i)]) & 0xff00u;
		prev = uint(prev/256);

		uint start_m = uint(g_OsdQuadOffsetBuffer[int(patchData.z + im)]) & 0x00ffu;
		uint prev_p	 = uint(g_OsdQuadOffsetBuffer[int(patchData.z + ip)]) & 0xff00u;
		prev_p = uint(prev_p/256);

		uint np = abs(g_valence[ip]);
		uint nm = abs(g_valence[im]);

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
		
		float3 Ep = g_position[i] + g_e0[i] * csf(n-3, 2*start) + g_e1[i]*csf(n-3, 2*start + 1);
		float3 Em = g_position[i] + g_e0[i] * csf(n-3, 2*prev ) + g_e1[i]*csf(n-3, 2*prev + 1);

		float3 Em_ip = g_position[ip] + g_e0[ip]*csf(np-3, 2*prev_p)  + g_e1[ip]*csf(np-3, 2*prev_p + 1);
		float3 Ep_im = g_position[im] + g_e0[im]*csf(nm-3, 2*start_m) + g_e1[im]*csf(nm-3, 2*start_m + 1);

		float s1 = 3-2*csf(n-3,2)-csf(np-3,2);
		float s2 = 2*csf(n-3,2);

		float3 Fp = (csf(np-3,2)*g_position[i] + s1*Ep + s2*Em_ip + g_r[i][start])/3.0f;
		s1 = 3.0f-2.0f*cos(2.0f*M_PI/float(n))-cos(2.0f*M_PI/float(nm));
		float3 Fm = (csf(nm-3,2)*g_position[i] + s1*Em +s2*Ep_im - g_r[i][prev])/3.0f;

		g_Ep[i] = Ep;
		g_Em[i] = Em;
		g_Fp[i] = Fp;
		g_Fm[i] = Fm;

	}

	float2 uv = UV;

    float3 p[20];

    p[0]  = g_position[0];
    p[1]  = g_Ep[0]	   ;
    p[2]  = g_Em[0]	   ;
    p[3]  = g_Fp[0]	   ;
    p[4]  = g_Fm[0]	   ;
		  	
    p[5]  = g_position[1];
    p[6]  = g_Ep[1]	   ;
    p[7]  = g_Em[1]	   ;
    p[8]  = g_Fp[1]	   ;
    p[9]  = g_Fm[1]	   ;
			
    p[10] = g_position[2];
    p[11] = g_Ep[2]	   ;
    p[12] = g_Em[2]	   ;
    p[13] = g_Fp[2]	   ;
    p[14] = g_Fm[2]	   ;
			
    p[15] = g_position[3];
    p[16] = g_Ep[3]	   ;
    p[17] = g_Em[3]	   ;
    p[18] = g_Fp[3]	   ;
    p[19] = g_Fm[3]	   ;

    float3 q[16];

    float U = 1-uv.x, V=1-uv.y;

    float d11 = uv.x+uv.y; if(uv.x+uv.y==0.0f) d11 = 1.0f;
    float d12 = U+uv.y; if(U+uv.y==0.0f) d12 = 1.0f;
    float d21 = uv.x+V; if(uv.x+V==0.0f) d21 = 1.0f;
    float d22 = U+V; if(U+V==0.0f) d22 = 1.0f;

    q[ 5] = (uv.x*p[3] + uv.y*p[4])/d11;
    q[ 6] = (U*p[9] + uv.y*p[8])/d12;
    q[ 9] = (uv.x*p[19] + V*p[18])/d21;
    q[10] = (U*p[13] + V*p[14])/d22;

    q[ 0] = p[0];
    q[ 1] = p[1];
    q[ 2] = p[7];
    q[ 3] = p[5];
    q[ 4] = p[2];
    q[ 7] = p[6];
    q[ 8] = p[16];
    q[11] = p[12];
    q[12] = p[15];
    q[13] = p[17];
    q[14] = p[11];
    q[15] = p[10];

    float3 Tangent   = float3(0, 0, 0);
    float3 BiTangent = float3(0, 0, 0);

    static float B[4], D[4];
    static float3 BUCP[4], DUCP[4];

    Univar4x4(uv.x, B, D);

    for (int i=0; i<4; ++i) {
        BUCP[i] =  float3(0, 0, 0);
        DUCP[i] =  float3(0, 0, 0);

        for (uint j=0; j<4; ++j) {
            // reverse face front
            float3 A = q[i + 4*j];

            BUCP[i] += A * B[j];
            DUCP[i] += A * D[j];
        }
    }


    Univar4x4(uv.y, B, D);

    for (uint j=0; j<4; ++j) {
        WorldPos  += B[j] * BUCP[j];
        Tangent   += B[j] * DUCP[j];
        BiTangent += D[j] * BUCP[j];
    }

	Normal = normalize(cross(BiTangent, Tangent));
	WorldPos += disp * Normal;
}

struct SGregory
{
	float3 position;
	int valence;
	float3 r[OSD_MAX_VALENCE];	
	float3 e0;
    float3 e1;

	float3 Ep;
    float3 Em;
    float3 Fp;
    float3 Fm;
};


groupshared SGregory dgreg[4];

void LoadGregorySharedMem(in uint3 patchData, int idx)
{

	//load to shared mem
	{
		int v = idx;
		SGregory greg;
		int vIdx = g_IndexBuffer[patchData.y + v];
		float3 pos = float3(	g_VertexBuffer[vIdx*OSD_NUM_ELEMENTS + 0],
									g_VertexBuffer[vIdx*OSD_NUM_ELEMENTS + 1],
									g_VertexBuffer[vIdx*OSD_NUM_ELEMENTS + 2]);
		    
		int ivalence = g_OsdValenceBuffer[int(vIdx * (2 * OSD_MAX_VALENCE + 1))]; 
		greg.valence = ivalence;
		uint valence = uint(abs(ivalence));

		float3 f[OSD_MAX_VALENCE]; 
		float3 opos = float3(0,0,0);
	
		float3 r[OSD_MAX_VALENCE]; 

		for (uint vl=0; vl<valence; ++vl) 
		{
			uint im=(vl+valence-1)%valence; 
			uint ip=(vl+1)%valence; 

			uint idx_neighbor = uint(g_OsdValenceBuffer[int(vIdx * (2*OSD_MAX_VALENCE+1) + 2*vl + 0 + 1)]);


			float3 neighbor = float3(	g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor+1)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor+2)]);

			uint idx_diagonal = uint(g_OsdValenceBuffer[int(vIdx * (2*OSD_MAX_VALENCE+1) + 2*vl + 1 + 1)]);

			float3 diagonal = float3(	g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+1)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+2)]);

			uint idx_neighbor_p = uint(g_OsdValenceBuffer[int(vIdx * (2*OSD_MAX_VALENCE+1) + 2*ip + 0 + 1)]);

			float3 neighbor_p =  float3(g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_p)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_p+1)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_p+2)]);

			uint idx_neighbor_m = uint(g_OsdValenceBuffer[int(vIdx * (2*OSD_MAX_VALENCE+1) + 2*im + 0 + 1)]);

			float3 neighbor_m =	float3(	g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_m)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_m+1)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_m+2)]);

			uint idx_diagonal_m = uint(g_OsdValenceBuffer[int(vIdx * (2*OSD_MAX_VALENCE+1) + 2*im + 1 + 1)]);

			float3 diagonal_m =	float3(	g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal_m)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal_m+1)],
										g_VertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal_m+2)]);

			f[vl] = (pos * float(valence) + (neighbor_p + neighbor)*2.0f + diagonal) / (float(valence)+5.0f);

			opos += f[vl];			
			r[vl] = (neighbor_p-neighbor_m)/3.0f + (diagonal - diagonal_m)/6.0f;
		}
		
		greg.r = r;
		opos /= valence;
		greg.position = opos;

		float3 e;
		greg.e0 = float3(0,0,0);
		greg.e1 = float3(0,0,0);

		for(uint vs=0; vs<valence; ++vs) 
		{
		    uint im = (vs + valence -1) % valence;
		    e = 0.5f * (f[vs] + f[im]);
		    greg.e0 += csf(valence-3, 2*vs) *e;
		    greg.e1 += csf(valence-3, 2*vs + 1)*e;
		}
		greg.e0 *= ef[valence - 3];
		greg.e1 *= ef[valence - 3];

		greg.Ep=0;
		greg.Em=0;
		greg.Fp=0;
		greg.Fm=0;	
		dgreg[v] = greg;
	}

	GroupMemoryBarrierWithGroupSync();
		
	{
		int v2 = idx;
		uint i = v2;
		uint ip = (i+1)%4;
		uint im = (i+3)%4;
		uint valence = abs(dgreg[i].valence); // defined above
		uint n = valence;

		uint start = uint(g_OsdQuadOffsetBuffer[int(patchData.z + i)]) & 0x00ffu;
		uint prev  = uint(g_OsdQuadOffsetBuffer[int(patchData.z + i)]) & 0xff00u;
		prev = uint(prev/256);

		uint start_m = uint(g_OsdQuadOffsetBuffer[int(patchData.z + im)]) & 0x00ffu;
		uint prev_p	 = uint(g_OsdQuadOffsetBuffer[int(patchData.z + ip)]) & 0xff00u;
		prev_p = uint(prev_p/256);


		uint np = abs(dgreg[ip].valence);
		uint nm = abs(dgreg[im].valence);

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
		
		float3 Ep = dgreg[i].position + dgreg[i].e0 * csf(n-3, 2*start) + dgreg[i].e1*csf(n-3, 2*start + 1);
		float3 Em = dgreg[i].position + dgreg[i].e0 * csf(n-3, 2*prev ) + dgreg[i].e1*csf(n-3, 2*prev + 1);

		float3 Em_ip = dgreg[ip].position + dgreg[ip].e0*csf(np-3, 2*prev_p)  + dgreg[ip].e1*csf(np-3, 2*prev_p + 1);
		float3 Ep_im = dgreg[im].position + dgreg[im].e0*csf(nm-3, 2*start_m) + dgreg[im].e1*csf(nm-3, 2*start_m + 1);

		float s1 = 3-2*csf(n-3,2)-csf(np-3,2);
		float s2 = 2*csf(n-3,2);

		float3 Fp = (csf(np-3,2)*dgreg[i].position + s1*Ep + s2*Em_ip + dgreg[i].r[start])/3.0f;
		s1 = 3.0f-2.0f*cos(2.0f*M_PI/float(n))-cos(2.0f*M_PI/float(nm));
		float3 Fm = (csf(nm-3,2)*dgreg[i].position + s1*Em +s2*Ep_im - dgreg[i].r[prev])/3.0f;

		dgreg[i].Ep = Ep;
		dgreg[i].Em = Em;
		dgreg[i].Fp = Fp;
		dgreg[i].Fm = Fm;

	}
}

float3 EvalGregorySharedMem(in float2 UV, in float disp, in int level, out float3 Normal)
{
	float2 uv = UV;
		
    float3 q[16];

    float U = 1-uv.x, V=1-uv.y;      

    float d11 = uv.x+uv.y;  if(d11==0.0f) d11 = 1.0f;
    q[ 5] = (uv.x*dgreg[0].Fp/*p[3]*/ + uv.y*dgreg[0].Fm/*p[4]*/)/d11;

    float d12 = U+uv.y;     if(d12==0.0f) d12 = 1.0f;
    q[ 6] = (U*dgreg[1].Fm/*p[9]*/ + uv.y*dgreg[1].Fp/*p[8]*/)/d12;

    float d21 = uv.x+V;     if(d21==0.0f) d21 = 1.0f;
    q[ 9] = (uv.x*dgreg[3].Fm/*p[19]*/ + V*dgreg[3].Fp/*p[18]*/)/d21;

    float d22 = U+V;        if(d22==0.0f) d22 = 1.0f;
    q[10] = (U*dgreg[2].Fp/*p[13]*/ + V*dgreg[2].Fm/*p[14]*/)/d22;

    q[ 0] = dgreg[0].position;
    q[ 1] = dgreg[0].Ep;
    q[ 2] = dgreg[1].Em;
    q[ 3] = dgreg[1].position;
    q[ 4] = dgreg[0].Em;
    q[ 7] = dgreg[1].Ep;
    q[ 8] = dgreg[3].Ep;
    q[11] = dgreg[2].Em;
    q[12] = dgreg[3].position;
    q[13] = dgreg[3].Em;
    q[14] = dgreg[2].Ep;
    q[15] = dgreg[2].position;
	    
		
    static float B[4], D[4];
    static float3 BUCP[4], DUCP[4];

    Univar4x4(uv.x, B, D);
        
    for (int i=0; i<4; ++i) {
        BUCP[i] =  float3(0, 0, 0);
        DUCP[i] =  float3(0, 0, 0);
        
        [unroll(4)]
        for (uint j=0; j<4; ++j) {
            // reverse face front
    		float3 A = q[i + 4*j];

            BUCP[i] += A * B[j];
            DUCP[i] += A * D[j];
        }
    }

    
    Univar4x4(uv.y, B, D);

    float3 WorldPos = float3(0,0,0);
    float3 Tangent   = float3(0, 0, 0);
    float3 BiTangent = float3(0, 0, 0);

    for (uint k=0; k<4; ++k) {
        WorldPos  += B[k] * BUCP[k];
        Tangent   += B[k] * DUCP[k];
        BiTangent += D[k] * BUCP[k];
    }

	BiTangent *= 3 * level;
    Tangent *= 3 * level;

	Normal = normalize(cross(BiTangent, Tangent));
	WorldPos += disp * Normal;    
    return WorldPos;
}

#ifdef USE_VOXELDEFORM
Buffer<uint>			g_bufVoxels				: register(t10);

#include "VoxelDDA.hlsl"
#include "Material.h.hlsl"

#endif



//should be set by applicaiton
#ifndef TILE_SIZE
#define TILE_SIZE 128
#endif

#ifndef COLOR_DISPATCH_TILE_SIZE
#define COLOR_DISPATCH_TILE_SIZE 16
#endif

#ifndef DISPLACEMENT_DISPATCH_TILE_SIZE
#define DISPLACEMENT_DISPATCH_TILE_SIZE 16
#endif

#define REGULAR_USE_SHARED_MEM
#define GREGORY_USE_SHARED_MEM

#ifdef REGULAR_USE_SHARED_MEM  
groupshared float3 g_CP[16];
#endif

#ifdef USE_COLOR_BRUSH
[numthreads(COLOR_DISPATCH_TILE_SIZE, COLOR_DISPATCH_TILE_SIZE, 1)]
#define NUM_BLOCKS (TILE_SIZE/COLOR_DISPATCH_TILE_SIZE)
#else
#define NUM_BLOCKS (TILE_SIZE/DISPLACEMENT_DISPATCH_TILE_SIZE)
[numthreads(DISPLACEMENT_DISPATCH_TILE_SIZE, DISPLACEMENT_DISPATCH_TILE_SIZE, 1)]
#endif	
void TileEditCS(
	uint3 blockIdx	: SV_GroupID, 
	uint3 DTid		: SV_DispatchThreadID, 
	uint3 threadIdx : SV_GroupThreadID,
	uint  GI		: SV_GroupIndex )
{

		
#ifdef REGULAR
#ifdef WITH_CULLING	
	uint2 patchData = g_patchDataRegular[blockIdx.x];
#else		
	uint localPatchID = blockIdx.x;
	uint2 patchData = uint2(localPatchID + g_PrimitiveIdBase, g_IndexStart + g_NumIndicesPerPatch * localPatchID);
#endif
	#ifdef REGULAR_USE_SHARED_MEM
	uint v = threadIdx.x % 16;
	int vIdx = g_IndexBuffer[patchData.y + v];
	g_CP[v] = float3(	g_VertexBuffer[vIdx*OSD_NUM_ELEMENTS + 0],
						g_VertexBuffer[vIdx*OSD_NUM_ELEMENTS + 1],
						g_VertexBuffer[vIdx*OSD_NUM_ELEMENTS + 2]);
	#endif

#else
#ifdef WITH_CULLING
	uint3 patchData = g_patchDataGregory[blockIdx.x];

#else
	uint localPatchID = blockIdx.x;
	uint3 patchData = uint3(localPatchID + g_PrimitiveIdBase,
									g_IndexStart + g_NumIndicesPerPatch * localPatchID,									
									4*localPatchID + g_GregoryQuadOffsetBase);		
#endif

    #ifdef GREGORY_USE_SHARED_MEM
    	LoadGregorySharedMem(patchData, threadIdx.x % 4);
    	GroupMemoryBarrierWithGroupSync(); //checkme remove sync
    #endif
#endif
	int patchLevel = GetLevel(patchData.x);
	uint tileSize = (uint) TILE_SIZE;

	if(patchLevel > 0)
	{
		float splitter = (0x1 << (patchLevel));		
		float ts =  (float)tileSize / splitter;

		if(ts < 1.0) return; // skip patchLevel that do not affect tile texels
		tileSize /= splitter;
				
		if(ts<1.f)
			tileSize = 1;
	}
	
	

	uint2 idx = threadIdx.xy;	// index in tile
#ifdef USE_COLOR_BRUSH
	if (NUM_BLOCKS > 1)
		idx += COLOR_DISPATCH_TILE_SIZE * uint2(blockIdx.y % NUM_BLOCKS, blockIdx.y / NUM_BLOCKS);
#else
	if (NUM_BLOCKS > 1)
		idx += DISPLACEMENT_DISPATCH_TILE_SIZE * uint2(blockIdx.y % NUM_BLOCKS, blockIdx.y / NUM_BLOCKS);
#endif

	
	if(idx.x >= tileSize || idx.y >= tileSize)
		return;
	
	float2 UV = 0.f;	// uv coord in patch space 0..1 spanned by updated tile size
	if(tileSize > 1)
	{		
		UV = 1.0f / (tileSize) * (idx.xy+0.5);
	}

	
	// get displacement
	float4 patchCoord = float4(0, 0, patchLevel, 0);  

	int4 ptexInfo; //(ptex u,v, ptex level, ptex rotation)
	GetPatchInfo(UV,patchData.x, patchCoord, ptexInfo);
	int faceID = int(patchCoord.w);
	if (!g_ptexFaceVisibleSRV[faceID]) // Early exit - visibility
		return;

#define _DEBUG_COLOR 	
#ifdef DEBUG_COLOR
	DebugColor(faceID, patchCoord, patchLevel);
	return;	
#endif

#ifdef USE_COLOR_BRUSH
    PtexPacking ppack = getPtexPacking(g_TileInfoColor, faceID);
#else // USE_SCULPT_BRUSH or USE_VOXELDEFORM
	PtexPacking ppack = getPtexPacking(g_TileInfo, faceID);
#endif

	if(ppack.page == -1)	// early exit if tile data is not allocated
		return ;	

	float2 coords = float2(patchCoord.x * ppack.tileSize + ppack.uOffset,
		patchCoord.y * ppack.tileSize + ppack.vOffset);
	

	uint2 ucoord = uint2(coords);    
	float disp = 0;
	disp = g_displacementUAV[int3(ucoord.x, ucoord.y, ppack.page)];

	// load control points // to shared memory
	
    float3 WorldPos		=  float3(0.0f, 0.0f, 0.0f);
	float3 Normal;


#ifdef REGULAR    
		
	int rotation = ptexInfo.w;
	if(rotation == 0) UV.xy = UV.xy;
	if(rotation == 1) UV.xy = float2(UV.y, 1.0-UV.x);
	if(rotation == 2) UV.xy = float2(1.0-UV.x, 1.0-UV.y);
	if(rotation == 3) UV.xy = float2(1.0-UV.y, UV.x);
	EvalRegular(patchData, UV, WorldPos, Normal, disp, ptexInfo);
		
#ifdef REGULAR_USE_SHAREDMEM

	Eval(UV, WorldPos, Normal, g_CP); 
#else	
	static float3 CP[16];	

	[unroll(16)]
	for(uint v2 = 0; v2 < 16; ++v2)
	{
		int vIdx = g_IndexBuffer[patchData.y + v2];
		CP[v2] = float3(	g_VertexBuffer[vIdx*OSD_NUM_ELEMENTS + 0],
						g_VertexBuffer[vIdx*OSD_NUM_ELEMENTS + 1],
						g_VertexBuffer[vIdx*OSD_NUM_ELEMENTS + 2]);

	}
	Eval(UV, WorldPos, Normal, CP); 
#endif
	
	WorldPos += disp * Normal;

#endif
#ifdef GREGORY
	
    #ifdef GREGORY_USE_SHARED_MEM
        WorldPos = EvalGregorySharedMem(UV.yx, disp, patchLevel, Normal);
    #else                
	    EvalGregory(patchData, UV.yx, WorldPos, Normal, disp, ptexInfo);
    #endif

#endif

#if defined(USE_COLOR_BRUSH) || defined (USE_SCULPT_BRUSH)
	BrushEdit(WorldPos, ucoord, ppack);
#endif

#ifdef USE_VOXELDEFORM

	float3 rayOrigin = mul((g_matModelToVoxel), float4(WorldPos,1.0)).xyz;
	float3 rayDir = normalize(mul(((float3x3)(g_matModelToVoxel)), float3(-Normal.xyz)).xyz);//
	static float dist = 0;
	
#ifdef WITH_MULTISAMPLING
	float3 tmp = rayDir;
	int which = 0;
	float val = tmp.x;
	if(tmp.y > val) {val = tmp.y; which  = 1;};
	if(tmp.z > val) {which  = 2;};

	if(which == 0) tmp.x = 0;
	if(which == 1) tmp.y = 0;
	if(which == 2) tmp.z = 0;
	

	float3 t = cross(rayDir, normalize(tmp));
	float3 b = cross(t, rayDir);
	float3 rayO = rayOrigin;	
	float sumDist = 0;
	for(int s = 0; s < 5; ++s)
	{
		if(s == 1) {rayOrigin = rayO+1.5*b;}
		if(s == 2) {rayOrigin = rayO-1.5*b;}
		if(s == 3) {rayOrigin = rayO+1.5*t;}
		if(s == 4) {rayOrigin = rayO-1.5*t;}
	
		float2 uvPos = UV;
		// variant 2
		if(s== 1)	uvPos.x += 1.f/128;
		if(s== 2)	uvPos.x -= 1.f/128;
		if(s== 3)	uvPos.y += 1.f/128;
		if(s== 4)	uvPos.y -= 1.f/128;
		WorldPos		=  float3(0.0f, 0.0f, 0.0f);
		#ifdef REGULAR 
			#ifdef REGULAR_USE_SHARED_MEM
				Eval(saturate(uvPos), WorldPos, Normal, g_CP); 
			#else
				Eval(saturate(uvPos), WorldPos, Normal, CP); 
			#endif
		#endif
		#ifdef GREGORY	
			#ifdef GREGORY_USE_SHARED_MEM
				WorldPos = EvalGregorySharedMem(saturate(uvPos).yx, disp, patchLevel, Normal);
			#else                
				EvalGregory(patchData, saturate(uvPos).yx, WorldPos, Normal, disp, ptexInfo);
			#endif
		#endif
		WorldPos += disp * Normal;
		rayOrigin = mul((g_matModelToVoxel), float4(WorldPos,1.0)).xyz;
		rayDir = normalize(mul(((float3x3)(g_matModelToVoxel)), float3(-Normal.xyz)).xyz);//

	
		// check if outside the box
		if (VoxelDDA(rayOrigin, rayDir, dist) == false) 
		{
			dist = 0;
			return;
		} 
		else // we have and intersection
		{
			float3 p = rayDir * dist;
			p = mul((float3x3)(((g_matNormal))), p);
			dist = disp - length(p);
		}
		sumDist += dist;
	}
	if(dist == 0) return;
	dist = sumDist * 0.2;	

#else
	//is only false if outside the box
	if (VoxelDDA(rayOrigin, rayDir, dist) == false) 
	{
		return;
	} 
	else // we have an intersection depth
	{
		if (dist == 0.0f)	
		{
			return;
		}
		float3 p = rayDir * dist;
		p = mul((float3x3)(((g_matNormal))), p);
		dist = disp - length(p);
	}
#endif


	float omega = g_Smoothness;
#ifdef WITH_CONSTRAINTS
	// constraints uav is bound, but we cannot reuse slot u0 due to compiler
	float dispOut = lerp(g_displacementUAV[int3(ucoord.x, ucoord.y, ppack.page)], dist, omega);
	g_displacementUAV[int3(ucoord.x, ucoord.y, ppack.page)] = dispOut;
#else
	float outDisp = lerp(g_displacementUAV[int3(ucoord.x, ucoord.y, ppack.page)], dist, omega);
	g_displacementUAV[int3(ucoord.x, ucoord.y, ppack.page)] = outDisp;
	uint oldVal = 0;
	InterlockedMax(g_maxPatchDisplacement[patchData.x], asuint(abs(outDisp)), oldVal);	
#endif

#endif


}


#ifdef DISPLACEMENT_TRANSFER

void GetPatchInfoTransfer(float2 UV, uint patchDataX, inout float4 patchCoord, inout int4 ptexInfo)
{
	int2 ptexIndex = g_OsdPatchParamBuffer[patchDataX].xy; 
    int ptexFaceID = ptexIndex.x;                                          
    int lv = 1 << ((ptexIndex.y & 0xf) - ((ptexIndex.y >> 4) & 1));    
    int u = (ptexIndex.y >> 17) & 0x3ff;				                           
    int v = (ptexIndex.y >> 7) & 0x3ff;                                
    int rotation = (ptexIndex.y >> 5) & 0x3;            // ptex rotation               
    patchCoord.w = ptexFaceID;                                  
    ptexInfo = int4(u, v, lv, rotation); 

	/////////////////////////////////////

	#if OSD_TRANSITION_ROTATE == 1
		patchCoord.xy = float2(UV.y, 1.0-UV.x);
	#elif OSD_TRANSITION_ROTATE == 3
		patchCoord.xy = float2(1.0-UV.x, 1.0-UV.y);
	#elif OSD_TRANSITION_ROTATE == 2
		patchCoord.xy = float2(1.0-UV.y, UV.x);
	#elif OSD_TRANSITION_ROTATE == 0 //, or non-transition patch
		patchCoord.xy = float2(UV.x, UV.y);
	#endif

	float2 uv = patchCoord.xy;
	int2 p = ptexInfo.xy;                                  
    int rot = ptexInfo.w;                                  

	if(rotation == 0) uv.xy = uv.xy;
	if(rotation == 1) uv.xy = float2(1.0-uv.y, uv.x);
	if(rotation == 2) uv.xy = float2(1.0-uv.x, 1.0-uv.y);
	if(rotation == 3) uv.xy = float2(uv.y, 1.0-uv.x); 

    patchCoord.xy = (uv * float2(1.0,1.0)/lv) + float2(p.x, p.y)/lv; 
	patchCoord.xy = (uv/lv) + p/(float)lv; 
}

Texture2D				g_txDisp	: register(t10);
Buffer<float>			g_atlasUV   : register(t11);

#define NUM_BLOCKS2 (TILE_SIZE/DISPLACEMENT_DISPATCH_TILE_SIZE)
[numthreads(DISPLACEMENT_DISPATCH_TILE_SIZE, DISPLACEMENT_DISPATCH_TILE_SIZE, 1)]
void DisplacementTransferOSDCS(
	uint3 blockIdx	: SV_GroupID, 
	uint3 DTid		: SV_DispatchThreadID, 
	uint3 threadIdx : SV_GroupThreadID,
	uint  GI		: SV_GroupIndex )
{

#ifdef REGULAR
	uint2 patchData = g_patchDataRegular[blockIdx.x];
#else
	uint3 patchData = g_patchDataGregory[blockIdx.x];
#endif
	int patchLevel = GetLevel(patchData.x);

	uint tileSize = (uint) TILE_SIZE;

	if(patchLevel > 0)
	{
		float splitter = (0x1 << (patchLevel));		
		float ts =  (float)tileSize / splitter;
		if(ts < 1) return; // skip patchLevel that do not affect tile texels
		tileSize /= splitter;
	}
	

	uint2 idx = threadIdx.xy;	// index in tile
	if (NUM_BLOCKS2 > 1)
		idx += DISPLACEMENT_DISPATCH_TILE_SIZE * uint2(blockIdx.y % NUM_BLOCKS2, blockIdx.y / NUM_BLOCKS2);


	if(idx.x >= tileSize || idx.y >= tileSize)
		return;
	
	float2 UV = 0.f;	// uv coord in patch space 0..1 spanned by updated tile size
	if(tileSize > 1)
	{
		UV = (idx.xy+0.5) / float(tileSize); // checkme was above line
	}

	
	// get displacement
	float4 patchCoord = float4(0, 0, patchLevel, 0);  // (patch u,v, subd level, patchID)

	int4 ptexInfo; //(ptex u,v, ptex level, ptex rotation)
	GetPatchInfoTransfer(UV,patchData.x, patchCoord, ptexInfo); 
	int faceID = int(patchCoord.w);
	if (!g_ptexFaceVisibleSRV[faceID]) // Early exit - visibility
		return;



	#define OSD_FVAR_WIDTH 2
	uint fvarOffset = 0;
	uint primOffset = (patchData.x)*4;
	float2 uvTex[4];
	for(int i =0; i < 4; ++i)
	{
		int index =  (primOffset+i)*OSD_FVAR_WIDTH+fvarOffset;
		uvTex[i] = float2( g_atlasUV[index],  g_atlasUV[index+1]);
	}

	float2 uvCoordTex = lerp(lerp(uvTex[0], uvTex[1], UV.x)	,	
							 lerp(uvTex[3], uvTex[2], UV.x)
							,UV.y);

	uvCoordTex = float2(uvCoordTex.x, 1-uvCoordTex.y);
	float disp = g_txDisp.SampleLevel(g_sampler,  uvCoordTex, 0, 0);
	
	
	PtexPacking ppack = getPtexPacking(g_TileInfo, faceID);
	if(ppack.page == -1)	return ;		// early exit if tile data is not allocated

	float2 coords = float2(patchCoord.x * ppack.width + ppack.uOffset,
                           patchCoord.y * ppack.height + ppack.vOffset);
	


	uint2 ucoord = uint2(coords);
	
	g_displacementUAV[int3(ucoord.x, ucoord.y, ppack.page)] = disp * 0.25;//UV.y/128.0;
}

#endif	


#ifdef WITH_CONSTRAINTS
Texture2DArray<float>	g_constraintsSRV		: register(t10);

bool checkFlagSet(float val)
{
	int bm = asint(val);
	if(bm&1 == 1) return true;
	return false;
}

float boxFilter(in uint i, in uint j, in uint page, in Texture2DArray<float> input) {
	return
		(
		input[uint3(i-1, j-1,page)] +
		input[uint3(i-1, j+1,page)] +
		input[uint3(i+1, j-1,page)] +
		input[uint3(i+1, j+1,page)] +
		input[uint3(i-1, j+0,page)] +
		input[uint3(i+1, j+0,page)] +
		input[uint3(i+0, j-1,page)] +
		input[uint3(i+0, j+1,page)] +
		input[uint3(i,j,page)]
	) * (1.0f/9.0f);
}

float solveXi(float b, uint i, uint j, in uint page, in Texture2DArray<float> input) 
{
	float otherX =
	(1.0f/64.0f)*(
		input[uint3(i-1, j-1,page)] +
		input[uint3(i-1, j+1,page)] +
		input[uint3(i+1, j-1,page)] +
		input[uint3(i+1, j+1,page)])
		+					
	(3.0f/32.0f)*(			
		input[uint3(i-1, j+0,page)] +
		input[uint3(i+1, j+0,page)] +
		input[uint3(i+0, j-1,page)] +
		input[uint3(i+0, j+1,page)]);
	return (b - otherX) / (9.0f/16.0f);
}

float evalDispMiddle(uint i, uint j, in uint page, in Texture2DArray<float> input)
{
	return
	(1.0f/64.0f)*(
		input[uint3(i-1, j-1,page)] +
		input[uint3(i-1, j+1,page)] +
		input[uint3(i+1, j-1,page)] +
		input[uint3(i+1, j+1,page)])
		+						  
	(3.0f/32.0f)*(				  
		input[uint3(i-1, j+0,page)] +
		input[uint3(i+1, j+0,page)] +
		input[uint3(i+0, j-1,page)] +
		input[uint3(i+0, j+1,page)]) 
		+
	(9.0f/16.0f)*
		input[uint3(i,j,page)];
}

float evalDispMiddleUAV(uint i, uint j, in uint page, in RWTexture2DArray<float> input)
{
	return
	(1.0f/64.0f)*(
		input[uint3(i-1, j-1,page)] +
		input[uint3(i-1, j+1,page)] +
		input[uint3(i+1, j-1,page)] +
		input[uint3(i+1, j+1,page)])
		+						  
	(3.0f/32.0f)*(				  
		input[uint3(i-1, j+0,page)] +
		input[uint3(i+1, j+0,page)] +
		input[uint3(i+0, j-1,page)] +
		input[uint3(i+0, j+1,page)]) 
		+
	(9.0f/16.0f)*
		input[uint3(i,j,page)];
}


float  gaussD(float sigma, int x, int y)
{
	return exp(-((x*x+y*y)/(2.0f*sigma*sigma)));
}

#define KERNEL_SIZE 1
float filterGauss(uint i, uint j, in uint page, in Texture2DArray<float> input) {
	float avg = 0.0f;
	float weight = 0.0f;
	for (int k = -KERNEL_SIZE; k <= KERNEL_SIZE; k++) {
		for (int l = -KERNEL_SIZE; l <= KERNEL_SIZE; l++) {
			float sigma = 0.5f * KERNEL_SIZE;
			float currW = gaussD(sigma, k, l);
			avg += input[uint3(k+i, l+j,page)] * currW;
			weight += currW;
		}
	}
	return avg /= (2*KERNEL_SIZE+1)*(2*KERNEL_SIZE+1);
	//return avg /= weight;
}

float getSmoothedValueQuadratic(uint i, uint j, in uint page) 
{
	float actual = g_constraintsSRV[uint3(i,j,page)];
	float middle = evalDispMiddle(i,j, page, g_constraintsSRV);
	return min(actual, middle);
}


float getSmoothedValueGauss(uint i, uint j, in uint page) 
{
	float actual = g_constraintsSRV[uint3(i,j,page)];
	float middle =  filterGauss(i,j, page, g_constraintsSRV);
	return min(actual, middle);
}

float getSmoothedValueLinear(uint i, uint j, uint page) 
{
	float actual = g_constraintsSRV[uint3(i,j,page)];
	return actual;

	float middle = boxFilter(i,j,page, g_constraintsSRV);
	return min(actual, middle);
}

#define NUM_BLOCKS3 (TILE_SIZE/DISPLACEMENT_DISPATCH_TILE_SIZE)
[numthreads(DISPLACEMENT_DISPATCH_TILE_SIZE, DISPLACEMENT_DISPATCH_TILE_SIZE, 1)]
void ApplyConstraintsOSDCulledCS(
	uint3 blockIdx	: SV_GroupID, 
	uint3 DTid		: SV_DispatchThreadID, 
	uint3 threadIdx : SV_GroupThreadID,
	uint  GI		: SV_GroupIndex )
{

#ifdef REGULAR
	uint2 patchData = g_patchDataRegular[blockIdx.x];
#else
	uint3 patchData = g_patchDataGregory[blockIdx.x];
#endif
	int patchLevel = GetLevel(patchData.x);
	uint tileSize = (uint) TILE_SIZE;

	if(patchLevel > 0)
	{
		float splitter = (0x1 << (patchLevel));		
		float ts =  (float)tileSize / splitter;
		if(ts < 1) return; // skip patchLevel that do not affect tile texels
				
		tileSize /= splitter;
		if(ts<1.f)
			tileSize = 1;
	}
	

	uint2 idx = threadIdx.xy;	// index in tile
	if (NUM_BLOCKS3 > 1)
		idx += DISPLACEMENT_DISPATCH_TILE_SIZE * uint2(blockIdx.y % NUM_BLOCKS3, blockIdx.y / NUM_BLOCKS3);


	if(idx.x >= tileSize || idx.y >= tileSize)
		return;
	
	float2 UV = 0.f;	// uv coord in patch space 0..1 spanned by updated tile size
	if(tileSize > 1)
	{		
		UV = 1.0f / (tileSize) * (idx.xy+0.5); // checkme was above line
	}

	
	// get displacement
	float4 patchCoord = float4(0, 0, patchLevel, 0);  // (patch u,v, subd level, patchID)

	int4 ptexInfo; //(ptex u,v, ptex level, ptex rotation)
	GetPatchInfo(UV,patchData.x, patchCoord, ptexInfo);
	int faceID = int(patchCoord.w);
	if (!g_ptexFaceVisibleSRV[faceID]) // Early exit - visibility
		return;


	PtexPacking ppack = getPtexPacking(g_TileInfo, faceID);

	if(ppack.page == -1)	return ;		// early exit if tile data is not allocated

	float2 coords = float2(patchCoord.x * ppack.tileSize + ppack.uOffset,
		patchCoord.y * ppack.tileSize  + ppack.vOffset);
	

	//coords -= 0.5;
	uint2 ucoord = uint2(coords);    
	float disp = getSmoothedValueQuadratic(ucoord.x, ucoord.y, ppack.page);
	g_displacementUAV[int3(ucoord.x, ucoord.y, ppack.page)] = disp;
}
#endif

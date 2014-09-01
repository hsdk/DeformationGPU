//
//   Copyright 2013 Pixar
//
//   Licensed under the Apache License, Version 2.0 (the "Apache License")
//   with the following modification; you may not use this file except in
//   compliance with the Apache License and the following modification to it:
//   Section 6. Trademarks. is deleted and replaced with:
//
//   6. Trademarks. This License does not grant permission to use the trade
//      names, trademarks, service marks, or product names of the Licensor
//      and its affiliates, except as required to comply with Section 4(c) of
//      the License and to reproduce the content of the NOTICE file.
//
//   You may obtain a copy of the Apache License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the Apache License with the above modification is
//   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
//   KIND, either express or implied. See the Apache License for the specific
//   language governing permissions and limitations under the Apache License.
//

#include "OSDPatchCommon.hlsl"

#ifndef OSD_MAX_VALENCE
#define OSD_MAX_VALENCE 3
#endif

#ifndef OSD_NUM_ELEMENTS
#define OSD_NUM_ELEMENTS 3
#endif

#if defined OSD_FRACTIONAL_ODD_SPACING
    #define HS_PARTITION "fractional_odd"
#elif defined OSD_FRACTIONAL_EVEN_SPACING
    #define HS_PARTITION "fractional_even"
#else
    #define HS_PARTITION "integer"
#endif

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
    if (j%2 == 0) {
        return cos((2.0f * M_PI * float(float(j-0)/2.0f))/(float(n)+3.0f));
    } else {
        return sin((2.0f * M_PI * float(float(j-1)/2.0f))/(float(n)+3.0f));
    }
}

Buffer<float> OsdVertexBuffer : register( t0 );
Buffer<int> OsdValenceBuffer : register( t1 );

#ifndef OSD_PATCH_GREGORY_BOUNDARY
#ifdef WITH_GREGORY
//#define FINAL_QUADS
#endif
#endif

#ifdef FINAL_QUADS

#ifndef M_PI
#define M_PI 3.14159265
#endif
#define FQ2
struct FinalQuadHullVertex {
    float3 position		 : POSITION0;
    float3 hullPosition  : HULLPOSITION;
    int3   clipFlag		 : CLIPFLAG;
#ifdef FQ2
	uint vID : vertexID;
#else
	uint vID : vertexID;
    int    valence		 : BLENDINDICE0;
	float3 normal		 : HNORMAL;
	float isExtraordinary: EXTRAORDINARY;
	float3 tangent		 : HTANGENT;
	float3 bitangent	 : HBITANGENT;
#endif
    //float3 e0			: POSITION1;
    //float3 e1			: POSITION2;
    //uint zerothNeighbor : BLENDINDICE1;
    //float3 org			: POSITION3;
//#if OSD_MAX_VALENCE > 0
//    float3 r[OSD_MAX_VALENCE] : POSITION4;
//#endif
};

struct FinalQuadDomainVertex {
    float3 position		: POSITION0;
    //float3 Ep			: POSITION1;
    //float3 Em			: POSITION2;
    //float3 Fp			: POSITION3;
    //float3 Fm			: POSITION4;
	float3 normal		 : HNORMAL;
	float isExtraordinary: EXTRAORDINARY;
	float3 tangent		 : HTANGENT;
	float3 bitangent	 : HBITANGENT;
    float4 patchCoord	: TEXTURE0;
    float4 ptexInfo		: TEXTURE1;
};

static const float Bx[3] = {1.0f/6.0f, 2.0f/3.0f, 1.0f/6.0f};
static const float Dx[3] = {-0.5f, 0.0f, 0.5f};
static const float Cx[3] = {1.0f, -2.0f, 1.0f};

#define By Bx
#define Dy Dx
#define Cy Cx

void vs_main_patches( in InputVertex input,
                      uint vID : SV_VertexID,
                      out FinalQuadHullVertex output )
{
    output.hullPosition = mul(ModelViewMatrix, input.position).xyz;
    OSD_PATCH_CULL_COMPUTE_CLIPFLAGS(input.position);

#ifdef FQ2
	output.position.xyz = input.position.xyz;
	output.vID = vID;
#else
	output.vID = vID;
	int ivalence = OsdValenceBuffer[int(vID * (2 * OSD_MAX_VALENCE + 1))];
    output.valence = ivalence;
	
	uint valence = uint(abs(ivalence));
	float fn = valence;

	float3 res_tangent1 = float3(0,0,0);
	float3 res_tangent2 = float3(0,0,0);


	//uint offset = 0;
	//for (uint k = 0; k < valence; k++) {		
	//	uint idx_neighbor = uint(OsdValenceBuffer[int(vID * (2*OSD_MAX_VALENCE+1) + 2*k + 0 + 1)]);
	//	if (idx_neighbor == A[(idx + 1)%4].vID) {
	//		offset = k;
	//		break;
	//	}
	//}
	if(valence != 4)
	{
		float cos_fn = cos(M_PI/fn);
		float tmp = (sqrt(4.0 + cos_fn*cos_fn)-cos_fn)*0.25;
		float3 pos = input.position*(fn*fn);

		for(uint i = 0; i < valence; ++i)
		{
			uint idx_neighbor = uint(OsdValenceBuffer[int(vID * (2*OSD_MAX_VALENCE+1) + 2*i + 0 + 1)]);

			float3 neighbor = float3(	OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor)],
									OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor+1)],
									OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor+2)]);

			uint idx_diagonal = uint(	OsdValenceBuffer[int(vID * (2*OSD_MAX_VALENCE+1) + 2*i + 1 + 1)]);

			float3 diagonal = float3(	OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal)],
									OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+1)],
									OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+2)]);
			pos += neighbor * 4.0 + diagonal;
			
			float alpha1 = cos((2.0 * M_PI * i)/fn);
			float beta1 = tmp*cos((2.0 * M_PI * i + M_PI)/ fn);
			res_tangent2 += alpha1 * neighbor.xyz + beta1 * diagonal.xyz;

			float alpha2 = sin((2.0 * M_PI * i)/fn);
			float beta2 = tmp*sin((2.0 * M_PI * i + M_PI)/ fn);			
			res_tangent1 += alpha2 * neighbor.xyz + beta2 * diagonal.xyz;
		}

		output.position = pos / (fn*(fn+5.0));
	}
	else // final quad vertex regular case
	{
		float3 CP[9];
		for (uint m = 0; m < 4; m++) 
		{
			//uint i = (m + offset) % 4;
		    //uint idx_neighbor = g_ValenceBuffer[(vID - g_Offset) * (2*MAX_VALENCE+1) + 2*i + 0 + 1];
			uint idx_neighbor = uint(OsdValenceBuffer[int(vID * (2*OSD_MAX_VALENCE+1) + 2*m + 0 + 1)]);
			float3 neighbor = float3(	OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor)],
									OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor+1)],
									OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor+2)]);
            CP[((m + 4 + vID) % 4) * 2 + 0] = neighbor;	//neighbor

			//uint idx_diagonal = g_ValenceBuffer[(vID - g_Offset) * (2*MAX_VALENCE+1) + 2*i + 1 + 1];
			uint idx_diagonal = uint(	OsdValenceBuffer[int(vID * (2*OSD_MAX_VALENCE+1) + 2*m + 1 + 1)]);
			float3 diagonal = float3(	OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal)],
									OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+1)],
									OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+2)]);

            CP[((m + 4 + vID) % 4) * 2 + 1] = diagonal;	//diagonal   

		}
		CP[8] = input.position.xyz;

		float3 BUCP[3], DUCP[3];
		BUCP[0] = CP[5] * Bx[0] + CP[6] * Bx[1] + CP[7] * Bx[2];
		BUCP[1] = CP[4] * Bx[0] + CP[8] * Bx[1] + CP[0] * Bx[2];
		BUCP[2] = CP[3] * Bx[0] + CP[2] * Bx[1] + CP[1] * Bx[2];

		output.position.xyz = BUCP[0] * By[0] + BUCP[1] * By[1] + BUCP[2] * By[2];
		
		DUCP[0] = CP[5] * Dx[0] + CP[7] * Dx[2];
		DUCP[1] = CP[4] * Dx[0] + CP[0] * Dx[2];
		DUCP[2] = CP[3] * Dx[0] + CP[1] * Dx[2];

		res_tangent1 = DUCP[0] * By[0] + DUCP[1] * By[1] + DUCP[2] * By[2];
		res_tangent2 = BUCP[0] * Dy[0] + BUCP[1] * Dy[1] + BUCP[2] * Dy[2];
			
		res_tangent1 *= -1.0f;
	}

	res_tangent1 *= -1.0f;
	output.normal = normalize(cross(res_tangent1, res_tangent2));

	if(valence != 4)
	{
		res_tangent1 = res_tangent2 = float3(0.0f, 0.0f, 0.0f);
		output.isExtraordinary = 1.0f;
	} else {
		output.isExtraordinary = 0.0f;
	}
	output.position.xyz = input.position.xyz;
	output.tangent = res_tangent1;
	output.bitangent = res_tangent2;
#endif

}

Buffer<int> OsdQuadOffsetBuffer : register( t2 );

HS_CONSTANT_FUNC_OUT HSConstFunc(
    InputPatch<FinalQuadHullVertex, 4> patch,
    uint primitiveID : SV_PrimitiveID)
{
    HS_CONSTANT_FUNC_OUT output;
    int patchLevel = GetPatchLevel(primitiveID);

    OSD_PATCH_CULL(4);

#ifdef OSD_ENABLE_SCREENSPACE_TESSELLATION
    //output.tessLevelOuter[0] = TessAdaptive(patch[0].hullPosition.xyz, patch[1].hullPosition.xyz);
    //
    //output.tessLevelOuter[1] = TessAdaptive(patch[0].hullPosition.xyz, patch[3].hullPosition.xyz);
    //
    //output.tessLevelOuter[2] = TessAdaptive(patch[2].hullPosition.xyz, patch[3].hullPosition.xyz);
    //
    //output.tessLevelOuter[3] = TessAdaptive(patch[1].hullPosition.xyz, patch[2].hullPosition.xyz);
    //
    //output.tessLevelInner[0] = max(output.tessLevelOuter[1], output.tessLevelOuter[3]);
    //
    //output.tessLevelInner[1] = max(output.tessLevelOuter[0], output.tessLevelOuter[2]);
	//output.tessLevelOuter[0] = TessAdaptive(patch[0].hullPosition.xyz, patch[1].hullPosition.xyz);
    //output.tessLevelOuter[1] = TessAdaptive(patch[0].hullPosition.xyz, patch[3].hullPosition.xyz);
    //output.tessLevelOuter[2] = TessAdaptive(patch[2].hullPosition.xyz, patch[3].hullPosition.xyz);
    //output.tessLevelOuter[3] = TessAdaptive(patch[1].hullPosition.xyz, patch[2].hullPosition.xyz);
    //output.tessLevelInner[0] = max(output.tessLevelOuter[1], output.tessLevelOuter[3]);
    //output.tessLevelInner[1] = max(output.tessLevelOuter[0], output.tessLevelOuter[2]);

	//output.tessLevelOuter[0] = TessAdaptive(patch[0].hullPosition.xyz, patch[3].hullPosition.xyz);
    //output.tessLevelOuter[1] = TessAdaptive(patch[0].hullPosition.xyz, patch[1].hullPosition.xyz);
    //output.tessLevelOuter[2] = TessAdaptive(patch[2].hullPosition.xyz, patch[3].hullPosition.xyz);
    //output.tessLevelOuter[3] = TessAdaptive(patch[1].hullPosition.xyz, patch[2].hullPosition.xyz);
	output.tessLevelOuter[1] = TessAdaptive(patch[0].hullPosition.xyz, patch[1].hullPosition.xyz);
    output.tessLevelOuter[0] = TessAdaptive(patch[0].hullPosition.xyz, patch[3].hullPosition.xyz);
    output.tessLevelOuter[3] = TessAdaptive(patch[2].hullPosition.xyz, patch[3].hullPosition.xyz);
    output.tessLevelOuter[2] = TessAdaptive(patch[1].hullPosition.xyz, patch[2].hullPosition.xyz);
    output.tessLevelInner[0] = max(output.tessLevelOuter[1], output.tessLevelOuter[3]);
    output.tessLevelInner[1] = max(output.tessLevelOuter[0], output.tessLevelOuter[2]);
#else

    output.tessLevelInner[0] = GetTessLevel(patchLevel);
    output.tessLevelInner[1] = GetTessLevel(patchLevel);
    output.tessLevelOuter[0] = GetTessLevel(patchLevel);
    output.tessLevelOuter[1] = GetTessLevel(patchLevel);
    output.tessLevelOuter[2] = GetTessLevel(patchLevel);
    output.tessLevelOuter[3] = GetTessLevel(patchLevel);
#endif

	#ifdef WITH_UV		
		#define OSD_FVAR_WIDTH 2
		uint fvarOffset = 0;
		uint primOffset = (primitiveID+PrimitiveIdBase)*4;
		for(int i =0; i < 4; ++i)
		{
			int index = (primOffset+i)*OSD_FVAR_WIDTH+fvarOffset;
			output.uv[i] = float2( g_atlasUV[index],  g_atlasUV[index+1]);
		}
	#endif

    return output;
}

[domain("quad")]
[partitioning(HS_PARTITION)]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("HSConstFunc")]
FinalQuadDomainVertex hs_main_patches(
    in InputPatch<FinalQuadHullVertex, 4> patch,
    uint primitiveID : SV_PrimitiveID,
    in uint ID : SV_OutputControlPointID )
{
	 FinalQuadDomainVertex output;

#ifdef FQ2
	FinalQuadHullVertex input = patch[ID];
	uint vID = patch[ID].vID;
	uint idx = ID;
	int ivalence = OsdValenceBuffer[int(vID * (2 * OSD_MAX_VALENCE + 1))];
    //output.valence = ivalence;
	
	uint valence = uint(abs(ivalence));
	float fn = valence;

	float3 res_tangent1 = float3(0,0,0);
	float3 res_tangent2 = float3(0,0,0);
	
	uint offset = 0;
	for (uint k = 0; k < valence; k++) 
	{		
		uint idx_neighbor = uint(OsdValenceBuffer[int(vID * (2*OSD_MAX_VALENCE+1) + 2*k + 0 + 1)]);
		if (idx_neighbor == patch[(idx + 1)%4].vID) {
			offset = k;
			break;
		}
	}

	if(valence != 4)
	{
		float cos_fn = cos(M_PI/fn);
		float tmp = (sqrt(4.0 + cos_fn*cos_fn)-cos_fn)*0.25;
		float3 pos = input.position*(fn*fn);

		for(uint l = 0; l < valence; ++l)
		{
			uint i = (l + offset)%valence;
			uint idx_neighbor = uint(OsdValenceBuffer[int(vID * (2*OSD_MAX_VALENCE+1) + 2*i + 0 + 1)]);

			float3 neighbor = float3(	OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor)],
									OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor+1)],
									OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor+2)]);

			uint idx_diagonal = uint(	OsdValenceBuffer[int(vID * (2*OSD_MAX_VALENCE+1) + 2*i + 1 + 1)]);

			float3 diagonal = float3(	OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal)],
									OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+1)],
									OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+2)]);
			pos += neighbor * 4.0 + diagonal;
			
			float alpha1 = cos((2.0 * M_PI * i)/fn);
			float beta1 = tmp*cos((2.0 * M_PI * i + M_PI)/ fn);
			res_tangent2 += alpha1 * neighbor.xyz + beta1 * diagonal.xyz;

			float alpha2 = sin((2.0 * M_PI * i)/fn);
			float beta2 = tmp*sin((2.0 * M_PI * i + M_PI)/ fn);			
			res_tangent1 += alpha2 * neighbor.xyz + beta2 * diagonal.xyz;
		}

		output.position = pos / (fn*(fn+5.0));
	}
	else // final quad vertex regular case
	{
		float3 CP[9];
		for (uint m = 0; m < 4; m++) 
		{
			uint i = (m + offset) % 4;
		    //uint idx_neighbor = g_ValenceBuffer[(vID - g_Offset) * (2*MAX_VALENCE+1) + 2*i + 0 + 1];
			uint idx_neighbor = uint(OsdValenceBuffer[int(vID * (2*OSD_MAX_VALENCE+1) + 2*i + 0 + 1)]);
			float3 neighbor = float3(	OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor)],
									OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor+1)],
									OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor+2)]);
            CP[((m + 4 + vID) % 4) * 2 + 0] = neighbor;	//neighbor

			//uint idx_diagonal = g_ValenceBuffer[(vID - g_Offset) * (2*MAX_VALENCE+1) + 2*i + 1 + 1];
			uint idx_diagonal = uint(	OsdValenceBuffer[int(vID * (2*OSD_MAX_VALENCE+1) + 2*i + 1 + 1)]);
			float3 diagonal = float3(	OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal)],
									OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+1)],
									OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+2)]);

            CP[((m + 4 + vID) % 4) * 2 + 1] = diagonal;	//diagonal   

		}
		CP[8] = input.position.xyz;

		float3 BUCP[3], DUCP[3];
		BUCP[0] = CP[5] * Bx[0] + CP[6] * Bx[1] + CP[7] * Bx[2];
		BUCP[1] = CP[4] * Bx[0] + CP[8] * Bx[1] + CP[0] * Bx[2];
		BUCP[2] = CP[3] * Bx[0] + CP[2] * Bx[1] + CP[1] * Bx[2];

		output.position.xyz = BUCP[0] * By[0] + BUCP[1] * By[1] + BUCP[2] * By[2];
		
		DUCP[0] = CP[5] * Dx[0] + CP[7] * Dx[2];
		DUCP[1] = CP[4] * Dx[0] + CP[0] * Dx[2];
		DUCP[2] = CP[3] * Dx[0] + CP[1] * Dx[2];

		res_tangent1 = DUCP[0] * By[0] + DUCP[1] * By[1] + DUCP[2] * By[2];
		res_tangent2 = BUCP[0] * Dy[0] + BUCP[1] * Dy[1] + BUCP[2] * Dy[2];
			
		res_tangent1 *= -1.0f;
	}

	res_tangent1 *= -1.0f;
	output.normal = normalize(cross(res_tangent1, res_tangent2));

	if(valence != 4)
	{
		res_tangent1 = res_tangent2 = float3(0.0f, 0.0f, 0.0f);
		output.isExtraordinary = 1.0f;
	} else {
		output.isExtraordinary = 0.0f;
	}
	//output.position = patch[ID].position;
	output.tangent = res_tangent1;
	output.bitangent = res_tangent2;

	

#else
    uint i = ID;

    uint ip = (i+1)%4;
    uint im = (i+3)%4;
    uint valence = abs(patch[i].valence);
    uint n = valence;
    int base = GregoryQuadOffsetBase;


    output.position		  = patch[ID].position;
	output.normal		  = patch[ID].normal;
	output.isExtraordinary= patch[ID].isExtraordinary;
	output.tangent		  = patch[ID].tangent;
	output.bitangent	  = patch[ID].bitangent;
#endif
	
    int patchLevel = GetPatchLevel(primitiveID);
    output.patchCoord = float4(0, 0,
                               patchLevel+0.5f,
                               primitiveID+PrimitiveIdBase+0.5f);

    OSD_COMPUTE_PTEX_COORD_HULL_SHADER;

    return output;


//
//
//    uint start = uint(OsdQuadOffsetBuffer[int(4*primitiveID+base + i)]) & 0x00ffu;
//    uint prev = uint(OsdQuadOffsetBuffer[int(4*primitiveID+base + i)]) & 0xff00u;
//    prev = uint(prev/256);
//
//    uint start_m = uint(OsdQuadOffsetBuffer[int(4*primitiveID+base + im)]) & 0x00ffu;
//    uint prev_p = uint(OsdQuadOffsetBuffer[int(4*primitiveID+base + ip)]) & 0xff00u;
//    prev_p = uint(prev_p/256);
//
//    uint np = abs(patch[ip].valence);
//    uint nm = abs(patch[im].valence);
//
//    // Control Vertices based on : 
//    // "Approximating Subdivision Surfaces with Gregory Patches for Hardware Tessellation" 
//    // Loop, Schaefer, Ni, Castafio (ACM ToG Siggraph Asia 2009)
//    //
//    //  P3         e3-      e2+         E2
//    //     O--------O--------O--------O
//    //     |        |        |        |
//    //     |        |        |        |
//    //     |        | f3-    | f2+    |
//    //     |        O        O        |
//    // e3+ O------O            O------O e2-
//    //     |     f3+          f2-     |
//    //     |                          |
//    //     |                          |
//    //     |      f0-         f1+     |
//    // e0- O------O            O------O e1+
//    //     |        O        O        |
//    //     |        | f0+    | f1-    |
//    //     |        |        |        |
//    //     |        |        |        |
//    //     O--------O--------O--------O
//    //  P0         e0+      e1-         E1
//    //
//
//#ifdef OSD_PATCH_GREGORY_BOUNDARY
//    float3 Ep = float3(0.0f,0.0f,0.0f);
//    float3 Em = float3(0.0f,0.0f,0.0f);
//    float3 Fp = float3(0.0f,0.0f,0.0f);
//    float3 Fm = float3(0.0f,0.0f,0.0f);
//
//    float3 Em_ip;
//    if (patch[ip].valence < -2) {
//        uint j = (np + prev_p - patch[ip].zerothNeighbor) % np;
//        Em_ip = patch[ip].position + cos((M_PI*j)/float(np-1))*patch[ip].e0 + sin((M_PI*j)/float(np-1))*patch[ip].e1;
//    } else {
//        Em_ip = patch[ip].position + patch[ip].e0*csf(np-3, 2*prev_p) + patch[ip].e1*csf(np-3, 2*prev_p + 1);
//    }
//
//    float3 Ep_im;
//    if (patch[im].valence < -2) {
//        uint j = (nm + start_m - patch[im].zerothNeighbor) % nm;
//        Ep_im = patch[im].position + cos((M_PI*j)/float(nm-1))*patch[im].e0 + sin((M_PI*j)/float(nm-1))*patch[im].e1;
//    } else {
//        Ep_im = patch[im].position + patch[im].e0*csf(nm-3, 2*start_m) + patch[im].e1*csf(nm-3, 2*start_m + 1);
//    }
//
//    if (patch[i].valence < 0) {
//        n = (n-1)*2;
//    }
//    if (patch[im].valence < 0) {
//        nm = (nm-1)*2;
//    }  
//    if (patch[ip].valence < 0) {
//        np = (np-1)*2;
//    }
//
//    if (patch[i].valence > 2) {
//        Ep = patch[i].position + (patch[i].e0*csf(n-3, 2*start) + patch[i].e1*csf(n-3, 2*start + 1));
//        Em = patch[i].position + (patch[i].e0*csf(n-3, 2*prev) +  patch[i].e1*csf(n-3, 2*prev + 1)); 
//
//        float s1=3-2*csf(n-3,2)-csf(np-3,2);
//        float s2=2*csf(n-3,2);
//
//        Fp = (csf(np-3,2)*patch[i].position + s1*Ep + s2*Em_ip + patch[i].r[start])/3.0f; 
//        s1 = 3.0f-2.0f*cos(2.0f*M_PI/float(n))-cos(2.0f*M_PI/float(nm));
//        Fm = (csf(nm-3,2)*patch[i].position + s1*Em + s2*Ep_im - patch[i].r[prev])/3.0f;
//
//    } else if (patch[i].valence < -2) {
//        uint j = (valence + start - patch[i].zerothNeighbor) % valence;
//
//        Ep = patch[i].position + cos((M_PI*j)/float(valence-1))*patch[i].e0 + sin((M_PI*j)/float(valence-1))*patch[i].e1;
//        j = (valence + prev - patch[i].zerothNeighbor) % valence;
//        Em = patch[i].position + cos((M_PI*j)/float(valence-1))*patch[i].e0 + sin((M_PI*j)/float(valence-1))*patch[i].e1;
//
//        float3 Rp = ((-2.0f * patch[i].org - 1.0f * patch[im].org) + (2.0f * patch[ip].org + 1.0f * patch[(i+2)%4].org))/3.0f;
//        float3 Rm = ((-2.0f * patch[i].org - 1.0f * patch[ip].org) + (2.0f * patch[im].org + 1.0f * patch[(i+2)%4].org))/3.0f;
//
//        float s1 = 3-2*csf(n-3,2)-csf(np-3,2);
//        float s2 = 2*csf(n-3,2);
//
//        Fp = (csf(np-3,2)*patch[i].position + s1*Ep + s2*Em_ip + patch[i].r[start])/3.0f; 
//        s1 = 3.0f-2.0f*cos(2.0f*M_PI/float(n))-cos(2.0f*M_PI/float(nm));
//        Fm = (csf(nm-3,2)*patch[i].position + s1*Em + s2*Ep_im - patch[i].r[prev])/3.0f;
//
//        if (patch[im].valence < 0) {
//            s1=3-2*csf(n-3,2)-csf(np-3,2);
//            Fp = Fm = (csf(np-3,2)*patch[i].position + s1*Ep + s2*Em_ip + patch[i].r[start])/3.0f;
//        } else if (patch[ip].valence < 0) {
//            s1 = 3.0f-2.0f*cos(2.0f*M_PI/n)-cos(2.0f*M_PI/nm);
//            Fm = Fp = (csf(nm-3,2)*patch[i].position + s1*Em + s2*Ep_im - patch[i].r[prev])/3.0f;
//        }
//
//    } else if (patch[i].valence == -2) {
//        Ep = (2.0f * patch[i].org + patch[ip].org)/3.0f;
//        Em = (2.0f * patch[i].org + patch[im].org)/3.0f;
//        Fp = Fm = (4.0f * patch[i].org + patch[(i+2)%n].org + 2.0f * patch[ip].org + 2.0f * patch[im].org)/9.0f;
//    }
//
//#else // not OSD_PATCH_GREGORY_BOUNDARY
//
//    float3 Ep = patch[i].position + patch[i].e0 * csf(n-3, 2*start) + patch[i].e1*csf(n-3, 2*start + 1);
//    float3 Em = patch[i].position + patch[i].e0 * csf(n-3, 2*prev ) + patch[i].e1*csf(n-3, 2*prev + 1);
//
//    float3 Em_ip = patch[ip].position + patch[ip].e0*csf(np-3, 2*prev_p) + patch[ip].e1*csf(np-3, 2*prev_p + 1);
//    float3 Ep_im = patch[im].position + patch[im].e0*csf(nm-3, 2*start_m) + patch[im].e1*csf(nm-3, 2*start_m + 1);
//
//    float s1 = 3-2*csf(n-3,2)-csf(np-3,2);
//    float s2 = 2*csf(n-3,2);
//
//    float3 Fp = (csf(np-3,2)*patch[i].position + s1*Ep + s2*Em_ip + patch[i].r[start])/3.0f;
//    s1 = 3.0f-2.0f*cos(2.0f*M_PI/float(n))-cos(2.0f*M_PI/float(nm));
//    float3 Fm = (csf(nm-3,2)*patch[i].position + s1*Em +s2*Ep_im - patch[i].r[prev])/3.0f;
//
//#endif
//
//    output.Ep = Ep;
//    output.Em = Em;
//    output.Fp = Fp;
//    output.Fm = Fm;

    //int patchLevel = GetPatchLevel(primitiveID);
    //output.patchCoord = float4(0, 0,
    //                           patchLevel+0.5f,
    //                           primitiveID+PrimitiveIdBase+0.5f);

    //OSD_COMPUTE_PTEX_COORD_HULL_SHADER;

    //return output;
}

//----------------------------------------------------------
// Patches.DomainGregory
//----------------------------------------------------------

[domain("quad")]
void ds_main_patches(
    in HS_CONSTANT_FUNC_OUT input,
    in OutputPatch<FinalQuadDomainVertex, 4> patch,
    in float2 uv : SV_DomainLocation,
    out OutputVertex output )
{
    float u = uv.x,
          v = uv.y;
	float2 UV = uv.xy;
	uint4 c = uint4(0,1,2,3);
	c = c.xyzw;
	//float3 WorldPos = (1.0f - UV.y) * ((1.0f - UV.x) * patch[0].position + UV.x * patch[1].position) + UV.y * ((1.0f - UV.x) * patch[3].position + UV.x * patch[2].position);
	float3 WorldPos  = (1.0f - UV.y) * ((1.0f - UV.x) * patch[c.x].position		+ UV.x * patch[c.y].position)	+ UV.y * ((1.0f - UV.x) * patch[c.w].position	+ UV.x * patch[c.z].position);
	float3 normal	 = (1.0f - UV.y) * ((1.0f - UV.x) * patch[c.x].normal		+ UV.x * patch[c.y].normal)		+ UV.y * ((1.0f - UV.x) * patch[c.w].normal		+ UV.x * patch[c.z].normal);
	float3 Tangent	 = (1.0f - UV.y) * ((1.0f - UV.x) * patch[c.x].tangent		+ UV.x * patch[c.y].tangent)	+ UV.y * ((1.0f - UV.x) * patch[c.w].tangent	+ UV.x * patch[c.z].tangent);
	float3 BiTangent = (1.0f - UV.y) * ((1.0f - UV.x) * patch[c.x].bitangent	+ UV.x * patch[c.y].bitangent)	+ UV.y * ((1.0f - UV.x) * patch[c.w].bitangent	+ UV.x * patch[c.z].bitangent);


//#ifdef OSD_COMPUTE_NORMAL_DERIVATIVES
//    float B[4], D[4], C[4];
//    float3 BUCP[4], DUCP[4], CUCP[4];
//    float3 dUU = float3(0, 0, 0);
//    float3 dVV = float3(0, 0, 0);
//    float3 dUV = float3(0, 0, 0);
//
//    Univar4x4(u, B, D, C);
//
//    for (int i=0; i<4; ++i) {
//        BUCP[i] = float3(0, 0, 0);
//        DUCP[i] = float3(0, 0, 0);
//        CUCP[i] = float3(0, 0, 0);
//
//		[unroll]
//        for (uint j=0; j<4; ++j) 
//		{
//            // reverse face front
//            float3 A = q[i + 4*j];
//
//            BUCP[i] += A * B[j];
//            DUCP[i] += A * D[j];
//            CUCP[i] += A * C[j];
//        }
//    }
//
//    Univar4x4(v, B, D, C);
//
//	float3 WorldPos  = float3(0, 0, 0);
//    float3 Tangent   = float3(0, 0, 0);
//    float3 BiTangent = float3(0, 0, 0);
//	[unroll]
//    for (int i=0; i<4; ++i) {
//        WorldPos  += B[i] * BUCP[i];
//        Tangent   += B[i] * DUCP[i];
//        BiTangent += D[i] * BUCP[i];
//        dUU += B[i] * CUCP[i];
//        dVV += C[i] * BUCP[i];
//        dUV += D[i] * DUCP[i];
//    }
//
//    int level = int(patch[0].ptexInfo.z);
//    BiTangent *= 3 * level;
//    Tangent *= 3 * level;
//    dUU *= 6 * level;
//    dVV *= 6 * level;
//    dUV *= 9 * level;
//
//    float3 n = cross(Tangent, BiTangent);
//    float3 normal = normalize(n);
//
//    float E = dot(Tangent, Tangent);
//    float F = dot(Tangent, BiTangent);
//    float G = dot(BiTangent, BiTangent);
//    float e = dot(normal, dUU);
//    float f = dot(normal, dUV);
//    float g = dot(normal, dVV);
//
//    float3 Nu = (f*F-e*G)/(E*G-F*F) * Tangent + (e*F-f*E)/(E*G-F*F) * BiTangent;
//    float3 Nv = (g*F-f*G)/(E*G-F*F) * Tangent + (f*F-g*E)/(E*G-F*F) * BiTangent;
//
//    Nu = Nu/length(n) - n * (dot(Nu,n)/pow(dot(n,n), 1.5));
//    Nv = Nv/length(n) - n * (dot(Nv,n)/pow(dot(n,n), 1.5));
//
//    BiTangent = mul(ModelViewMatrix, float4(BiTangent, 0)).xyz;
//    Tangent = mul(ModelViewMatrix, float4(Tangent, 0)).xyz;
//
//    normal = normalize(cross(BiTangent, Tangent));
//
//    output.Nu = Nu;
//    output.Nv = Nv;
//
//#else
//	// henry: new dx compiler wont compile without static here
//    float B[4], D[4];
//    float3 BUCP[4], DUCP[4];
//    //float B[4], D[4];
//    //float3 BUCP[4], DUCP[4];	
//    Univar4x4(uv.x, B, D);
//
//    for (int k=0; k<4; ++k) {
//        BUCP[k] =  float3(0, 0, 0);
//        DUCP[k] =  float3(0, 0, 0);
//
//		[unroll]
//        for (uint m=0; m<4; ++m) 
//		{
//            // reverse face front
//            float3 A = q[k + 4*m];
//
//            BUCP[k] += A * B[m];
//            DUCP[k] += A * D[m];
//        }
//    }
//
//    Univar4x4(uv.y, B, D);
//
//	float3 WorldPos  = float3(0, 0, 0);
//    float3 Tangent   = float3(0, 0, 0);
//    float3 BiTangent = float3(0, 0, 0);
//	[unroll]
//    for (uint i=0; i<4; ++i) 
//	{
//        WorldPos  += B[i] * BUCP[i];
//        Tangent   += B[i] * DUCP[i];
//        BiTangent += D[i] * BUCP[i];
//    }
//	
//
//    int level = int(patch[0].ptexInfo.z);
//    BiTangent *= 3 * level;
//    Tangent *= 3 * level;
//	
//    BiTangent = mul(ModelViewMatrix, float4(BiTangent, 0)).xyz;
//    Tangent = mul(ModelViewMatrix, float4(Tangent, 0)).xyz;
//	
//    float3 normal = normalize(cross(BiTangent, Tangent));
//
//	//DeCasteljau_BICUBIC(uv.yx, q, WorldPos, normal);
//	//normal = -normal;
//	//normal = normalize(cross(BiTangent, Tangent));
//
//#endif	
    output.position = mul(ModelViewMatrix, float4(WorldPos, 1.0f));
	//int level = int(patch[0].ptexInfo.z);
    //BiTangent *= 3 * level;
    //Tangent *= 3 * level;
    //BiTangent	    = normalize(mul(ModelViewMatrix, float4(BiTangent, 0)).xyz);
    //Tangent			= normalize(mul(ModelViewMatrix, float4(Tangent, 0)).xyz);
	BiTangent	    = normalize(mul(ModelViewMatrix, float4(BiTangent, 0)).xyz);
    Tangent			= normalize(mul(ModelViewMatrix, float4(Tangent, 0)).xyz);
	normal			= normalize(cross(BiTangent, Tangent));

    output.normal = normal;
    output.tangent = BiTangent;
    output.bitangent = Tangent;
#ifdef WITH_PICKING
	output.posSurface = mul(ModelViewMatrix, float4(WorldPos, 1.0f));
#endif
    output.patchCoord = patch[0].patchCoord;
    output.patchCoord.xy = float2(u, v);

    OSD_COMPUTE_PTEX_COORD_DOMAIN_SHADER;

	// HENRY checkme needed?
	//output.isExtraordinary = (1.0f - UV.y) * ((1.0f - UV.x) * CP[0].isExtraordinary  + UV.x * CP[1].isExtraordinary ) + UV.y * ((1.0f - UV.x) * CP[3].isExtraordinary  + UV.x * CP[2].isExtraordinary);

#ifdef WITH_UV
	output.uvCoord = lerp(	lerp(input.uv[0], input.uv[1], v)	,	
							lerp(input.uv[3], input.uv[2], v)
							,u);
#endif

    OSD_DISPLACEMENT_CALLBACK;

//#if USE_SHADOW	
//	float4 posWorld = mul(mInverseView, float4(output.position.xyz, 1));
//	output.vTexShadow = mul(g_lightViewMatrix, posWorld); // transform to light space
//#endif

    output.positionOut = mul(ProjectionMatrix,
                             float4(output.position.xyz, 1.0f));

#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)
    output.edgeDistance = 0;
#endif

}

#else

//----------------------------------------------------------
// Patches.TessVertexGregory
//----------------------------------------------------------


void vs_main_patches( in InputVertex input,
                      uint vID : SV_VertexID,
                      out GregHullVertex output )
{
    output.hullPosition = mul(ModelViewMatrix, input.position).xyz;
    OSD_PATCH_CULL_COMPUTE_CLIPFLAGS(input.position);

    int ivalence = OsdValenceBuffer[int(vID * (2 * OSD_MAX_VALENCE + 1))];
    output.valence = ivalence;
    uint valence = uint(abs(ivalence));

    float3 f[OSD_MAX_VALENCE]; 
    float3 pos = input.position.xyz;
    float3 opos = float3(0,0,0);

#ifdef OSD_PATCH_GREGORY_BOUNDARY
    output.org = input.position.xyz;
    int boundaryEdgeNeighbors[2];
    uint currNeighbor = 0;
    uint ibefore = 0;
    uint zerothNeighbor = 0;
#endif

    for (uint i=0; i<valence; ++i) {
        uint im=(i+valence-1)%valence; 
        uint ip=(i+1)%valence; 

        uint idx_neighbor = uint(OsdValenceBuffer[int(vID * (2*OSD_MAX_VALENCE+1) + 2*i + 0 + 1)]);

#ifdef OSD_PATCH_GREGORY_BOUNDARY
        bool isBoundaryNeighbor = false;
        int valenceNeighbor = OsdValenceBuffer[int(idx_neighbor * (2*OSD_MAX_VALENCE+1))];

        if (valenceNeighbor < 0) {
            isBoundaryNeighbor = true;
            //boundaryEdgeNeighbors[currNeighbor++] = int(idx_neighbor);
			if (currNeighbor<2) {
                boundaryEdgeNeighbors[currNeighbor] = int(idx_neighbor);
            }
            currNeighbor++;
            if (currNeighbor == 1)    {
                ibefore = i;
                zerothNeighbor = i;
            } else {
                if (i-ibefore == 1) {
                    int tmp = boundaryEdgeNeighbors[0];
                    boundaryEdgeNeighbors[0] = boundaryEdgeNeighbors[1];
                    boundaryEdgeNeighbors[1] = tmp;
                    zerothNeighbor = i;
                } 
            }
        }
#endif

        float3 neighbor =
            float3(OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor)],
                   OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor+1)],
                   OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor+2)]);

        uint idx_diagonal = uint(OsdValenceBuffer[int(vID * (2*OSD_MAX_VALENCE+1) + 2*i + 1 + 1)]);

        float3 diagonal =
            float3(OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal)],
                   OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+1)],
                   OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+2)]);

        uint idx_neighbor_p = uint(OsdValenceBuffer[int(vID * (2*OSD_MAX_VALENCE+1) + 2*ip + 0 + 1)]);

        float3 neighbor_p =
            float3(OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_p)],
                   OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_p+1)],
                   OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_p+2)]);

        uint idx_neighbor_m = uint(OsdValenceBuffer[int(vID * (2*OSD_MAX_VALENCE+1) + 2*im + 0 + 1)]);

        float3 neighbor_m =
            float3(OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_m)],
                   OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_m+1)],
                   OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor_m+2)]);

        uint idx_diagonal_m = uint(OsdValenceBuffer[int(vID * (2*OSD_MAX_VALENCE+1) + 2*im + 1 + 1)]);

        float3 diagonal_m =
            float3(OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal_m)],
                   OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal_m+1)],
                   OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal_m+2)]);

        f[i] = (pos * float(valence) + (neighbor_p + neighbor)*2.0f + diagonal) / (float(valence)+5.0f);
		//f[i] = (pos * float(valence) + neighbor_p + neighbor*3.0 + diagonal) / (float(valence)+5.0);

        opos += f[i];
        output.r[i] = (neighbor_p-neighbor_m)/3.0f + (diagonal - diagonal_m)/6.0f;
    }

    opos /= valence;
    output.position = float4(opos, 1.0f).xyz;

    float3 e;
    output.e0 = float3(0,0,0);
    output.e1 = float3(0,0,0);

    for(uint val=0; val<valence; ++val) {
        uint im = (val + valence -1) % valence;
        e = 0.5f * (f[val] + f[im]);
        output.e0 += csf(valence-3, 2*val) *e;
        output.e1 += csf(valence-3, 2*val + 1)*e;
    }
    output.e0 *= ef[valence - 3];
    output.e1 *= ef[valence - 3];

#ifdef OSD_PATCH_GREGORY_BOUNDARY
    output.zerothNeighbor = zerothNeighbor;
    if (currNeighbor == 1) {
        boundaryEdgeNeighbors[1] = boundaryEdgeNeighbors[0];
    }

    if (ivalence < 0) {
        if (valence > 2) {
            output.position = (
                float3(OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[0])],
                       OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[0]+1)],
                       OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[0]+2)]) +
                float3(OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[1])],
                       OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[1]+1)],
                       OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[1]+2)]) +
                4.0f * pos)/6.0f;        
        } else {
            output.position = pos;                    
        }

        output.e0 = ( 
            float3(OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[0])],
                   OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[0]+1)],
                   OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[0]+2)]) -
            float3(OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[1])],
                   OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[1]+1)],
                   OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[1]+2)]) 
            )/6.0;

        float k = float(float(valence) - 1.0f);    //k is the number of faces
        float c = cos(M_PI/k);
        float s = sin(M_PI/k);
        float gamma = -(4.0f*s)/(3.0f*k+c);
        float alpha_0k = -((1.0f+2.0f*c)*sqrt(1.0f+c))/((3.0f*k+c)*sqrt(1.0f-c));
        float beta_0 = s/(3.0f*k + c); 


        int idx_diagonal = OsdValenceBuffer[int((vID) * (2*OSD_MAX_VALENCE+1) + 2*zerothNeighbor + 1 + 1)];
        idx_diagonal = abs(idx_diagonal);
        float3 diagonal =
                float3(OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal)],
                       OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+1)],
                       OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+2)]);

        output.e1 = gamma * pos + 
            alpha_0k * float3(OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[0])],
                              OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[0]+1)],
                              OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[0]+2)]) +
            alpha_0k * float3(OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[1])],
                              OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[1]+1)],
                              OsdVertexBuffer[int(OSD_NUM_ELEMENTS*boundaryEdgeNeighbors[1]+2)]) +
            beta_0 * diagonal;

        for (uint x=1; x<valence - 1; ++x) {
            uint curri = ((x + zerothNeighbor)%valence);
            float alpha = (4.0f*sin((M_PI * float(x))/k))/(3.0f*k+c);
            float beta = (sin((M_PI * float(x))/k) + sin((M_PI * float(x+1))/k))/(3.0f*k+c);

            int idx_neighbor = OsdValenceBuffer[int((vID) * (2*OSD_MAX_VALENCE+1) + 2*curri + 0 + 1)];
            idx_neighbor = abs(idx_neighbor);

            float3 neighbor =
                float3(OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor)],
                       OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor+1)],
                       OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_neighbor+2)]);

            idx_diagonal = OsdValenceBuffer[int((vID) * (2*OSD_MAX_VALENCE+1) + 2*curri + 1 + 1)];

            diagonal =
                float3(OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal)],
                       OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+1)],
                       OsdVertexBuffer[int(OSD_NUM_ELEMENTS*idx_diagonal+2)]);

            output.e1 += alpha * neighbor + beta * diagonal;                         
        }

        output.e1 /= 3.0f;
    } 
#endif
}

//----------------------------------------------------------
// Patches.HullGregory
//----------------------------------------------------------

Buffer<int> OsdQuadOffsetBuffer : register( t2 );

HS_CONSTANT_FUNC_OUT HSConstFunc(
    InputPatch<GregHullVertex, 4> patch,
    uint primitiveID : SV_PrimitiveID)
{
    HS_CONSTANT_FUNC_OUT output;
    int patchLevel = GetPatchLevel(primitiveID);

    OSD_PATCH_CULL(4);

#ifdef OSD_ENABLE_SCREENSPACE_TESSELLATION
    //output.tessLevelOuter[0] = TessAdaptive(patch[0].hullPosition.xyz, patch[1].hullPosition.xyz);
    //
    //output.tessLevelOuter[1] = TessAdaptive(patch[0].hullPosition.xyz, patch[3].hullPosition.xyz);
    //
    //output.tessLevelOuter[2] = TessAdaptive(patch[2].hullPosition.xyz, patch[3].hullPosition.xyz);
    //
    //output.tessLevelOuter[3] = TessAdaptive(patch[1].hullPosition.xyz, patch[2].hullPosition.xyz);
    //
    //output.tessLevelInner[0] = max(output.tessLevelOuter[1], output.tessLevelOuter[3]);
    //
    //output.tessLevelInner[1] = max(output.tessLevelOuter[0], output.tessLevelOuter[2]);
	    output.tessLevelOuter[0] =
        TessAdaptive(patch[0].hullPosition.xyz, patch[1].hullPosition.xyz);
    output.tessLevelOuter[1] =
        TessAdaptive(patch[0].hullPosition.xyz, patch[3].hullPosition.xyz);
    output.tessLevelOuter[2] =
        TessAdaptive(patch[2].hullPosition.xyz, patch[3].hullPosition.xyz);
    output.tessLevelOuter[3] =
        TessAdaptive(patch[1].hullPosition.xyz, patch[2].hullPosition.xyz);
    output.tessLevelInner[0] =
        max(output.tessLevelOuter[1], output.tessLevelOuter[3]);
    output.tessLevelInner[1] =
        max(output.tessLevelOuter[0], output.tessLevelOuter[2]);
#else
    output.tessLevelInner[0] = GetTessLevel(patchLevel);
    output.tessLevelInner[1] = GetTessLevel(patchLevel);
    output.tessLevelOuter[0] = GetTessLevel(patchLevel);
    output.tessLevelOuter[1] = GetTessLevel(patchLevel);
    output.tessLevelOuter[2] = GetTessLevel(patchLevel);
    output.tessLevelOuter[3] = GetTessLevel(patchLevel);
#endif

	#ifdef WITH_UV		
		#define OSD_FVAR_WIDTH 2
		uint fvarOffset = 0;
		uint primOffset = (primitiveID+PrimitiveIdBase)*4;
		for(int i =0; i < 4; ++i)
		{
			int index = (primOffset+i)*OSD_FVAR_WIDTH+fvarOffset;
			output.uv[i] = float2( g_atlasUV[index],  g_atlasUV[index+1]);
		}
	#endif
    return output;
}

[domain("quad")]
[partitioning(HS_PARTITION)]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(4)]
[patchconstantfunc("HSConstFunc")]
GregDomainVertex hs_main_patches(
    in InputPatch<GregHullVertex, 4> patch,
    uint primitiveID : SV_PrimitiveID,
    in uint ID : SV_OutputControlPointID )
{
    uint i = ID;
    uint ip = (i+1)%4;
    uint im = (i+3)%4;
    uint valence = abs(patch[i].valence);
    uint n = valence;
    int base = GregoryQuadOffsetBase;

    GregDomainVertex output;
    output.position = patch[ID].position;

    uint start = uint(OsdQuadOffsetBuffer[int(4*primitiveID+base + i)]) & 0x00ffu;
    uint prev = uint(OsdQuadOffsetBuffer[int(4*primitiveID+base + i)]) & 0xff00u;
    prev = uint(prev/256);

    uint start_m = uint(OsdQuadOffsetBuffer[int(4*primitiveID+base + im)]) & 0x00ffu;
    uint prev_p = uint(OsdQuadOffsetBuffer[int(4*primitiveID+base + ip)]) & 0xff00u;
    prev_p = uint(prev_p/256);

    uint np = abs(patch[ip].valence);
    uint nm = abs(patch[im].valence);

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

#ifdef OSD_PATCH_GREGORY_BOUNDARY
    float3 Ep = float3(0.0f,0.0f,0.0f);
    float3 Em = float3(0.0f,0.0f,0.0f);
    float3 Fp = float3(0.0f,0.0f,0.0f);
    float3 Fm = float3(0.0f,0.0f,0.0f);

    float3 Em_ip;
    if (patch[ip].valence < -2) {
        uint j = (np + prev_p - patch[ip].zerothNeighbor) % np;
        Em_ip = patch[ip].position + cos((M_PI*j)/float(np-1))*patch[ip].e0 + sin((M_PI*j)/float(np-1))*patch[ip].e1;
    } else {
        Em_ip = patch[ip].position + patch[ip].e0*csf(np-3, 2*prev_p) + patch[ip].e1*csf(np-3, 2*prev_p + 1);
    }

    float3 Ep_im;
    if (patch[im].valence < -2) {
        uint j = (nm + start_m - patch[im].zerothNeighbor) % nm;
        Ep_im = patch[im].position + cos((M_PI*j)/float(nm-1))*patch[im].e0 + sin((M_PI*j)/float(nm-1))*patch[im].e1;
    } else {
        Ep_im = patch[im].position + patch[im].e0*csf(nm-3, 2*start_m) + patch[im].e1*csf(nm-3, 2*start_m + 1);
    }

    if (patch[i].valence < 0) {
        n = (n-1)*2;
    }
    if (patch[im].valence < 0) {
        nm = (nm-1)*2;
    }  
    if (patch[ip].valence < 0) {
        np = (np-1)*2;
    }

    if (patch[i].valence > 2) {
        Ep = patch[i].position + (patch[i].e0*csf(n-3, 2*start) + patch[i].e1*csf(n-3, 2*start + 1));
        Em = patch[i].position + (patch[i].e0*csf(n-3, 2*prev) +  patch[i].e1*csf(n-3, 2*prev + 1)); 

        float s1=3-2*csf(n-3,2)-csf(np-3,2);
        float s2=2*csf(n-3,2);

        Fp = (csf(np-3,2)*patch[i].position + s1*Ep + s2*Em_ip + patch[i].r[start])/3.0f; 
        s1 = 3.0f-2.0f*cos(2.0f*M_PI/float(n))-cos(2.0f*M_PI/float(nm));
        Fm = (csf(nm-3,2)*patch[i].position + s1*Em + s2*Ep_im - patch[i].r[prev])/3.0f;

    } else if (patch[i].valence < -2) {
        uint j = (valence + start - patch[i].zerothNeighbor) % valence;

        Ep = patch[i].position + cos((M_PI*j)/float(valence-1))*patch[i].e0 + sin((M_PI*j)/float(valence-1))*patch[i].e1;
        j = (valence + prev - patch[i].zerothNeighbor) % valence;
        Em = patch[i].position + cos((M_PI*j)/float(valence-1))*patch[i].e0 + sin((M_PI*j)/float(valence-1))*patch[i].e1;

        float3 Rp = ((-2.0f * patch[i].org - 1.0f * patch[im].org) + (2.0f * patch[ip].org + 1.0f * patch[(i+2)%4].org))/3.0f;
        float3 Rm = ((-2.0f * patch[i].org - 1.0f * patch[ip].org) + (2.0f * patch[im].org + 1.0f * patch[(i+2)%4].org))/3.0f;

        float s1 = 3-2*csf(n-3,2)-csf(np-3,2);
        float s2 = 2*csf(n-3,2);

        Fp = (csf(np-3,2)*patch[i].position + s1*Ep + s2*Em_ip + patch[i].r[start])/3.0f; 
        s1 = 3.0f-2.0f*cos(2.0f*M_PI/float(n))-cos(2.0f*M_PI/float(nm));
        Fm = (csf(nm-3,2)*patch[i].position + s1*Em + s2*Ep_im - patch[i].r[prev])/3.0f;

        if (patch[im].valence < 0) {
            s1=3-2*csf(n-3,2)-csf(np-3,2);
            Fp = Fm = (csf(np-3,2)*patch[i].position + s1*Ep + s2*Em_ip + patch[i].r[start])/3.0f;
        } else if (patch[ip].valence < 0) {
            s1 = 3.0f-2.0f*cos(2.0f*M_PI/n)-cos(2.0f*M_PI/nm);
            Fm = Fp = (csf(nm-3,2)*patch[i].position + s1*Em + s2*Ep_im - patch[i].r[prev])/3.0f;
        }

    } else if (patch[i].valence == -2) {
        Ep = (2.0f * patch[i].org + patch[ip].org)/3.0f;
        Em = (2.0f * patch[i].org + patch[im].org)/3.0f;
        Fp = Fm = (4.0f * patch[i].org + patch[(i+2)%n].org + 2.0f * patch[ip].org + 2.0f * patch[im].org)/9.0f;
    }

#else // not OSD_PATCH_GREGORY_BOUNDARY

    float3 Ep = patch[i].position + patch[i].e0 * csf(n-3, 2*start) + patch[i].e1*csf(n-3, 2*start + 1);
    float3 Em = patch[i].position + patch[i].e0 * csf(n-3, 2*prev ) + patch[i].e1*csf(n-3, 2*prev + 1);

    float3 Em_ip = patch[ip].position + patch[ip].e0*csf(np-3, 2*prev_p) + patch[ip].e1*csf(np-3, 2*prev_p + 1);
    float3 Ep_im = patch[im].position + patch[im].e0*csf(nm-3, 2*start_m) + patch[im].e1*csf(nm-3, 2*start_m + 1);

    float s1 = 3-2*csf(n-3,2)-csf(np-3,2);
    float s2 = 2*csf(n-3,2);

    float3 Fp = (csf(np-3,2)*patch[i].position + s1*Ep + s2*Em_ip + patch[i].r[start])/3.0f;
    s1 = 3.0f-2.0f*cos(2.0f*M_PI/float(n))-cos(2.0f*M_PI/float(nm));
    float3 Fm = (csf(nm-3,2)*patch[i].position + s1*Em +s2*Ep_im - patch[i].r[prev])/3.0f;

#endif

    output.Ep = Ep;
    output.Em = Em;
    output.Fp = Fp;
    output.Fm = Fm;

    int patchLevel = GetPatchLevel(primitiveID);
    output.patchCoord = float4(0, 0,
                               patchLevel+0.5f,
                               primitiveID+PrimitiveIdBase+0.5f);

    OSD_COMPUTE_PTEX_COORD_HULL_SHADER;

    return output;
}

void DeCasteljau(float u, float3 p0, float3 p1, float3 p2, float3 p3, out float3 p)
{
    float3 q0 = lerp(p0, p1, u);
    float3 q1 = lerp(p1, p2, u);
    float3 q2 = lerp(p2, p3, u);
    float3 r0 = lerp(q0, q1, u);
    float3 r1 = lerp(q1, q2, u);

    p = lerp(r0, r1, u);
}
void DeCasteljau(float u, float3 p0, float3 p1, float3 p2, out float3 p)
{
    float3 q0 = lerp(p0, p1, u);
    float3 q1 = lerp(p1, p2, u);
    
    p = lerp(q0, q1, u);
}
void DeCasteljau(float u, float3 p0, float3 p1, float3 p2, float3 p3, out float3 p, out float3 dp)
{
    float3 q0 = lerp(p0, p1, u);
    float3 q1 = lerp(p1, p2, u);
    float3 q2 = lerp(p2, p3, u);
    float3 r0 = lerp(q0, q1, u);
    float3 r1 = lerp(q1, q2, u);
    
    dp = r0 - r1;
    p = lerp(r0, r1, u);
}

void DeCasteljau_BICUBIC(float2 uv, float3 p[16], out float3 WorldPos, out float3 nor)
{

	float3 t0, t1, t2, t3;
    float3 p0, p1, p2, p3;

    DeCasteljau(uv.x, p[ 0] , p[ 1] , p[ 2] , p[ 3] , p0, t0);
    DeCasteljau(uv.x, p[ 4] , p[ 5] , p[ 6] , p[ 7] , p1, t1);
    DeCasteljau(uv.x, p[ 8] , p[ 9] , p[10] , p[11] , p2, t2);
    DeCasteljau(uv.x, p[12] , p[13] , p[14] , p[15] , p3, t3);

    float3 du, dv;

    DeCasteljau(uv.y, p0, p1, p2, p3, WorldPos, dv);
    DeCasteljau(uv.y, t0, t1, t2, t3, du);
	nor = normalize(cross(du, dv));

}

//----------------------------------------------------------
// Patches.DomainGregory
//----------------------------------------------------------

[domain("quad")]
void ds_main_patches(
    in HS_CONSTANT_FUNC_OUT input,
    in OutputPatch<GregDomainVertex, 4> patch,
    in float2 uv : SV_DomainLocation,
    out OutputVertex output )
{
    float u = uv.x,
          v = uv.y;


#if 0
    float3 q[16];

    float U = 1-u, V=1-v;
    float d11 = u+v; if(d11==0.0f) d11 = 1.0f;    
    q[ 5] = (u*patch[0].Fp + v*patch[0].Fm)/d11;

	float d12 = U+v; if(d12==0.0f) d12 = 1.0f;
    q[ 6] = (U*patch[1].Fm + v*patch[1].Fp)/d12;

	float d21 = u+V; if(d21==0.0f) d21 = 1.0f;
    q[ 9] = (u*patch[3].Fm + V*patch[3].Fp)/d21;

	float d22 = U+V; if(d22==0.0f) d22 = 1.0f;
    q[10] = (U*patch[2].Fp + V*patch[2].Fm)/d22;

    q[ 0] = patch[0].position;
    q[ 1] = patch[0].Ep;//p[1];
    q[ 2] = patch[1].Em;//p[7];
    q[ 3] = patch[1].position;//p[5];
    q[ 4] = patch[0].Em;//p[2];
    q[ 7] = patch[1].Ep;//p[6];
    q[ 8] = patch[3].Ep;//p[16];
    q[11] = patch[2].Em;//p[12];
    q[12] = patch[3].position;//p[15];
    q[13] = patch[3].Em;//p[17];
    q[14] = patch[2].Ep;//p[11];
    q[15] = patch[2].position;//p[10];

	


    //float3 WorldPos  = float3(0, 0, 0);
    //float3 Tangent   = float3(0, 0, 0);
    //float3 BiTangent = float3(0, 0, 0);

#else
	//float3 p[20];

    //p[0] = patch[0].position;
    //p[1] = patch[0].Ep;
    //p[2] = patch[0].Em;
    //p[3] = patch[0].Fp;
    //p[4] = patch[0].Fm;

    //p[5] = patch[1].position;
    //p[6] = patch[1].Ep;
    //p[7] = patch[1].Em;
    //p[8] = patch[1].Fp;
    //p[9] = patch[1].Fm;

    //p[10] = patch[2].position;
    //p[11] = patch[2].Ep;
    //p[12] = patch[2].Em;
    //p[13] = patch[2].Fp;
    //p[14] = patch[2].Fm;

    //p[15] = patch[3].position;
    //p[16] = patch[3].Ep;
    //p[17] = patch[3].Em;
    //p[18] = patch[3].Fp;
    //p[19] = patch[3].Fm;

    float3 p[20];

    p[0] = patch[0].position;
    p[1] = patch[0].Ep;
    p[2] = patch[0].Em;
    p[3] = patch[0].Fp;
    p[4] = patch[0].Fm;

    p[5] = patch[1].position;
    p[6] = patch[1].Ep;
    p[7] = patch[1].Em;
    p[8] = patch[1].Fp;
    p[9] = patch[1].Fm;

    p[10] = patch[2].position;
    p[11] = patch[2].Ep;
    p[12] = patch[2].Em;
    p[13] = patch[2].Fp;
    p[14] = patch[2].Fm;

    p[15] = patch[3].position;
    p[16] = patch[3].Ep;
    p[17] = patch[3].Em;
    p[18] = patch[3].Fp;
    p[19] = patch[3].Fm;

    float3 q[16];

    [precise] float U = 1.0-u;
	[precise] float V = 1.0-v;
	


    float d11 = u+v; if(u+v==0.0f) d11 = 1.0f;
    float d12 = U+v; if(U+v==0.0f) d12 = 1.0f;
    float d21 = u+V; if(u+V==0.0f) d21 = 1.0f;
    float d22 = U+V; if(U+V==0.0f) d22 = 1.0f;

    //float d11 = u+v; if(abs(u+v)<=0.0001f) d11 = 1.0f;
    //float d12 = U+v; if(abs(U+v)<=0.0001f) d12 = 1.0f;
    //float d21 = u+V; if(abs(u+V)<=0.0001f) d21 = 1.0f;
    //float d22 = U+V; if(abs(U+V)<=0.0001f) d22 = 1.0f;


    q[ 5] = (u*p[3] + v*p[4])/d11;
    q[ 6] = (U*p[9] + v*p[8])/d12;
    q[ 9] = (u*p[19] + V*p[18])/d21;
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
#endif

#line 634

#ifdef OSD_COMPUTE_NORMAL_DERIVATIVES
    float B[4], D[4], C[4];
    float3 BUCP[4], DUCP[4], CUCP[4];
    float3 dUU = float3(0, 0, 0);
    float3 dVV = float3(0, 0, 0);
    float3 dUV = float3(0, 0, 0);

    Univar4x4(u, B, D, C);

    for (int i=0; i<4; ++i) {
        BUCP[i] = float3(0, 0, 0);
        DUCP[i] = float3(0, 0, 0);
        CUCP[i] = float3(0, 0, 0);

		[unroll]
        for (uint j=0; j<4; ++j) 
		{
            // reverse face front
            float3 A = q[i + 4*j];

            BUCP[i] += A * B[j];
            DUCP[i] += A * D[j];
            CUCP[i] += A * C[j];
        }
    }

    Univar4x4(v, B, D, C);

	float3 WorldPos  = float3(0, 0, 0);
    float3 Tangent   = float3(0, 0, 0);
    float3 BiTangent = float3(0, 0, 0);
	[unroll]
    for (int i=0; i<4; ++i) {
        WorldPos  += B[i] * BUCP[i];
        Tangent   += B[i] * DUCP[i];
        BiTangent += D[i] * BUCP[i];
        dUU += B[i] * CUCP[i];
        dVV += C[i] * BUCP[i];
        dUV += D[i] * DUCP[i];
    }

    int level = int(patch[0].ptexInfo.z);
    BiTangent *= 3 * level;
    Tangent *= 3 * level;
    dUU *= 6 * level;
    dVV *= 6 * level;
    dUV *= 9 * level;

    float3 n = cross(Tangent, BiTangent);
    float3 normal = normalize(n);

    float E = dot(Tangent, Tangent);
    float F = dot(Tangent, BiTangent);
    float G = dot(BiTangent, BiTangent);
    float e = dot(normal, dUU);
    float f = dot(normal, dUV);
    float g = dot(normal, dVV);

    float3 Nu = (f*F-e*G)/(E*G-F*F) * Tangent + (e*F-f*E)/(E*G-F*F) * BiTangent;
    float3 Nv = (g*F-f*G)/(E*G-F*F) * Tangent + (f*F-g*E)/(E*G-F*F) * BiTangent;

    Nu = Nu/length(n) - n * (dot(Nu,n)/pow(dot(n,n), 1.5));
    Nv = Nv/length(n) - n * (dot(Nv,n)/pow(dot(n,n), 1.5));

    BiTangent = mul(ModelViewMatrix, float4(BiTangent, 0)).xyz;
    Tangent = mul(ModelViewMatrix, float4(Tangent, 0)).xyz;

    normal = normalize(cross(BiTangent, Tangent));

    output.Nu = Nu;
    output.Nv = Nv;

#else
	// henry: new dx compiler wont compile without static here
    float B[4], D[4];
    float3 BUCP[4], DUCP[4];
    //float B[4], D[4];
    //float3 BUCP[4], DUCP[4];	
    Univar4x4(uv.x, B, D);

    for (int k=0; k<4; ++k) {
        BUCP[k] =  float3(0, 0, 0);
        DUCP[k] =  float3(0, 0, 0);

		[unroll]
        for (uint m=0; m<4; ++m) 
		{
            // reverse face front
            float3 A = q[k + 4*m];

            BUCP[k] += A * B[m];
            DUCP[k] += A * D[m];
        }
    }

    Univar4x4(uv.y, B, D);

	float3 WorldPos  = float3(0, 0, 0);
    float3 Tangent   = float3(0, 0, 0);
    float3 BiTangent = float3(0, 0, 0);
	[unroll]
    for (uint i=0; i<4; ++i) 
	{
        WorldPos  += B[i] * BUCP[i];
        Tangent   += B[i] * DUCP[i];
        BiTangent += D[i] * BUCP[i];
    }
	

    int level = int(patch[0].ptexInfo.z);
    BiTangent *= 3 * level;
    Tangent *= 3 * level;
	
    BiTangent = mul(ModelViewMatrix, float4(BiTangent, 0)).xyz;
    Tangent = mul(ModelViewMatrix, float4(Tangent, 0)).xyz;
	
    float3 normal = normalize(cross(BiTangent, Tangent));

	//DeCasteljau_BICUBIC(uv.yx, q, WorldPos, normal);
	//normal = -normal;
	//normal = normalize(cross(BiTangent, Tangent));

#endif	
    output.position = mul(ModelViewMatrix, float4(WorldPos, 1.0f));
    output.normal = normal;
    output.tangent = BiTangent;
    output.bitangent = Tangent;
#ifdef WITH_PICKING
	output.posSurface = mul(ModelViewMatrix, float4(WorldPos, 1.0f));
#endif
    output.patchCoord = patch[0].patchCoord;
    output.patchCoord.xy = float2(v, u);

    OSD_COMPUTE_PTEX_COORD_DOMAIN_SHADER;


#ifdef WITH_UV
	output.uvCoord = lerp(	lerp(input.uv[0], input.uv[1], v)	,	
							lerp(input.uv[3], input.uv[2], v)
							,u);
#endif

    OSD_DISPLACEMENT_CALLBACK;

//#if USE_SHADOW	
//	float4 posWorld = mul(mInverseView, float4(output.position.xyz, 1));
//	output.vTexShadow = mul(g_lightViewMatrix, posWorld); // transform to light space
//#endif

    output.positionOut = mul(ProjectionMatrix,
                             float4(output.position.xyz, 1.0f));

#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)
    output.edgeDistance = 0;
#endif

}

#endif

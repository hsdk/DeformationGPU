#ifndef OSD_RENDER_PIXEL_HLSL
#include "OSDPatchCommon.hlsl"
#include "Material.h.hlsl"
#include "ShadingLib.h.hlsl"

#define OSD_RENDER_PIXEL_HLSL
#if defined(COLOR_PTEX_NEAREST) ||     \
    defined(COLOR_PTEX_HW_BILINEAR) || \
    defined(COLOR_PTEX_BILINEAR) ||    \
    defined(COLOR_PTEX_BIQUADRATIC)
Texture2DArray textureImage_Data : register(t4);
Buffer<uint> textureImage_Packing : register(t5);
#endif

#ifdef USE_PTEX_OCCLUSION
Texture2DArray textureOcclusion_Data : register(t8);
Buffer<uint> textureOcclusion_Packing : register(t9);
#endif

#ifdef USE_PTEX_SPECULAR
Texture2DArray textureSpecular_Data : register(t10);
Buffer<uint> textureSpecular_Packing : register(t11);
#endif

struct PS_OUTPUT {
	float4 color		: SV_Target0;

#ifdef WITH_SSAO
	float4 normalDepth	: SV_Target1;	
#endif

}; 

float4
edgeColor(float4 Cfill, float4 edgeDistance)
{
#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)
#ifdef PRIM_TRI
    float d =
        min(edgeDistance[0], min(edgeDistance[1], edgeDistance[2]));
#endif
#ifdef PRIM_QUAD
    float d =
        min(min(edgeDistance[0], edgeDistance[1]),
            min(edgeDistance[2], edgeDistance[3]));
#endif
    //float4 Cedge = float4(1.0, 1.0, 0.0, 1.0);
	float4 Cedge = float4(0.0, 0.0, 0.0, 1.0);
    //float p = exp2(-2 * d * d);
	float p = exp2(-d * d);

#if defined(GEOMETRY_OUT_WIRE)
    if (p < 0.25) discard;		
#endif

    Cfill.rgb = lerp(Cfill.rgb, Cedge.rgb, p);


#endif
    return Cfill;
}

float4 edgeColor2(float4 Cfill, float4 edgeDistance, float2 pc)
{
#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)
#ifdef PRIM_TRI
    float d =
        min(edgeDistance[0], min(edgeDistance[1], edgeDistance[2]));
#endif
#ifdef PRIM_QUAD
    float d =
        min(min(edgeDistance[0], edgeDistance[1]),
            min(edgeDistance[2], edgeDistance[3]));
#endif
    //float4 Cedge = float4(1.0, 1.0, 0.0, 1.0);
	float4 Cedge = float4(0.0, 0.0, 0.0, 1.0);
    //float p = exp2(-2 * d * d);
	float p = exp2(-0.5 * d * d);

#if defined(GEOMETRY_OUT_WIRE)
    if (p < 0.25) discard;		
#endif

#if defined(SHOW_ALLOCS) || defined(DEBUG_SHOW_INTERSECTION)
#else
    Cfill.rgb = lerp(Cfill.rgb, Cedge.rgb, p);
#endif


	// color coarse face edges
	float d2 = min(
					min(pc.x, 1-pc.x),
					min(pc.y, 1-pc.y));

	Cfill.rgb =  lerp(Cfill.rgb, float3(1,0,0), 1-smoothstep(0.002,0.02,d2));

#endif


    return Cfill;
}

PS_OUTPUT
ps_main(in OutputVertex input)
{	
	PS_OUTPUT output;

	//output.color = float4(g_MatDiffuse,1);
    // ------------ normal ---------------
#if defined(NORMAL_HW_SCREENSPACE) || defined(NORMAL_SCREENSPACE)
    float3 normal = perturbNormalFromDisplacement(input.position.xyz,
                                                  input.normal,
                                                  input.patchCoord);
#elif defined(NORMAL_BIQUADRATIC) || defined(NORMAL_BIQUADRATIC_WG)
    float4 du, dv;
    float4 disp = PTexMipmapLookupQuadratic(du, dv, input.patchCoord,
                                            mipmapBias,
                                            textureDisplace_Data,
                                            textureDisplace_Packing);

   // float4 disp = PTexLookupQuadratic(du, dv, input.patchCoord,
   //                                         mipmapBias,
   //                                         textureDisplace_Data,
   //                                         textureDisplace_Packing);

	float displacementScale = g_displacementScale;
	disp *= displacementScale;
	
    du *= displacementScale;
    dv *= displacementScale;
	
    float3 n = normalize(cross(input.tangent, input.bitangent));
    float3 tangent = input.tangent + n * du.x;
    float3 bitangent = input.bitangent + n * dv.x;

#if defined(NORMAL_BIQUADRATIC_WG)
    tangent += input.Nu * disp.x;
    bitangent += input.Nv * disp.x;
#endif

    float3 normal = normalize(cross(tangent, bitangent));
#else
    float3 normal = input.normal;
#endif

    // ------------ color ---------------
#if defined(COLOR_UV_TEX)
	//float4 texColor = float4(1,0,1,1);		
	float4 uvtexColor = float4(input.patchCoord.xy,0,1);//
	//texColor = float4(g_texDiffuse.Sample(g_samplerTex, input.patchCoord).xyz,1);
	
	uvtexColor = float4(pow(g_texDiffuse.Sample(g_samplerTex, float2(input.uvCoord.x,1-input.uvCoord.y)).xyz, 1.6),1);
#endif

#if defined(COLOR_PTEX_NEAREST)
    float4 texColor = PTexLookupNearest(input.patchCoord,
                                        textureImage_Data,
                                        textureImage_Packing);
	// debug
	//outColor = texColor;
	//outColor = float4(input.patchCoord.xy,1,1);

	//return;
#elif defined(COLOR_PTEX_HW_BILINEAR)
    float4 texColor = PTexLookupFast(input.patchCoord,
                                   textureImage_Data,
                                   textureImage_Packing);
#elif defined(COLOR_PTEX_BILINEAR)
    float4 texColor = PTexMipmapLookup(input.patchCoord, mipmapBias, textureImage_Data, textureImage_Packing);

	//float4 texColor = PTexLookupNearest(input.patchCoord, textureImage_Data, textureImage_Packing);
	//float4 texColor = PTexLookupNearestInclOverlap(input.patchCoord, textureImage_Data, textureImage_Packing);
	//float4 du2;
	//float4 dv2;
	//int level2 =0;
	//float4 texColor = PTexLookupQuadratic(du2, dv2,	input.patchCoord,level2,textureImage_Data, textureImage_Packing);
	// displacement 
	//float4 texColor = PTexLookupQuadratic(du2, dv2,	input.patchCoord,level2,textureDisplace_Data, textureDisplace_Packing);

	//texColor = float4(texColor.x,texColor.x,texColor.x,1);
	//texColor = float4(input.patchCoord.xy,1,1);
	//float2 test = exp(-5*abs(1.0-2*input.patchCoord.xy));
	//texColor = float4(test,1,1);
	//outColor = texColor;
	//return;

	//if(input.patchCoord.x <0 || input.patchCoord.x >1 || input.patchCoord.y <0 || input.patchCoord.y > 1)
	//	texColor = float4(1,0 , 1, 1);
	//texColor = abs(texColor)*10.0;
	//output.color = texColor;
	//output.color = float4(normal*0.5+0.5,1);
	
	//texColor = float4(input.normal, 1);
	//output.color = texColor;
	//return output;

#elif defined(COLOR_PTEX_BIQUADRATIC)
    float4 texColor = PTexMipmapLookupQuadratic(input.patchCoord, mipmapBias, textureImage_Data, textureImage_Packing);
#elif defined(COLOR_PATCHTYPE)
#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)
	float4 overrideColor = float4(1,1,0,1);
    float4 texColor = edgeColor(lighting(overrideColor, input.position.xyz, normal, 0),
                                input.edgeDistance);
    outColor = texColor;
    return;
#endif
#elif defined(COLOR_PATCHCOORD)
	#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)
    float4 texColor = edgeColor(lighting(input.patchCoord, input.position.xyz, normal, 0),
                                input.edgeDistance);
    outColor = texColor;
    return;
	#endif
#elif defined(COLOR_NORMAL)
	#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)
    float4 texColor = edgeColor(float4(normal.x, normal.y, normal.z, 1),
                                input.edgeDistance);
    outColor = texColor;
    return;
	#endif
#else // COLOR_NONE
    float4 texColor = float4(g_MatDiffuse, 1);	
#endif

#if defined(COLOR_UV_TEX)
	float alphaTex = max(0,texColor.a);
	if(alphaTex > 0)
		texColor =	float4(lerp( uvtexColor.rgb, texColor.rgb,  max(0,texColor.a)),alphaTex);
	else
		texColor = uvtexColor;
#endif

#define _DEBUG_PATCH_COORD
#ifdef DEBUG_PATCH_COORD
	//texColor = float4(input.patchCoord.xy, 0, 1);
	float2 pc = input.patchCoord.xy*2-1;
	float2 pca = abs(pc);

	// x=0 rot		// edge 0
	// x=1 gruen
	// y=0 blau
	// y=1 gelb
	texColor = float4(input.patchCoord.xy,1,1);
	if (pca.x > 0.8) {
		texColor = float4(1,0,0,1);			
		if (pc.x > 0)
			texColor = float4(0,1,0,1);		
	} else if (pca.y > 0.8) {
		texColor = float4(0,0,1,1);
		if (pc.y > 0)
			texColor = float4(1,1,0,1);
	}

	
#endif


    // ------------ occlusion ---------------

#ifdef USE_PTEX_OCCLUSION
    float occ = PTexLookup(input.patchCoord,
                           textureOcclusion_Data,
                           textureOcclusion_Packing).x;
#else
    float occ = 0.0;
#endif

    // ------------ specular ---------------

#ifdef USE_PTEX_SPECULAR
    float specular = PTexLookup(input.patchCoord,
                                textureSpecular_Data,
                                textureSpecular_Packing).x;
#else
    float specular = 1.0;
#endif
//#ifdef USE_SHADOW	
//	outColor = texColor * float4(EvalCascadeShadow(input.position.z, input.vTexShadow, normal, normalize(g_lightDirES.xyz)), 1);
//	return;
//#else
//    // ------------ lighting ---------------
//    float4 Cf = lighting(texColor, input.position.xyz, normal, occ);
//#endif

	// not allocated color
	if (texColor.w < 0)
		texColor.xyz = g_MatDiffuse;//float3(0.5, 0.5, 0.5);
	else
		texColor =	float4(lerp( g_MatDiffuse, texColor.rgb,  max(0,texColor.a)),max(0,texColor.a));


#ifdef SHOW_ALLOCS
	
	//#if (defined(NORMAL_BIQUADRATIC) || defined(NORMAL_BIQUADRATIC_WG) )
	//if(disp.w < 0 && texColor.w < 0)
	//{
	//	//texColor = float4(0.08, 0.7, 0.7, 1);
	//	texColor *= float4(0.18, 0.9, 0.18, 1);
	//}
	//#endif	

	if (texColor.w < 0 && disp.w < 0)
        // 0.18, 0.9, 0.18
        texColor.xyz = 0.5*float3(0.18, 0.9, 0.18);
    else
	{
        //texColor.xyz = texColor.xyz* 0.5*float3( 0.9, 0.18, 0.18); 
		//texColor.xyz = texColor.xyz* 0.5*float3( 0.9, 0.18, 0.18); 
	}
		
	//else 
	//	texColor *= float4(1.0, 0.8, 1, 1);
	//#ifdef _WITH_PICKING	
	//	int ptexFaceID = input.patchCoord.w;
	//	if(g_visibilitySRV[ptexFaceID] == 1)
	//		texColor *= 0.75;//float4(,1,0,1);	
	//	//if (ptexFaceID == 233)
	//	//	texColor = float4(1,0.5,0.5,1.0);
	//	//if (ptexFaceID == 242)
	//	//	texColor = float4(1,0.5,1,1.0);
	//	//if (ptexFaceID == 809)
	//	//	texColor = float4(1,0.5,0.5,1.0);
	//	//if (ptexFaceID == 234)
	//	//	texColor = float4(1,0.5,0.5,1.0);
	//	//if (ptexFaceID == 473)
	//	//	texColor = float4(1,0.5,0.5,1.0);
	//#endif
#endif


	// test SNOW
	
	if (g_Special > 0.0) 
	{
		float3 worldPosition = mul(mInverseView, float4(  input.position.xyz,1)).xyz;
		
		#ifdef SHOW_ALLOCS
			texColor *= shadingWrapper2(normal, input.position.xyzw, float4(worldPosition.xyz,1));
		#else
			//texColor = shadingWrapper2(normal, input.position.xyzw, float4(worldPosition.xyz,1));
			texColor =	float4(lerp( shadingWrapper2(normal, input.position, float4(worldPosition,1)).rgb, texColor.rgb, max(0,texColor.a)),1);
		#endif

		// shade displacement depth
#if defined(NORMAL_BIQUADRATIC) || defined(NORMAL_BIQUADRATIC_WG)
		if (disp.x < 0)			texColor -= lerp(0.0, 0.5, abs(disp.r));
#endif
	}

    // ------------ wireframe ---------------
    //outColor = edgeColor(Cf, input.edgeDistance);
	// no lighting
	//outColor = edgeColor(texColor, input.edgeDistance);
#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)	
	output.color = edgeColor2(texColor, input.edgeDistance, input.patchCoord.xy);
	//output.color = edgeColor2(float4(1,1,1,1), input.edgeDistance, input.patchCoord.xy);
#else
	output.color = texColor;
#endif


	const float3 vLightDir1 = float3( -1.0f, 1.0f, -1.0f ); 
    const float3 vLightDir2 = float3( 1.0f, 1.0f, -1.0f ); 
    const float3 vLightDir3 = float3( 0.0f, -1.0f, 0.0f );
    const float3 vLightDir4 = float3( 1.0f, 1.0f, 1.0f );  
	float3 lightDir = normalize(g_lightDirES.xyz);
	float nDotL = dot(normal, lightDir);
	float3 shadow = 1;

	#ifdef USE_SHADOW	
		shadow = ShadowVisibility(input.position.xyz, input.position.z, nDotL, normal);
	#endif
	   // Some ambient-like lighting.
    float3 fLighting = saturate( dot( vLightDir1 , normal ) )*0.05f +
                      saturate( dot( vLightDir2 , normal ) )*0.05f +
                      saturate( dot( vLightDir3 , normal ) )*0.05f +
                      saturate( dot( vLightDir4 , normal ) )*0.05f ;
    
    float3 vShadowLighting = fLighting * 0.5f;  	
	fLighting += saturate( nDotL ) * shadow ; //*  (1.f/3.14159f);
    fLighting = lerp( vShadowLighting, fLighting, max(shadow.x,max(shadow.y,shadow.z)) );
	
	float3 V = normalize(float3(0,0,0) - input.position.xyz);
	float3 H = normalize(V + lightDir);

	output.color.xyz *=  fLighting
				   + shadow * g_MatSpecular * pow(max(dot(normal, H), 0), g_MatShininess);
	if (g_Special > 1.0) 
	{
		output.color.xyz = texColor.rgb * (0.3+max(shadow.x,max(shadow.y,shadow.z))*0.7);//
#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)
		output.color.xyz = edgeColor2(output.color, input.edgeDistance, input.patchCoord.xy).xyz;
#endif
	}

	#ifdef WITH_SSAO	
	output.normalDepth = float4(normal.xyz, input.position.z/g_fCamDepthRange);
	#endif


	#define _DEBUG_INTERSECTION
	#ifdef DEBUG_SHOW_INTERSECTION
	if(g_visibilitySRV[input.patchCoord.w]!=0)
	{
		output.color *= float4(0.5,0.5,0,1);			
		//return output; 
	}	
	#endif

	//// henry debug displacement transfer remove next lines befor return output
	//#ifdef DEBUG_SHOW_INTERSECTION
	//	output.color = float4(input.uvCoord.x,input.uvCoord.y,0,1);
	//	return output; 
	//#endif
	//#if defined(COLOR_PTEX_BILINEAR)
	//	output.color = PTexMipmapLookup(input.patchCoord, mipmapBias, textureImage_Data, textureImage_Packing);
	//#endif
#define _NORMAL_DEBUG
#ifdef NORMAL_DEBUG
	output.color.rgb = mul((float3x3)mInverseView,normal.xyz)*0.5+0.5;
#endif

#define _PATCHCOORD_DEBUG
#ifdef PATCHCOORD_DEBUG
	output.color.rgb = float3(input.patchCoord.xy,0);
#endif
	return output;
}


//float4 PS_VoxelizeSolid(centroid PS_INPUT In) : SV_TARGET
#ifdef VOXELIZE

cbuffer cbVoxelGrid : register(b9) {
	float4x4 g_matModelToProj;
	float4x4 g_matModelToVoxel;
	uint2 g_stride;
	uint2 g_vgpadding;
	uint3 g_gridSize;
};

RWByteAddressBuffer		g_rwbufVoxels :			register(u1);

float4 PS_VoxelizeSolid(centroid in OutputVertex input) : SV_TARGET 
{
	float3 gridPos = input.positionOut.xyz / input.positionOut.w;
	//gridPos.xy = (gridPos+g_gridSize/2)*2;
	int3 gridSize = g_gridSize;
	

	gridPos.z *= gridSize.z;

	int3 p = int3(gridPos.x, gridPos.y, gridPos.z + 0.5);

	// forward
	// flip all voxels below
	if (p.z < int(gridSize.z)) 
	{
		uint address = p.x * g_stride.x + p.y * g_stride.y + (p.z >> 5) * 4;

		// enable for whole volume voxelization
#if 0
		g_rwbufVoxels.Store(address, 0xffffffffu);
		discard;
		return float4(1,1,0,1);	
#endif

#ifdef VOXELIZE_BACKWARD											
		g_rwbufVoxels.InterlockedXor(address, ~(0xffffffffu << (p.z & 31)));
		for (p.z = (p.z & (~31)); p.z > 0; p.z -= 32) {
			address -= 4;
			g_rwbufVoxels.InterlockedXor(address, 0xffffffffu);
			
		}
#else		
		g_rwbufVoxels.InterlockedXor(address, 0xffffffffu << (p.z & 31));
		for(p.z = (p.z | 31) + 1; p.z < int(gridSize.z); p.z += 32) {
			address += 4;
			g_rwbufVoxels.InterlockedXor(address, 0xffffffffu);						
		}
		
		
#endif
	}
	// kill fragment
	discard;	
	return float4(1,1,0,1);	
}
#endif


float2 ps_genShadow(in OutputVertex input) : SV_Target 
{
	float2 rt;
	rt.x = input.positionOut.z;
	rt.y = rt.x * rt.x;
	return rt;
}

#endif
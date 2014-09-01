#include "OSDPatchCommon.hlsl"

#include "PTexLookup.hlsl"

#ifdef OSD_PATCH_TRANSITION
#include "OSDPatchTransition.hlsl"
#endif

#ifdef USE_SHADOW
#include "CascadeShadowRender.h.hlsl"
#endif





struct SPicking
{
	float4x4    TBNP;  
	float		depth;
	//
	int			objectID;
};

struct OutputPointVertex 
{
    float4 positionOut : SV_Position;
};

cbuffer Config : register( b3 ) 
{
    float g_displacementScale;
    float mipmapBias;
};

cbuffer cbEyeZDist : register( b4 )
{
	float3 g_fCamEyePos;					// CHECKME not found, is it used somewhere @maddi?
	float  g_fCamDepthRange;
}

cbuffer cbPicking : register( b8 )
{
	float2		g_cursorPos;
	float2		g_cursorDirection;
	int			g_objectID;
}


// ---------------------------------------------------------------------------

#if    defined(DISPLACEMENT_HW_BILINEAR)        \
    || defined(DISPLACEMENT_BILINEAR)           \
    || defined(DISPLACEMENT_BIQUADRATIC)        \
    || defined(NORMAL_HW_SCREENSPACE)           \
    || defined(NORMAL_SCREENSPACE)              \
    || defined(NORMAL_BIQUADRATIC)              \
    || defined(NORMAL_BIQUADRATIC_WG)

Texture2DArray<float> textureDisplace_Data : register(t6);
Buffer<uint> textureDisplace_Packing : register(t7);

#endif

#if defined(DISPLACEMENT_HW_BILINEAR) || defined(DISPLACEMENT_BILINEAR) || defined(DISPLACEMENT_BIQUADRATIC) 
float4 displacement(float4 position, float3 normal, float4 patchCoord)
{
	float disp = 0;
#if defined(DISPLACEMENT_HW_BILINEAR)
    disp = PTexLookupFast(patchCoord, textureDisplace_Data, textureDisplace_Packing).x;
#elif defined(DISPLACEMENT_BILINEAR)
    disp = PTexMipmapLookup(patchCoord, mipmapBias, textureDisplace_Data, textureDisplace_Packing).x;
#elif defined(DISPLACEMENT_BIQUADRATIC)
    disp = PTexMipmapLookupQuadratic(patchCoord, mipmapBias, textureDisplace_Data, textureDisplace_Packing).x;
	//float4 du, dv;
	//disp = PTexLookupQuadratic(du,dv,patchCoord, mipmapBias, textureDisplace_Data, textureDisplace_Packing).x;
	
#endif
	//return position + float4(disp*normal, 0) * g_displacementScale;
	//normal = float3(1,0,0);
	return position + float4(disp*normal, 0) * g_displacementScale;
}
#endif

#if defined(DISPLACEMENT_HW_BILINEAR) || defined(DISPLACEMENT_BILINEAR) || defined(DISPLACEMENT_BIQUADRATIC) 
#undef OSD_DISPLACEMENT_CALLBACK
#define OSD_DISPLACEMENT_CALLBACK output.position = displacement(output.position, output.normal, output.patchCoord);	   
#endif

// picking
#ifdef WITH_PICKING
#define PICKING_APPEND
#ifdef PICKING_APPEND
AppendStructuredBuffer<SPicking> g_pickingResultUAV		: register(u6);
#else
RWStructuredBuffer<SPicking> g_pickingResultUAV		: register(u6);
#endif

cbuffer BrushOrientationCB : register( b9 ) // picking matrix
{	
	float4x4		g_mBrushToWorld;		// pickingMatrix
	float4x4		g_mWorldToBrush;		// pickingMatrixInverse
}

cbuffer cbBrushInfo : register(b10)
{
	float3 g_brushExtends;	
	float  g_brushOffsetZ;				// offset with pressure
	float4 g_brushColor;	
	uint   g_brushTypeColor;
	uint   g_brushTypeSculpt;
	float2 g_brushUVDisplacement;		
}
#endif


#if defined(COLOR_UV_TEX) || defined(DISPLACEMENT_UV_TEX) || defined(NORMAL_UV_TEX) 
SamplerState g_samplerTex : register(s0);
#endif

#ifdef DISPLACEMENT_UV_TEX
Texture2D g_texDisplacement	: register(t9);	// CHECKME conflict with visibility
#endif

#ifdef NORMAL_UV_TEX
Texture2D g_texNormal	: register(t10);
#endif

#ifdef COLOR_UV_TEX
Texture2D g_texDiffuse	: register(t11);
#endif


// debug visibility
Buffer<uint> g_visibilitySRV : register(t9);

// ---------------------------------------------------------------------------
//  Vertex Shader
// ---------------------------------------------------------------------------

void vs_main( in InputVertex input,
              out OutputVertex output )
{
    output.positionOut = mul(ModelViewProjectionMatrix, input.position);
    output.position = mul(ModelViewMatrix, input.position);
    //output.normal = mul(ModelViewMatrix,float4(input.normal, 0)).xyz;
//#ifdef WITH_UV
//	output.uvCoord = input.uv;
//#endif
}

// ---------------------------------------------------------------------------
//  Geometry Shader
// ---------------------------------------------------------------------------

OutputVertex
outputVertex(OutputVertex input, float3 normal)
{
    OutputVertex v = input;
    v.normal = normal;
    return v;
}

#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)
#ifdef PRIM_TRI
    #define EDGE_VERTS 3
#endif
#ifdef PRIM_QUAD
    #define EDGE_VERTS 4
#endif

#ifndef EDGE_VERTS
#define EDGE_VERTS 3
#endif

static float2 VIEWPORT_SCALE = 1280; // XXXdyu

float edgeDistance(float2 p, float2 p0, float2 p1)
{
    return VIEWPORT_SCALE *
        abs((p.x - p0.x) * (p1.y - p0.y) -
            (p.y - p0.y) * (p1.x - p0.x)) / length(p1.xy - p0.xy);
}

OutputVertex
outputWireVertex(OutputVertex input, float3 normal,
                 int index, float2 edgeVerts[EDGE_VERTS])
{
    OutputVertex v = input;
    v.normal = normal;

    v.edgeDistance[0] =
        edgeDistance(edgeVerts[index], edgeVerts[0], edgeVerts[1]);
    v.edgeDistance[1] =
        edgeDistance(edgeVerts[index], edgeVerts[1], edgeVerts[2]);
#ifdef PRIM_TRI
    v.edgeDistance[2] =
        edgeDistance(edgeVerts[index], edgeVerts[2], edgeVerts[0]);
#endif
#ifdef PRIM_QUAD
    v.edgeDistance[2] =
        edgeDistance(edgeVerts[index], edgeVerts[2], edgeVerts[3]);
    v.edgeDistance[3] =
        edgeDistance(edgeVerts[index], edgeVerts[3], edgeVerts[0]);
#endif

    return v;
}
#endif

#ifdef PRIM_QUAD
[maxvertexcount(6)]
void gs_main( lineadj OutputVertex input[4],
              inout TriangleStream<OutputVertex> triStream )
{
    float3 A = (input[0].position - input[1].position).xyz;
    float3 B = (input[3].position - input[1].position).xyz;
    float3 C = (input[2].position - input[1].position).xyz;

    float3 n0 = normalize(cross(B, A));

    triStream.Append(outputVertex(input[0], n0));
    triStream.Append(outputVertex(input[1], n0));
    triStream.Append(outputVertex(input[3], n0));
    triStream.RestartStrip();
    triStream.Append(outputVertex(input[3], n0));
    triStream.Append(outputVertex(input[1], n0));
    triStream.Append(outputVertex(input[2], n0));
    triStream.RestartStrip();
}
#else // PRIM_TRI
[maxvertexcount(3)]
void gs_main( triangle OutputVertex input[3],
              inout TriangleStream<OutputVertex> triStream )
{
    static float4 position[3];
    static float4 patchCoord[3];
    static float3 normal[3];

    // patch coords are computed in tessellation shader
    patchCoord[0] = input[0].patchCoord;
    patchCoord[1] = input[1].patchCoord;
    patchCoord[2] = input[2].patchCoord;

    position[0] = input[0].position;
    position[1] = input[1].position;
    position[2] = input[2].position;

#ifdef NORMAL_FACET
    // emit flat normals for displaced surface
    float3 A = (position[0] - position[1]).xyz;
    float3 B = (position[2] - position[1]).xyz;
    normal[0]= normalize(cross(B, A));
    normal[1] = normal[0];
    normal[2] = normal[0];
#else
    normal[0] = input[0].normal;
    normal[1] = input[1].normal;
    normal[2] = input[2].normal;
#endif

#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)
    float2 edgeVerts[3];
    edgeVerts[0] = input[0].positionOut.xy / input[0].positionOut.w;
    edgeVerts[1] = input[1].positionOut.xy / input[1].positionOut.w;
    edgeVerts[2] = input[2].positionOut.xy / input[2].positionOut.w;

    triStream.Append(outputWireVertex(input[0], normal[0], 0, edgeVerts));
    triStream.Append(outputWireVertex(input[1], normal[1], 1, edgeVerts));
    triStream.Append(outputWireVertex(input[2], normal[2], 2, edgeVerts));
#else
    triStream.Append(outputVertex(input[0], normal[0]));
    triStream.Append(outputVertex(input[1], normal[1]));
    triStream.Append(outputVertex(input[2], normal[2]));
#endif
}

#endif

#ifdef GEN_SHADOW_FAST
#include "LightProperties.h.hlsl"

struct OutputVertexGS {
    float4 positionOut : SV_Position;
	uint   RTIndex : SV_RenderTargetArrayIndex;
};

#ifndef NUM_CASCADES
#define NUM_CASCADES 3
#endif


#ifdef PRIM_QUAD
[maxvertexcount(NUM_CASCADES*6)]
void gs_mainShadow( lineadj OutputVertex input[4],
              inout TriangleStream<OutputVertexGS> triStream )
{

	for(int i = 0; i < NUM_CASCADES; ++i)
	{		
		OutputVertexGS output;
		output.RTIndex = i;

		output.positionOut = mul(g_mCascadeProj[i], input[0].position);		triStream.Append(output);
		output.positionOut = mul(g_mCascadeProj[i], input[1].position);		triStream.Append(output);
		output.positionOut = mul(g_mCascadeProj[i], input[3].position);		triStream.Append(output);
		
		triStream.RestartStrip();
		
		output.positionOut = mul(g_mCascadeProj[i], input[3].position);		triStream.Append(output);
		output.positionOut = mul(g_mCascadeProj[i], input[1].position);		triStream.Append(output);
		output.positionOut = mul(g_mCascadeProj[i], input[2].position);		triStream.Append(output);

		triStream.RestartStrip();
	}
}
#else // PRIM_TRI

[maxvertexcount(NUM_CASCADES*3)]
void gs_mainShadow( triangle OutputVertex input[3],
              inout TriangleStream<OutputVertexGS> triStream )
{
	for(int i = 0; i < NUM_CASCADES; ++i)
	{		
		OutputVertexGS output;
		output.RTIndex = i;

		output.positionOut = mul(g_mCascadeProj[i], input[0].position);		triStream.Append(output);
		output.positionOut = mul(g_mCascadeProj[i], input[1].position);		triStream.Append(output);
		output.positionOut = mul(g_mCascadeProj[i], input[2].position);		triStream.Append(output);
		triStream.RestartStrip();
	}
}

#endif

float2 ps_genShadowFast(in OutputVertexGS input) : SV_Target 
{
	float2 rt;
	rt.x = input.positionOut.z;
	rt.y = rt.x * rt.x;
	return rt;
}
#endif



// ---------------------------------------------------------------------------
//  Lighting
// ---------------------------------------------------------------------------

#define NUM_LIGHTS 2

struct LightSource {
    float4 position;
    float4 ambient;
    float4 diffuse;
    float4 specular;
};

cbuffer Lighting : register( b2 ) {
    LightSource lightSource[NUM_LIGHTS];
};

float4
lighting(float4 texColor, float3 Peye, float3 Neye, float occ)
{
    float4 color = float4(0.0, 0.0, 0.0, 0.0);
    float3 n = Neye;

    for (int i = 0; i < NUM_LIGHTS; ++i) {

        float4 Plight = lightSource[i].position;
        float3 l = (Plight.w == 0.0)
                    ? normalize(Plight.xyz) : normalize(Plight.xyz - Peye);

        float3 h = normalize(l + float3(0,0,1));    // directional viewer

        float d = max(0.0, dot(n, l));
        float s = pow(max(0.0, dot(n, h)), 64.0f);

        color += (1.0 - occ) * ((lightSource[i].ambient +
                                 d * lightSource[i].diffuse) * texColor +
                                s * lightSource[i].specular);
    }

    color.a = 1.0;
    return color;
}

// ---------------------------------------------------------------------------
//  Pixel Shader
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//  Pixel Shader
// ---------------------------------------------------------------------------

#ifdef PIXEL_SHADING
#include "OSDRenderPixel.hlsl"


#endif

#ifdef WITH_GREGORY
#include "OSDPatchGregory.hlsl"
#else
#include "OSDPatchBSpline.hlsl"
#endif

#ifdef OSD_PATCH_TRANSITION
#include "OSDPatchTransition.hlsl"
#endif

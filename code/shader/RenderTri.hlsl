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

#include "LightProperties.h.hlsl"
#ifdef WITH_SHADOW
#include "CascadeShadowRender.h.hlsl"
#endif

#ifdef PIXEL_SHADING

#endif

sampler		g_linearSampler  : register(s0);
Texture2D	g_txDiffuse		 : register(t0);
Texture2D	g_txNormal		 : register(t1);
Texture2D	g_txDisplacement : register(t2);
Texture2D	g_txAlpha		 : register(t3);
Texture2D	g_txSpecular	 : register(t4);
TextureCube g_envMap		 : register(t5);

RWByteAddressBuffer		g_rwbufVoxels :			register(u1);

// constant buffers
cbuffer cbFrame : register(b0)
{
	float4x4 g_mWorldViewProjection;
	float4x4 g_mWorldView;		
	float4x4 g_mProjection;
	float4x4 g_mWorld;
	float4x4 g_mView;
	float4x4 g_mInverseView;		// inverse view matrix
}



#ifdef WITH_PICKING
#define PICKING_APPEND
cbuffer cbPicking : register( b8 )
{
	float2		g_cursorPos;
	float2		g_cursorDirection;
	int			g_objectID;
};


struct SPicking
{
	float4x4    TBNP;  
	float		depth;	
	int			objectID;
};

cbuffer BrushOrientationCB : register( b9 ) // picking matrix
{	
	float4x4		g_mBrushToWorld;		// pickingMatrix
	float4x4		g_mWorldToBrush;		// pickingMatrixInverse
}


#ifdef PICKING_APPEND
	AppendStructuredBuffer<SPicking> g_pickingResultUAV		: register(u6);
	//RWStructuredBuffer<SPicking> g_pickingResultUAV		: register(u6);
#else
	RWStructuredBuffer<SPicking> g_pickingResultUAV		: register(u6);
#endif

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


//static float3 g_lightDirES = mul((float3x3)g_mView, g_lightDir.xyz);	

//float4x4 inverse(float4x4 m)
//{
//	float4x4 mT = transpose(m);
//	float3 T = float3(mT[3].xyz);	
//	mT[3] = float4(0,0,0,1);
//
//	float3x3 R = (float3x3)mT;
//	
//	float3 iT = float3( -dot(R[0],T),
//						-dot(R[1],T),
//						-dot(R[2],T));
//
//	mT[0].w = iT.x; //-dot(R[0],T);
//	mT[1].w = iT.y; //-dot(R[1],T);
//	mT[2].w = iT.z; //-dot(R[2],T);
//	return mT;	
//}

#include "ShadingLib.h.hlsl"
#include "Material.h.hlsl"
//cbuffer cbMat : register(b2)
//{
//	float3	g_MatAmbient;		// constant ambient color
//	float	g_Special;
//	float3	g_MatDiffuse;		// constant diffuse color
//	float	g_smoothness;
//	float3	g_MatSpecular;		// constant specular color
//	float	g_MatShininess;		// shininess
//}

cbuffer cbVoxelGrid : register(b9) {
	float4x4 g_matModelToProj;
	float4x4 g_matModelToVoxel;
	uint2 g_stride;
	uint2 g_vgpadding;
	uint3 g_gridSize;
};

cbuffer cbCamAndDist : register(b4)
{
	float3 g_eye;						
	float g_depthRange;
}



struct VS_INPUT
{
    float4 f4Position   : POSITION;
	float4 f3Normal		: NORMAL;
//#ifdef WITH_TEXTURE
	float2 f2TexCoord	: TEXCOORD;
	//uint vertexId		: SV_VertexID;
//#endif
};


struct PS_INPUT
{
    float4 pos			: SV_Position;
	float3 normalES		: NORMAL;
	float4 posES		: TEXCOORD4;	
	float4 posWS		: TEXCOORD6;	
#ifdef WITH_TEXTURE
	float2 uv			: TEXCOORD;
#endif
	noperspective float4 edgeDistance : EDGEDISTANCE;

#ifdef PHONG_TESSELLATION
	float3 posOS : POSOS;
	float3 normalOS : NORMALOS;
#endif
#ifdef WITH_WS_SHADING
	float3 normalWS		: NORMALWS;
#endif
};

struct PS_INPUT_SOLID
{
    float4 pos			: SV_Position;
#ifdef WITH_ALPHA_TEX
	float2 uv			: TEXCOORD0;
#endif
};


struct PS_OUTPUT
{
#ifdef WITH_SSAO
	float4 f4Color : SV_Target0;
	float4 f4Normal_Depth : SV_Target1;
#else
	float4 f4Color : SV_Target0;	
#endif
};




// HELPER FUNCS



PS_INPUT_SOLID VS_Solid(VS_INPUT I)
{
	PS_INPUT_SOLID O;			
	O.pos = mul( g_mWorldViewProjection, float4( I.f4Position.xyz, 1.0 ) );

#ifdef WITH_SHADOW	
	//float4 posWorld = mul( g_mWorld, float4(I.f4Position.xyz, 1.0));
	//float4 posES = mul( g_mWorldView, float4( I.f4Position.xyz, 1.0 ) );
	//O.vDepth = posES.z;		
	//O.posLightES	=	mul(g_lightViewMatrix, posWorld);	// transform to light space
#endif
#ifdef WITH_ALPHA_TEX	
	O.uv = float2(I.f2TexCoord.x, 1-I.f2TexCoord.y);
#endif
	return O;
}





PS_INPUT VS_Std(VS_INPUT I)
{
	PS_INPUT O;			

	O.pos	= mul( g_mWorldViewProjection, float4( I.f4Position.xyz, 1.0 ) );
	O.posES = mul( g_mWorldView, float4( I.f4Position.xyz, 1.0 ) );	
	O.normalES		=	mul((float3x3)g_mWorldView, I.f3Normal.xyz);
	
	float4 posWorld = mul( g_mWorld, float4(I.f4Position.xyz, 1.0));
	O.posWS = posWorld;
#ifdef WITH_WS_SHADING
	O.normalWS = mul((float3x3)g_mWorld, I.f3Normal.xyz);
#endif
#ifdef WITH_TEXTURE	
	O.uv = I.f2TexCoord;
#endif

#ifdef PHONG_TESSELLATION
	O.posOS = I.f4Position.xyz;
	O.normalOS = I.f3Normal.xyz; 
#endif
	return O;
}

static float2 VIEWPORT_SCALE = 1280; // XXXdyu

float edgeDistance(float2 p, float2 p0, float2 p1)
{
    return VIEWPORT_SCALE.x *
        abs((p.x - p0.x) * (p1.y - p0.y) -
            (p.y - p0.y) * (p1.x - p0.x)) / length(p1.xy - p0.xy);
}

PS_INPUT
outputWireVertex(PS_INPUT input, int index, float2 edgeVerts[3])
{
    PS_INPUT v = input;
    v.edgeDistance[0] =   edgeDistance(edgeVerts[index], edgeVerts[0], edgeVerts[1]);
    v.edgeDistance[1] =   edgeDistance(edgeVerts[index], edgeVerts[1], edgeVerts[2]);
    v.edgeDistance[2] =   edgeDistance(edgeVerts[index], edgeVerts[2], edgeVerts[0]);
    return v;
}


[maxvertexcount(3)]
void gs_main( triangle PS_INPUT input[3],
              inout TriangleStream<PS_INPUT> triStream )
{
#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)
    float2 edgeVerts[3];
    edgeVerts[0] = input[0].pos.xy / input[0].pos.w;
    edgeVerts[1] = input[1].pos.xy / input[1].pos.w;
    edgeVerts[2] = input[2].pos.xy / input[2].pos.w;

    triStream.Append(outputWireVertex(input[0], 0, edgeVerts));
    triStream.Append(outputWireVertex(input[1], 1, edgeVerts));
    triStream.Append(outputWireVertex(input[2], 2, edgeVerts));
#else
    triStream.Append(input[0]);
    triStream.Append(input[1]);
    triStream.Append(input[2]);
#endif
}

float4
edgeColor(float4 Cfill, float4 edgeDistance)
{
#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)
    float d = min(edgeDistance[0], min(edgeDistance[1], edgeDistance[2]));

	float4 Cedge = float4(0.9, 0.9, 0.2, 1.0);
	float p = exp2(-0.8* d * d);

#if defined(GEOMETRY_OUT_WIRE)
    if (p < 0.25) discard;		
#endif

    Cfill.rgb = lerp(Cfill.rgb, Cedge.rgb, p);

#endif
    return Cfill;
}

#ifdef WITH_NORMAL_TEX
float3x3 calcWsCotangentFrame (float3 wsNormal, float3 wsInvViewDir, float2 tsCoord)
{
    // get edge vectors of the pixel triangle
    float3 dp1 = ddx(wsInvViewDir);
    float3 dp2 = ddy(wsInvViewDir);
    float2 duv1 = ddx(tsCoord);
    float2 duv2 = ddy(tsCoord);
 
    // solve the linear system
    float3 dp2perp = cross(dp2, wsNormal);
    float3 dp1perp = cross(wsNormal, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;
 
    // construct and return a scale-invariant cotangent frame
    float invmax = rsqrt(max(dot(T,T), dot(B,B)));
    return float3x3(T * invmax, B * invmax, wsNormal);
}

float ApplyChainRule(float dhdu, float dhdv, float dud_, float dvd_)
{
    return dhdu * dud_ + dhdv * dvd_;
}

float3 CalculateSurfaceGradient(float3 n, float3 dpdx, float3 dpdy, float dhdx, float dhdy)
{
    float3 r1 = cross(dpdy, n);
    float3 r2 = cross(n, dpdx);
 
    return (r1 * dhdx + r2 * dhdy) / dot(dpdx, r1);
}

float3 PerturbNormal(float3 n, float3 dpdx, float3 dpdy, float dhdx, float dhdy)
{
    return normalize(n - CalculateSurfaceGradient(n, dpdx, dpdy, dhdx, dhdy));
}

float3 perturb_normal(in float3 N, in float3 V, in float2 uv)
{
	uv = saturate(uv);
	float3 dp1 = ddx(-V);		
    float3 dp2 = ddy(-V);
	
    float2 duv1 = ddx(uv);	
    float2 duv2 = ddy(uv);

	duv1 = ApplyChainRule(dp1,dp2, ddx(uv.x), ddx(uv.y));
    duv2 = ApplyChainRule(dp1,dp2, ddy(uv.x), ddy(uv.y));


	float3 r1 = cross(dp2, N);
    float3 r2 = cross(N, dp1);

	float3 map = g_txNormal.Sample( g_linearSampler,  uv).rgb*2-1;

	return PerturbNormal(map, dp1, dp2, duv1, duv2);
	return float3(duv1.x,0,0);
	// solve the linear system
    float3 dp2perp = cross(dp2, N);

    float3 dp1perp = cross(N, dp1);
	
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;
	//return float3(duv2,0);

	float invmax = rsqrt(max(dot(T,T), dot(B,B)));
	return T*invmax;
	//float3 map = g_txNormal.Sample( g_linearSampler,  uv).rgb*2-1;
	float3x3 TBN = calcWsCotangentFrame( N, -V, uv );
	return normalize( mul(map,TBN));
}
#endif

#ifdef PIXEL_SHADING
#include "Shading.h.hlsl"
#define DISPLAY_GAMMA 1.6
float3 ToLinear(float3 v) { return pow(abs(v),     DISPLAY_GAMMA); }
float3 ToSRGB(float3 v)   { return pow(abs(v), 1.0/DISPLAY_GAMMA); }

PS_OUTPUT PS_Std(PS_INPUT I)
{
	PS_OUTPUT O;
	//discard;
	float3 esNormal = normalize(I.normalES.xyz);
	float3 L = normalize(g_lightDirES.xyz);

	const float ambientScale = 0.1;
	float nDotL = saturate(dot(esNormal, L));
	float3 color = 0;

	O.f4Color.w =1.f;
#ifdef WITH_TEXTURE	
	float2 tc = float2(I.uv.x, 1.f-I.uv.y);		
	color = ToLinear(g_txDiffuse.Sample( g_linearSampler,  tc));

// visualize texel density
#ifdef SHOW_TEXEL_DENSITY
	uint2 texSize = 16*8;
	g_txDiffuse.GetDimensions(texSize.x, texSize.y);
	float2 texelPos = tc*texSize;
	float dist = length(texelPos+0.5-round(texelPos+0.5));
	color = lerp(color, float3(1,0,0), smoothstep(0.60, 1.0, 1-dist));
#endif

#ifdef WITH_ALPHA_TEX
	float alpha = g_txAlpha.Sample( g_linearSampler,  float2(I.uv.x, 1-I.uv.y)/*tc*/).w;
	if(alpha <0.5) discard;		
	O.f4Color.w = alpha;	
#endif

#ifdef WITH_NORMAL_TEX
	float3 normalTex = g_txNormal.Sample( g_linearSampler,  float2(I.uv.x, 1-I.uv.y)/*tc*/).rgb;
#endif

#else
	color = g_MatDiffuse;
	//color = magicSand(I); 
#endif
	if (g_Special == 0.5)
		color = 1;
	else if (g_Special > 0.0) 
			color = shadingWrapper2(esNormal, I.posES, I.posWS);
	
	



#ifdef WITH_SSAO	
	O.f4Normal_Depth = float4(normalize(I.normalES.xyz)/*0.5+0.5*/, (I.posES.z)/ g_depthRange);
#endif

	float3 shadow = 1;
#ifdef WITH_SHADOW
	shadow = ShadowVisibility(I.posES, I.pos.w, nDotL, esNormal).rgb;	
#endif

	static const float3 vLightDir1 = float3( -1.0f, 1.0f, -1.0f ); 
    static const float3 vLightDir2 = float3( 1.0f, 1.0f, -1.0f ); 
    static const float3 vLightDir3 = float3( 0.0f, -1.0f, 0.0f );
    static const float3 vLightDir4 = float3( 1.0f, 1.0f, 1.0f );     
    // Some ambient-like lighting.
    float3 fLighting = saturate( dot( vLightDir1 , esNormal ) )*0.05f +
                      saturate( dot( vLightDir2 , esNormal ) ) *0.05f +
                      saturate( dot( vLightDir3 , esNormal ) ) *0.05f +
                      saturate( dot( vLightDir4 , esNormal ) ) *0.05f ;
    
    float3 vShadowLighting = fLighting * 0.5f;  	
	fLighting += saturate( dot( normalize(L).xyz , esNormal.xyz ) ) * shadow ;
    fLighting = lerp( vShadowLighting, fLighting, max(shadow.x,max(shadow.y,shadow.z)) );

	float3 V = normalize(float3(0,0,0) - I.posES.xyz);
	float3 H = normalize(V + L);

	
#ifdef WITH_WS_SHADING

	//float4 eyePos = mul(float4(0, 0, 0, 1), g_mView);
	//eyePos = mul(g_mWorld, eyePos);  //CHECKME
	//float3 incident = normalize(eyePos.xyz-I.posWS.xyz);
	float3 incident = normalize(g_eye-I.posWS.xyz);
	float3 wsNormal = normalize(I.normalWS);
#endif 

#ifdef WITH_ENVMAP
	float3 reflectionV = normalize(reflect(-incident, wsNormal));
	float3 refColor = ToLinear(g_envMap.Sample(g_linearSampler, reflectionV.xzy));
	//if(color.r == 0 && color.g == 0 && color.b == 0)
	//	color = refColor;
#endif


	// gamma correct color
		//color = pow(color,1.6);
	O.f4Color.xyz = color * fLighting// * (1.f/3.14159f)
				  //+ g_MatDiffuse*(ambientScale)
				  + shadow*g_MatSpecular * pow(max(dot(esNormal, H), 0), g_MatShininess);
	if (g_Special >  0.0) 
	{
		O.f4Color.xyz = color * (0.3+max(shadow.x,max(shadow.y,shadow.z))*0.7);//	
		return O;
	}

	 
// BEGIN PHYSICALLY LIGHTING
#ifdef WITH_WS_SHADING
#ifndef M_PI
#define M_PI 3.14159f
#endif
#ifndef M_INV_PI
#define M_INV_PI 1.f/3.14159f
#endif
	float3 L_WS = normalize(g_lightDirWS.xyz);
	//float nDotLWS = dot(wsNormal, L_WS);
	//float wrapL = 0.0;
	//float3 Clight = float3(1.0,1.0,1);// * M_PI;
	//float shininess = g_MatShininess;
	////color = 0.9;
	////float3 specColor = 0.017908907; //
	////float3 specColor = 0.0417908907; //
	//float3 specColor = 0.0417908907; //
	////float3 specColor = g_MatSpecular;
	//float3 schlick = specColor + (min(1,50*specColor) - specColor)*pow(1-dot(V,H), 5); // modified schlick approx
	//O.f4Color.xyz = color / (M_PI*(1+wrapL)) * (nDotL+wrapL)/(1+wrapL) * Clight* clamp(shadow+0.2,0.2, 1)
	//				+ shadow * schlick/(M_PI*(1+wrapL)) * (shininess+1)/2 * pow( (dot(esNormal, H)+wrapL)/(1+wrapL), shininess);
	////O.f4Color.xyz = pow(O.f4Color.xyz, 1.2);
	////O.f4Color.xyz = shadow;

	//// spherical harmonic lighting
	////TODO  normal, viewdir and light dir should all be in world space
	////TODO create coeffs for snow scene env map
	////float3 refl = normalize(reflect(V,esNormal.xyz));
	//O.f4Color.xyz = //float3(1,1,1)/*color*//(M_PI*(1+wrapL)) *(nDotLWS+wrapL)/(1+wrapL)*
	//				//0.05*shadingSH(wsNormal, L_WS, wrapL) * clamp(shadow+0.2,0.2, 1) +
	//				color / (M_PI*(1+wrapL)) * (nDotLWS+wrapL)/(1+wrapL) * Clight * M_PI//* clamp(shadow+0.2,0.2, 1)
	//				//; 
	//				+ shadow * shadingSH(reflectionV, L_WS, wrapL) * schlick/(M_PI*(1+wrapL)) * (shininess+1)/2 * pow( (dot(esNormal, H)+wrapL)/(1+wrapL), shininess);
	//float3 Cdiff = color/M_PI;
	//float3 H_WS = normalize(incident + L_WS);

	//schlick = specColor + (min(1,50*specColor) - specColor)*pow(1-dot(incident,H_WS), 5); // modified schlick approx
	//float3 blinn = pow(max(0,dot(reflectionV, L_WS)), 100) * float3(0.3, 0.3, 0.3) * nDotLWS;
	//float3 blinnphong = pow(dot(wsNormal, H_WS), 100) * float3(0.3, 0.3, 0.3) * nDotLWS;
	//float3 Lv = M_PI * Cdiff * Clight * nDotLWS;
	//float3 Lo = 0;
	////Lo = Lv + blinnphong;
	//Lo = Lv + blinn;
	////Lo = H_WS;//incident;//reflectionV;
	////Lo = dot(reflectionV,incident);
	////Lo = dot(incident,wsNormal);
	//Lo = abs(dot(incident,wsNormal) - dot(reflectionV,wsNormal)) * 100;

	//shininess = 100;
	//float3 ndf = (shininess+2)/8 * pow( max(0,dot(wsNormal, H_WS)), shininess);

	//float3 D = ndf;//blinnphong;
	//float3 G = 1;//shadow;
	//float3 F = schlick;

	//float nDotV = dot(wsNormal, incident);
	//Lo = Lv + M_PI * D * G / (4*max(0,nDotLWS) * max(nDotV,0)) * F * Clight * shadow * nDotLWS;

	//float3 schlickEnv = specColor + (min(1,50*specColor) - specColor)*pow(1-dot(incident,wsNormal), 5); // modified schlick approx
	//Lo = //shadingSH(reflectionV, L_WS, 0) + M_PI * D * G / (4*max(0,nDotLWS) * max(nDotV,0)) * schlickEnv * shadingSH(reflectionV, L_WS, 0) * shadow * nDotLWS;
	//M_PI* schlickEnv * shadingSH(reflectionV, L_WS, 0) + Lv; 

	//Lo = schlickEnv;
#ifdef WITH_NORMAL_TEX
	//color = normalTex.rgb;
	//color = perturb_normal(esNormal, V, float2(I.uv.x, 1-I.uv.y));
	//color = normalize(perturb_normal(I.normalWS, incident, float2(I.uv)));
	//wsNormal = normalize(perturb_normal(I.normalWS, incident, float2(I.uv.x, 1-I.uv.y)));
	wsNormal ;
	//wsNormal =  normalize(mul((float3x3)g_mInverseView, normalize(0.5*(I.normalES + (normalTex.rbg*0.5+0.5))))); //normalize(perturb_normal(I.normalWS, incident, float2(I.uv.x, 1-I.uv.y)));
	//color =  normalize(mul((float3x3)g_mInverseView, normalize(I.normalES)));
	//color = float3(I.uv,0);
	//color = wsNormal;
#endif
	float3 Lo = clamp(shadow+0.1,0.1, 1) * CalcLighting(wsNormal, L_WS, float3(1,1,1),
					color, float3(0.03, 0.03, 0.03), (1-g_MatShininess/511), // roughness:0 .. 1 : 1 == very rough
					I.posWS.xyz,g_eye);
					//+ Fresnel(color, H_WS, lightDir) *shadingSH(incident, L_WS, 0);
	
	//if(length(Lo)==0.0)		Lo += refColor;// * Fresnel(refColor, incident, reflectionV);
	O.f4Color.xyz = Lo;// + refColor;

#ifndef WITH_TEXTURE
	if(length(color)==0.0)
		O.f4Color.xyz = 0.2*refColor;//shadingSH(reflectionV, L_WS, wrapL);// * clamp(shadow+0.2,0.2, 1);
#endif

	O.f4Color.xyz = ToSRGB(O.f4Color);
						//+ shadow * shadingSH(refl, normalize(g_lightDirWS), wrapL);// * schlick/(M_PI*(1+wrapL)) * (shininess+1)/2 * pow( (dot(esNormal, H)+wrapL)/(1+wrapL), shininess);
// END PHYSICALLY LIGHTING
#endif


#ifdef WITH_PICKING	
	float2 diff = abs(floor(I.pos.xy + float2(0.5f, 0.5f)-g_cursorPos));
		
	if(diff.x < 1 && diff.y < 1 && g_objectID != -1)
	{
		// direction based
		float3x3 normalMatrix = (float3x3)g_mWorldView;		     
		float3 pN = normalize(esNormal);     
		float3 pS = normalize(cross(pN, normalize(float3(g_cursorDirection.xy, 0) *  float3(1,-1, 0))));
		float3 pC = normalize(cross(pS,pN));

		//C = normalize(float3(g_cursorDirection.xy, 0) * float3(1, -1, 0));
		pN = normalize(cross(pC,pS));

		// in normal direction
		//matrix<float,4,4> tnbp = {float4( mul( C.xyz, normalMatrix),0),
		//						  float4( mul( N.xyz, normalMatrix),0),
		//						  float4( mul( S.xyz, normalMatrix),0),
		//						  float4( I.f4vPos.xyz,1)};
		//float4x4 tbnp
		float4 worldPos = mul(g_mInverseView, float4( I.posES.xyz,1));
		matrix<float,4,4> tbnp = {float4( mul( pS.xyz, normalMatrix),0),
								  float4( mul( pC.xyz, normalMatrix),0),
								  float4( mul( pN.xyz, normalMatrix),0),
								  worldPos		// is world pos		  								
								 };

				
		SPicking picking;
		picking.TBNP = transpose(tbnp);		
		picking.depth = I.pos.z/I.pos.w; 
		picking.objectID = g_objectID;

		{
#ifdef PICKING_APPEND
			g_pickingResultUAV.Append(picking);
			
			//if(g_pickingResultUAV[0].depth > picking.depth);
			//	g_pickingResultUAV[0]= picking;
#else
			uint uPixelCount = g_pickingResultUAV.IncrementCounter();	
			g_pickingResultUAV[uPixelCount] = picking;
#endif
			
		}
	}
#endif
	
// visualize circle at pick pos
#ifdef WITH_PICKING
	if(g_objectID != -1)
	{
		float3 worldPos = mul(g_mInverseView, float4( I.posES.xyz,1)).xyz;
		float3 brushPos = g_mBrushToWorld._m03_m13_m23;

		float dist = length(worldPos.xyz-brushPos.xyz);
		//#define thresh 2
		float thresh = length(g_brushExtends*0.5);
		if(dist < thresh)
		{	
			if(dist>0.75*thresh)
				O.f4Color.xyz = float3(1,1,0);
		}
	}
#endif

#if defined(GEOMETRY_OUT_LINE) || defined(GEOMETRY_OUT_WIRE)
	O.f4Color = edgeColor(O.f4Color,I.edgeDistance);
	//O.f4Color.xyz = 1;//I.edgeDistance;	
#endif

	return O;
}
#endif

/// FASTER SHADOW MAPPING
struct GS_PS_INPUT
{
    float4 pos			: SV_Position;
	uint   RTIndex		: SV_RenderTargetArrayIndex;
};

struct VS_GS_INPUT
{
    float4 pos			: SV_Position;	
};

VS_GS_INPUT VS_GenVarianceShadowFast(VS_INPUT I)
{
	VS_GS_INPUT O;			
	O.pos = mul( g_mWorldView, float4( I.f4Position.xyz, 1.0 ) );
	return O;
}

#ifndef NUM_CASCADES
#define NUM_CASCADES 3
#endif

[maxvertexcount(NUM_CASCADES*3)]
void GS_GenVarianceShadowFast( triangle VS_GS_INPUT input[3],
								inout TriangleStream<GS_PS_INPUT> triStream )
{
	GS_PS_INPUT O;	
	for(int i = 0; i < NUM_CASCADES; ++i)
	{		
		O.RTIndex = i;

		O.pos = mul(g_mCascadeProj[i], float4(input[0].pos.xyz,1));		triStream.Append(O);							  
		O.pos = mul(g_mCascadeProj[i], float4(input[1].pos.xyz,1));		triStream.Append(O);		   					  		 
		O.pos = mul(g_mCascadeProj[i], float4(input[2].pos.xyz,1));		triStream.Append(O);
		
		triStream.RestartStrip();
	}
}


float2 PS_GenVarianceShadowFast(in GS_PS_INPUT Input) : SV_TARGET
{
	float2 rt;
    rt.x = Input.pos.z;
    rt.y = rt.x * rt.x;
    return rt;
}

float2 PS_GenVarianceShadow(PS_INPUT_SOLID I) : SV_TARGET
{
	#ifdef WITH_ALPHA_TEX
	float alpha = g_txAlpha.Sample( g_linearSampler,  I.uv).w;
	if(alpha <0.5) discard;		
	#endif

	float2 rt;
    rt.x = I.pos.z;
    rt.y = rt.x * rt.x;
    return rt;
}

static float g_SkydomeScale = 100.;
PS_INPUT VS_Skydome(VS_INPUT I) {
	PS_INPUT O;			
	float4 posW = float4( I.f4Position.xyz * g_SkydomeScale, 1.0 );
	O.pos = mul( g_mProjection, posW );
	// checkme is wrong
	//O.normalES.xyz = mul(float3(I.f3Normal.xyz), (float3x3)(g_mWorldView));
	O.normalES.xyz = mul(float3(I.f3Normal.xyz), (float3x3)(g_mWorldView));

	//matrix mView = mul(g_mWorldView, inverse(g_mWorld));
	//O.lightDirES = mul((float3x3)mView, g_lightDir);
	//O.lightDirES = normalize(mul((float3x3)g_mWorldView, g_lightDir));	
	//O.lightDirES = normalize(mul((float3x3)g_mView, g_lightDir.xyz));	


	return O;
}

PS_OUTPUT PS_Skydome(PS_INPUT I)
{
	PS_OUTPUT O;

	float3 N = normalize(I.normalES.xyz);
	//O.f4Color.xyz = N;// * 0.5f + 0.5f;
	
	float cosAngleHeight = dot(N, float3(0.0, 0.0, 1.0));
	float angleZ = acos(cosAngleHeight);
	float Z = smoothstep(-3.1415*0.001, 3.1415, angleZ);
	float Z2 = smoothstep(3.1415*0.4, 3.1415, angleZ);
	float4 skyBlue = float4(0.2, 0.2, 0.8, 1.0);
	float4 skyLightBlue = float4(0.6, 0.6, 0.8, 1.0);
	float S = snoise(N*8.0) + snoise(N*16.0) * 0.5 + snoise(N*32.0) * 0.125;

	float cloudNoise = (smoothstep(-0.5, 1, S)) * Z2;
	//float lightSource = pow(max(dot(N, normalize(I.lightDirES)), 0), 960.0)*700.0;	
	float lightSource = pow(max(dot(N, normalize(g_lightDirES).xyz), 0), 960.0)*700.0;	

	O.f4Color = lerp(skyLightBlue, skyBlue, Z) + float4(1,1,1,1) * cloudNoise * 0.35 + 0*lightSource*float4(1.0,0.8,0.2,1.0);
	O.f4Color.w = 1.f;

	return O;
}

float4 PS_VoxelizeSolidBackward(centroid PS_INPUT In) : SV_TARGET 
{
	float3 gridPos = In.pos.xyz / In.pos.w;
	gridPos.z *= g_gridSize.z;

	int3 p = int3(gridPos.x, gridPos.y, gridPos.z + 0.5);

	// flip all voxels below
	if (p.z < int(g_gridSize.z)) {
		uint address = p.x * g_stride.x + p.y * g_stride.y + (p.z >> 5) * 4;	
		uint tmp = ~(0xffffffffu << (p.z & 31));
		g_rwbufVoxels.InterlockedXor(address, tmp);
		for (p.z = (p.z & (~31)); p.z > 0; p.z -= 32) {
			address -= 4;
			g_rwbufVoxels.InterlockedXor(address, 0xffffffffu);
		}
	}
	// kill fragment
	discard;
	
	return float4(1,1,0,1);	
}


float4 PS_VoxelizeSolidForward(centroid PS_INPUT In) : SV_TARGET 
{
	// determine first voxel
	float3 gridPos = In.pos.xyz / In.pos.w;
	gridPos.z *= g_gridSize.z;

	int3 p = int3(gridPos.x, gridPos.y, gridPos.z + 0.5);

	// flip all voxels below
	if(p.z < int(g_gridSize.z)) {
		uint address = p.x * g_stride.x + p.y * g_stride.y + (p.z >> 5) * 4;
		g_rwbufVoxels.InterlockedXor(address, 0xffffffffu << (p.z & 31));
		//g_rwbufVoxels.Store(address, 0xffffffffu << (p.z & 31));
		for(p.z = (p.z | 31) + 1; p.z < int(g_gridSize.z); p.z += 32) {
			address += 4;
			g_rwbufVoxels.InterlockedXor(address, 0xffffffffu);
			//g_rwbufVoxels.Store(address, 0xffffffffu);
		}
	}

	// kill fragment
	discard;
	
	return float4(0,1,0,1);
  }


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

cbuffer cbCamAndDist : register(b4)
{
	float3 g_eye;						
	float g_depthRange;
	float g_zNear;
	float g_zFar;
}

struct Position_Input
{
    float4 f4Position   : POSITION;  	
	float2 f2uv			: TEXCOORD0;
};

struct PS_RenderSceneInput
{
    float4 f4Position   : SV_Position;
	float3 f3vNormal	: NORMAL;
	float2 f2uv			: TEXCOORD0;
};

struct PS_RenderOutput
{
    float4 f4Color      : SV_Target0;	
};

Texture2D           g_txShadow             : register( t0 );
SamplerState		g_samplerClamp			: register( s0 );


PS_RenderSceneInput fsQuadVS( Position_Input I )
{
    PS_RenderSceneInput O;    
	// Pass through world space position    
	//O.f4Position = mul( g_f4x4WorldViewProjection, I.f4Position ) ;
	O.f4Position = float4(I.f4Position.xyz,1.0 ) ;
    O.f2uv = I.f2uv;
	//O.f2uv.y = 1.0 - O.f2uv.y;

    return O;    
}

float ZClipToZEye(float zClip)
{
    return g_zFar*g_zNear / (g_zFar - zClip*(g_zFar-g_zNear));
}
//
//float4 VisShadowMapPS(PostProc_VSOut IN) : SV_TARGET
//{
//    float z = g_txShadow.Sample(g_samplerClamp, float3(IN.uv, 0));
//   // z = (ZClipToZEye(z) - g_LightZNear) / (g_LightZFar - g_LightZNear);
//    return z * 10;
//}



PS_RenderOutput visShadowPS( PS_RenderSceneInput I )
{
    PS_RenderOutput O;
	

	//float4 tex0Color = g_txDiffuse.Sample(g_samplerRepeat, I.f2uv);
	//float4 tex1Color = g_txNormalDepth.Sample(g_samplerRepeat, I.f2uv);
	//O.f4Color = float4(tex0Color.rgb*0.5+0.5,1);
	//O.f4Color = float4(I.f2uv.xy, 0, 1);

	float z = g_txShadow.Sample(g_samplerClamp, float3(I.f2uv, 0));
    z = (ZClipToZEye(z) - g_zNear) / (g_zFar - g_zNear);
	z =  z;
	////if(z>0.5)
	////	z = 1;
	//if(z < 0.999)
	//	z = 0.3;
	//else
	//	z = 1;
	O.f4Color = float4(z,z,z, 1);
	
    return O;
}

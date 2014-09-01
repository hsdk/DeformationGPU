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

Texture2D           g_txDiffuse             : register( t0 );
Texture2D           g_txNormalDepth         : register( t1 );
Texture2D           g_txNoise				: register( t2 );
Texture2D           g_txSSAO				: register( t1 );


SamplerState		g_samplerRepeat			: register( s0 );
SamplerState		g_samplerClamp			: register( s1 );


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




PS_RenderOutput fsQuadPS( PS_RenderSceneInput I )
{
    PS_RenderOutput O;
	

	float4 tex0Color = g_txDiffuse.Sample(g_samplerRepeat, I.f2uv);
	float4 tex1Color = g_txNormalDepth.Sample(g_samplerRepeat, I.f2uv);
	O.f4Color = float4(tex0Color.rgb*0.5+0.5,1);
	
    return O;
}


// SSAO


PS_RenderOutput ssaoPS( PS_RenderSceneInput I )
{
		PS_RenderOutput O;

	/*
	STANDARD
    const float totalStrength = 1.38;
	const float strength = 0.005;
	const float offset = 18.0;
	const float falloff = 0.00002;
	const float radius = 0.0008;
	*/
	/*
	// gut bei subD
	const float totalStrength = 2.38;
	const float strength = 0.05;
	const float offset = 18.0;
	const float falloff = 0.000001;
	const float radius = 0.0005;
	
	*/

	const float totalStrength = 1.58;
	const float strength = 0.0015;
	const float offset = 18.0;
	const float falloff = 0.000001;
	const float radius = 0.0008;


	#define SAMPLES 12
	const float invSamples = 1.f/float(SAMPLES);


	const float3 sphereRandomVectors[12] = {float3(-0.13657719, 0.30651027, 0.16118456),
									      float3(-0.14714938, 0.33245975, -0.113095455),
										  float3(0.030659059, 0.27887347, -0.7332209),
										  float3(0.009913514, -0.89884496, 0.07381549),
										  float3(0.040318526, 0.40091, 0.6847858),
										  float3(0.22311053, -0.3039437, -0.19340435),
										  float3(0.36235332, 0.21894878, -0.05407306),
										  float3(-0.15198798, -0.38409665, -0.46785462),
										  float3(-0.013492276, -0.5345803, 0.11307949),
										  float3(-0.4972847, 0.037064247, -0.4381323),
										  float3(-0.024175806, -0.008928787, 0.17719103),
										  float3(0.694014, -0.122672155, 0.33098832)
										};
	
	float3 random = g_txNoise.Sample(g_samplerRepeat, I.f2uv*offset).rgb * 2 - 1;
	//float3 random = normalize(texture2D(randomTex, textureCoordinate * offset).xyz * 2 - 1);
	float4 normalAndDepth = g_txNormalDepth.Sample(g_samplerClamp, I.f2uv);
	//normalAndDepth.xyz = (normalAndDepth.xyz*2-1);
	//O.f4Color = normalAndDepth * 0.5 + 0.5;
	//return O;
	//normalAndDepth.xyz *= -1;

	float3 position = float3(I.f2uv.xy, normalAndDepth.w);
	
	if (normalAndDepth.w == 1.0) 
	{
		//O.f4Color = float4(1,1,1,1);
		O.f4Color = float4(0,0,0,1);
		return O;
	}
	float radiusD = radius / normalAndDepth.w;
	float S = 0;
	
	for(int i = 0; i < SAMPLES; ++i) {
		float3 ray = radiusD * reflect(sphereRandomVectors[i], random);
	
		//float3 sampleS = ;
		//float4 occluderFragment = texture2D(normalAndDepthTex, position.xy + sign(dot(ray, normalAndDepth.xyz)) * ray.xy);
		float4 occluderFragment =	g_txNormalDepth.Sample(g_samplerRepeat, position.xy + sign(dot(ray, normalAndDepth.xyz)) * ray.xy ); 
		
		float depthDifference = normalAndDepth.w - occluderFragment.w;
		float normalDifference = (1.0 - dot(occluderFragment.xyz, normalAndDepth.xyz));
		S += step(falloff, depthDifference) * normalDifference * (1.0 - smoothstep(falloff, strength, depthDifference));
	}

	float ambientOcclusion = 1.0 - totalStrength * invSamples * S;
	//O.f4Color = normalAndDepth * 0.5 + 0.5;//
	//return O;
	//flo color = texture2D(colorTex, textureCoordinate);
	
	//if(normalAndDepth.w > 1)
	//	O.f4Color = float4(1,0,0,1);
	//else
	O.f4Color = ambientOcclusion;// * (1.-mix(0.0, 1.0, min(color.w*2., 1.0))) * vec4(1);// * color;// * texture2D(colorTex, textureCoordinate);



		

	//float4 tex0Color = g_txDiffuse.Sample(g_sampler, I.f2uv);
	//float4 tex1Color = g_txNormalDepth.Sample(g_sampler, I.f2uv);
	//O.f4Color = float4(noiseTex.rgb,1);
		
	
    return O;
}


static const float offset[3] = {0.0, 1.3846153846, 3.2307692308};
static const float weight[3] = {0.2270270270, 0.3162162162, 0.0702702703};

PS_RenderOutput blurVPS( PS_RenderSceneInput I )
{
    PS_RenderOutput O;

	uint mipLvl,width,height,levels;
	g_txDiffuse.GetDimensions(1,width,height,levels);

	float4 out0 = g_txDiffuse.Sample(g_samplerClamp, I.f2uv) * weight[0];
	[unroll]
	for (int i = 1; i < 3; ++i) {
		out0 += g_txDiffuse.Sample(g_samplerClamp, I.f2uv + float2(0, offset[i])/(float)720) * weight[i];
		out0 +=g_txDiffuse.Sample(g_samplerClamp,  I.f2uv - float2(0, offset[i])/(float)720) * weight[i];
	}	

	O.f4Color = float4(out0.rgb,1);

    return O;
}

PS_RenderOutput blurHPS( PS_RenderSceneInput I )
{
    PS_RenderOutput O;
	
	uint mipLvl,width,height,levels;
	g_txDiffuse.GetDimensions(1,width,height,levels);

	float4 out0 = g_txDiffuse.Sample(g_samplerClamp, I.f2uv) * weight[0];
	[unroll]
	for (int i = 1; i < 3; ++i) {
		out0 += g_txDiffuse.Sample(g_samplerClamp, I.f2uv + float2(offset[i], 0)/(float)1280) * weight[i];
		out0 += g_txDiffuse.Sample(g_samplerClamp,  I.f2uv - float2(offset[i], 0)/(float)1280) * weight[i];
	}	

	O.f4Color = float4(out0.rgb,1);
	
    return O;
}




PS_RenderOutput composePS( PS_RenderSceneInput I )
{
    PS_RenderOutput O;
	
	float4 textureColor = g_txDiffuse.Sample(g_samplerRepeat, I.f2uv);
	float4 ssaoColor = g_txSSAO.Sample(g_samplerRepeat, I.f2uv);
	O.f4Color = float4(textureColor.rgb,1);	
	//O.f4Color = float4(textureColor.rgb*ssaoColor.rgb,1);	
	//O.f4Color = float4(textureColor.rgb*saturate(sqrt(ssaoColor.rgb)),1);	
	O.f4Color = float4(textureColor.rgb*saturate(ssaoColor.rgb),1);	
	//O.f4Color = float4(1.0, 0.25, 0.25,1);
	//O.f4Color = float4(sqrt(ssaoColor.rgb),1.0);
	//O.f4Color = float4((ssaoColor.rgb),1.0);
    return O;
}
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

#ifndef BLUR_KERNEL_SIZE
#define BLUR_KERNEL_SIZE 3
#endif


#ifndef SHADOW_MAP_RES
#define SHADOW_MAP_RES 1024
#endif

#ifndef NUM_CASCADES
#define NUM_CASCADES 3
#endif


//static const uint numCascades = CASCADES;
static const int BLUR_KERNEL_BEGIN = BLUR_KERNEL_SIZE / -2.0; 
static const int BLUR_KERNEL_END = BLUR_KERNEL_SIZE / 2.0 +1;
static const float FLOAT_BLUR_KERNEL_SIZE = (float)BLUR_KERNEL_SIZE;
static const int2 g_iWidthHeight = int2(SHADOW_MAP_RES, SHADOW_MAP_RES);

//--------------------------------------------------------------------------------------
// defines
//--------------------------------------------------------------------------------------

Texture2DArray g_txShadowPrepass : register( t11 );
Texture2DArray g_txShadow : register( t12 );
SamplerState g_samShadow : register( s6 );

//--------------------------------------------------------------------------------------
// Input/Output structures
//--------------------------------------------------------------------------------------

struct PSIn
{
    float4      Pos	    : SV_Position;		//Position
    float2      Tex	    : TEXCOORD;		    //Texture coordinate
    float2      ITex    : TEXCOORD2;
};

struct VSIn
{
    uint Pos	: SV_VertexID ;
};


PSIn VSMain(VSIn inn)
{
    PSIn output;

    output.Pos.y  = -1.0f + (inn.Pos%2) * 2.0f ;
    output.Pos.x  = -1.0f + (inn.Pos/2) * 2.0f;
    output.Pos.z = .5;
    output.Pos.w = 1;
    output.Tex.x = inn.Pos/2;
    output.Tex.y = 1.0f - inn.Pos%2;
    output.ITex.x = (float)(g_iWidthHeight.x * output.Tex.x);
    output.ITex.y = (float)(g_iWidthHeight.y * output.Tex.y);
    return output;
}

//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
float2 PSBlurX(PSIn input) : SV_Target
{	
    float2 dep=0;
    [unroll] for ( int x = BLUR_KERNEL_BEGIN; x < BLUR_KERNEL_END; ++x ) {
        dep += g_txShadow.Sample( g_samShadow,  float3( input.Tex.x, input.Tex.y, 0 ), int2( x,0 ) ).rg;
    }
    dep /= FLOAT_BLUR_KERNEL_SIZE;
    return dep;  
}

//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
float2 PSBlurY(PSIn input) : SV_Target
{	
    
    float2 dep=0;
    [unroll]for ( int y = BLUR_KERNEL_BEGIN; y < BLUR_KERNEL_END; ++y ) {
        dep += g_txShadow.Sample( g_samShadow,  float3( input.Tex.x, input.Tex.y, 0 ), int2( 0,y ) ).rg;
    }
    dep /= FLOAT_BLUR_KERNEL_SIZE;
    return dep;  
}



float4 PSDebugShadow(PSIn input) : SV_Target
{	
	float2 uv = input.Tex;
	float2 tc = input.Tex;
    //float2 dep = g_txShadow.Sample( g_samShadow,  float3( input.Tex.x, input.Tex.y, 0 ), int2( 0,y ) ).rg;
	//float2 dep = g_txShadow.Sample( g_samShadow,  float3( input.Tex, 2 )).rg;
	

	int level = 0;
	if(tc.x < 0.5 && tc.y < 0.5)
	{
		uv.xy *= 2;
		level = 0;	
	}		

	if(tc.x > 0.5 && tc.y < 0.5)
	{
		uv.x = (uv.x-0.5)*2;
		uv.y *= 2;
		level = 1;	
	}

	if(tc.x < 0.5 && tc.y > 0.5)
	{
		uv.x *= 2;
		uv.y = (uv.y-0.5)*2;
		level = 2;	
	}		

#if NUM_CASCADES == 4
{
	if(tc.x > 0.5 && tc.y > 0.5)
	{
		uv = (uv-0.5)*2;
		level=3;
	}
}
#endif

	//float dep = g_txShadowPrepass.Sample( g_samShadow,  float3( uv, level )).r;
	float dep = g_txShadow.Sample( g_samShadow,  float3( uv, level )).r;

	float3 depth = 1-dep.r;
	
	if(abs(tc.x -0.5) < 0.001 || abs(tc.y -0.5) < 0.001)
		depth = float3(0,1,0);

#if NUM_CASCADES == 3
{
	if(tc.x > 0.5 && tc.y > 0.5)
	{
		depth= float3((uv-0.5)*2,0);
	}
}
#endif
	//if(dep.r < 1 && dep.r > 0.99996)
	//	depth.xyz = float3(0,1,0);

	float4 col = float4(depth, 1);


    return col;  
}

//#define SHARED_MEM_BLUR

// compute shader variant
RWTexture2DArray<float2> g_blurResult : register( u1 );

//#ifndef SHARED_MEM_BLUR
#ifndef WORK_GROUP_SIZE_BLUR 
#define WORK_GROUP_SIZE_BLUR 4 
#endif

[numthreads(WORK_GROUP_SIZE_BLUR, WORK_GROUP_SIZE_BLUR, 1)]
void ShadowBlurXCS(				uint3 blockIdx	: SV_GroupID, 
							uint3 DTid		: SV_DispatchThreadID, 
							uint3 threadIdx : SV_GroupThreadID,
							uint  GI		: SV_GroupIndex ) 
{	
	int3 idx = int3(blockIdx.xy * WORK_GROUP_SIZE_BLUR+threadIdx.xy, 0);
    float2 dep=0;
    [unroll] 
	for ( int x = BLUR_KERNEL_BEGIN; x < BLUR_KERNEL_END; ++x ) {
        //dep += g_txShadow.Sample( g_samShadow,  float3( input.Tex.x, input.Tex.y, 0 ), int2( x,0 ) ).rg;
		dep += g_txShadow[int3( idx.x+x, idx.yz )].rg;
    }	
    dep /= FLOAT_BLUR_KERNEL_SIZE;
	g_blurResult[idx] = dep;

}

[numthreads(WORK_GROUP_SIZE_BLUR, WORK_GROUP_SIZE_BLUR, 1)]
void ShadowBlurYCS(	uint3 blockIdx	: SV_GroupID, 
							uint3 DTid		: SV_DispatchThreadID, 
							uint3 threadIdx : SV_GroupThreadID,
							uint  GI		: SV_GroupIndex ) 
{	
    int3 idx = int3(blockIdx.xy * WORK_GROUP_SIZE_BLUR+threadIdx.xy, 0);
    float2 dep=0;
    [unroll]
	for ( int y = BLUR_KERNEL_BEGIN; y < BLUR_KERNEL_END; ++y )
	{
       // dep += g_txShadow[int3( input.Tex.x, input.Tex.y+y, 0 )].rg;
		dep += g_txShadow[int3( idx.x, idx.y+y, idx.z )].rg;
    }
    dep /= FLOAT_BLUR_KERNEL_SIZE;

	g_blurResult[idx] = dep;  
}


//#else
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//SHARED MEMORY BLUR

SamplerState g_samShadowPoint : register( s6 );
SamplerState g_samShadowLinearClamp : register( s7 );

float2 LinearSampleVSM(float2 uv)
{
    return g_txShadow.SampleLevel(g_samShadowLinearClamp, float3(uv, 0), 0).rg;
}

//----------------------------------------------------------------------------------
float2 PointSampleVSM(float2 uv)
{
    return g_txShadow.SampleLevel(g_samShadowPoint, float3(uv, 0), 0).rg;
}

#define ROW_TILE_W 256

#define  g_BlurRadius 2
//BLUR_KERNEL_SIZE
//BLUR_KERNEL_SIZE
//BLUR_KERNEL_SIZE
#define g_HalfBlurRadius 1.0
//(g_BlurRadius/2.0)
#define g_InvRes 1.f/(SHADOW_MAP_RES-1)

#define SHARED_MEM_SIZE (g_BlurRadius + ROW_TILE_W + g_BlurRadius)
groupshared float2 g_sharedMem[SHARED_MEM_SIZE];


float2 GetSample(uint j) 
{
    return g_sharedMem[j];
}
#define g_BlurFalloff  0.5f
#define g_BlurDepthThreshold 0.125
float CrossBilateralWeight(in float r, in float d, in float d0)
{
    // The exp2(-r*r*g_BlurFalloff) expression below is pre-computed by fxc.
    // On GF100, this ||d-d0||<threshold test is significantly faster than an exp weight.
    //return exp2(-r*r*g_BlurFalloff) * (abs(d - d0) < g_BlurDepthThreshold);
	//return exp2(-r*r) * (abs(d - d0) < g_BlurDepthThreshold);
	return 1;
}

#define skip 2
void CopyToSharedMem(float x, float y, uint3 threadIdx)
{
	const float2 uv = ((float2(x,y))+0.5f) * g_InvRes;
	//const float2 uv = (float2(x,y)) * g_InvRes;
	g_sharedMem[threadIdx.x] = LinearSampleVSM(uv);
	//g_sharedMem[threadIdx.x] = PointSampleVSM(uv);
	//g_sharedMem[threadIdx.x] = g_txShadow[int3(x,y,0)];
}

[numthreads(SHARED_MEM_SIZE, 1, 1)]
void ShadowBlurSharedMemXCS(			uint3 blockIdx	: SV_GroupID, 
							uint3 DTid		: SV_DispatchThreadID, 
							uint3 threadIdx : SV_GroupThreadID,
							uint  GI		: SV_GroupIndex ) 
{	
	
	const uint        row = blockIdx.y;
    const uint  tileStart = blockIdx.x * ROW_TILE_W;
    const uint    tileEnd = tileStart + ROW_TILE_W;
    const uint apronStart = tileStart - g_BlurRadius;
    const uint   apronEnd = tileEnd   + g_BlurRadius;
		  
    const float		x = apronStart + threadIdx.x;
    const float		y = row;

	CopyToSharedMem(x,y,threadIdx);
	GroupMemoryBarrierWithGroupSync();

	// now row + overlap is in shared mem

	const float writePos = tileStart + threadIdx.x;	
    const float tileEndClamped = min(tileEnd, SHADOW_MAP_RES);

	[branch]
    if (writePos < tileEndClamped)
    {
		float2 uv = ((float2(writePos,y)) + 0.5f) * g_InvRes; // checkme
		//float2 uv = ((float2(writePos,y))) * g_InvRes; // checkme
		float w_total = 1;
		float2 dep = PointSampleVSM(uv)*w_total;
		//float2 dep = GetSample(threadIdx.x+g_HalfBlurRadius)*w_total;
		float center_d = dep.x;
		float i;


		[unroll]
		for(i = 0; i < g_HalfBlurRadius; ++i)
		{
			uint j = skip*(uint)i+threadIdx.x;
			float2 sample = GetSample(j);	

			float r = skip * i + (-g_BlurRadius+0.5);
			float w = CrossBilateralWeight(r, sample.x, center_d);
			dep += sample * w;

			w_total+= w;
		}

		[unroll]
		for(i = 0; i < g_HalfBlurRadius; ++i)
		{
			uint j = skip*(uint)i+threadIdx.x + g_BlurRadius +1;
			float2 sample = GetSample(j);	
			float r = skip * i + 1.5;
			float w = CrossBilateralWeight(r, sample.x, center_d);
			dep += sample * w;
			w_total+= w;
		}
		dep /=w_total;
		g_blurResult[int3(writePos,blockIdx.y,0)] = dep;
	}
}



[numthreads(SHARED_MEM_SIZE, 1, 1)]
void ShadowBlurSharedMemYCS(	uint3 blockIdx	: SV_GroupID, 
							uint3 DTid		: SV_DispatchThreadID, 
							uint3 threadIdx : SV_GroupThreadID,
							uint  GI		: SV_GroupIndex ) 
{	

	const uint        col = blockIdx.y;
    const uint  tileStart = blockIdx.x * ROW_TILE_W;
    const uint    tileEnd = tileStart + ROW_TILE_W;
    const uint apronStart = tileStart - g_BlurRadius;
    const uint   apronEnd = tileEnd   + g_BlurRadius;
		  
    const float			x = col;
    const float			y = apronStart + threadIdx.x;

	CopyToSharedMem(x,y,threadIdx);
	GroupMemoryBarrierWithGroupSync();

	// now col + overlap is in shared mem	
	const float writePos = tileStart + threadIdx.x;	
    const float tileEndClamped = min(tileEnd, SHADOW_MAP_RES);
	//if (writePos >=tileStart && writePos < tileEndClamped)
    [branch]
    if (writePos < tileEndClamped)
    {
		float2 uv = ((float2(x,writePos)) + 0.5f) * g_InvRes; // checkme
		//float2 uv = ((float2(x,writePos))) * g_InvRes; // checkme
		float w_total = 1;
		float2 dep = PointSampleVSM(uv)*w_total;
		//float2 dep = GetSample(threadIdx.x+g_HalfBlurRadius)*w_total;
		float center_d = dep.x;
		float i;

		[unroll]
		for(i = 0; i < g_HalfBlurRadius; ++i)
		{
			uint j = skip*(uint)i+threadIdx.x;
			float2 sample = GetSample(j);	
			float r = skip * i + (-g_BlurRadius+0.5);
			float w = CrossBilateralWeight(r, sample.x, center_d);
			dep += sample * w;
			w_total+= w;
		}

		[unroll]
		for(i = 0; i < g_HalfBlurRadius; ++i)
		{
			uint j = skip*(uint)i+threadIdx.x + g_BlurRadius +1;
			float2 sample = GetSample(j);	
			float r = skip * i + 1.5;
			float w = CrossBilateralWeight(r, sample.x, center_d);
			dep += sample * w;
			w_total+= w;
		}
		dep /=w_total;

		g_blurResult[int3(blockIdx.y,writePos,0)] =  dep;
	}

}

//#endif // SHARED_MEM_BLUR





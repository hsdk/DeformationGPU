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

#ifndef WORKGROUP_SIZE_SCAN
#define WORKGROUP_SIZE_SCAN 256
#endif

#ifndef BUCKET_SIZE
#define BUCKET_SIZE WORKGROUP_SIZE_SCAN
#endif

#ifndef BUCKET_BLOCK_SIZE
#define BUCKET_BLOCK_SIZE WORKGROUP_SIZE_SCAN
#endif

#ifndef NUM_BUCKET_BLOCKS
#define NUM_BUCKET_BLOCKS WORKGROUP_SIZE_SCAN
#endif

#ifndef DISPATCH_THREADS_X
#define DISPATCH_THREADS_X 128
#endif

#ifndef SHADOW_MAP_RES
#define SHADOW_MAP_RES 1024
#endif

#ifndef WORK_GROUP_SIZE_ATOMIC_REDUCTION_Y
#define WORK_GROUP_SIZE_ATOMIC_REDUCTION_X 16
#endif

#ifndef WORK_GROUP_SIZE_ATOMIC_REDUCTION_Y
#define WORK_GROUP_SIZE_ATOMIC_REDUCTION_Y 16
#endif

#ifndef FLT_MAX
#define FLT_MAX 3.999e+10F
#endif

cbuffer cbFrame : register(cb0)
{
	float4x4 g_mWorldViewProjection;
	float4x4 g_mWorldView;		
	float4x4 g_mProjection;
	float4x4 g_mWorld;
	float4x4 g_mView;
}

cbuffer ReductionCB : register( b11 ) 
{    
	float	g_nearClip;
	float	g_farClip;
	uint	g_numTexel;
	//uint	g_padding;
};


//Texture2DArray<float>	g_txShadow						: register(t0);
#ifdef WITH_MSAA
Texture2DMS<float>		g_txShadow						: register(t0);
#else
Texture2D<float>		g_txShadow						: register(t0);
#endif
Buffer<float2>			g_scanInput						: register(t1);		// is output pass1, input pass2, was visibleAllocDeallocs
Buffer<float2>			g_scanResultBucketsPass1		: register(t1);		// is output pass1, input pass2, was visibleAllocDeallocs
Buffer<float2>			g_scanResultBucketsDownSweep	: register(t2);		// is output pass1, input pass2, was visibleAllocDeallocsIncr

RWBuffer<float2>		g_outputScanPassUAV				: register(u0);			// 	
RWBuffer<float2>		g_depthMinMaxResultUAV			: register(u0);			// 
RWBuffer<uint>			g_depthMinMaxReductionResultUAV	: register(u1);	  // 0 -> min, 1 -> max

groupshared float2 bucket[BUCKET_SIZE];

float2 minMax(float2 a, float2 b)
{
	float2 ret;
	ret.x = 0.0;//min(a.x, b.x);
	ret.y = max(a.y, b.y);

	//if(ret.x == 0)
	//{
	//	ret.x = max(a.x, b.x);;
	//}
	return ret;
}

float2 CSWarpScan(uint lane, uint i)
{
	uint4 access = i - uint4(1,2,4,8);
	if(lane >  0){ bucket[i]  = minMax( bucket[i], bucket[access.x]); } 	
	if(lane >  1){ bucket[i]  = minMax( bucket[i], bucket[access.y]); }	
	if(lane >  3){ bucket[i]  = minMax( bucket[i], bucket[access.z]); }	
	if(lane >  7){ bucket[i]  = minMax( bucket[i], bucket[access.w]); }	
	if(lane > 15){ bucket[i]  = minMax( bucket[i], bucket[i-0x10]  );    }
	
	return bucket[i];
}

void CSScan(uint3 DTid, uint GI, float2 x)
{
	bucket[GI] = x;
	uint lane = GI & 31u;
	uint warp = GI >> 5u;
 
	x = CSWarpScan(lane, GI);
	GroupMemoryBarrierWithGroupSync();

	if (lane == 31) bucket[warp] = x;	
	GroupMemoryBarrierWithGroupSync(); // uncomment if fail (works on fermi, breaks on kepler)

	if (warp == 0)	CSWarpScan(lane, lane);
	GroupMemoryBarrierWithGroupSync();
	
	if (warp > 0)   // uncomment if fail	
	   x = minMax(x, bucket[warp-1]);
	

	g_outputScanPassUAV[DTid.x] = x;
}

#define SHADOW_RES_SQ SHADOW_MAP_RES * SHADOW_MAP_RES
/////////////////////////////////////////////////////////////////
// init and scan bucket pass

//[numthreads(BUCKET_SIZE, 1, 1)]
//void ScanBucketCS(uint3 DTid: SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex, uint3 Gid : SV_GroupID)
[numthreads(BUCKET_SIZE, 1, 1)]
void ScanBucketCS(uint GI : SV_GroupIndex, uint3 Gid : SV_GroupID, uint3 DTid: SV_DispatchThreadID)
{	
	
	
	const uint linearID = (Gid.x + Gid.y*DISPATCH_THREADS_X)*BUCKET_SIZE + GI;
	//const uint linearID = DTid.x + DTid.y * BUCKET_SIZE * DISPATCH_THREADS_X;
	// whats wrong with min? where do we get a zero?

	float2 initMinMax = float2(1,0);

	if(linearID <= g_numTexel)
	{
		int x = linearID%SHADOW_MAP_RES;
		int y = linearID/SHADOW_MAP_RES;
		if (x < SHADOW_MAP_RES && y < SHADOW_MAP_RES) {
			//float depth = g_txShadow[int3(linearID%SHADOW_MAP_RES, linearID/SHADOW_MAP_RES, 0)].r;			
			float depth = g_txShadow[int2(linearID%SHADOW_MAP_RES, linearID/SHADOW_MAP_RES)].r;			
				
			if(depth < 1.0)
			{
				float linearDepth = g_mProjection._34 / (depth - g_mProjection._33);
				linearDepth = saturate((linearDepth - g_nearClip) / (g_farClip - g_nearClip));	

				initMinMax = linearDepth;
				//if(depth<=0)
				//	initMinMax = float2(1,0);
			}
		}
	}
	GroupMemoryBarrierWithGroupSync();
	//initMinMax = float2(0.5, 1);
	CSScan(linearID, GI, initMinMax);	
}

// record and scan the minmax of each bucket
[numthreads(BUCKET_BLOCK_SIZE, 1, 1)]
void ScanBucketResultsCS(uint3 DTid: SV_DispatchThreadID, uint GI : SV_GroupIndex)
{		
	//float2 x = g_scanInput[(DTid.x+1) * BUCKET_SIZE -1];
	float2 x = g_scanInput[(DTid.x+1) * BUCKET_SIZE-1];

	CSScan(DTid, GI, x);
}

// record and scan minmax of each bucket block
[numthreads(NUM_BUCKET_BLOCKS, 1, 1)]
void ScanBucketBlockResultsCS(uint3 DTid: SV_DispatchThreadID, uint GI : SV_GroupIndex)
{	
	//float2 x = g_scanInput[GI * BUCKET_BLOCK_SIZE -1];
	float2 x = g_scanInput[GI * BUCKET_BLOCK_SIZE-1];

	CSScan(DTid, GI, x);
}

[numthreads(1, 1, 1)]
void WriteDepthReductionResultCS(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
	g_depthMinMaxResultUAV[0] = g_scanInput[(g_numTexel)/(BUCKET_BLOCK_SIZE*BUCKET_BLOCK_SIZE)];//2(1,0);//g_scanInput[DTid.x/BUCKET_BLOCK_SIZE];

}

//groupshared float2 reductionBucket[WORK_GROUP_SIZE_ATOMIC_REDUCTION_X * WORK_GROUP_SIZE_ATOMIC_REDUCTION_Y];

float2 localMinMax(float2 a, float2 b)
{
	float2 ret;
	ret.x = min(a.x, b.x);
	ret.y = max(a.y, b.y);
	return ret;
}

//float2 BucketReduction(uint lane, uint i) {
//	uint4 access = i - uint4(1,2,4,8);
//	if(lane >  0){ reductionBucket[i-1]  = localMinMax(  reductionBucket[i], reductionBucket[i-1]); } 	
//	if(lane >  1){ reductionBucket[i-2]  = localMinMax(  reductionBucket[i], reductionBucket[i-2]); }	
//	if(lane >  3){ reductionBucket[i-4]  = localMinMax(  reductionBucket[i], reductionBucket[i-4]); }	
//	if(lane >  7){ reductionBucket[i-8]  = localMinMax(  reductionBucket[i], reductionBucket[i-8]); }	
//	if(lane > 15){ reductionBucket[i-16]  = localMinMax( reductionBucket[i], reductionBucket[i-16]);}
//	return bucket[i];
//}


[numthreads(WORK_GROUP_SIZE_ATOMIC_REDUCTION_X,WORK_GROUP_SIZE_ATOMIC_REDUCTION_Y,1)]
void ReductionAtomic(uint3 DTid: SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex, uint3 Gid : SV_GroupID) 
{

	float2 bucketInit = float2(1,0);

	#ifdef WITH_MSAA
        uint2 textureSize;
        uint numSamples;
        g_txShadow.GetDimensions(textureSize.x, textureSize.y, numSamples);

		for(uint sIdx = 0; sIdx < numSamples; ++sIdx)
		{
			uint2 samplePos = min(DTid.xy, textureSize-1);
			float depth = g_txShadow.Load(samplePos,sIdx);
		
			if ( depth < 1.0) 
			{		
				float linearDepth = g_mProjection._34 / (depth - g_mProjection._33);
				linearDepth = saturate((linearDepth - g_nearClip) / (g_farClip - g_nearClip));
				bucketInit.x = min(bucketInit.x, linearDepth);
				bucketInit.y = max(bucketInit.x, linearDepth);
			}
		}

    #else
        uint2 textureSize;
        g_txShadow.GetDimensions(textureSize.x, textureSize.y);
		uint2 samplePos = min(DTid.xy, textureSize-1);
		float depth = g_txShadow[samplePos].r;

		if ( depth < 1.0) 
		{		
			float linearDepth = g_mProjection._34 / (depth - g_mProjection._33);
			linearDepth = saturate((linearDepth - g_nearClip) / (g_farClip - g_nearClip));
			bucketInit = float2(linearDepth, linearDepth);
		}


    #endif
	
	
	//if (depth >= 0.0 && depth < 1.0) {

	InterlockedMin(g_depthMinMaxReductionResultUAV[0], asuint(bucketInit.x));
	InterlockedMax(g_depthMinMaxReductionResultUAV[1], asuint(bucketInit.y));



	// Debug
//	if (GI == 0) {
//		g_depthMinMaxReductionResultUAV[0] = asuint(0.0); // Min
//		g_depthMinMaxReductionResultUAV[1] = asuint(0.5); // Max
//	}
}




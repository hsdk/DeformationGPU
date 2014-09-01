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

#define ALLOC_IN_SCAN

#ifndef SCAN_ON_UINT
#define SCAN_ON_UINT
#endif

//#define SCAN_ON_UINT			// compacted alloc; dont write compacted deallocs, dont write compacted visible
//#define SCAN_ON_UINT2
//#define SCAN_ON_UINT3
//#define SCAN_ON_UINT4

#ifdef SCAN_ON_UINT
#define SCAN_ELEM uint
#endif

#ifdef SCAN_ON_UINT2
#define SCAN_ELEM uint2
#endif

#ifdef SCAN_ON_UINT3
#define SCAN_ELEM uint3
#endif

#ifdef SCAN_ON_UINT4
#define SCAN_ELEM uint4
#endif

///////////////////////////////////////////////////
#define ALLOCATOR_BLOCKSIZE 128
#define BLOCKSIZE_SCAN 512

#ifndef BUCKET_SIZE
#define BUCKET_SIZE BLOCKSIZE_SCAN
#endif

#ifndef BUCKET_BLOCK_SIZE
#define BUCKET_BLOCK_SIZE BLOCKSIZE_SCAN
#endif

#ifndef NUM_BUCKET_BLOCKS
#define NUM_BUCKET_BLOCKS BLOCKSIZE_SCAN
#endif

#define DISPATCH_THREADS_X 128

//g_targetTileInfo[i*6+0] = g_FreeMemoryTable[idxFreeMemStart*3+0];
//g_targetTileInfo[i*6+2] = g_FreeMemoryTable[idxFreeMemStart*3+1];
//g_targetTileInfo[i*6+3] = g_FreeMemoryTable[idxFreeMemStart*3+2];


//#ifdef ALLOC_IN_SCAN
//Buffer<uint>				g_memoryTableScanSRV		: register(t4);
//RWBuffer<int>				g_descriptorBufferScanUAV	: register(u3);
//#endif

cbuffer ManageTilesCB : register( b11 ) 
{    
	uint g_NumTiles;
};

Buffer<uint>			g_tileVisiblity	 : register(t0);  // result of intersection with brush pass, 1: for ptex face id was intersected
Buffer<uint>			g_tileDescriptors: register(t1);  //
Buffer<uint>			g_memoryTable	 : register(t1);  // free memory table

Buffer<SCAN_ELEM>		g_scanInput				     : register(t2);		// is output pass1, input pass2, was visibleAllocDeallocs
Buffer<SCAN_ELEM>		g_scanResultBucketsPass1     : register(t2);		// is output pass1, input pass2, was visibleAllocDeallocs
Buffer<SCAN_ELEM>		g_scanResultBucketsDownSweep : register(t3);		// is output pass1, input pass2, was visibleAllocDeallocsIncr

Buffer<uint>			g_memoryState	 : register(t4);  // memory table state SRV for alloc, kernel


RWBuffer<SCAN_ELEM>	g_outputScanPassUAV		: register(u0);			// 	
RWBuffer<uint>		g_tileDescriptorsUAV	: register(u1);			// per tile descriptors, write mem locs on allocate
RWBuffer<uint>		g_memoryStateUAV		: register(u2);			// 
RWBuffer<uint4>		g_dispatchIndirectUAV	: register(u3);			// 	
RWBuffer<uint>		g_compactedAllocateUAV	: register(u4);			// 

//! dispatch indirect entries
// 0 dispatch alloc
// 1 dispatch dealloc
// 2 dispatch sampling  // not used for osd



// scan entrys: 
// x: alloc   (visible but not allocated)
// y: dealloc (time is up or brush paint dealloc) 
// z: visible (no matter if allocated or not)		

groupshared SCAN_ELEM bucket[BUCKET_SIZE];


int IsNotAllocated(uint tileID)
{
	if(g_tileDescriptors[tileID*4+0] == 0xffff)		// we use page id to describe allocated or not, 0xffff means not allocated
		return 1;
	else
		return 0;
}

int IsTileVisible(uint tileID)
{
	if(g_tileVisiblity[tileID] == 1)		// we use page id to describe allocated or not
		return 1;
	else
		return 0;
}

void AllocTileMem(in uint tileID, in uint memTableLoc)
{
	g_tileDescriptorsUAV[tileID*4+0] = g_memoryTable[memTableLoc*3+0];		// page id
	g_tileDescriptorsUAV[tileID*4+1] = g_memoryTable[memTableLoc*3+1];		// start offset u
	g_tileDescriptorsUAV[tileID*4+2] = g_memoryTable[memTableLoc*3+2];		// start offset v
}

SCAN_ELEM getIncrValue(uint i) 
{
	SCAN_ELEM x = g_scanResultBucketsPass1[i];
	x += g_scanResultBucketsDownSweep[i/BLOCKSIZE_SCAN-1];
	return x;
}


uint getNumWorkGroups(uint numThreads, uint workGroupSize) 
{
	return numThreads/workGroupSize + 1;
	//return (numThreads + workGroupSize - 1) / workGroupSize;
}

SCAN_ELEM CSWarpScan(uint lane, uint i)
{
	uint4 access = i - uint4(1,2,4,8);
	if(lane >  0){ bucket[i]  += bucket[access.x]; } 	
	if(lane >  1){ bucket[i]  += bucket[access.y]; }	
	if(lane >  3){ bucket[i]  += bucket[access.z]; }	
	if(lane >  7){ bucket[i]  += bucket[access.w]; }	
	if(lane > 15){ bucket[i] += bucket[i-0x10];    }
	
	return bucket[i];
}

void CSScan(uint3 DTid, uint GI, SCAN_ELEM x)
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
	
	//if (warp > 0)   // uncomment if fail
	  x += bucket[warp-1];

	g_outputScanPassUAV[DTid.x] = x;
}


/////////////////////////////////////////////////////////////////
[numthreads(BUCKET_SIZE, 1, 1)]
void ScanBucketCS(uint3 DTid: SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex, uint3 Gid : SV_GroupID)
{
	const uint tileID = (Gid.x + Gid.y*DISPATCH_THREADS_X)*BUCKET_SIZE + GI;
	
	//int visible = g_manageTasks[idx]; 

	int visible		= IsTileVisible(tileID);
	int needsAlloc	= IsNotAllocated(tileID);
	//int needsAlloc = 1;

	// check if in range
	uint m = tileID < g_NumTiles ? 1: 0; // checkme reverse 1:0 to 0:1
	
	// create scan input, no multiscan variant
	
	#ifdef SCAN_ON_UINT
		SCAN_ELEM x = (visible*needsAlloc*m);
	#endif

	#ifdef SCAN_ON_UINT2
		SCAN_ELEM x = SCAN_ELEM(visible*needsAlloc*m, 0);
	#endif

	#ifdef SCAN_ON_UINT3
		SCAN_ELEM x = SCAN_ELEM(visible*needsAlloc*m, 0,0);
	#endif

	#ifdef SCAN_ON_UINT4
		SCAN_ELEM x = SCAN_ELEM(visible*needsAlloc*m, 0,0,0);
	#endif
	
	
	CSScan(tileID, GI, x);	
}

// record and scan the sum of each bucket
[numthreads(BUCKET_BLOCK_SIZE, 1, 1)]
void ScanBucketResultsCS(uint3 DTid: SV_DispatchThreadID, uint GI : SV_GroupIndex)
{		
	SCAN_ELEM x = g_scanInput[(DTid.x+1) * BUCKET_SIZE -1];
	CSScan(DTid, GI, x);
}

// record and scan the sum of each bucket block
[numthreads(NUM_BUCKET_BLOCKS, 1, 1)]
void ScanBucketBlockResultsCS(uint3 DTid: SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
	SCAN_ELEM x = g_scanInput[GI * BUCKET_BLOCK_SIZE -1];
	CSScan(DTid, GI, x);
}

// add the bucket block scanned result to each bucket block to get the buck block results
[numthreads(BUCKET_BLOCK_SIZE, 1, 1)]
void ScanApplyBucketBlockResultsCS(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
	g_outputScanPassUAV[DTid.x] += g_scanInput[DTid.x/BUCKET_BLOCK_SIZE];
}

// change code to support resizing, 
// for now use stack with max loc == initially filled stack 
#define MEMSTATE_CUR_LOC_DISPLACEMENT	0
#define MEMSTATE_MAX_LOC_DISPLACEMENT	1
#define MEMSTATE_CUR_LOC_COLOR			2
#define MEMSTATE_MAX_LOC_COLOR			3

uint MemAvailableRW()
{
	#ifdef DISPLACEMENT_MODE
		return  g_memoryStateUAV[MEMSTATE_CUR_LOC_DISPLACEMENT].x;
	#else
		return  g_memoryStateUAV[MEMSTATE_CUR_LOC_COLOR].x;	
	#endif
}

uint MemAvailableR()
{
	#ifdef DISPLACEMENT_MODE
		return  g_memoryState[MEMSTATE_CUR_LOC_DISPLACEMENT].x;
	#else
		return  g_memoryState[MEMSTATE_CUR_LOC_COLOR].x;	
	#endif
}



void UpdateMemState(uint offset)
{
	#ifdef DISPLACEMENT_MODE
		g_memoryStateUAV[MEMSTATE_CUR_LOC_DISPLACEMENT].x -= offset;	
	#else
		g_memoryStateUAV[MEMSTATE_CUR_LOC_COLOR].x -= offset;	
	#endif
}

uint AtomicAlloc()
{
	int loc = 0;	
	#ifdef DISPLACEMENT_MODE
		InterlockedAdd(g_memoryStateUAV[MEMSTATE_CUR_LOC_DISPLACEMENT], -1, loc);		
	#else		
		InterlockedAdd(g_memoryStateUAV[MEMSTATE_CUR_LOC_COLOR], -1, loc);
	#endif
		return loc;
}

// add the bucket block scanned result to each bucket to get the final result
[numthreads(BUCKET_SIZE, 1, 1)]
void ScanApplyBucketResultsCS(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex, uint3 Gid : SV_GroupID)
{
	int tileID = (Gid.x + Gid.y*DISPATCH_THREADS_X)*BUCKET_SIZE + GI;
	
	// enable for debuging
	g_outputScanPassUAV[tileID] = getIncrValue(tileID-1);			//g_outputScanPassUAV[idx] += input[((idx)/BUCKET_SIZE)-1];

	if(tileID >= g_NumTiles)		return;

	
	SCAN_ELEM t =  getIncrValue(tileID - 1);
	SCAN_ELEM s = getIncrValue(tileID);

	SCAN_ELEM delta = s - t;

#ifdef ALLOC_IN_SCAN
	// t: offset in stack
	// elem id idx
	if(delta.x == 1)		
	{		
		//uint memLoc = g_memoryTable[MemAvailableRW()-t.x];
		uint memLoc =  AtomicAlloc();
		AllocTileMem(tileID, memLoc); 
	}

	if(tileID == 0)
	{
		#define DISPATCH_ALLOC    0
		g_dispatchIndirectUAV[DISPATCH_ALLOC]	= uint4( 0, 0, 0, 1);	// allocs dispatches
	}

#else
	if(delta.x == 1)		
	{		
		g_compactedAllocateUAV[t.x] = tileID; 
	}

	if(tileID == 0)
	{
		SCAN_ELEM prefixSum = getIncrValue(int(g_NumTiles)-1);
		uint numAllocs = prefixSum.x;
		uint numDeallocs = 0;

		#ifdef SCAN_ON_UINT2
			numDeallocs = prefixSum.y;
		#endif


		int progress = 1;
		if (MemAvailableRW() < ( numAllocs - numDeallocs ) )
			progress = 0; // Not enough memory to proceed - do nothing but deallocation

		UpdateMemState(numAllocs*progress - numDeallocs);		
				
		#define DISPATCH_ALLOC    0
		g_dispatchIndirectUAV[DISPATCH_ALLOC]	= uint4( progress * getNumWorkGroups(numAllocs, ALLOCATOR_BLOCKSIZE), 1,1,1);	// allocs dispatches
		//g_dispatchIndirectUAV[DISPATCH_ALLOC]	= uint4( progress * numAllocs, 1,1,1);	// allocs dispatches
	}

#endif
	
}




Buffer<uint>				g_compactedAllocateSRV	: register(t1);

[numthreads(ALLOCATOR_BLOCKSIZE, 1, 1)]
void AllocateCS(uint3 DTid: SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
	if(DTid.x >= g_NumTiles)		return;

	uint tileID = g_compactedAllocateSRV[DTid.x]; // location in descriptor buffer

	uint stackLoc = g_memoryTable[MemAvailableR()-DTid.x];
	//AllocTileMemR(tileID, memLoc); 
}
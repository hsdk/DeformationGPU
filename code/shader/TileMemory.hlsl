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

#define ALLOCATOR_BLOCKSIZE 32

cbuffer ManageTilesCB : register(b11)
{
	uint g_NumTiles;
};

Buffer<uint>		g_tileVisiblity			: register(t0);  // result of intersection with brush pass, 1: for ptex face id was intersected
Buffer<uint>		g_memoryTable			: register(t1);  // free memory table

RWBuffer<uint>		g_tileDescriptorsUAV	: register(u0);			// per tile descriptors, write mem locs on allocate
RWBuffer<uint>		g_memoryStateUAV		: register(u1);			// 


int IsNotAllocated(uint tileID)
{
	if (g_tileDescriptorsUAV[tileID * 4 + 0] == 0xffff)		// we use page id to describe allocated or not, 0xffff means not allocated
		return 1;
	else
		return 0;
}

int IsTileVisible(uint tileID)
{
	if (g_tileVisiblity[tileID] == 1)		// we use page id to describe allocated or not
		return 1;
	else
		return 0;
}

void AllocTileMem(in uint tileID, in uint memTableLoc)
{
	g_tileDescriptorsUAV[tileID * 4 + 0] = g_memoryTable[memTableLoc * 3 + 0];		// page id
	g_tileDescriptorsUAV[tileID * 4 + 1] = g_memoryTable[memTableLoc * 3 + 1];		// start offset u
	g_tileDescriptorsUAV[tileID * 4 + 2] = g_memoryTable[memTableLoc * 3 + 2];		// start offset v
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

//uint MemAvailableR()
//{
//#ifdef DISPLACEMENT_MODE
//	return  g_memoryState[MEMSTATE_CUR_LOC_DISPLACEMENT].x;
//#else
//	return  g_memoryState[MEMSTATE_CUR_LOC_COLOR].x;
//#endif
//}



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




Buffer<uint>				g_compactedAllocateSRV	: register(t1);

[numthreads(ALLOCATOR_BLOCKSIZE, 1, 1)]
void AllocateTilesCS(uint3 DTid: SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
	uint tileID = DTid.x;
	if (tileID >= g_NumTiles)		return;
			
	if (IsTileVisible(tileID))
	{
		if (IsNotAllocated(tileID))
		{
			uint memLoc = AtomicAlloc();
			AllocTileMem(tileID, memLoc);
		}
	}
}
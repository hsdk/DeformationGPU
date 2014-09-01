
cbuffer cbCBMemoryManageTask : register( b12 ) {
	uint g_numAllocs;			// number of allocations
	uint g_numDeallocs;			// number of deallocations
	uint2 paddingmemtask;
}


struct FreeMemoryTableState
{
	uint curLocTileDisplacement;	// free memory table pointer
	uint maxLocTileDisplacement;	// max value of table pointer
	
	uint curLocTileColor;			// free memory table pointer
	uint maxLocTileColor;			// max value of table pointer	

	uint curLocParticles;			// free memory table pointer
	uint maxLocParticles;			// max value of table pointer 
};


#define MEMSTATE_CUR_LOC_DISPLACEMENT	0
#define MEMSTATE_MAX_LOC_DISPLACEMENT	1
#define MEMSTATE_CUR_LOC_COLOR			2
#define MEMSTATE_MAX_LOC_COLOR			3


Buffer<uint>	g_FreeMemoryTable: register(t0);
RWBuffer<uint>	g_targetTileInfo: register(u0);
//RWStructuredBuffer<FreeMemoryTableState> g_FreeMemoryTableStateUAV	: register ( u1 );
RWBuffer<uint>  g_memoryStateUAV	: register ( u1 );

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

[numthreads(64, 1, 1)]
void AllocTilesCS(	uint3 blockIdx : SV_GroupID, 
					uint3 DTid : SV_DispatchThreadID, 
					uint3 threadIdx : SV_GroupThreadID,
					uint GI : SV_GroupIndex ) 
{	
	uint i = DTid.x;
	if(DTid.x >= g_numAllocs) return;

	//uint idxFreeMemStart = i;
	//#ifdef DISPLACEMENT_TILES
	//	idxFreeMemStart += g_FreeMemoryTableStateUAV[0].x;
	//#else
	//	idxFreeMemStart += g_FreeMemoryTableStateUAV[2].x;
	//#endif

	uint memLoc = AtomicAlloc();
	
	g_targetTileInfo[i*6+0] = g_FreeMemoryTable[memLoc*3+0];
	g_targetTileInfo[i*6+2] = g_FreeMemoryTable[memLoc*3+1];
	g_targetTileInfo[i*6+3] = g_FreeMemoryTable[memLoc*3+2];

	//g_targetTileInfo[i*6+0] = g_FreeMemoryTable[i*3+0];
	//g_targetTileInfo[i*6+2] = g_FreeMemoryTable[i*3+1];
	//g_targetTileInfo[i*6+3] = g_FreeMemoryTable[i*3+2];

	// TODO, we cannot be sure that last thread block is executed last
	// update state buffer somewhere else
//	if(DTid.x == g_numAllocs-1)	// update memory table pointer
//	{
//#ifdef DISPLACEMENT_TILES
//		g_FreeMemoryTableStateUAV[0].x += g_numAllocs;	
//#else
//		g_FreeMemoryTableStateUAV[2].x += g_numAllocs;	
//#endif
//	}

}


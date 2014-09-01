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

#pragma once

#include <DXUT.h>
#include <SDX/DXShaderManager.h>
#include <SDX/DXBuffer.h>

// fwd decls
class ModelInstance;

struct ParticleTableEntry
{
	int loc;
};

// PTEX/Tile freeMemory location entry
// these entrys are precomputed on init
// on alloc one such entry is written to a meshs tile descriptor
// on dealloc the this entry is written back from a meshs tile descriptor
struct TileTableEntry
{
	uint16_t page;		// which texture array slice
	uint16_t u_offset;	// start of tile data in tile texture in u dir
	uint16_t v_offset;	// start of tile data in tile texture in v dir 
};


// we grow free memory table location pointer from 0
// this will allow easy growing of memory tables (init new table buffer -> copy resource -> switch table)
struct FreeMemoryTableState
{
	UINT curLocTileDisplacement;	// free memory table pointer
	UINT maxLocTileDisplacement;	// max value of table pointer
	
	UINT curLocTileColor;			// free memory table pointer
	UINT maxLocTileColor;			// max value of table pointer	

	UINT curLocParticles;			// free memory table pointer
	UINT maxLocParticles;			// max value of table pointer 	
};

struct CBMemoryManageTask
{
	UINT numAllocs;					// number of allocations
	UINT numDeallocs;				// number of deallocations
	DirectX::XMUINT2 padding;
};

class MemoryManager {
public:
	MemoryManager();
	~MemoryManager();

	HRESULT Create(ID3D11Device1* pd3dDevice);
	void Destroy();
	
	HRESULT InitTileDisplacementMemory( ID3D11Device1* pd3dDevice, UINT numTiles, int tileSize, int nMipMaps, float defaultDispl = 0.1f, bool withOverlap = false, bool useHalfFloat = false, bool createConstraintsMemory = false);
	HRESULT InitTileColorMemory( ID3D11Device1* pd3dDevice, UINT numTiles, int tileSize, int nMipMaps, const DirectX::XMFLOAT3A& defaultColor = DirectX::XMFLOAT3A(0,0,0),bool withOverlap = false);
	HRESULT InitParticleMemory(  ID3D11Device1* pd3dDevice, UINT numParticles, UINT particleByteSize);

	ID3D11ShaderResourceView*  GetDisplacementDataSRV()	{ return m_dataTileDisplacementSRV; }
	ID3D11UnorderedAccessView* GetDisplacementDataUAV()	{ return m_dataTileDisplacementUAV; }
	ID3D11ShaderResourceView*  GetDisplacementConstraintsSRV()	{ return m_dataTileDisplacementConstraintsSRV; }
	ID3D11UnorderedAccessView* GetDisplacementConstraintsUAV()	{ return m_dataTileDisplacementConstraintsUAV; }

	ID3D11ShaderResourceView*  GetColorDataSRV()		{ return m_dataTileColorSRV; }
	ID3D11UnorderedAccessView* GetColorDataUAV()		{ return m_dataTileColorUAV; }
	ID3D11ShaderResourceView*  GetParticleDataSRV()		{ return m_dataParticlesSRV; }
	ID3D11UnorderedAccessView* GetParticleDataUAV()		{ return m_dataParticlesUAV; }

	// debug preallocate for all tiles, copy count data locs from mem tables to uav
	HRESULT PreallocateDisplacementTiles( ID3D11DeviceContext1* pd3dImmediateContext, UINT count, ID3D11UnorderedAccessView* targetTileLayoutUAV);
	HRESULT PreallocateColorTiles( ID3D11DeviceContext1* pd3dImmediateContext, UINT count, ID3D11UnorderedAccessView* targetTileLayoutUAV);

	
	HRESULT ManageColorTiles( ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance);
	HRESULT ManageDisplacementTiles( ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance);

	HRESULT Scan(ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance, bool paintMode);
	HRESULT Alloc(ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance);
	
	HRESULT UpdateMemTableStates(UINT newMaxNumDiplacementTiles =0u, UINT newMaxNumColorTiles=0u, UINT newMaxNumParticles=0u);

	HRESULT	PrintTableState();
	HRESULT PrintTileInfo( ID3D11DeviceContext1* pd3dImmediateContext, UINT numTiles, ID3D11Buffer* tileInfoBUF) const;

	// create tile info buffer for meshes
	HRESULT CreateLocalTileInfo(ID3D11Device1* pd3dDevice, UINT numTiles, UINT tileSize, ID3D11Buffer*& tileInfoBUF, ID3D11ShaderResourceView*& tileInfoSRV, ID3D11UnorderedAccessView*& tileInfoUAV, UINT numMipMaps = 0u);

	// visibility buffer for multires attributes
	//HRESULT CreateVisibilityBufferMA(ID3D11Device1* pd3dDevice, UINT numTiles, ID3D11Buffer*& visibilityBUF, ID3D11ShaderResourceView*& visibilitySRV, ID3D11UnorderedAccessView*& visibilityUAV);
protected:
	void	UpdateTileCB( ID3D11DeviceContext1* pd3dImmediateContext, UINT numPatches);
	HRESULT ScanInternalOSD(ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance, bool paintMode);
	HRESULT AllocInternal(ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance);

	HRESULT CreateTileMemoryTable(ID3D11Device1* pd3dDevice, ID3D11Buffer*& layoutBUF, ID3D11ShaderResourceView*& layoutSRV, ID3D11UnorderedAccessView*& layoutUAV,
								  UINT numPages, UINT numTilesX, UINT numTilesY, UINT tileWidth, UINT tileHeight, bool withOverlap);

	ID3D11Buffer				*m_memTableStateStagingBUF;
	ID3D11Buffer				*m_memTableStateBUF;
	ID3D11ShaderResourceView	*m_memTableStateSRV;
	ID3D11UnorderedAccessView	*m_memTableStateUAV;

	ID3D11Buffer				*m_cbMemManageTask;					// task constant buffer
	ID3D11Buffer				*m_memManageTaskBUF;				// task buffer writable from gpu
	ID3D11ShaderResourceView	*m_memManageTaskSRV;
	ID3D11UnorderedAccessView	*m_memManageTaskUAV;

	// numTiles cb
	ID3D11Buffer				*m_tilesInfoCB;

	/////////////////////////////////////////////////////////
	// displacement tile data and management
	ID3D11Texture2D				*m_dataTileDisplacementTEX;			// per face tile data, texture 2D arrays
	ID3D11ShaderResourceView	*m_dataTileDisplacementSRV;			// per face tile data SRV
	ID3D11UnorderedAccessView	*m_dataTileDisplacementUAV;			// per face tile data UAV

	ID3D11Texture2D				*m_dataTileDisplacementConstraintsTEX; // tile displacement constraints texture 2d array
	ID3D11ShaderResourceView	*m_dataTileDisplacementConstraintsSRV; // tile displacement constraints SRV
	ID3D11UnorderedAccessView	*m_dataTileDisplacementConstraintsUAV; // tile displacement constraints UAV


	ID3D11Buffer				*m_memoryTableTileDisplacementBUF;
	ID3D11ShaderResourceView	*m_memoryTableTileDisplacementSRV;
	ID3D11UnorderedAccessView	*m_memoryTableTileDisplacementUAV;

	Shader<ID3D11ComputeShader> *m_preallocDisplacementTilesCS;

	UINT						 m_maxNumDisplacementTiles;			// max number of displacement tiles in managed buffer
	UINT						 m_displacementTileSize;			// tile width, height
	UINT						 m_displacementTileNumMipmaps;		// number of mipmaps per tile
	bool						 m_displacementTileWithOverlap;		// tiles and mipmaps have overlap pixels


	/////////////////////////////////////////////////////////
	// color tile data and management
	ID3D11Texture2D				*m_dataTileColorTEX;				// per face tile data, texture 2D arrays
	ID3D11ShaderResourceView	*m_dataTileColorSRV;				// per face tile data SRV
	ID3D11UnorderedAccessView	*m_dataTileColorUAV;				// per face tile data UAV

	ID3D11Buffer				*m_memoryTableTileColorBUF;
	ID3D11ShaderResourceView	*m_memoryTableTileColorSRV;
	ID3D11UnorderedAccessView	*m_memoryTableTileColorUAV;

	Shader<ID3D11ComputeShader> *m_preallocColorTilesCS;


	Shader<ID3D11ComputeShader> *m_scanOSDBucketCS					;
	Shader<ID3D11ComputeShader> *m_scanOSDBucketResultsCS			;
	Shader<ID3D11ComputeShader> *m_scanOSDBucketBlockResultsCS		;
	Shader<ID3D11ComputeShader> *m_scanOSDApplyBucketBlockResultsCS ; 
	Shader<ID3D11ComputeShader> *m_scanOSDApplyBucketResultsColorCS	;
	Shader<ID3D11ComputeShader> *m_scanOSDApplyBucketResultsDisplacementCS	;
	Shader<ID3D11ComputeShader> *m_allocOSDCS						;
	Shader<ID3D11ComputeShader> *m_deallocOSDCS						;

	Shader<ID3D11ComputeShader> *m_allocTilesCS;
	
	UINT						 m_maxNumColorTiles;
	UINT						 m_colorTileSize;					// tile width, height
	UINT						 m_colorTileNumMipmaps;				// number of mipmaps per tile
	bool						 m_colorTileWithOverlap;			// tiles and mipmaps have overlap pixels

	UINT						 m_scanBucketSize;
	UINT						 m_scanBucketBlockSize;
	UINT						 m_numScanBuckets;	
	UINT						 m_numScanBucketBlocks;


	/////////////////////////////////////////////////////////
	// Particle Management
	ID3D11Buffer				*m_dataParticlesBUF;				
	ID3D11ShaderResourceView	*m_dataParticlesSRV;				
	ID3D11UnorderedAccessView	*m_dataParticlesUAV;				
	
	// free memory location
	ID3D11Buffer				*m_memoryTableParticlesBUF;
	ID3D11ShaderResourceView	*m_memoryTableParticlesSRV;
	ID3D11UnorderedAccessView	*m_memoryTableParticlesUAV;
	
	Shader<ID3D11ComputeShader> *m_allocParticlesCS;
	
	UINT						 m_maxNumParticles;
	UINT						 m_particleEntryByteSize;			// byte size of one particle in data buffer


	// scan 
	ID3D11Buffer				*m_scanBucketsBUF;				
	ID3D11ShaderResourceView	*m_scanBucketsSRV;
	ID3D11UnorderedAccessView	*m_scanBucketsUAV;

	ID3D11Buffer				*m_scanBucketResultsBUF;
	ID3D11ShaderResourceView	*m_scanBucketResultsSRV;
	ID3D11UnorderedAccessView	*m_scanBucketResultsUAV;

	ID3D11Buffer				*m_scanBucketBlockResultsBUF;	 // TODO create buffer and srv/uav
	ID3D11ShaderResourceView	*m_scanBucketBlockResultsSRV;
	ID3D11UnorderedAccessView	*m_scanBucketBlockResultsUAV;

	// for debugging
	ID3D11Buffer				*m_scanResultBUF;
	ID3D11ShaderResourceView	*m_scanResultSRV;
	ID3D11UnorderedAccessView	*m_scanResultUAV;


	ID3D11Buffer				*m_compactedAllocateBUF;
	ID3D11ShaderResourceView	*m_compactedAllocateSRV;
	ID3D11UnorderedAccessView	*m_compactedAllocateUAV;

	ID3D11Buffer				*m_compactedDeallocateBUF;
	ID3D11ShaderResourceView	*m_compactedDeallocateSRV;
	ID3D11UnorderedAccessView	*m_compactedDeallocateUAV;

	ID3D11Buffer				*m_compactedActiveBUF;
	ID3D11ShaderResourceView	*m_compactedActiveSRV;
	ID3D11UnorderedAccessView	*m_compactedActiveUAV;


	// indirect dispatches	
	ID3D11Buffer				*m_dispatchIndirectBUF; 
	ID3D11Buffer				*m_dispatchIndirectBUFStaging; 
	ID3D11UnorderedAccessView	*m_dispatchIndirectUAV;


};

extern MemoryManager g_memoryManager;
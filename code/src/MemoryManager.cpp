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

#include "stdafx.h"
#include "MemoryManager.h"

#include "App.h"
#include "scene/ModelInstance.h"
#include "scene/DXSubDModel.h"
#include "utils/MathHelpers.h"

#include <DirectXPackedVector.h>

//Henry: has to be last header
#include "utils/DbgNew.h"

using namespace DirectX;

#define MEM_GTX780 3072
#define MEM_TITAN 6144

#define WORKGROUP_SIZE_SCAN 512 // todo test lower vals

MemoryManager g_memoryManager;

MemoryManager::MemoryManager()
{
	///////////////////////////////////////////////
	// global
	m_memTableStateBUF				 = NULL;
	m_memTableStateSRV				 = NULL;
	m_memTableStateUAV				 = NULL;
									 
	m_cbMemManageTask				 = NULL;
	m_memManageTaskBUF				 = NULL;
	m_memManageTaskSRV				 = NULL;
	m_memManageTaskUAV				 = NULL;
	
	// tile scan
	m_scanBucketSize = 0;
	m_scanBucketBlockSize = 0;
	m_numScanBuckets = 0;	
	m_numScanBucketBlocks = 0;

	///////////////////////////////////////////////
	// displacement
	m_dataTileDisplacementTEX		 = NULL;		
	m_dataTileDisplacementSRV		 = NULL;		
	m_dataTileDisplacementUAV		 = NULL;	

	m_dataTileDisplacementConstraintsTEX = NULL;
	m_dataTileDisplacementConstraintsSRV = NULL;
	m_dataTileDisplacementConstraintsUAV = NULL;

	m_memoryTableTileDisplacementBUF = NULL;
	m_memoryTableTileDisplacementSRV = NULL;
	m_memoryTableTileDisplacementUAV = NULL;	

	m_maxNumDisplacementTiles		 = 0;
	m_displacementTileSize			 = 16;
	m_displacementTileNumMipmaps	 = 0;
	m_displacementTileWithOverlap	 = true;

	m_preallocDisplacementTilesCS		 = NULL;


	///////////////////////////////////////////////
	// color
	m_dataTileColorTEX				 = NULL;				
	m_dataTileColorSRV				 = NULL;				
	m_dataTileColorUAV				 = NULL;				
									 
	m_memoryTableTileColorBUF		 = NULL;
	m_memoryTableTileColorSRV		 = NULL;
	m_memoryTableTileColorUAV		 = NULL;
									 
	m_preallocColorTilesCS				 = NULL;

	m_scanOSDBucketCS					= NULL;
	m_scanOSDBucketResultsCS			= NULL;
	m_scanOSDBucketBlockResultsCS		= NULL;
	m_scanOSDApplyBucketBlockResultsCS  = NULL;
	m_scanOSDApplyBucketResultsColorCS		= NULL;
	m_scanOSDApplyBucketResultsDisplacementCS= NULL;
	m_allocOSDCS						= NULL;
	m_deallocOSDCS						= NULL;
									 
	m_maxNumColorTiles				 = 0;
	m_colorTileSize					 = 128;
	m_colorTileNumMipmaps			 = 1;
	m_colorTileWithOverlap			 = true;	


	///////////////////////////////////////////////
	// particles	
	m_dataParticlesBUF				 = NULL;
	m_dataParticlesSRV				 = NULL;
	m_dataParticlesUAV				 = NULL;

	m_memoryTableParticlesBUF		 = NULL;
	m_memoryTableParticlesSRV		 = NULL;
	m_memoryTableParticlesUAV		 = NULL;

	m_allocParticlesCS				 = NULL;

	m_particleEntryByteSize			 = 0;						
	m_maxNumParticles				 = 0;


	// scan
	m_scanBucketsBUF = NULL;			
	m_scanBucketsSRV = NULL;
	m_scanBucketsUAV = NULL;

	m_scanBucketResultsBUF = NULL;
	m_scanBucketResultsSRV = NULL;
	m_scanBucketResultsUAV = NULL;

	m_scanBucketBlockResultsBUF	= NULL;
	m_scanBucketBlockResultsSRV	= NULL;
	m_scanBucketBlockResultsUAV	= NULL;

	m_scanResultBUF = NULL;
	m_scanResultSRV = NULL;
	m_scanResultUAV = NULL;

	m_compactedAllocateBUF = NULL;
	m_compactedAllocateSRV = NULL;
	m_compactedAllocateUAV = NULL;

	m_compactedDeallocateBUF = NULL;
	m_compactedDeallocateSRV = NULL;
	m_compactedDeallocateUAV = NULL;

	m_compactedActiveBUF = NULL;
	m_compactedActiveSRV = NULL;
	m_compactedActiveUAV = NULL;

	m_dispatchIndirectBUF = NULL; 
	m_dispatchIndirectBUFStaging = NULL; 
	m_dispatchIndirectUAV = NULL;

	m_tilesInfoCB = NULL;
}

MemoryManager::~MemoryManager()
{
	Destroy();
}

HRESULT MemoryManager::Create( ID3D11Device1* pd3dDevice )
{
	HRESULT hr;

	D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
	ZeroMemory(&descSRV, sizeof(descSRV));
	descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;

	D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
	ZeroMemory(&descUAV, sizeof(descUAV));
	descUAV.ViewDimension =  D3D11_UAV_DIMENSION_BUFFER;

	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CBMemoryManageTask),
							  D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, m_cbMemManageTask));

	
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_ManageTiles), D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, m_tilesInfoCB ));


	FreeMemoryTableState initMemState;
	initMemState.curLocParticles = 0;
	initMemState.curLocTileColor = 0;
	initMemState.curLocTileDisplacement = 0;
	initMemState.maxLocParticles = 0;
	initMemState.maxLocTileColor = 0;
	initMemState.maxLocTileDisplacement = 0;
	
	V_RETURN(DXCreateBuffer(pd3dDevice, 0, sizeof(FreeMemoryTableState), D3D11_CPU_ACCESS_READ,  D3D11_USAGE_STAGING, m_memTableStateStagingBUF, &initMemState.curLocTileDisplacement)); 

	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, sizeof(FreeMemoryTableState),  0, D3D11_USAGE_DEFAULT, m_memTableStateBUF, &initMemState.curLocTileDisplacement,
							  0, sizeof(FreeMemoryTableState)));



	descSRV.Format = DXGI_FORMAT_R32_UINT;		
	descSRV.Buffer.FirstElement = 0;	
	descSRV.Buffer.NumElements = 6;
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_memTableStateBUF, &descSRV, &m_memTableStateSRV));
	
	descUAV.Format = DXGI_FORMAT_R32_UINT;		
	descUAV.Buffer.FirstElement = 0;	
	descUAV.Buffer.NumElements = 6;
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_memTableStateBUF, &descUAV, &m_memTableStateUAV));
	
	// load shaders
	ID3DBlob* pBlob = NULL;
	D3D_SHADER_MACRO macro_displacement[]	= { {"DISPLACEMENT_MODE" , "1"}, { 0 } };
	D3D_SHADER_MACRO macro_color[]	= { {"COLOR_TILES" , "1"}, { 0 } };
	m_preallocDisplacementTilesCS = g_shaderManager.AddComputeShader(L"shader/TileMemoryManagerCS.hlsl", "AllocTilesCS", "cs_5_0", &pBlob, macro_displacement);
	m_preallocColorTilesCS = g_shaderManager.AddComputeShader(L"shader/TileMemoryManagerCS.hlsl", "AllocTilesCS", "cs_5_0", &pBlob,macro_color);


	/////////////////// SCAN STUFF ///////////////////////////

	m_scanBucketSize		= WORKGROUP_SIZE_SCAN;
	m_scanBucketBlockSize	= m_scanBucketSize;
	m_numScanBuckets		= WORKGROUP_SIZE_SCAN*WORKGROUP_SIZE_SCAN;
	m_numScanBucketBlocks	= WORKGROUP_SIZE_SCAN;

	UINT scanElemSize = sizeof(UINT);		
	DXGI_FORMAT formatScan = DXGI_FORMAT_R32_UINT;
	if(scanElemSize == 4)			formatScan = DXGI_FORMAT_R32_UINT;				// no multiscan
	//else if(scanElemSize == 12)	formatScan = DXGI_FORMAT_R32G32B32_UINT;		// for multiscan


	// SRVs
	descSRV.ViewDimension		= D3D11_SRV_DIMENSION_BUFFER;
	descSRV.Buffer.FirstElement = 0;
	descSRV.Format = formatScan;

	// UAVs	
	descUAV.ViewDimension		= D3D11_UAV_DIMENSION_BUFFER;
	descUAV.Buffer.FirstElement = 0;
	descUAV.Format				= formatScan;	

	// scan buffers, srvs, uavs
	UINT numElements  = g_app.g_memMaxNumTilesPerObject;
	descSRV.Buffer.NumElements = numElements;
	descUAV.Buffer.NumElements	= numElements;
	descUAV.Buffer.Flags = 0;
	UINT byteSize	  = numElements * scanElemSize;
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, byteSize, 0, D3D11_USAGE_DEFAULT, m_scanBucketsBUF)); 
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, byteSize, 0, D3D11_USAGE_DEFAULT, m_scanResultBUF)); 

	V_RETURN(pd3dDevice->CreateShaderResourceView(m_scanBucketsBUF,		&descSRV, &m_scanBucketsSRV));
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_scanResultBUF,		&descSRV, &m_scanResultSRV));		

	V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_scanBucketsBUF,	&descUAV, &m_scanBucketsUAV));
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_scanResultBUF,		&descUAV, &m_scanResultUAV));


	// aux buffer for bucket results
	numElements  = m_numScanBuckets;
	byteSize= numElements * scanElemSize;
	descSRV.Buffer.NumElements = numElements;		
	descUAV.Buffer.NumElements = numElements;		
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, byteSize, 0, D3D11_USAGE_DEFAULT, m_scanBucketResultsBUF)); 						
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_scanBucketResultsBUF,		&descSRV, &m_scanBucketResultsSRV));		
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_scanBucketResultsBUF,	&descUAV, &m_scanBucketResultsUAV));

	// aux buffer for bucket block results
	numElements  = m_numScanBucketBlocks;
	byteSize = numElements * scanElemSize;
	descSRV.Buffer.NumElements = numElements;		
	descUAV.Buffer.NumElements = numElements;
	descUAV.Buffer.Flags = 0;
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, byteSize, 0, D3D11_USAGE_DEFAULT, m_scanBucketBlockResultsBUF)); 						
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_scanBucketBlockResultsBUF,	&descSRV, &m_scanBucketBlockResultsSRV));		
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_scanBucketBlockResultsBUF,	&descUAV, &m_scanBucketBlockResultsUAV));


	// compacted buffers
	numElements = g_app.g_memMaxNumTilesPerObject;
	DXGI_FORMAT compactedBufferFormat = DXGI_FORMAT_R32_UINT;

	// compacted allocate,deallocate,active buffer
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, numElements*sizeof(UINT), 0, D3D11_USAGE_DEFAULT, m_compactedAllocateBUF));		
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, numElements*sizeof(UINT), 0, D3D11_USAGE_DEFAULT, m_compactedDeallocateBUF));
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, numElements*sizeof(UINT), 0, D3D11_USAGE_DEFAULT, m_compactedActiveBUF));

	descSRV.Format				= compactedBufferFormat;
	descSRV.Buffer.NumElements	= numElements;
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_compactedAllocateBUF,	 &descSRV, &m_compactedAllocateSRV));
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_compactedDeallocateBUF,  &descSRV, &m_compactedDeallocateSRV));
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_compactedActiveBUF,		 &descSRV, &m_compactedActiveSRV));

	descUAV.Format				= compactedBufferFormat;	
	descUAV.Buffer.NumElements	= numElements;
	descUAV.Buffer.Flags = 0;
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_compactedAllocateBUF,	 &descUAV, &m_compactedAllocateUAV));
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_compactedDeallocateBUF, &descUAV, &m_compactedDeallocateUAV));
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_compactedActiveBUF,	 &descUAV, &m_compactedActiveUAV));

	// DISPATCH INDIRECT BUFFERS
	// dispatch indirect, 3 uint4: 0 = alloc, 1 = dealloc, 2 = dowork (e.g. sampling/painting), use offset in buffer for starting indirect dispatches
	V_RETURN(DXCreateBuffer( DXUTGetD3D11Device(), D3D11_BIND_UNORDERED_ACCESS , 3 * sizeof(XMUINT4), 0, D3D11_USAGE_DEFAULT, m_dispatchIndirectBUF, NULL, D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS));	

	descUAV.Format				= DXGI_FORMAT_R32G32B32A32_UINT;	
	descUAV.Buffer.NumElements	= 3;
	descUAV.Buffer.Flags		= 0;
	V_RETURN(DXUTGetD3D11Device()->CreateUnorderedAccessView(m_dispatchIndirectBUF, &descUAV, &m_dispatchIndirectUAV));


	// SCAN SHADER
	m_scanOSDBucketCS					= g_shaderManager.AddComputeShader(L"shader/MemoryManagerOSD.hlsl", "ScanBucketCS",					"cs_5_0", &pBlob);	// scan pass 1
	m_scanOSDBucketResultsCS			= g_shaderManager.AddComputeShader(L"shader/MemoryManagerOSD.hlsl", "ScanBucketResultsCS",			"cs_5_0", &pBlob);	// scan pass 2
	m_scanOSDBucketBlockResultsCS		= g_shaderManager.AddComputeShader(L"shader/MemoryManagerOSD.hlsl", "ScanBucketBlockResultsCS",		"cs_5_0", &pBlob);	// scan pass 3
	m_scanOSDApplyBucketBlockResultsCS  = g_shaderManager.AddComputeShader(L"shader/MemoryManagerOSD.hlsl", "ScanApplyBucketBlockResultsCS","cs_5_0", &pBlob);	// scan pass 4


	D3D_SHADER_MACRO macro_displacement_mode[]	= { {"DISPLACEMENT_MODE" , "1"}, { 0 } };	
	m_scanOSDApplyBucketResultsColorCS			= g_shaderManager.AddComputeShader(L"shader/MemoryManagerOSD.hlsl", "ScanApplyBucketResultsCS",		"cs_5_0", &pBlob);	// scan pass 5
	m_scanOSDApplyBucketResultsDisplacementCS	= g_shaderManager.AddComputeShader(L"shader/MemoryManagerOSD.hlsl", "ScanApplyBucketResultsCS",		"cs_5_0", &pBlob, macro_displacement_mode);	// scan pass 5

	m_allocOSDCS						= g_shaderManager.AddComputeShader(L"shader/MemoryManagerOSD.hlsl", "AllocateCS",					"cs_5_0", &pBlob);	// copy mem locs from stack to descriptor buffer

	m_allocTilesCS = g_shaderManager.AddComputeShader(L"shader/TileMemory.hlsl", "AllocateTilesCS", "cs_5_0", &pBlob, macro_displacement_mode);	// copy mem locs from stack to descriptor buffer
	//m_deallocOSDCS						= g_shaderManager.AddComputeShader(L"shader/MemoryManagerOSD.hlsl", "DeallocateCS",					"cs_5_0", &pBlob);	// copy mem locs from descriptor buffer to stack

	SAFE_RELEASE(pBlob);

	return hr;
}

void MemoryManager::Destroy()
{
	// global
	SAFE_RELEASE(m_memTableStateStagingBUF);
	SAFE_RELEASE(m_memTableStateBUF);
	SAFE_RELEASE(m_memTableStateSRV);
	SAFE_RELEASE(m_memTableStateUAV);
					  
	SAFE_RELEASE(m_cbMemManageTask );
	SAFE_RELEASE(m_memManageTaskBUF);
	SAFE_RELEASE(m_memManageTaskSRV);
	SAFE_RELEASE(m_memManageTaskUAV);
	
	///////////////////////////////////////////////
	// displacement
	SAFE_RELEASE(m_dataTileDisplacementTEX);		
	SAFE_RELEASE(m_dataTileDisplacementSRV);		
	SAFE_RELEASE(m_dataTileDisplacementUAV);

	SAFE_RELEASE(m_dataTileDisplacementConstraintsTEX);
	SAFE_RELEASE(m_dataTileDisplacementConstraintsSRV);
	SAFE_RELEASE(m_dataTileDisplacementConstraintsUAV);

	SAFE_RELEASE(m_memoryTableTileDisplacementBUF);
	SAFE_RELEASE(m_memoryTableTileDisplacementSRV);
	SAFE_RELEASE(m_memoryTableTileDisplacementUAV);

	///////////////////////////////////////////////
	// color
	SAFE_RELEASE(m_dataTileColorTEX);							
	SAFE_RELEASE(m_dataTileColorSRV);							
	SAFE_RELEASE(m_dataTileColorUAV);							

	SAFE_RELEASE(m_memoryTableTileColorBUF);	
	SAFE_RELEASE(m_memoryTableTileColorSRV);	
	SAFE_RELEASE(m_memoryTableTileColorUAV);	

	///////////////////////////////////////////////
	// particles
	SAFE_RELEASE(m_dataParticlesBUF);
	SAFE_RELEASE(m_dataParticlesSRV);
	SAFE_RELEASE(m_dataParticlesUAV);

	SAFE_RELEASE(m_memoryTableParticlesBUF);
	SAFE_RELEASE(m_memoryTableParticlesSRV);
	SAFE_RELEASE(m_memoryTableParticlesUAV);

	// SCAN
	SAFE_RELEASE(m_scanBucketsBUF);				
	SAFE_RELEASE(m_scanBucketsSRV);
	SAFE_RELEASE(m_scanBucketsUAV);
	
	SAFE_RELEASE(m_scanBucketResultsBUF);
	SAFE_RELEASE(m_scanBucketResultsSRV);
	SAFE_RELEASE(m_scanBucketResultsUAV);
	
	SAFE_RELEASE(m_scanBucketBlockResultsBUF);	
	SAFE_RELEASE(m_scanBucketBlockResultsSRV);
	SAFE_RELEASE(m_scanBucketBlockResultsUAV);

	SAFE_RELEASE(m_compactedAllocateBUF);
	SAFE_RELEASE(m_compactedAllocateSRV);
	SAFE_RELEASE(m_compactedAllocateUAV);

	SAFE_RELEASE(m_compactedDeallocateBUF);
	SAFE_RELEASE(m_compactedDeallocateSRV);
	SAFE_RELEASE(m_compactedDeallocateUAV);

	SAFE_RELEASE(m_compactedActiveBUF);
	SAFE_RELEASE(m_compactedActiveSRV);
	SAFE_RELEASE(m_compactedActiveUAV);

	SAFE_RELEASE(m_dispatchIndirectBUF); 
	SAFE_RELEASE(m_dispatchIndirectBUFStaging); 
	SAFE_RELEASE(m_dispatchIndirectUAV);

	SAFE_RELEASE(m_scanResultBUF);
	SAFE_RELEASE(m_scanResultSRV);
	SAFE_RELEASE(m_scanResultUAV);

	SAFE_RELEASE(m_tilesInfoCB);

}

HRESULT MemoryManager::PrintTableState()
{
	HRESULT hr = S_OK;
	FreeMemoryTableState tableState;
	g_app.WaitForGPU();
	DXUTGetD3D11DeviceContext()->CopyResource(m_memTableStateStagingBUF, m_memTableStateBUF);
	g_app.WaitForGPU();

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	V_RETURN(DXUTGetD3D11DeviceContext()->Map(m_memTableStateStagingBUF, D3D11CalcSubresource(0,0,0), D3D11_MAP_READ, 0, &mappedResource));	
	memcpy((void*)&tableState.curLocTileDisplacement, (void*)mappedResource.pData, sizeof(FreeMemoryTableState));
	DXUTGetD3D11DeviceContext()->Unmap( m_memTableStateStagingBUF, 0 );
	g_app.WaitForGPU();

	std::cout << "tableState" << std::endl;
	std::cout << "displacement: \tcur loc: " << tableState.curLocTileDisplacement << ", maxLoc " <<  tableState.maxLocTileDisplacement << std::endl;
	std::cout << "color: \t\tcur loc: " << tableState.curLocTileColor << ", maxLoc " <<  tableState.maxLocTileColor << std::endl;
	std::cout << "particles: \tcur loc: " << tableState.curLocParticles << ", maxLoc " <<  tableState.maxLocParticles << std::endl;

	return hr;
}

HRESULT MemoryManager::CreateTileMemoryTable(ID3D11Device1* pd3dDevice, ID3D11Buffer*& layoutBUF, ID3D11ShaderResourceView*& layoutSRV, ID3D11UnorderedAccessView*& layoutUAV,
					   UINT numPages, UINT numTilesX, UINT numTilesY, UINT tileWidth, UINT tileHeight, bool withOverlap)
{
	UINT numTiles = numPages*numTilesY*numTilesX;

	TileTableEntry* tileLocTable = new TileTableEntry[numTiles];
		
	UINT tileId = 0;
	for(UINT page = 0; page < numPages; ++page)
	{
		for(UINT y = 0; y < numTilesY; ++y)
		{
			for(UINT x = 0; x < numTilesX; ++x)
			{
				tileLocTable[tileId].page = page;				
				tileLocTable[tileId].u_offset = x * tileWidth  + (withOverlap ? 1 : 0) ;// /*+1 start after overlap */ );
				tileLocTable[tileId].v_offset = y * tileHeight + (withOverlap ? 1 : 0) ;// /*+1 start after overlap */ );
				//std::cout <<  "tile: " << tileId << " start uv " << tileLocTable[tileId].u_offset << ", " << tileLocTable[tileId].v_offset << std::endl;
				// break all loops, goto is ugly but in this case useful
				if(++tileId >=numTiles)
					goto JUMP_STOP_ASSIGN_TILES;
			
			}

		}	

	}
JUMP_STOP_ASSIGN_TILES:

#if 0		// debug
	for(UINT i = 0; i < numTiles; ++i)
	{
		std::cout << "tile table entry " << i << std::endl;
		std::cout << "page: " << tileLocTable[i].page << " uv: " << tileLocTable[i].u_offset << "," << tileLocTable[i].v_offset << std::endl;
	}
#endif

	HRESULT hr;
	// create mem table gpu buffer 	
	V_RETURN(DXCreateBuffer(pd3dDevice,D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, numTiles*3*sizeof(USHORT), 0, D3D11_USAGE_DEFAULT, layoutBUF, &tileLocTable[0]));

	DXGI_FORMAT memTableFormat =  DXGI_FORMAT_R16_UINT;
	UINT		numTableElements = numTiles*3;//sizeof(TileTableEntry)/sizeof(USHORT); // table entry has ushorts

	D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
	ZeroMemory(&descSRV, sizeof(descSRV));
	descSRV.Format = memTableFormat;	
	descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	descSRV.Buffer.FirstElement = 0;	
	descSRV.Buffer.NumElements = numTableElements;
	V_RETURN(pd3dDevice->CreateShaderResourceView(layoutBUF, &descSRV, &layoutSRV));

	D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
	ZeroMemory(&descUAV, sizeof(descUAV));
	descUAV.Format = memTableFormat;	
	descUAV.ViewDimension =  D3D11_UAV_DIMENSION_BUFFER;
	descUAV.Buffer.FirstElement = 0;	
	descUAV.Buffer.NumElements = numTableElements;
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(layoutBUF, &descUAV, &layoutUAV));

	delete[] tileLocTable;

	return hr;
}

HRESULT MemoryManager::CreateLocalTileInfo( ID3D11Device1* pd3dDevice, UINT numTiles, UINT tileSize, ID3D11Buffer*& tileInfoBUF, ID3D11ShaderResourceView*& tileInfoSRV, ID3D11UnorderedAccessView*& tileInfoUAV, UINT numMipMaps /*=0*/ )
{
	// create default layout, pages and uv offsets are set by memory manager	
	// struct Layout {
	//     uint16_t page;
	//     uint16_t nMipmap;
	//     uint16_t u;
	//     uint16_t v;
	//     uint16_t adjSizeDiffs; //(4:4:4:4)
	//     uint8_t  width log2;
	//     uint8_t  height log2;
	// };

	// struct Layout {
	//     uint16_t page;	
	//     uint16_t u;
	//     uint16_t v;	
	//     uint8_t  size // log2;
	//     uint8_t nMipmap;
	
	// };

	// use page = maxushort for uninitialized, max pages is limited by directx = 2048	

	HRESULT hr;
	uint16_t initPage = USHRT_MAX;		// not initialized	
	uint16_t u = 0;						// not initialized
	uint16_t v = 0;						// not initialized	
	uint8_t  log2Size = log2Integer(tileSize);	
	uint8_t  nMipMap = numMipMaps;		// we dont have mipmaps yet // TODo

	uint16_t* tileInfo = new uint16_t[numTiles*4];
	for(unsigned int i = 0; i < numTiles; ++i)
	{				
		uint16_t* p = tileInfo+4*i;
		*p++ = initPage;		
		*p++ = u;
		*p++ = v;		
		*p++ = log2Size << 8 | nMipMap;
	}

#if 0	// debug
	for(int i = 0; i < numTiles; ++i)
	{				
		uint16_t* p = tileInfo+6*i;

		uint16_t initPage = (*p++);
		uint16_t nMipMap = (*p++);
		uint16_t u= (*p++);
		uint16_t v= (*p++);
		uint16_t adjSizeDiffs = (*p++);
		uint16_t wh = (*p++);

		std::cout	<< "page: " << initPage << ", nMipMap: " << nMipMap << ", u: " << u << ", v: " << " adjSizeDiffs " 
			<< ((adjSizeDiffs >> 12) & 0xf) << ", " << ((adjSizeDiffs >> 8) & 0xf) << " , " << ((adjSizeDiffs >> 4) &0xf) << ", " << (adjSizeDiffs & 0xf) << ", "
			<< "width: " << (1<<((wh >> 8)&0xf)) <<  ", height: " << (1 << (wh & 0xf)) << std::endl;
	}
#endif
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, numTiles * 4 * sizeof(USHORT), 0,
		D3D11_USAGE_DEFAULT, tileInfoBUF, &tileInfo[0]));// , D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, sizeof(UINT16)));

	int numElements = numTiles * 4;
	DXGI_FORMAT format = DXGI_FORMAT_R16_UINT;
	{			
		D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
		ZeroMemory(&descSRV, sizeof(descSRV));
		descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		descSRV.Format = format;			
		descSRV.Buffer.FirstElement = 0;
		descSRV.Buffer.NumElements = numElements;
		V_RETURN(pd3dDevice->CreateShaderResourceView(tileInfoBUF, &descSRV, &tileInfoSRV));
	}

	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
		ZeroMemory(&descUAV, sizeof(descUAV));
		descUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		descUAV.Format = format;// DXGI_FORMAT_UNKNOWN;// format; // fix not all vendors support R16_UINT UAV  // 
		descUAV.Buffer.FirstElement = 0;
		descUAV.Buffer.NumElements = numElements;				
		V_RETURN(pd3dDevice->CreateUnorderedAccessView(tileInfoBUF, &descUAV, &tileInfoUAV));
	}

	SAFE_DELETE_ARRAY(tileInfo);

	return hr;
}


HRESULT MemoryManager::InitTileDisplacementMemory(  ID3D11Device1* pd3dDevice, UINT numTiles, int tileSize, int nMipMaps, float defaultDispl /* = 0.1f */, bool withOverlap /* = false */, bool useHalfFloat /* = false */, bool createConstraintsMemory /*= false*/)
{
	HRESULT hr = S_OK;

	int maxTexWH = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	int maxNumPages = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
	

	// we dont care about mip maps for now
	if(nMipMaps > 0)
	{
		std::cerr << "tile mipmaps not implemented, aborting" << std::endl;
		exit(1);
	}

	// we want analytic displacement mapping, this requires overlaps
	if(!withOverlap)
	{
		std::cerr << "tiles without overlap are not implemented, aborting" << std::endl;
		exit(1);
	}


	m_displacementTileSize		 = tileSize; // + overlap?
	m_maxNumDisplacementTiles	 = numTiles;	
	m_displacementTileNumMipmaps = nMipMaps;
	m_displacementTileWithOverlap= withOverlap;

	// distribute numTile face textures in texture array
	// determine min required tex size and slices 
	// n = tile number
	// m = mip level

	// without overlap
	// -------------------------
	// |       |       |       |
	// |  n-1  |   n   |  n+1  |
	// |       |       |       |
	// -------------------------
	
	// with overlap
	// --------------------------------
	// |**********|*********|*********|
	// |*        *|*       *|*       *|
	// |*  n-1   *|*   n   *|*  n+1  *|
	// |*        *|*       *|*       *|
	// |**********|*********|*********|
	// --------------------------------

	// with mipmaps and overlap
	// ----------------------------------------------------------
	// |******************|******************|******************|
	// |*          **m2*  |*          **m2*  |*          **m2*  |
	// |*          *****  |*          *****  |*          *****  |
	// |*   n-1    *******|*   n      *******|*  n+1     *******|
	// |*  mip0    ** m1 *|*  mip0    ** m1 *|*  mip0    ** m1 *|
	// |*          **    *|*          **    *|*          **    *|
	// |******************|******************|******************|
	// ----------------------------------------------------------
	// |******************|******************|******************|
	// |*          **m2*  |*          **m2*  |*          **m2*  |
	// |*          *****  |*          *****  |*          *****  |
	// |*   n+2    *******|*  n+3     *******|*  n+4     *******|
	// |*  mip0    ** m1 *|*  mip0    ** m1 *|*  mip0    ** m1 *|
	// |*          **    *|*          **    *|*          **    *|
	// |******************|******************|******************|
	// ----------------------------------------------------------

	UINT numOverlapPixels = (withOverlap?2:0); // 1 on each side
	UINT tileWidth  = tileSize + numOverlapPixels;
	UINT tileHeight = tileSize + numOverlapPixels;

	UINT maxTilesX = static_cast<UINT>(floor((float)maxTexWH/tileWidth));
	UINT maxTilesY = static_cast<UINT>(floor((float)maxTexWH/tileHeight));

	UINT numTilesX = XMMin(maxTilesX, numTiles);
	UINT tilesYneeded = static_cast<UINT>(ceil((float)numTiles/maxTilesX));
	UINT numPages     = static_cast<UINT>(ceil((float)tilesYneeded/maxTilesY));

	UINT numTilesY = XMMin(maxTilesY,tilesYneeded);
	UINT texWidth  = numTilesX * tileWidth;
	UINT texHeight = numTilesY * tileHeight;

	std::cout << "Memory Manager create texture array for tile textures"  << std::endl;
	std::cout << "tile size: " << tileWidth << ", " << tileHeight << std::endl;
	std::cout << "requested memory for " << numTiles << " tiles " << std::endl;
	std::cout << "create tex array: w,h, slices " << texWidth << ", " << texHeight << ", " << numPages << std::endl;

	CreateTileMemoryTable(pd3dDevice, m_memoryTableTileDisplacementBUF, m_memoryTableTileDisplacementSRV, m_memoryTableTileDisplacementUAV,
					numPages, numTilesX, numTilesY, tileWidth, tileHeight, withOverlap);


	// init displacement texel data
	{	
		void* displData = NULL;
		if(useHalfFloat)	displData = new PackedVector::HALF[texWidth*texHeight*numPages];
		else				displData = new float[texWidth*texHeight*numPages];

		PackedVector::HALF defaultDisplAsHalf = PackedVector::XMConvertFloatToHalf(defaultDispl);

		
		for(unsigned int i = 0; i < texWidth*texHeight*numPages; ++i)
		{
			if(useHalfFloat)	 ((PackedVector::HALF*) displData)[i] = defaultDisplAsHalf;
			else				 ((float*)			    displData)[i] = defaultDispl;			
		}

		
#if 0 // debug: disp in faces	
		float cornerDisp = 0.5;
		int tileID = 0;
		for(unsigned int page = 0; page < numPages; ++page)
		{
			int pageStart = page*texWidth*texHeight;
			for(unsigned int tileY = 0; tileY < numTilesY; ++tileY)
			{

				int tileLineStart = pageStart + tileY*texWidth*tileHeight;
				for(unsigned int tileX = 0; tileX < numTilesX; ++tileX)
				{

					// now we are at a tile
					int tileStart = tileLineStart+tileX*tileWidth;
					

					UINT innerBorder = 0;


					for(unsigned int y = innerBorder; y < (tileHeight-innerBorder); ++y)
					{
						int currTileY = tileStart + y* texWidth; // go to next line
						for(unsigned int x = innerBorder; x < (tileWidth-innerBorder); ++x)
						{
							float innerDispl = defaultDispl+(x/36.f)+(y/36.f);//*y;
							//float innerDispl = defaultDispl+0.2f;
#if 1
							//if((int)numTiles-tileID < 300)
							{								
								if(useHalfFloat) ((PackedVector::HALF*) displData)[currTileY+x] = PackedVector::XMConvertFloatToHalf(innerDispl);						
								else			 ((float*)			    displData)[currTileY+x] = innerDispl;	
							}

							//// set corner val
							//if(y==0 || x == 0 || y == tileHeight-1 || tileWidth-1)
							//{								
							//	if(useHalfFloat) ((PackedVector::HALF*) displData)[currTileY+x] = PackedVector::XMConvertFloatToHalf(cornerDisp);						
							//	else			 ((float*)			    displData)[currTileY+x] = cornerDisp;	
							//}
#else // only for subset
							if((numTilesX-tileX) < 1000 && (numTilesY - tileY) < 1000)
							{							
								if(useHalfFloat) ((PackedVector::HALF*) displData)[currTileY+x] = PackedVector::XMConvertFloatToHalf(innerDispl);						
								else			 ((float*)			    displData)[currTileY+x] = innerDispl;	
							}
#endif

						}
					}

					//for(unsigned int y = 0; y < tileHeight; ++y)
					//{
					//	int currTileY = tileStart + y* texWidth; // go to next line
					//	for(unsigned int x = 0; x < tileWidth; ++x)
					//	{
					//		if(    (x > 0 && x < (tileWidth -1)) && (y > 0 && y < (tileHeight-1)) )
					//		{
					//			float innerDispl = defaultDispl+0.2f;
					//			if(useHalfFloat) ((PackedVector::HALF*) displData)[currTileY+x] = PackedVector::XMConvertFloatToHalf(innerDispl);						
					//			else			 ((float*)			    displData)[currTileY+x] = innerDispl;						
					//		}

					//	}
					//}
					tileID++;
				}
			}
		}
#endif

#if 0
		for(int s = 0; s < slicesNeeded; ++s)
		{
			int sliceStart = s*texSizeX*texSizeY;
			for(int y = 0; y < texSizeY; ++y)
			{
				for(int x =0 ;  x < texSizeX; ++x)
				{
					int i = sliceStart+y*texSizeX+x;
					if(((float*) displData)[i] != defaultDispl)
						std::cout << "wrong val at slice " << s<< " pos: " << x<<", " << y << std::endl;
				}
			}
		}
#endif
		{
			DXGI_FORMAT format = DXGI_FORMAT_R32_FLOAT;
			int bpp = 4;

			if(useHalfFloat)
			{
				format = DXGI_FORMAT_R16_FLOAT;
				bpp = 2;
			}

			// actual texels texture array
			D3D11_TEXTURE2D_DESC desc;
			desc.Width = texWidth;
			desc.Height = texHeight;
			desc.MipLevels = 1;
			desc.ArraySize = numPages;
			desc.Format = format;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS ;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;

			D3D11_SUBRESOURCE_DATA *initData = new D3D11_SUBRESOURCE_DATA[desc.ArraySize];
			int pageStride = texWidth * texHeight * bpp;
			for (unsigned int i = 0; i < desc.ArraySize; ++i) {
				if(useHalfFloat)
					initData[i].pSysMem = &((PackedVector::HALF*)displData)[0] + i * pageStride;
				else
					initData[i].pSysMem = &((float*)displData)[0] + i * pageStride;			
				initData[i].SysMemPitch = texWidth * bpp;
				initData[i].SysMemSlicePitch = pageStride;
			}
			V_RETURN(pd3dDevice->CreateTexture2D(&desc, initData, &m_dataTileDisplacementTEX));
			

			D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
			ZeroMemory(&descSRV, sizeof(descSRV));
			descSRV.Format = format;
			descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			descSRV.Texture2DArray.MipLevels = 1;
			descSRV.Texture2DArray.ArraySize = desc.ArraySize;
			V_RETURN(pd3dDevice->CreateShaderResourceView(m_dataTileDisplacementTEX, &descSRV, &m_dataTileDisplacementSRV));
			

			D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
			ZeroMemory(&descUAV, sizeof(descUAV));
			descUAV.Format = format;	
			descUAV.ViewDimension =  D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
			descUAV.Texture2DArray.FirstArraySlice = 0;
			descUAV.Texture2DArray.ArraySize = desc.ArraySize;	
			descUAV.Texture2DArray.MipSlice = 0;	
			pd3dDevice->CreateUnorderedAccessView(m_dataTileDisplacementTEX, &descUAV, &m_dataTileDisplacementUAV);

			// TODO: improve me, for now create full size tile array, better allocate constraints if needed using much smaller texture array
			if(createConstraintsMemory)
			{
				V_RETURN(pd3dDevice->CreateTexture2D(&desc, initData, &m_dataTileDisplacementConstraintsTEX));
				V_RETURN(pd3dDevice->CreateShaderResourceView(m_dataTileDisplacementConstraintsTEX, &descSRV, &m_dataTileDisplacementConstraintsSRV));
				V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_dataTileDisplacementConstraintsTEX, &descUAV, &m_dataTileDisplacementConstraintsUAV));
			}

			delete[] initData;
			delete[] displData;
		}
	}
	
	// update mem table state
	UpdateMemTableStates(numTiles, 0, 0);

	return hr;
}

HRESULT MemoryManager::InitTileColorMemory(  ID3D11Device1* pd3dDevice, UINT numTiles, int tileSize, int nMipMaps, const XMFLOAT3A& defaultColor /* = DirectX::XMFLOAT3A(0,0,0) */, bool withOverlap /* = false */)
{
	HRESULT hr = S_OK;

	int maxTexWH = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	int maxNumPages = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;


	// we dont care about mip maps for now
	if(nMipMaps > 0)
	{
		std::cerr << "tile mipmaps not implemented, aborting" << std::endl;
		exit(1);
	}

	// we want analytic displacement mapping, this requires overlaps
	if(!withOverlap)
	{
		std::cerr << "tiles without overlap are not implemented, aborting" << std::endl;
		exit(1);
	}

	
	// check resource size 
	{
		float resourceLimit = XMMin((float)D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_C_TERM,
									XMMax((float) D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_A_TERM,
										  (float) D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_B_TERM 
										  * (MEM_GTX780))); // replace with your card
		// TODO check with mipmaps
		UINT tileSizeWithOverlap = (tileSize + (withOverlap?2:0));
		UINT bpp = 4; // a8r8g8b8 format
		
		float requestedMem = numTiles * tileSizeWithOverlap * tileSizeWithOverlap * bpp / float(1024*1204); 

		std::cout << "requested " << requestedMem << " MB resource (max = " <<resourceLimit<< " MB)"  << std::endl;

		if(requestedMem > resourceLimit)
		{
			std::cerr << "cannot allocate a buffer of that size in InitTileColorMemory, aborting" << std::endl;
			exit(1);
		}
	}

	m_colorTileSize		 = tileSize; // + overlap?
	m_maxNumColorTiles	 = numTiles;	
	m_colorTileNumMipmaps = nMipMaps;
	m_colorTileWithOverlap= withOverlap;

	// distribute numTile face textures in texture array
	// determine min required tex size and slices 
	// n = tile number
	// m = mip level

	// without overlap
	// -------------------------
	// |       |       |       |
	// |  n-1  |   n   |  n+1  |
	// |       |       |       |
	// -------------------------

	// with overlap
	// --------------------------------
	// |**********|*********|*********|
	// |*        *|*       *|*       *|
	// |*  n-1   *|*   n   *|*  n+1  *|
	// |*        *|*       *|*       *|
	// |**********|*********|*********|
	// --------------------------------

	// with mipmaps and overlap
	// ----------------------------------------------------------
	// |******************|******************|******************|
	// |*          **m2*  |*          **m2*  |*          **m2*  |
	// |*          *****  |*          *****  |*          *****  |
	// |*   n-1    *******|*   n      *******|*  n+1     *******|
	// |*  mip0    ** m1 *|*  mip0    ** m1 *|*  mip0    ** m1 *|
	// |*          **    *|*          **    *|*          **    *|
	// |******************|******************|******************|
	// ----------------------------------------------------------
	// |******************|******************|******************|
	// |*          **m2*  |*          **m2*  |*          **m2*  |
	// |*          *****  |*          *****  |*          *****  |
	// |*   n+2    *******|*  n+3     *******|*  n+4     *******|
	// |*  mip0    ** m1 *|*  mip0    ** m1 *|*  mip0    ** m1 *|
	// |*          **    *|*          **    *|*          **    *|
	// |******************|******************|******************|
	// ----------------------------------------------------------

	UINT numOverlapPixels = (withOverlap?2:0); // 1 on each side
	UINT tileWidth  = tileSize + numOverlapPixels;
	UINT tileHeight = tileSize + numOverlapPixels;

	UINT maxTilesX = static_cast<UINT>(floor((float)maxTexWH/tileWidth));
	UINT maxTilesY = static_cast<UINT>(floor((float)maxTexWH/tileHeight));

	UINT numTilesX = XMMin(maxTilesX, numTiles);
	UINT tilesYneeded = static_cast<UINT>(ceil((float)numTiles/maxTilesX));
	UINT numPages     = static_cast<UINT>(ceil((float)tilesYneeded/maxTilesY));

	UINT numTilesY = XMMin(maxTilesY,tilesYneeded);
	UINT texWidth  = numTilesX * tileWidth;
	UINT texHeight = numTilesY * tileHeight;

	std::cout << "Memory Manager create texture array for tile textures"  << std::endl;
	std::cout << "tile size: " << tileWidth << ", " << tileHeight << std::endl;
	std::cout << "requested memory for " << numTiles << " tiles " << std::endl;
	std::cout << "create tex array: w,h, slices " << texWidth << ", " << texHeight << ", " << numPages << std::endl;

	CreateTileMemoryTable(pd3dDevice, m_memoryTableTileColorBUF, m_memoryTableTileColorSRV, m_memoryTableTileColorUAV,
						  numPages, numTilesX, numTilesY, tileWidth, tileHeight, withOverlap);


	// init displacement texel data
	{	
		//PackedVector::XMCOLOR lrDefaultColor = PackedVector::XMCOLOR(defaultColor.x, defaultColor.y, defaultColor.z, 1.f);
		PackedVector::XMUBYTE4 lrDefaultColor = PackedVector::XMUBYTE4(defaultColor.x*255.f, defaultColor.y*255.f, defaultColor.z*255.f, 0.f);
		//PackedVector::XMUBYTE4 lrDefaultColor = PackedVector::XMUBYTE4(255.f, 0.f, 255.f, 255.f);
		PackedVector::XMUBYTE4* colorData = new PackedVector::XMUBYTE4[texWidth*texHeight*numPages];
		
		
		for(unsigned int i = 0; i < texWidth*texHeight*numPages; ++i)
		{
			colorData[i] = lrDefaultColor;			
			if(i==0)
				std::cout << colorData[i].x << ", " << colorData[i].y << ", "<< colorData[i].z << ", "<< colorData[i].w << std::endl;
		}


#if 0// debug: color in faces
		std::cout << "TILE WIDTH " << tileWidth << std::endl;
		PackedVector::XMUBYTE4 innerColor = PackedVector::XMUBYTE4(255.f, 0.f, 0.f, 255.f);
		std::cout << "num tiles X: " << numTilesX << std::endl;
		//exit(1);
		int dbgId = 0;
		for(unsigned int page = 0; page < numPages; ++page)
		{
			int pageStart = page*texWidth*texHeight;
			for(unsigned int tileY = 0; tileY < numTilesY; ++tileY)
			{

				int tileLineStart = pageStart + tileY*texWidth*tileHeight;
				for(unsigned int tileX = 0; tileX < numTilesX; ++tileX)
				{					
					// now we are at a tile
					int tileStart = tileLineStart+tileX*tileWidth;

					UINT innerBorder = 1;
					for(unsigned int y = innerBorder; y < (tileHeight-innerBorder); ++y)
					{
						int currTileY = tileStart + y* texWidth; // go to next line
						for(unsigned int x = innerBorder; x < (tileWidth-innerBorder); ++x)
						{
							//if(    (x > 0 && x < (tileWidth -1)) 
							//	&& (y > 0 && y < (tileHeight-1)) )
							{								
								//colorData[currTileY+x] = innerColor;						
								//colorData[currTileY+x] = PackedVector::XMUBYTE4(x*2.f,y*2.f,255.f,255.f);						
								//colorData[currTileY+x] = PackedVector::XMUBYTE4(900*(float)(tileX+1)/numTilesX,255*(float)(tileY+1)/numTilesY,0.f,255.f);						

								float r = dbgId%64;//XMMin(255, dbgId);
								float g = XMMin(dbgId/64.f, 255.f);
								float b = 0;//XMMin(dbgId/(255*255.f), 255.f);
								colorData[currTileY+x] = PackedVector::XMUBYTE4(r,g,b,255.f);	

								//PackedVector::XMUBYTE4 black = PackedVector::XMUBYTE4(0.f,0.f,0.f,255.f);
								//PackedVector::XMUBYTE4 gray = PackedVector::XMUBYTE4(128.f,128.f,128.f,255.f);
								//if(y%2 == 0)
								//{
								//	if(x%2 == 0 )
								//		colorData[currTileY+x] = black;
								//	else
								//		colorData[currTileY+x] = gray;
								//}
								//else
								//{
								//	if(x%2 == 0 )
								//		colorData[currTileY+x] = gray;
								//	else
								//		colorData[currTileY+x] = black;

								//}
							}
						}
					}
					dbgId++;
				}
			}
		}
#endif

		{
			DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_TYPELESS;		// CHECKME 8bit per channel should be enough
			//DXGI_FORMAT_R8G8B8A8_UNORM;
			int bpp = 4;


			// actual texels texture array
			D3D11_TEXTURE2D_DESC desc;
			desc.Width = texWidth;
			desc.Height = texHeight;
			desc.MipLevels = 1;
			desc.ArraySize = numPages;
			desc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS ;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;

			D3D11_SUBRESOURCE_DATA *initData = new D3D11_SUBRESOURCE_DATA[desc.ArraySize];
			int pageStride = texWidth * texHeight * bpp;
			for (unsigned int i = 0; i < desc.ArraySize; ++i) {
				initData[i].pSysMem = &((unsigned char*)colorData)[0] + i * pageStride;			
				initData[i].SysMemPitch = texWidth * bpp;
				initData[i].SysMemSlicePitch = pageStride;
			}

			V_RETURN(pd3dDevice->CreateTexture2D(&desc, initData, &m_dataTileColorTEX));
			

			D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
			ZeroMemory(&descSRV, sizeof(descSRV));
			descSRV.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			descSRV.Texture2DArray.MipLevels = 1;
			descSRV.Texture2DArray.ArraySize = desc.ArraySize;
			V_RETURN(pd3dDevice->CreateShaderResourceView(m_dataTileColorTEX, &descSRV, &m_dataTileColorSRV));


			D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
			ZeroMemory(&descUAV, sizeof(descUAV));
			descUAV.Format = DXGI_FORMAT_R32_UINT;	
			descUAV.ViewDimension =  D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
			descUAV.Texture2DArray.FirstArraySlice = 0;
			descUAV.Texture2DArray.ArraySize = desc.ArraySize;	
			descUAV.Texture2DArray.MipSlice = 0;	
			pd3dDevice->CreateUnorderedAccessView(m_dataTileColorTEX, &descUAV, &m_dataTileColorUAV);

			delete[] initData;
			delete[] colorData;
		}
	}

	// update mem table state
	V_RETURN(UpdateMemTableStates(0, numTiles, 0));
	//exit(1);

	return hr;
}

HRESULT MemoryManager::InitParticleMemory(  ID3D11Device1* pd3dDevice, UINT numParticles, UINT particleByteSize )
{
	HRESULT hr = S_OK;
	return hr;
}

HRESULT MemoryManager::PreallocateDisplacementTiles(  ID3D11DeviceContext1* pd3dImmediateContext, UINT count, ID3D11UnorderedAccessView* targetTileLayoutUAV )
{
	HRESULT hr = S_OK;

	// write memory management task
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map( m_cbMemManageTask, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CBMemoryManageTask* pCBTask = ( CBMemoryManageTask* )MappedResource.pData;		
	pCBTask->numAllocs = count;		
	pCBTask->numDeallocs = 0;
	pCBTask->padding = XMUINT2(0,0);
	pd3dImmediateContext->Unmap( m_cbMemManageTask, 0 );

	pd3dImmediateContext->CSSetConstantBuffers(12, 1, &m_cbMemManageTask);


	ID3D11UnorderedAccessView* ppUAVNULL[] = {NULL, NULL, NULL};
	ID3D11ShaderResourceView*  ppSRVNULL[] = {NULL, NULL, NULL};
	
	pd3dImmediateContext->CSSetShaderResources(0, 1, &m_memoryTableTileDisplacementSRV);	// source
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, &targetTileLayoutUAV, NULL);		// target
	pd3dImmediateContext->CSSetUnorderedAccessViews(1, 1, &m_memTableStateUAV, NULL);		// mem state
	
	pd3dImmediateContext->CSSetShader(m_preallocDisplacementTilesCS->Get(),NULL, 0);

	const UINT workGroupSize = 64;
	pd3dImmediateContext->Dispatch(count/workGroupSize + 1, 1, 1);
	
	pd3dImmediateContext->CSSetShaderResources(0, 1, ppSRVNULL);
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, ppUAVNULL,NULL);
		
	return hr;
}

HRESULT MemoryManager::PreallocateColorTiles( ID3D11DeviceContext1* pd3dImmediateContext, UINT count, ID3D11UnorderedAccessView* targetTileLayoutUAV )
{
	HRESULT hr = S_OK;

	// write memory management task
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map( m_cbMemManageTask, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CBMemoryManageTask* pCBTask = ( CBMemoryManageTask* )MappedResource.pData;		
	pCBTask->numAllocs = count;		
	pCBTask->numDeallocs = 0;
	pCBTask->padding = XMUINT2(0,0);
	pd3dImmediateContext->Unmap( m_cbMemManageTask, 0 );

	pd3dImmediateContext->CSSetConstantBuffers(12, 1, &m_cbMemManageTask);


	ID3D11UnorderedAccessView* ppUAVNULL[] = {NULL, NULL, NULL};
	ID3D11ShaderResourceView*  ppSRVNULL[] = {NULL, NULL, NULL};

	pd3dImmediateContext->CSSetShaderResources(0, 1, &m_memoryTableTileColorSRV);	// source
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, &targetTileLayoutUAV, NULL);		// target
	pd3dImmediateContext->CSSetUnorderedAccessViews(1, 1, &m_memTableStateUAV, NULL);		// mem state

	pd3dImmediateContext->CSSetShader(m_preallocColorTilesCS->Get(),NULL, 0);

	const UINT workGroupSize = 64;
	pd3dImmediateContext->Dispatch(count/workGroupSize + 1, 1, 1);

	pd3dImmediateContext->CSSetShaderResources(0, 1, ppSRVNULL);
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, ppUAVNULL,NULL);


	return hr;
}

HRESULT MemoryManager::PrintTileInfo( ID3D11DeviceContext1* pd3dImmediateContext, UINT numTiles, ID3D11Buffer* tileInfoBUF ) const
{
	HRESULT hr;
	ID3D11Buffer* tmpStagingBUF = NULL;
	V_RETURN(DXCreateBuffer(DXUTGetD3D11Device(), 0, numTiles * 6 * sizeof(USHORT), D3D11_CPU_ACCESS_READ,  D3D11_USAGE_STAGING, tmpStagingBUF));

	g_app.WaitForGPU();
	DXUTGetD3D11DeviceContext()->CopyResource(tmpStagingBUF, tileInfoBUF);
	g_app.WaitForGPU();

	uint16_t* tileInfoCPU = new uint16_t[numTiles*6];
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	V_RETURN(DXUTGetD3D11DeviceContext()->Map(tmpStagingBUF, D3D11CalcSubresource(0,0,0), D3D11_MAP_READ, 0, &mappedResource));	
	memcpy((void*)&tileInfoCPU[0], (void*)mappedResource.pData, numTiles * 6 * sizeof(USHORT));
	DXUTGetD3D11DeviceContext()->Unmap( tmpStagingBUF, 0 );
	g_app.WaitForGPU();

	for(unsigned int i = 0; i < numTiles; ++i)
	{				
		uint16_t* p = tileInfoCPU+6*i;

		uint16_t initPage = (*p++);
		uint16_t nMipMap = (*p++);
		uint16_t u= (*p++);
		uint16_t v= (*p++);
		uint16_t adjSizeDiffs = (*p++);
		uint16_t wh = (*p++);

		std::cout	<< "page: " << initPage << ", nMipMap: " << nMipMap << ", u: " << u << ", v: " << v << " adjSizeDiffs " 
					<< ((adjSizeDiffs >> 12) & 0xf) << ", " << ((adjSizeDiffs >> 8) & 0xf) << " , " << ((adjSizeDiffs >> 4) &0xf) << ", " << (adjSizeDiffs & 0xf) << ", "
					<< "width: " << (1<<((wh >> 8)&0xf)) <<  ", height: " << (1 << (wh & 0xf)) << std::endl;
	}

	SAFE_DELETE_ARRAY(tileInfoCPU);
	SAFE_RELEASE(tmpStagingBUF);

	return hr;
}

HRESULT MemoryManager::UpdateMemTableStates( UINT newMaxNumDiplacementTiles, UINT newMaxNumColorTiles, UINT newMaxNumParticles )
{
	HRESULT hr = S_OK;

	// print current state
	PrintTableState();

	// download current state
	g_app.WaitForGPU();
	DXUTGetD3D11DeviceContext()->CopyResource(m_memTableStateStagingBUF, m_memTableStateBUF);
	g_app.WaitForGPU();

	FreeMemoryTableState tableState;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	V(DXUTGetD3D11DeviceContext()->Map(m_memTableStateStagingBUF, D3D11CalcSubresource(0,0,0), D3D11_MAP_READ, 0, &mappedResource));	
	memcpy((void*)&tableState.curLocTileDisplacement, (void*)mappedResource.pData, sizeof(FreeMemoryTableState));
	DXUTGetD3D11DeviceContext()->Unmap( m_memTableStateStagingBUF, 0 );
	g_app.WaitForGPU();

	// update state
	if(newMaxNumDiplacementTiles > 0)	tableState.maxLocTileDisplacement = newMaxNumDiplacementTiles;
	if(newMaxNumColorTiles > 0)			tableState.maxLocTileColor = newMaxNumColorTiles;
	if(newMaxNumColorTiles > 0)			tableState.maxLocParticles = newMaxNumParticles;

	// no easy resizing, init stack pointer with maxloc
	if(newMaxNumDiplacementTiles > 0)	tableState.curLocTileDisplacement = newMaxNumDiplacementTiles;
	if(newMaxNumColorTiles > 0)			tableState.curLocTileColor		  = newMaxNumColorTiles;
	if(newMaxNumColorTiles > 0)			tableState.curLocParticles		  = newMaxNumParticles;


	// create new buffer for updating state on gpu
	ID3D11Buffer* tmpStateBuffer = NULL;
	V_RETURN(DXCreateBuffer(DXUTGetD3D11Device(), D3D11_BIND_SHADER_RESOURCE, sizeof(FreeMemoryTableState), 0, D3D11_USAGE_IMMUTABLE, tmpStateBuffer, &tableState.curLocTileDisplacement));
	DXUTGetD3D11DeviceContext()->CopyResource(m_memTableStateBUF, tmpStateBuffer);
	
	g_app.WaitForGPU();
	SAFE_RELEASE(tmpStateBuffer);
	
	// print updated state
	PrintTableState();

	return hr;
}

HRESULT MemoryManager::ManageColorTiles( ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance )
{
	HRESULT hr = S_OK;

	// scan
	// alloc in scan?
	bool paintMode = true;
	V_RETURN(Scan(pd3dImmediateContext, instance, paintMode));

	return hr;
}

HRESULT MemoryManager::ManageDisplacementTiles(ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* deformable)
{
	HRESULT hr = S_OK;
	
	bool paintMode = false;
	//V_RETURN(Scan(pd3dImmediateContext, deformable, paintMode));	// slightly slower, but with out of memory handling
	V_RETURN(Alloc(pd3dImmediateContext, deformable));			// just atomic alloc tile memory faster but does not handle out of memory

	return hr;
}

HRESULT MemoryManager::Scan(ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance, bool paintMode)
{
	HRESULT hr = S_OK;


	if(g_app.g_withPaintSculptTimings)
	{
		g_app.GPUPerfTimerStart(pd3dImmediateContext);
		if(instance->IsSubD()) ScanInternalOSD(pd3dImmediateContext, instance, paintMode);
		g_app.GPUPerfTimerEnd(pd3dImmediateContext);		
		std::cout << "scan time: " << (paintMode ? "paing mode" : "sculpt mode: ") << g_app.GPUPerfTimerElapsed(pd3dImmediateContext) << std::endl;
	}
	else
	{
		if(instance->IsSubD()) ScanInternalOSD(pd3dImmediateContext, instance, paintMode);
	}


	return hr;	
}

void MemoryManager::UpdateTileCB( ID3D11DeviceContext1* pd3dImmediateContext, UINT numPatches)
{
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map( m_tilesInfoCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CB_ManageTiles* pCB = ( CB_ManageTiles* )MappedResource.pData;		
	pCB->numTiles = numPatches;
	pCB->padding = XMUINT3(0,0,0);
	pd3dImmediateContext->Unmap( m_tilesInfoCB, 0 );
	pd3dImmediateContext->CSSetConstantBuffers(CB_LOC::MANAGE_TILES, 1, &m_tilesInfoCB);
}

HRESULT MemoryManager::ScanInternalOSD( ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance, bool paintMode )
{
	HRESULT hr = S_OK;
	
	//Buffer<uint>			g_tileVisiblity	 : register(t0);  // result of intersection with brush pass, 1: for ptex face id was intersected
	//Buffer<uint>			g_tileDescriptors: register(t1);  //
	//Buffer<uint>			g_memoryTable	 : register(t1);  // free memory table, readable for alloc
	
	//Buffer<SCAN_ELEM>		g_scanInput				     : register(t2);		// is output pass1, input pass2, was visibleAllocDeallocs
	//Buffer<SCAN_ELEM>		g_scanResultBucketsPass1     : register(t2);		// is output pass1, input pass2, was visibleAllocDeallocs
	//Buffer<SCAN_ELEM>		g_scanResultBucketsDownSweep : register(t3);		// is output pass1, input pass2, was visibleAllocDeallocsIncr
	//Buffer<uint>			g_memoryState				 : register(t4);		// memory table state, readable for
	//
	//
	//RWBuffer<SCAN_ELEM>	g_outputScanPassUAV		: register(u0);			// 	
	//RWBuffer<uint>		g_tileDescriptorsUAV	: register(u1);			// per tile descriptors, write mem locs on allocate
	//RWBuffer<uint>		g_memoryStateUAV		: register(u2);			// 
	//RWBuffer<uint4>		g_dispatchIndirectUAV	: register(u3);			// 	
	//RWBuffer<uint>		g_compactedAllocateUAV	: register(u4);			// 

	// scan buffers
	// compacted buffers?

	UINT numTiles = instance->GetOSDMesh()->GetNumPTexFaces();

	if(numTiles > static_cast<UINT>(g_app.g_memMaxNumTilesPerObject))
		std::cerr << "warning numPatches>g_app.g_memMaxNumTiles" << std::endl;

	// constant buffer update	
	UpdateTileCB(pd3dImmediateContext, numTiles);


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// PASS 1 - UP-SWEEP  scan buckets
	pd3dImmediateContext->CSSetShader(m_scanOSDBucketCS->Get(), NULL, 0);

	if(paintMode)
	{
		ID3D11ShaderResourceView* ppSRV[] = { 	instance->GetVisibility()->SRV,	instance->GetColorTileLayout()->SRV	};
		pd3dImmediateContext->CSSetShaderResources(0, 2, ppSRV);						// m_indirectionSRV, tasksSRV
	}
	else
	{
		ID3D11ShaderResourceView* ppSRV[] = { 	instance->GetVisibility()->SRV,	instance->GetDisplacementTileLayout()->SRV	};
		pd3dImmediateContext->CSSetShaderResources(0, 2, ppSRV);						// m_indirectionSRV, tasksSRV
	}
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, &m_scanBucketsUAV, NULL); 

	UINT groupsPass1 = (numTiles + m_scanBucketSize - 1) / m_scanBucketSize;
	UINT dimX = 128;
	UINT dimY = (groupsPass1 + dimX - 1) / dimX;
	pd3dImmediateContext->Dispatch(dimX, dimY, 1); // CHECKME

	pd3dImmediateContext->CSSetShaderResources(0, 2, g_ppSRVNULL);	
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, g_ppUAVNULL,NULL);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// pass 2 UP-SWEEP scanBucketResults
	pd3dImmediateContext->CSSetShader(m_scanOSDBucketResultsCS->Get(), NULL, 0);

	pd3dImmediateContext->CSSetShaderResources(2, 1, &m_scanBucketsSRV);
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, &m_scanBucketResultsUAV, NULL);

	UINT groupsPass2 = (groupsPass1 + m_scanBucketBlockSize - 1) / m_scanBucketBlockSize;
	pd3dImmediateContext->Dispatch(groupsPass2, 1, 1);

	pd3dImmediateContext->CSSetShaderResources(2, 1, g_ppSRVNULL);
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, g_ppUAVNULL, NULL);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// pass 3 UP-SWEEP

	pd3dImmediateContext->CSSetShader(m_scanOSDBucketBlockResultsCS->Get(), NULL, 0);

	pd3dImmediateContext->CSSetShaderResources(2, 1, &m_scanBucketResultsSRV);
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, &m_scanBucketBlockResultsUAV, NULL);

	pd3dImmediateContext->Dispatch(1,1,1);

	pd3dImmediateContext->CSSetShaderResources(2, 1, g_ppSRVNULL);
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, g_ppUAVNULL, NULL);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// pass 4 DOWN-SWEEP

	pd3dImmediateContext->CSSetShader(m_scanOSDApplyBucketBlockResultsCS->Get(), NULL, 0);

	pd3dImmediateContext->CSSetShaderResources(2, 1, &m_scanBucketBlockResultsSRV);
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, &m_scanBucketResultsUAV, NULL);

	pd3dImmediateContext->Dispatch(groupsPass2, 1, 1);

	pd3dImmediateContext->CSSetShaderResources(2, 1, g_ppSRVNULL);
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, g_ppUAVNULL, NULL);


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// PASS 5 Down-sweep
//#define ALLOC_IN_SCAN
//#ifndef ALLOC_IN_SCAN
//	ID3D11ShaderResourceView* ppSRVPass3[]  = { m_scanBucketsSRV, m_scanBucketResultsSRV};
//	ID3D11UnorderedAccessView* ppUAVPass3[] = { m_scanResultUAV, m_compactedAllocateUAV, m_dispatchIndirectUAV};
//
//	pd3dImmediateContext->CSSetShader(m_scanApplyBucketResultsCS->Get(), NULL, 0);
//	pd3dImmediateContext->CSSetShaderResources(2, 2, ppSRVPass3);
//	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 3, ppUAVPass3, NULL);
//
//	pd3dImmediateContext->Dispatch(dimX, dimY, 1);
//
//	pd3dImmediateContext->CSSetShaderResources(2, 2, g_ppSRVNULL);
//	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 3, g_ppUAVNULL,NULL);
//#else

	//Buffer<uint>			g_memoryTable				 : register(t1);	// free memory table, readable for alloc
	//Buffer<SCAN_ELEM>		g_scanResultBucketsPass1     : register(t2);	// is output pass1, input pass2, was visibleAllocDeallocs
	//Buffer<SCAN_ELEM>		g_scanResultBucketsDownSweep : register(t3);	// is output pass1, input pass2, was visibleAllocDeallocsIncr
	//Buffer<uint>			g_memoryState				 : register(t4);	// memory table state, readable for

	//RWBuffer<SCAN_ELEM>	g_outputScanPassUAV			 : register(u0);	// 	
	//RWBuffer<uint>		g_tileDescriptorsUAV		 : register(u1);	// per tile descriptors, write mem locs on allocate
	//RWBuffer<uint>		g_memoryStateUAV			 : register(u2);	// 
	//RWBuffer<uint4>		g_dispatchIndirectUAV		 : register(u3);	// 	
	//RWBuffer<uint>		g_compactedAllocateUAV		 : register(u4);	// 
	//RWBuffer<uint>		g_compactedDeallocateUAV	 : register(u5);	// 

	
	


	if(paintMode)
	{
		pd3dImmediateContext->CSSetShader(m_scanOSDApplyBucketResultsColorCS->Get(), NULL, 0);
		
		ID3D11ShaderResourceView* ppColorSRVS[]  = { m_memoryTableTileColorSRV, m_scanBucketsSRV, m_scanBucketResultsSRV};
		pd3dImmediateContext->CSSetShaderResources(1, 3, ppColorSRVS);

		ID3D11UnorderedAccessView* ppColorUAVS[] = { /*0: ,*/ instance->GetColorTileLayout()->UAV, m_memTableStateUAV , m_dispatchIndirectUAV, m_compactedAllocateUAV, m_compactedDeallocateUAV};
		pd3dImmediateContext->CSSetUnorderedAccessViews(1, 5, ppColorUAVS, NULL);

		// debug
		//ID3D11UnorderedAccessView* ppColorUAVS[] = { m_scanResultUAV,/*0: ,*/ instance->GetColorTileLayoutUAV(), m_memTableStateUAV , m_dispatchIndirectUAV, m_compactedAllocateUAV, m_compactedDeallocateUAV};
		//pd3dImmediateContext->CSSetUnorderedAccessViews(0, 6, ppColorUAVS, NULL);
		
	}
	else
	{
		ID3D11ShaderResourceView* ppSRVDispl[]  = { m_memoryTableTileDisplacementSRV, m_scanBucketsSRV, m_scanBucketResultsSRV};
		pd3dImmediateContext->CSSetShader(m_scanOSDApplyBucketResultsDisplacementCS->Get(), NULL, 0);
				
		pd3dImmediateContext->CSSetShaderResources(1, 3, ppSRVDispl);

		ID3D11UnorderedAccessView* ppDisplUAVS[] = { /*0: m_scanResultUAV, */ instance->GetDisplacementTileLayout()->UAV, m_memTableStateUAV , m_dispatchIndirectUAV, m_compactedAllocateUAV,m_compactedDeallocateUAV};
		
		pd3dImmediateContext->CSSetUnorderedAccessViews(1, 5, ppDisplUAVS, NULL);
	}


	pd3dImmediateContext->Dispatch(dimX, dimY, 1);

	pd3dImmediateContext->CSSetShaderResources(1, 3, g_ppSRVNULL);
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 6, g_ppUAVNULL,NULL);

	if(0)
	{
		// enable writing result in shader
		ID3D11Buffer* stagingBUF;
		DXCreateBuffer(DXUTGetD3D11Device(), 0, sizeof(UINT),  D3D11_CPU_ACCESS_READ, D3D11_USAGE_STAGING, stagingBUF );

		D3D11_BOX sourceRegion;
		sourceRegion.left = 4*(numTiles-1);
		sourceRegion.right = 4*numTiles;
		sourceRegion.top = sourceRegion.front = 0;
		sourceRegion.bottom = sourceRegion.back = 1;

		// Get last element of the prefix sum (sum of all elements)
		pd3dImmediateContext->CopySubresourceRegion( stagingBUF, 0, 0, 0, 0, m_scanResultBUF, 0, &sourceRegion);

		UINT sum = 0;
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		pd3dImmediateContext->Map(stagingBUF, 0, D3D11_MAP_READ, 0, &MappedResource);   
		sum = ((UINT*)MappedResource.pData)[0];
		pd3dImmediateContext->Unmap( stagingBUF, 0 );

		SAFE_RELEASE(stagingBUF);
		std::cout << "prefix sum: " << sum << std::endl;
	}
//#endif


	return hr;
}


HRESULT MemoryManager::Alloc(ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance)
{
	HRESULT hr = S_OK;


	if (g_app.g_withPaintSculptTimings)
	{
		g_app.GPUPerfTimerStart(pd3dImmediateContext);
		if (instance->IsSubD()) AllocInternal(pd3dImmediateContext, instance);
		g_app.GPUPerfTimerEnd(pd3dImmediateContext);
		std::cout << "alloc time: " << g_app.GPUPerfTimerElapsed(pd3dImmediateContext) << std::endl;
	}
	else
	{
		if (instance->IsSubD()) AllocInternal(pd3dImmediateContext, instance);;
	}


	return hr;
}

HRESULT MemoryManager::AllocInternal(ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance)
{
	HRESULT hr = S_OK;

	UINT numTiles = instance->GetOSDMesh()->GetNumPTexFaces();

	if (numTiles > static_cast<UINT>(g_app.g_memMaxNumTilesPerObject))
		std::cerr << "warning numPatches>g_app.g_memMaxNumTiles" << std::endl;

	// constant buffer update	
	UpdateTileCB(pd3dImmediateContext, numTiles);

	pd3dImmediateContext->CSSetShader(m_allocTilesCS->Get(), NULL, 0);

	ID3D11ShaderResourceView* ppSRV[] = { instance->GetVisibility()->SRV, m_memoryTableTileDisplacementSRV };
	ID3D11UnorderedAccessView* ppDisplUAVS[] = { instance->GetDisplacementTileLayout()->UAV, m_memTableStateUAV };

	pd3dImmediateContext->CSSetShaderResources(0, 2, ppSRV);
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, ppDisplUAVS, NULL);
		
	UINT groupsPass1 = (numTiles + 32 - 1) / 32;
	pd3dImmediateContext->Dispatch(groupsPass1, 1, 1); // CHECKME

	pd3dImmediateContext->CSSetShaderResources(0, 2, g_ppSRVNULL);
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, g_ppUAVNULL, NULL);
		
	if (0)
	{
		// enable writing result in shader
		ID3D11Buffer* stagingBUF;
		DXCreateBuffer(DXUTGetD3D11Device(), 0, sizeof(UINT), D3D11_CPU_ACCESS_READ, D3D11_USAGE_STAGING, stagingBUF);

		D3D11_BOX sourceRegion;
		sourceRegion.left = 4 * (numTiles - 1);
		sourceRegion.right = 4 * numTiles;
		sourceRegion.top = sourceRegion.front = 0;
		sourceRegion.bottom = sourceRegion.back = 1;

		// Get last element of the prefix sum (sum of all elements)
		pd3dImmediateContext->CopySubresourceRegion(stagingBUF, 0, 0, 0, 0, m_scanResultBUF, 0, &sourceRegion);

		UINT sum = 0;
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		pd3dImmediateContext->Map(stagingBUF, 0, D3D11_MAP_READ, 0, &MappedResource);
		sum = ((UINT*)MappedResource.pData)[0];
		pd3dImmediateContext->Unmap(stagingBUF, 0);

		SAFE_RELEASE(stagingBUF);
		std::cout << "prefix sum: " << sum << std::endl;
	}
	//#endif


	return hr;
}
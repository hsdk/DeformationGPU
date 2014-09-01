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
#include "TileOverlapUpdater.h"
#include "MemoryManager.h"

#include "scene/ModelInstance.h"
#include "scene/DXSubDModel.h"

#include "utils/DbgNew.h" // has to be last include

using namespace DirectX;

#define COLOR_DISPATCH_TILE_SIZE 16
#define WORK_GROUP_SIZE_EXTRAORDINARY 128

TileOverlapUpdater g_overlapUpdater;

TileOverlapUpdater::TileOverlapUpdater()
{
	m_updateExtraordinaryCB = NULL;
	m_dispatchIndirectBUF = NULL;

	// opensubdiv
	m_updateOverlapEdgesDisplacementOSD_CS			= NULL;
	m_updateOverlapCornersDisplacementOSD_CS		= NULL;
	m_updateOverlapEqualizeCornerDisplacementOSD_CS	= NULL;
	
	m_updateOverlapEdgesColorOSD_CS					= NULL;
	m_updateOverlapCornersColorOSD_CS				= NULL;
	m_updateOverlapEqualizeCornerColorOSD_CS		= NULL;


	m_updateOverlapCompacted_EdgesDisplacementOSD_CS  = NULL;
	m_updateOverlapCompacted_CornersDisplacementOSD_CS= NULL;
	m_updateOverlapCompacted_EdgesColorOSD_CS		  = NULL;
	m_updateOverlapCompacted_CornersColorOSD_CS		  = NULL;

	m_compactIntersectAll_CS = NULL;
	m_constraintsMode = false;
}

TileOverlapUpdater::~TileOverlapUpdater()
{
	Destroy();
}

HRESULT TileOverlapUpdater::Create( ID3D11Device1* pd3dDevice )
{
	HRESULT hr = S_OK;
	V_RETURN(DXUTCreateBuffer(pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_UpdateExtarodinary)	, D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, m_updateExtraordinaryCB));
	
	// indirect dispatch buffer to update overlap only on modified tiles
	UINT indirectInitialState[] = {	0, 1, 1, 1 };
	V_RETURN(DXUTCreateBuffer(DXUTGetD3D11Device(),	0,	sizeof(UINT)*4, 0,	D3D11_USAGE_DEFAULT, m_dispatchIndirectBUF, indirectInitialState, D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS, sizeof(UINT)*4));
	DXUT_SetDebugName(m_dispatchIndirectBUF, "TileOverlapUpdater dispatchIndirectBUF");


	// SHADER

	ID3DBlob* pBlob = NULL;
	// color overlap
	char tileSizeColor[4];
	sprintf_s(tileSizeColor, "%d", g_app.g_colorTileSize);
	D3D_SHADER_MACRO macro_overlap_color[]	= { { "TILE_SIZE", tileSizeColor }, { 0 } };
	m_updateOverlapEdgesColorOSD_CS					= g_shaderManager.AddComputeShader(L"shader/UpdateOverlap.hlsl", "OverlapEdgesCS",					"cs_5_0", &pBlob, macro_overlap_color);	
	m_updateOverlapCornersColorOSD_CS				= g_shaderManager.AddComputeShader(L"shader/UpdateOverlap.hlsl", "OverlapCornersCS",				"cs_5_0", &pBlob, macro_overlap_color);	
	m_updateOverlapEqualizeCornerColorOSD_CS		= g_shaderManager.AddComputeShader(L"shader/UpdateOverlap.hlsl", "OverlapEqualizeExtraordinaryCS",	"cs_5_0", &pBlob, macro_overlap_color);


	D3D_SHADER_MACRO macro_overlap_color_compacted[]	= { { "TILE_SIZE", tileSizeColor },{"COMPACTED_VISIBILITY", "1"}, { 0 } };
	m_updateOverlapCompacted_EdgesColorOSD_CS			= g_shaderManager.AddComputeShader(L"shader/UpdateOverlap.hlsl", "OverlapEdgesCS",				"cs_5_0", &pBlob, macro_overlap_color_compacted);	
	m_updateOverlapCompacted_CornersColorOSD_CS			= g_shaderManager.AddComputeShader(L"shader/UpdateOverlap.hlsl", "OverlapCornersCS",			"cs_5_0", &pBlob, macro_overlap_color_compacted);	

	// displacement overlap
	char tileSizeDisplacement[4];
	sprintf_s(tileSizeDisplacement, "%d", g_app.g_displacementTileSize);
	D3D_SHADER_MACRO macro_overlap_displacement[]	= { { "TILE_SIZE", tileSizeDisplacement }, { "DISPLACEMENT_MODE", "1" }, { 0 } };
	m_updateOverlapEdgesDisplacementOSD_CS			= g_shaderManager.AddComputeShader(L"shader/UpdateOverlap.hlsl", "OverlapEdgesCS", "cs_5_0", &pBlob, macro_overlap_displacement);	
	m_updateOverlapCornersDisplacementOSD_CS		= g_shaderManager.AddComputeShader(L"shader/UpdateOverlap.hlsl", "OverlapCornersCS", "cs_5_0", &pBlob, macro_overlap_displacement);
	m_updateOverlapEqualizeCornerDisplacementOSD_CS	= g_shaderManager.AddComputeShader(L"shader/UpdateOverlap.hlsl", "OverlapEqualizeExtraordinaryCS", "cs_5_0", &pBlob, macro_overlap_displacement);
	
	D3D_SHADER_MACRO macro_overlap_displacement_compacted[]	= { { "TILE_SIZE", tileSizeDisplacement }, { "DISPLACEMENT_MODE", "1" }, {"COMPACTED_VISIBILITY", "1"}, { 0 } };
	m_updateOverlapCompacted_EdgesDisplacementOSD_CS	= g_shaderManager.AddComputeShader(L"shader/UpdateOverlap.hlsl", "OverlapEdgesCS", "cs_5_0", &pBlob, macro_overlap_displacement_compacted);	
	m_updateOverlapCompacted_CornersDisplacementOSD_CS	= g_shaderManager.AddComputeShader(L"shader/UpdateOverlap.hlsl", "OverlapCornersCS", "cs_5_0", &pBlob, macro_overlap_displacement_compacted);
	
	m_compactIntersectAll_CS = g_shaderManager.AddComputeShader(L"shader/UpdateOverlap.hlsl", "CompactIntersectAll", "cs_5_0", &pBlob, macro_overlap_displacement);


	SAFE_RELEASE(pBlob);

	return hr;
}


void TileOverlapUpdater::Destroy()
{
	SAFE_RELEASE(m_updateExtraordinaryCB);
	SAFE_RELEASE(m_dispatchIndirectBUF);
}


HRESULT TileOverlapUpdater::UpdateOverlapDisplacement( ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance )
{
	HRESULT hr = S_OK;

if(g_app.g_useCompactedVisibilityOverlap)
{
	if (g_app.g_bTimingsEnabled) 
	{
		g_app.WaitForGPU();
		g_app.g_TimingLog.m_dCompactVisibilityAllTotal -= GetTimeMS()/(double)g_app.g_TimingLog.m_uNumRuns;
		for (UINT i = 0; i < g_app.g_TimingLog.m_uNumRuns; i++)
			CompactIntersectAllBuffer(pd3dImmediateContext, instance);
		g_app.WaitForGPU();
		g_app.g_TimingLog.m_dCompactVisibilityAllTotal += GetTimeMS()/(double)g_app.g_TimingLog.m_uNumRuns;
		g_app.g_TimingLog.m_uCompactVisibilityAllOverlapCount++;
	}
	else
	{
		PERF_EVENT_SCOPED(perf, L"Intersect Buffer Compaction");
		CompactIntersectAllBuffer(pd3dImmediateContext, instance);
	}
}


	if (g_app.g_bTimingsEnabled) {
		g_app.WaitForGPU();
		g_app.g_TimingLog.m_dOverlapTotal -= GetTimeMS()/(double)g_app.g_TimingLog.m_uNumRuns;
		for (UINT i = 0; i < g_app.g_TimingLog.m_uNumRuns; i++)
			UpdateOverlapDisplacementInternal(pd3dImmediateContext, instance);
		g_app.WaitForGPU();
		g_app.g_TimingLog.m_dOverlapTotal += GetTimeMS()/(double)g_app.g_TimingLog.m_uNumRuns;
		g_app.g_TimingLog.m_uOverlapCount++;
	} else {
		PERF_EVENT_SCOPED(perf, L"Update Overlap");
		UpdateOverlapDisplacementInternal(pd3dImmediateContext, instance);
	}

	//if(g_app.g_useCompactedVisibilityOverlap)
	//	ClearIntersectAllBuffer(pd3dImmediateContext, instance);
	return hr;
}


HRESULT TileOverlapUpdater::UpdateOverlapColor( ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance )
{
	
	HRESULT hr = S_OK;
	return hr;
	if(g_app.g_useCompactedVisibilityOverlap)
	{
		if (g_app.g_bTimingsEnabled) 
		{
			g_app.WaitForGPU();
			g_app.g_TimingLog.m_dCompactVisibilityAllTotal -= GetTimeMS()/(double)g_app.g_TimingLog.m_uNumRuns;
			for (UINT i = 0; i < g_app.g_TimingLog.m_uNumRuns; i++)
				CompactIntersectAllBuffer(pd3dImmediateContext, instance);
			g_app.WaitForGPU();
			g_app.g_TimingLog.m_dCompactVisibilityAllTotal += GetTimeMS()/(double)g_app.g_TimingLog.m_uNumRuns;
			g_app.g_TimingLog.m_uCompactVisibilityAllOverlapCount++;
		}
		else
		{
			CompactIntersectAllBuffer(pd3dImmediateContext, instance);
		}
	}

	// TODO own timer for color overlap
	if (g_app.g_bTimingsEnabled) {
		g_app.WaitForGPU();
		g_app.g_TimingLog.m_dOverlapTotal -= GetTimeMS()/(double)g_app.g_TimingLog.m_uNumRuns;
		for (UINT i = 0; i < g_app.g_TimingLog.m_uNumRuns; i++)
			UpdateOverlapColorInternal(pd3dImmediateContext, instance);
		g_app.WaitForGPU();
		g_app.g_TimingLog.m_dOverlapTotal += GetTimeMS()/(double)g_app.g_TimingLog.m_uNumRuns;
		g_app.g_TimingLog.m_uOverlapCount++;
	} else {
		UpdateOverlapColorInternal(pd3dImmediateContext, instance);
	}

	//if(g_app.g_useCompactedVisibilityOverlap)
	//	ClearIntersectAllBuffer(pd3dImmediateContext, instance);

	return hr;
}

HRESULT TileOverlapUpdater::UpdateOverlapExtraordinaryCB( ID3D11DeviceContext1 *pd3dImmediateContext, UINT numExtraordinary ) const
{
	HRESULT hr = S_OK;
	D3D11_MAPPED_SUBRESOURCE MappedResource;

	V_RETURN(pd3dImmediateContext->Map( m_updateExtraordinaryCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ));
	
	CB_UpdateExtarodinary* pCB = ( CB_UpdateExtarodinary* )MappedResource.pData;		
	pCB->numExtraordinary = numExtraordinary;
	pCB->padding = XMUINT3(0,0,0);

	pd3dImmediateContext->Unmap( m_updateExtraordinaryCB, 0 );
	pd3dImmediateContext->CSSetConstantBuffers(CB_LOC::UPDATE_OVERLAP_EXTRAORDINARY, 1, &m_updateExtraordinaryCB );

	return hr;
}

HRESULT TileOverlapUpdater::UpdateOverlapDisplacementInternal( ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance ) const
{
	HRESULT hr = S_OK;
	if(instance->IsSubD())
	{
		// TODO overlap update only on modified
		auto mesh = instance->GetOSDMesh();
		UINT numPtexFaces = mesh->GetNumPTexFaces();

		// update extraordinary		
		
		UpdateOverlapExtraordinaryCB(pd3dImmediateContext,mesh->GetNumExtraordinary());


		ID3D11ShaderResourceView* ppEdgeCornerSRV[] = {	
			instance->GetDisplacementTileLayout()->SRV,	// t0 tile descriptors/layout				
			mesh->GetPTexNeighborDataSRV(),				// t1 ptex neighbor data: UINT4 edge to neighbor ptex ids, UINT4 edge to opposite edgeID
			NULL, NULL,
			instance->GetCompactedVisibility()->SRV		// t4
		};


		ID3D11ShaderResourceView* ppSRVExtraordinary[] = {			
			mesh->GetExtraordinaryInfoSRV(),			// t2 offset and valence per extraordinary vertex
			mesh->GetExtraordinaryDataSRV()				// t3 ptex face id and vertex index in that face (0,1,2,3) of extraordinary vertex
		};

		if(m_constraintsMode)
		{
			
			ID3D11UnorderedAccessView* ppUAV[] = 
			{
				g_memoryManager.GetDisplacementConstraintsUAV()				// u0 displacement data
			};

			pd3dImmediateContext->CSSetShaderResources(0, 5, ppEdgeCornerSRV);
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);		

			if(g_app.g_useCompactedVisibilityOverlap)
			{
				PERF_EVENT_SCOPED(perf, L"Compacted Overlap Update");
				pd3dImmediateContext->CopyStructureCount(m_dispatchIndirectBUF, 0, instance->GetCompactedVisibility()->UAV);
				// update edges
				{
					PERF_EVENT_SCOPED(perf, L"Overlap Update Edges");
					pd3dImmediateContext->CSSetShader(m_updateOverlapCompacted_EdgesDisplacementOSD_CS->Get(), NULL, 0);						
					pd3dImmediateContext->DispatchIndirect(m_dispatchIndirectBUF,0);
				}

				// update corners
				{
					PERF_EVENT_SCOPED(perf, L"Overlap Update Corners");	
					pd3dImmediateContext->CSSetShader(m_updateOverlapCompacted_CornersDisplacementOSD_CS->Get(), NULL, 0);						
					pd3dImmediateContext->DispatchIndirect(m_dispatchIndirectBUF,0);
				}
			}
			else
			{		
				{
					PERF_EVENT_SCOPED(perf, L"Overlap Update Edges");
				// update edges
				pd3dImmediateContext->CSSetShader(m_updateOverlapEdgesDisplacementOSD_CS->Get(), NULL, 0);						
				pd3dImmediateContext->Dispatch(numPtexFaces, 1, 1);
				}
				{
					PERF_EVENT_SCOPED(perf, L"Overlap Update Corners");	
				// update corners
				pd3dImmediateContext->CSSetShader(m_updateOverlapCornersDisplacementOSD_CS->Get(), NULL, 0);						
				pd3dImmediateContext->Dispatch(numPtexFaces, 1, 1);
				}
			}

			// update extraordinary
			{
				PERF_EVENT_SCOPED(perf, L"Overlap Update Extraordinary");									
				pd3dImmediateContext->CSSetShaderResources(2, 2, ppSRVExtraordinary);
				pd3dImmediateContext->CSSetShader(m_updateOverlapEqualizeCornerDisplacementOSD_CS->Get(), NULL, 0);							
				pd3dImmediateContext->Dispatch((mesh->GetNumExtraordinary() + WORK_GROUP_SIZE_EXTRAORDINARY-1)/WORK_GROUP_SIZE_EXTRAORDINARY, 1, 1);
			}

			pd3dImmediateContext->CSSetShaderResources(0, 5, g_ppSRVNULL);
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, g_ppUAVNULL, NULL);
		}
		else
		{		
			ID3D11UnorderedAccessView* ppUAV[] = {	g_memoryManager.GetDisplacementDataUAV()				// u0 displacement data
												 };

			pd3dImmediateContext->CSSetShaderResources(0, 5, ppEdgeCornerSRV);
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);		

			if(g_app.g_useCompactedVisibilityOverlap)
			{
				pd3dImmediateContext->CopyStructureCount(m_dispatchIndirectBUF, 0, instance->GetCompactedVisibility()->UAV);
				// update edges
				pd3dImmediateContext->CSSetShader(m_updateOverlapCompacted_EdgesDisplacementOSD_CS->Get(), NULL, 0);						
				pd3dImmediateContext->DispatchIndirect(m_dispatchIndirectBUF,0);

				// update corners
				pd3dImmediateContext->CSSetShader(m_updateOverlapCompacted_CornersDisplacementOSD_CS->Get(), NULL, 0);						
				pd3dImmediateContext->DispatchIndirect(m_dispatchIndirectBUF,0);
			}
			else
			{		
				// update edges
				pd3dImmediateContext->CSSetShader(m_updateOverlapEdgesDisplacementOSD_CS->Get(), NULL, 0);						
				pd3dImmediateContext->Dispatch(numPtexFaces, 1, 1);

				// update corners
				pd3dImmediateContext->CSSetShader(m_updateOverlapCornersDisplacementOSD_CS->Get(), NULL, 0);						
				pd3dImmediateContext->Dispatch(numPtexFaces, 1, 1);
			}
		}

		// update extraordinary
		pd3dImmediateContext->CSSetShaderResources(2, 2, ppSRVExtraordinary);
		pd3dImmediateContext->CSSetShader(m_updateOverlapEqualizeCornerDisplacementOSD_CS->Get(), NULL, 0);							
		pd3dImmediateContext->Dispatch((mesh->GetNumExtraordinary() + WORK_GROUP_SIZE_EXTRAORDINARY-1)/WORK_GROUP_SIZE_EXTRAORDINARY, 1, 1);

		pd3dImmediateContext->CSSetShaderResources(0, 5, g_ppSRVNULL);
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, g_ppUAVNULL, NULL);		

		
	}
	return hr;
}

HRESULT TileOverlapUpdater::UpdateOverlapColorInternal( ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance ) const
{
	HRESULT hr = S_OK;
	if(instance->IsSubD())
	{	
		// TODO overlap update only on modified
		auto mesh = instance->GetOSDMesh();
		UINT numPtexFaces = mesh->GetNumPTexFaces();

		UpdateOverlapExtraordinaryCB(pd3dImmediateContext,mesh->GetNumExtraordinary());

		ID3D11ShaderResourceView* ppSRV[] = {			
			instance->GetColorTileLayout()->SRV,				// t0 tile descriptors/layout				
			mesh->GetPTexNeighborDataSRV(),					// t1 ptex neighbor data: UINT4 edge to neighbor ptex ids, UINT4 edge to opposite edgeID
			NULL, NULL,
			instance->GetCompactedVisibility()->SRV		// t4
		};

		ID3D11ShaderResourceView* ppSRVExtraordinary[] = {			
			mesh->GetExtraordinaryInfoSRV(),				// t2 offset and valence per extraordinary vertex
			mesh->GetExtraordinaryDataSRV()					// t3 ptex face id and vertex index in that face (0,1,2,3) of extraordinary vertex
		};

		ID3D11UnorderedAccessView* ppUAV[] = 
		{
			g_memoryManager.GetColorDataUAV()				// u0 displacement data
		};


		pd3dImmediateContext->CSSetShaderResources(0, 5, ppSRV);
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);		

		if(g_app.g_useCompactedVisibilityOverlap)
		{
			pd3dImmediateContext->CopyStructureCount(m_dispatchIndirectBUF, 0, instance->GetCompactedVisibility()->UAV);
			// update edges
			pd3dImmediateContext->CSSetShader(m_updateOverlapCompacted_EdgesColorOSD_CS->Get(), NULL, 0);						
			pd3dImmediateContext->DispatchIndirect(m_dispatchIndirectBUF,0);

			// update corners
			pd3dImmediateContext->CSSetShader(m_updateOverlapCompacted_CornersColorOSD_CS->Get(), NULL, 0);						
			pd3dImmediateContext->DispatchIndirect(m_dispatchIndirectBUF,0);
		}
		else
		{	
			// update edges
			pd3dImmediateContext->CSSetShader(m_updateOverlapEdgesColorOSD_CS->Get(), NULL, 0);						
			pd3dImmediateContext->Dispatch(numPtexFaces, 1, 1);

			// update corners
			pd3dImmediateContext->CSSetShader(m_updateOverlapCornersColorOSD_CS->Get(), NULL, 0);						
			pd3dImmediateContext->Dispatch(numPtexFaces, 1, 1);
		}
		// update extraordinary				
		pd3dImmediateContext->CSSetShaderResources(2, 2, ppSRVExtraordinary);
		pd3dImmediateContext->CSSetShader(m_updateOverlapEqualizeCornerColorOSD_CS->Get(), NULL, 0);							
		pd3dImmediateContext->Dispatch((mesh->GetNumExtraordinary() + WORK_GROUP_SIZE_EXTRAORDINARY-1)/WORK_GROUP_SIZE_EXTRAORDINARY, 1, 1);


		pd3dImmediateContext->CSSetShaderResources(0, 5, g_ppSRVNULL);
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, g_ppUAVNULL, NULL);		
	}

	return hr;
}


HRESULT TileOverlapUpdater::CompactIntersectAllBuffer( ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance ) const
{
	HRESULT hr = S_OK;
#define BLOCK_SIZE_COMPACT 32
	if(instance->IsSubD())
	{	
		// TODO overlap update only on modified
		auto mesh = instance->GetOSDMesh();
		UINT numPtexFaces = mesh->GetNumPTexFaces();			

		ID3D11ShaderResourceView* ppSRV[] = {	instance->GetVisibilityAll()->SRV	};			// t0 combined visibility per patch after all collisions
		ID3D11UnorderedAccessView* ppUAV[] = {	instance->GetCompactedVisibility()->UAV	};	// u0 append buffer that will contain only visible patch ids
		
		UINT uavCounterVals[] = {0};
		pd3dImmediateContext->CSSetShaderResources(0, 1, ppSRV);
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, uavCounterVals);		
		
		
		pd3dImmediateContext->CSSetShader(m_compactIntersectAll_CS->Get(), NULL, 0);						
		pd3dImmediateContext->Dispatch((numPtexFaces+(BLOCK_SIZE_COMPACT-1))/BLOCK_SIZE_COMPACT, 1, 1);

		pd3dImmediateContext->CSSetShaderResources(0, 1, g_ppSRVNULL);
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, g_ppUAVNULL, NULL);
	}

	return hr;
}

HRESULT TileOverlapUpdater::ClearIntersectAllBuffer( ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance ) const
{		
	HRESULT hr=S_OK;
	const UINT clearVals[] = {0,0,0,0};
	pd3dImmediateContext->ClearUnorderedAccessViewUint(instance->GetVisibilityAll()->UAV, clearVals );
	//ID3D11UnorderedAccessView* ppUAV[] = {
	//	instance->GetVisibilityAllUAV()							// u0 write per tile intersection result
	//};

	//pd3dImmediateContext->CSSetShader(m_intersectClear_CS->Get(), NULL, 0);
	//pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);
	//pd3dImmediateContext->Dispatch(instance->GetOSDMesh()->GetNumPTexFaces()/512 + 1, 1, 1);	
	//pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, g_ppUAVNULL, NULL);


	return hr;
}

HRESULT TileOverlapUpdater::UpdateOverlapDisplacementConstraints( ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance )
{
	HRESULT hr = S_OK;

	SetConstraintsOverlap(true);
	if(g_app.g_useCompactedVisibilityOverlap)
	{
		PERF_EVENT_SCOPED(perf, L"Overlap Update Compact Intersect Buffer");
		CompactIntersectAllBuffer(pd3dImmediateContext, instance);
	}
	
	UpdateOverlapDisplacementInternal(pd3dImmediateContext, instance);
	
	//if(g_app.g_useCompactedVisibilityOverlap)
	//	ClearIntersectAllBuffer(pd3dImmediateContext, instance);
	SetConstraintsOverlap(false);
	return hr;
}

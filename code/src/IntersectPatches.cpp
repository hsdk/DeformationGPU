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

#include "IntersectPatches.h"
#include "MemoryManager.h"

#include "scene/ModelInstance.h"
#include "scene/DXSubDModel.h"
#include "scene/DXModel.h"


#include <sstream>

using namespace DirectX;

IntersectGPU g_intersectGPU;

EffectRegistryIntersect g_intersectEffectRegistry;

IntersectGPU::IntersectGPU()
{
	m_intersectMode = IntersectMode::Brush;
	
	m_intersectClear_CS = NULL;

	m_osdConfigCB			= NULL;		// constant buffer for osd patch config
	m_intersectModelCB		= NULL;
	
	m_isctMeshMaxValence = 4;
	m_setAllActive = false;	
}

IntersectGPU::~IntersectGPU()
{
	Destroy();
}

HRESULT IntersectGPU::Create( ID3D11Device1* pd3dDevice )
{
	HRESULT hr = S_OK;
	ID3DBlob* pBlob = nullptr;
	m_intersectClear_CS		= g_shaderManager.AddComputeShader(L"shader/IntersectOSDCS.hlsl", "IntersectClearCS", "cs_5_0", &pBlob);
	SAFE_RELEASE(pBlob);	


	// DATA
	// constant buffers	
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_OSDConfig)			, D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, m_osdConfigCB));
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_IntersectModel)		, D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, m_intersectModelCB));	
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_IntersectOBBBatch)	, D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, m_intersectOBBBatchCB));
	


	// create at least two of this or move to modelinstance or osd mesh
	// append buffer for queuing render patches that need work
	// regular patches: we need to store an UINT3( localPatchID + PrimitiveIdBase,  indices_start + NumIndicesPerPatch * localPatchID, g_NumVertexComponents) // TODO num vertex components should always be 3 
	// gregory patches: 
	UINT numPatchDataElements = 600000; // about 7 MB for UINT3 (gregory), 4.5 MB for UINT2 regular - has to be that large for complete displacement map transfer where all patches are active/intersected
	for (uint32_t i = 0; i < DEFORMATION_BATCH_SIZE; ++i)
	{		
		V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, numPatchDataElements*sizeof(UINT) * 2,
			0, D3D11_USAGE_DEFAULT, m_patchAppendRegular[i].BUF, NULL, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, sizeof(UINT) * 2));

		V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, numPatchDataElements*sizeof(UINT) * 3,
								  0, D3D11_USAGE_DEFAULT, m_patchAppendGregory[i].BUF, NULL, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, sizeof(UINT) * 3));

		// SRV
		D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
		ZeroMemory(&descSRV, sizeof(descSRV));
		descSRV.Format = DXGI_FORMAT_UNKNOWN;
		descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		descSRV.Buffer.FirstElement = 0;
		descSRV.Buffer.NumElements = numPatchDataElements;

		V_RETURN(pd3dDevice->CreateShaderResourceView(m_patchAppendRegular[i].BUF, &descSRV, &m_patchAppendRegular[i].SRV));
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_patchAppendGregory[i].BUF, &descSRV, &m_patchAppendGregory[i].SRV));


		// UAV
		D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
		ZeroMemory(&descUAV, sizeof(descUAV));
		descUAV.Format = DXGI_FORMAT_UNKNOWN;

		descUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		descUAV.Buffer.FirstElement = 0;
		descUAV.Buffer.NumElements = numPatchDataElements;
		descUAV.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;

		V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_patchAppendRegular[i].BUF, &descUAV, &m_patchAppendRegular[i].UAV));
		V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_patchAppendGregory[i].BUF, &descUAV, &m_patchAppendGregory[i].UAV));
	}
	return hr;
}

void IntersectGPU::Destroy()
{
	SAFE_RELEASE(m_osdConfigCB);			
	SAFE_RELEASE(m_intersectModelCB);
	SAFE_RELEASE(m_intersectOBBBatchCB);
	
	for (int i = 0; i < DEFORMATION_BATCH_SIZE; ++i)
	{
		m_patchAppendRegular[i].Destroy();
		m_patchAppendGregory[i].Destroy();
	}

	g_intersectEffectRegistry.Reset();
}

//HRESULT IntersectGPU::IntersectModel( ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance, D3D11ObjectOrientedBoundingBox* obb )
//{
//	HRESULT hr = S_OK;
//
//	m_intersectMode = IntersectMode::OBB;
//
//	//V_RETURN(UpdateIntersectCB(pd3dImmediateContext, instance));
//
//	// set ModelOS to OBB matrix
//	XMMATRIX model2OBB = instance->GetModelMatrix() * obb->getWorldToOOBB();		// modelToWorld * world2OBB
//	// todo use model max displacement
//	{	
//		D3D11_MAPPED_SUBRESOURCE MappedResource;
//		V_RETURN(pd3dImmediateContext->Map( m_intersectModelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ));
//
//		CB_IntersectModel* pCB = ( CB_IntersectModel* )MappedResource.pData;		
//		pCB->modelToWorld = model2OBB;
//		pCB->g_displacementScale = g_app.g_fDisplacementScalar; 
//		pCB->padding = XMFLOAT3(0,0,0);
//		pd3dImmediateContext->Unmap( m_intersectModelCB, 0 );
//
//		pd3dImmediateContext->CSSetConstantBuffers( CB_LOC::INTERSECT, 1, &m_intersectModelCB );
//	}
//
//		
//	if(instance->IsSubD())
//	{
//		ClearIntersectBuffer(pd3dImmediateContext, instance);
//
//		if (g_app.g_bTimingsEnabled) 
//		{
//			g_app.WaitForGPU();
//			g_app.g_TimingLog.m_dCullingTotal -= GetTimeMS()/(double)g_app.g_TimingLog.m_uNumRuns;
//			for (UINT i = 0; i < g_app.g_TimingLog.m_uNumRuns; i++)
//			{
//				hr = IntersectOSD(pd3dImmediateContext, instance);
//			}
//			g_app.WaitForGPU();
//			g_app.g_TimingLog.m_dCullingTotal += GetTimeMS()/(double)g_app.g_TimingLog.m_uNumRuns;
//			g_app.g_TimingLog.m_uCullingCount++;
//		} 
//		else 
//		{
//			hr = IntersectOSD(pd3dImmediateContext, instance);
//		}
//	}
//
//	return hr;
//}

HRESULT IntersectGPU::IntersectOBBBatch(ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* deformableInstance, const std::unordered_map<ModelInstance*, DXObjectOrientedBoundingBox>& batch)
{
	HRESULT hr = S_OK;
	m_intersectMode = IntersectMode::OBB;
		
	{
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		V_RETURN(pd3dImmediateContext->Map(m_intersectOBBBatchCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_IntersectOBBBatch* pCB = (CB_IntersectOBBBatch*)MappedResource.pData;

		int penetratorID = 0;
		for (auto penetrator : batch)
		{
			const DXObjectOrientedBoundingBox& isctObb = penetrator.second;					
			pCB->modelToWorld[penetratorID] = deformableInstance->GetModelMatrix() * isctObb.getWorldToOOBB(); // modelToWorld * world2OBB					
			penetratorID++;
		}
		pCB->g_displacementScale = g_app.g_fDisplacementScalar;

		pd3dImmediateContext->Unmap(m_intersectOBBBatchCB, 0);
		pd3dImmediateContext->CSSetConstantBuffers(CB_LOC::INTERSECT, 1, &m_intersectOBBBatchCB);
	}
	
	if (deformableInstance->IsSubD())
	{
		//std::cout << "batch size " << batch.size() << std::endl;
		ClearIntersectBuffer(pd3dImmediateContext, deformableInstance);

		if (g_app.g_bTimingsEnabled)
		{
			g_app.WaitForGPU();
			g_app.g_TimingLog.m_dCullingTotal -= GetTimeMS() / (double)g_app.g_TimingLog.m_uNumRuns;
			for (UINT i = 0; i < g_app.g_TimingLog.m_uNumRuns; i++)
			{
				hr = IntersectOSDBatch(pd3dImmediateContext, deformableInstance, static_cast<uint32_t>(batch.size()));
			}
			g_app.WaitForGPU();
			g_app.g_TimingLog.m_dCullingTotal += GetTimeMS() / (double)g_app.g_TimingLog.m_uNumRuns;
			g_app.g_TimingLog.m_uCullingCount += static_cast<uint32_t>(batch.size());
		}
		else
		{
			hr = IntersectOSDBatch(pd3dImmediateContext, deformableInstance, static_cast<uint32_t>(batch.size()));
		}
	}


	return hr;
}

void IntersectGPU::ClearIntersectBuffer( ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance )
{	

	ID3D11UnorderedAccessView* ppUAV[] = {
		instance->GetVisibility()->UAV							// u0 write per tile intersection result
	};

	pd3dImmediateContext->CSSetShader(m_intersectClear_CS->Get(), NULL, 0);
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);
	pd3dImmediateContext->Dispatch(instance->GetOSDMesh()->GetNumPTexFaces()/512 + 1, 1, 1);	
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, g_ppUAVNULL, NULL);
	
}

HRESULT IntersectGPU::UpdateIntersectCB( ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance )
{
	HRESULT hr = S_OK;
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V_RETURN(pd3dImmediateContext->Map( m_intersectModelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ));

	CB_IntersectModel* pCB = ( CB_IntersectModel* )MappedResource.pData;		
	pCB->modelToWorld = instance->GetModelMatrix();
	pCB->g_displacementScale = g_app.g_fDisplacementScalar; 
	pCB->padding = XMFLOAT3(0,0,0);
	pd3dImmediateContext->Unmap( m_intersectModelCB, 0 );
	
	pd3dImmediateContext->CSSetConstantBuffers( CB_LOC::INTERSECT, 1, &m_intersectModelCB );

	return hr;
}


HRESULT IntersectGPU::IntersectOSDBatch(ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance, uint32_t batchSize)
{
	HRESULT hr = S_OK;

	PERF_EVENT_SCOPED(perf, L"Intersect OBB");

	DXOSDMesh* mesh = instance->GetOSDMesh();

	auto osdMesh = mesh->GetMesh();
	//UINT strides = mesh->GetNumVertexElements();

	const auto& patches = osdMesh->GetDrawContext()->patchArrays;

	assert(instance->GetVisibility()->UAV);

	ID3D11ShaderResourceView* ppSRV[] = { osdMesh->GetDrawContext()->vertexBufferSRV,				// t0 vertex buffer, float, numcomponents set in cb
		osdMesh->GetDrawContext()->patchIndexBufferSRV,			// t1 index buffer			
		osdMesh->GetDrawContext()->ptexCoordinateBufferSRV,		// t2 tile rotation info and patch to tile mapping
		osdMesh->GetDrawContext()->vertexValenceBufferSRV,
		osdMesh->GetDrawContext()->quadOffsetBufferSRV,
		instance->GetMaxDisplacement()->SRV
	};


	ID3D11UnorderedAccessView* ppUAV[] = {  instance->GetVisibility()->UAV,							// u0 write per tile intersection result
											instance->GetVisibilityAll()->UAV,
	};

	ID3D11UnorderedAccessView* ppRegularUAV[] = {
											m_patchAppendRegular[0].UAV,
		
											#if DEFORMATION_BATCH_SIZE > 1
											m_patchAppendRegular[1].UAV,		
											#endif
											#if DEFORMATION_BATCH_SIZE > 2
											m_patchAppendRegular[2].UAV,		
											#endif
											#if DEFORMATION_BATCH_SIZE > 3
											m_patchAppendRegular[3].UAV,		
											#endif
											#if DEFORMATION_BATCH_SIZE > 4
											m_patchAppendRegular[4].UAV,		
											#endif
											#if DEFORMATION_BATCH_SIZE > 5
											m_patchAppendRegular[5].UAV,		
											#endif
											#if DEFORMATION_BATCH_SIZE > 6
											m_patchAppendRegular[6].UAV,		
											#endif
										#if DEFORMATION_BATCH_SIZE > 7
											m_patchAppendRegular[7].UAV,		
										#endif
	};

	ID3D11UnorderedAccessView* ppGregoryUAV[] = {											
											m_patchAppendGregory[0].UAV,
										#if DEFORMATION_BATCH_SIZE > 1											
											m_patchAppendGregory[1].UAV,
										#endif
										#if DEFORMATION_BATCH_SIZE > 2											
											m_patchAppendGregory[2].UAV,
										#endif
										#if DEFORMATION_BATCH_SIZE > 3											
											m_patchAppendGregory[3].UAV,
										#endif
										#if DEFORMATION_BATCH_SIZE > 4											
											m_patchAppendGregory[4].UAV,
										#endif
										#if DEFORMATION_BATCH_SIZE > 5											
											m_patchAppendGregory[5].UAV,
										#endif
										#if DEFORMATION_BATCH_SIZE > 6											
											m_patchAppendGregory[6].UAV,
										#endif
										#if DEFORMATION_BATCH_SIZE > 7											
											m_patchAppendGregory[7].UAV,
										#endif	
	};

	UINT uavCounterValsInit[] = { 0, 0, 0, 0, 0, 0,  0, 0,  0, 0,  0, 0};
	UINT uavCounterValsInitDone[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
	pd3dImmediateContext->CSSetShaderResources(0, 6, ppSRV);
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, ppUAV, uavCounterValsInit);
	pd3dImmediateContext->CSSetUnorderedAccessViews(2, batchSize, ppGregoryUAV, uavCounterValsInit);
	pd3dImmediateContext->CSSetUnorderedAccessViews(2, batchSize, ppRegularUAV, uavCounterValsInit);
	

	UINT numRuns = 1;
	if (g_app.g_withPaintSculptTimings)
	{
		g_app.GPUPerfTimerStart(pd3dImmediateContext);
		numRuns = 100;
	}
	bool isRegular = false;


	for (UINT i = 0; i < numRuns; ++i)
	{
		for (const auto& patch : patches)
		{
			auto numPatches = patch.GetNumPatches();
			if (numPatches == 0) continue;

			if (	patch.GetDescriptor().GetType() != OpenSubdiv::OPENSUBDIV_VERSION::FarPatchTables::REGULAR
				&&	patch.GetDescriptor().GetType() != OpenSubdiv::OPENSUBDIV_VERSION::FarPatchTables::GREGORY)			
				continue;

			if (patch.GetDescriptor().GetType() == OpenSubdiv::OPENSUBDIV_VERSION::FarPatchTables::GREGORY)
			{
				pd3dImmediateContext->CSSetUnorderedAccessViews(2, batchSize, ppGregoryUAV, NULL);
			}
			else
			{
				pd3dImmediateContext->CSSetUnorderedAccessViews(2, batchSize, ppRegularUAV, NULL);
			}

			// Problem 1: we dont have end regular patches in opensubdiv
			// opensubdiv packs patches in arrays by type(e.g. regular, boundary,...), pattern (no transition, transition 1..) and subpatches in patterns
			// workaround: run intersection for each pattern only on subpatch 0
			if (patch.GetDescriptor().GetSubPatch() > 0) continue;

			//if(patch.GetDescriptor().GetType() == OpenSubdiv::OPENSUBDIV_VERSION::FarPatchTables::GREGORY)	continue;

			isRegular = patch.GetDescriptor().GetType() == OpenSubdiv::OPENSUBDIV_VERSION::FarPatchTables::REGULAR ? true : false;

			IntersectConfig config;
			config.value = 0; // resets all 
			config.batch_size = batchSize;
			config.face_mode = static_cast<unsigned int>(IntersectFaceType::OSD);
			config.patch_type = isRegular ? static_cast<unsigned int>(IntersectPatchType::REGULAR) : static_cast<unsigned int>(IntersectPatchType::GREGORY);
			config.isct_mode = static_cast<unsigned int>(m_intersectMode);
			config.max_valence = patch.GetDescriptor().GetMaxValence();
			config.all_active = m_setAllActive;
			config.use_maxdisp = true;
			m_isctMeshMaxValence = patch.GetDescriptor().GetMaxValence();
			
			BindShaders(pd3dImmediateContext, config, instance);

			// Update config state	
			{
				assert(m_osdConfigCB);

				D3D11_MAPPED_SUBRESOURCE MappedResource;
				pd3dImmediateContext->Map(m_osdConfigCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				CB_OSDConfig * pData = (CB_OSDConfig*)MappedResource.pData;
				//std::cout << "greg quad offset" << patch.GetQuadOffsetIndex() << std::endl;
				//pData->TessLevel = static_cast<float>(int(g_app.g_fTessellationFactor+0.5f));
				pData->GregoryQuadOffsetBase = patch.GetQuadOffsetIndex();
				pData->PrimitiveIdBase = patch.GetPatchIndex();			// patch id for ptex/far table access
				pData->IndexStart = patch.GetVertIndex();			// index of first control vertex in global index array							
				pData->NumVertexComponents = mesh->GetNumVertexElements();	// for stride in vertex buffer			
				pData->NumIndicesPerPatch = patch.GetDescriptor().GetNumControlVertices(); // how many indices per patch, todo set using define in shader
				pData->NumPatches = numPatches;

				pd3dImmediateContext->Unmap(m_osdConfigCB, 0);
				pd3dImmediateContext->CSSetConstantBuffers(CB_LOC::OSD_DEFORMATION_CONFIG, 1, &m_osdConfigCB);
			}

			//if(isRegular)
			//{
			//	if(m_intersectMode == IntersectMode::Brush)	
			//		pd3dImmediateContext->CSSetShader(m_intersectBrush_OSD_CS->Get(), NULL, 0);
			//	else
			//		pd3dImmediateContext->CSSetShader(m_intersectOBB_OSD_CS->Get(), NULL, 0);
			//}
			//else
			//{
			//	// gregory
			//	bool maxValenceLess11 = patch.GetDescriptor().GetMaxValence()<11 ? true : false;
			//	if(m_intersectMode == IntersectMode::Brush)	
			//	{
			//		pd3dImmediateContext->CSSetShader(maxValenceLess11 ? m_intersectBrush_OSD_Irregular_CS->Get() : m_intersectBrush_OSD_Irregular_Valence_Greater_10_CS->Get() , NULL, 0);
			//	}
			//	else
			//	{
			//		pd3dImmediateContext->CSSetShader(maxValenceLess11 ? m_intersectOBB_OSD_CS->Get() : m_intersectOBB_OSD_Irregular_Valence_Greater_10_CS->Get(), NULL, 0);
			//	}
			//}		





			//pd3dImmediateContext->Dispatch(numPatches,1,1);				// 1 patch per block
			pd3dImmediateContext->Dispatch((numPatches + 1) / 2, 1, 1);	// 2 patches per block
		}

		pd3dImmediateContext->CSSetShaderResources(0, 6, g_ppSRVNULL);
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2+batchSize, g_ppUAVNULL, NULL);
	}

	// TODO write compacted intersect buffer using append buffer
	// this will speed up memory management and update overlap

	if (g_app.g_withPaintSculptTimings)
	{
		g_app.GPUPerfTimerEnd(pd3dImmediateContext);
		double elapsed = g_app.GPUPerfTimerElapsed(pd3dImmediateContext);
		std::cout << "intersect time:" << elapsed / numRuns << "ms " << std::endl;
	}

	//else
	//{
	//	for(const auto& patch : patches)
	//	{
	//		auto numPatches = patch.GetNumPatches();

	//		if(numPatches == 0) continue;

	//		if(patch.GetDescriptor().GetType() != OpenSubdiv::OPENSUBDIV_VERSION::FarPatchTables::REGULAR)			// for now only regular
	//			continue;

	//		// Problem 1: we dont have end regular patches in opensubdiv
	//		// opensubdiv packs patches in arrays by type(e.g. regular, boundary,...), pattern (no transition, transition 1..) and subpatches in patterns
	//		// workaround: run intersection for each pattern only on subpatch 0
	//		if(patch.GetDescriptor().GetSubPatch() > 0) continue;

	//		// Update config state	
	//		{	
	//			assert(m_osdConfigCB);

	//			D3D11_MAPPED_SUBRESOURCE MappedResource;
	//			pd3dImmediateContext->Map(m_osdConfigCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	//			CB_OSDConfig * pData = ( CB_OSDConfig* )MappedResource.pData;

	//			//pData->TessLevel = static_cast<float>(int(g_app.g_fTessellationFactor+0.5f));
	//			pData->GregoryQuadOffsetBase = patch.GetQuadOffsetIndex();
	//			pData->PrimitiveIdBase		 = patch.GetPatchIndex();			// patch id for ptex/far table access
	//			pData->IndexStart			 = patch.GetVertIndex();			// index of first control vertex in global index array			
	//			pData->NumVertexComponents   = mesh->GetNumVertexElements();	// for stride in vertex buffer			
	//			pData->NumIndicesPerPatch	 = patch.GetDescriptor().GetNumControlVertices(); // how many indices per patch, todo set using define in shader
	//			pData->NumPatches			 = numPatches;

	//			pd3dImmediateContext->Unmap( m_osdConfigCB, 0 );
	//			pd3dImmediateContext->CSSetConstantBuffers( CB_LOC::OSD_DEFORMATION_CONFIG, 1, &m_osdConfigCB );
	//		}


	//		//pd3dImmediateContext->Dispatch(numPatches,1,1);				// 1 patch per block
	//		pd3dImmediateContext->Dispatch((numPatches+1)/2, 1, 1);	// 2 patches per block
	//	}

	//	pd3dImmediateContext->CSSetShaderResources(0, 3, g_ppSRVNULL);
	//	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, g_ppUAVNULL, NULL);
	//}


	// debug check visibility buffer
	if (0)
	{
		int numFaces = instance->GetOSDMesh()->GetNumPTexFaces();

		ID3D11Buffer* stagingBUF = NULL;
		V_RETURN(DXCreateBuffer(DXUTGetD3D11Device(), 0, numFaces*sizeof(UCHAR), D3D11_CPU_ACCESS_READ, D3D11_USAGE_STAGING, stagingBUF));

		pd3dImmediateContext->CopyResource(stagingBUF, instance->GetVisibility()->BUF);
		//g_app.WaitForGPU();

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		V_RETURN(pd3dImmediateContext->Map(stagingBUF, D3D11CalcSubresource(0, 0, 0), D3D11_MAP_READ, 0, &mappedResource));
		UCHAR *cpuMemory = (UCHAR*)mappedResource.pData;

		UINT countVisibile = 0;
		for (int i = 0; i < numFaces; ++i)
		{
			if (cpuMemory[i] != 0)	countVisibile++;
		}
		pd3dImmediateContext->Unmap(stagingBUF, 0);

		std::cout << "num visibile " << countVisibile << std::endl;
		SAFE_RELEASE(stagingBUF);
	}

	if (0)
	{
		for (uint32_t i = 0; i < batchSize; ++i)
		{
			ID3D11Buffer* stagingBUF;
			DXCreateBuffer(DXUTGetD3D11Device(), 0, 1 * sizeof(UINT), D3D11_CPU_ACCESS_READ, D3D11_USAGE_STAGING, stagingBUF);
			pd3dImmediateContext->CopyStructureCount(stagingBUF, 0, m_patchAppendRegular[i].UAV);

			UINT numWorkRenderPatches = 0;
			D3D11_MAPPED_SUBRESOURCE MappedResource;
			pd3dImmediateContext->Map(stagingBUF, 0, D3D11_MAP_READ, 0, &MappedResource);
			numWorkRenderPatches = ((UINT*)MappedResource.pData)[0];
			pd3dImmediateContext->Unmap(stagingBUF, 0);
			SAFE_RELEASE(stagingBUF);

			std::cerr << "BatchIdx: " << i << " COUNT: " << numWorkRenderPatches << std::endl;
		}
	}

	return hr;
}

HRESULT IntersectGPU::CreateVisibilityBuffer( ID3D11Device1* pd3dDevice, UINT numTiles, ID3D11Buffer*& visibilityBUF, ID3D11ShaderResourceView*& visibilitySRV, ID3D11UnorderedAccessView*& visibilityUAV ) const
{
	HRESULT hr = S_OK;
	//V_RETURN(DXUTCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE |D3D11_BIND_UNORDERED_ACCESS, numTiles * sizeof(UCHAR), 0, D3D11_USAGE_DEFAULT, visibilityBUF));
	//DXGI_FORMAT format = DXGI_FORMAT_R8_UINT;

	// we want UINT32 to reuse this buffer as compacted buffer
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE |D3D11_BIND_UNORDERED_ACCESS, numTiles * sizeof(UINT), 0, D3D11_USAGE_DEFAULT, visibilityBUF));
	DXGI_FORMAT format = DXGI_FORMAT_R32_UINT;

	int numElements = numTiles;
	{			
		D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
		ZeroMemory(&descSRV, sizeof(descSRV));
		descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		descSRV.Format = format;			
		descSRV.Buffer.FirstElement = 0;
		descSRV.Buffer.NumElements = numElements;
		V_RETURN(pd3dDevice->CreateShaderResourceView(visibilityBUF, &descSRV, &visibilitySRV));
	}

	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
		ZeroMemory(&descUAV, sizeof(descUAV));
		descUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		descUAV.Format = format;				
		descUAV.Buffer.FirstElement = 0;
		descUAV.Buffer.NumElements = numElements;				
		V_RETURN(pd3dDevice->CreateUnorderedAccessView(visibilityBUF, &descUAV, &visibilityUAV));
	}

	return hr;
}


HRESULT IntersectGPU::CreateCompactedVisibilityBuffer( ID3D11Device1* pd3dDevice, UINT numTiles, ID3D11Buffer*& visibilityBUF, ID3D11ShaderResourceView*& visibilitySRV, ID3D11UnorderedAccessView*& visibilityUAV ) const
{
	// append buffer with tile ids that are visible
	HRESULT hr = S_OK;
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE |D3D11_BIND_UNORDERED_ACCESS, numTiles * sizeof(UINT), 0, D3D11_USAGE_DEFAULT, visibilityBUF,NULL, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, sizeof(UINT)));
	DXGI_FORMAT format = DXGI_FORMAT_R32_UINT;

	int numElements = numTiles;

	{			
		D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
		ZeroMemory(&descSRV, sizeof(descSRV));
		descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		descSRV.Format = DXGI_FORMAT_UNKNOWN;			
		descSRV.Buffer.FirstElement = 0;
		descSRV.Buffer.NumElements = numElements;
		V_RETURN(pd3dDevice->CreateShaderResourceView(visibilityBUF, &descSRV, &visibilitySRV));
	}

	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
		ZeroMemory(&descUAV, sizeof(descUAV));
		descUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		descUAV.Format = DXGI_FORMAT_UNKNOWN;				
		descUAV.Buffer.FirstElement = 0;
		descUAV.Buffer.NumElements = numElements;	
		descUAV.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
		V_RETURN(pd3dDevice->CreateUnorderedAccessView(visibilityBUF, &descUAV, &visibilityUAV));
	}	
	return hr;
}

void IntersectGPU::BindShaders( ID3D11DeviceContext1* pd3dImmediateContext, const IntersectConfig effect, const ModelInstance* instance ) const
{
	const D3D11_INPUT_ELEMENT_DESC hInElementDesc[] = 	{		{"POSITION" , 0, DXGI_FORMAT_R32G32B32A32_FLOAT	, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}	};

	ID3D11InputLayout* tmp = NULL;
	EffectRegistryIntersect::ConfigType * config = g_intersectEffectRegistry.GetDrawConfig( effect, DXUTGetD3D11Device(),
													&(const_cast<ID3D11InputLayout*>(tmp)), hInElementDesc, ARRAYSIZE(hInElementDesc));

	SAFE_RELEASE(tmp);

	pd3dImmediateContext->CSSetShader(config->computeShader->Get(),NULL, 0);

}

HRESULT IntersectGPU::SetAllActive( ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance )
{
	HRESULT hr = S_OK;
	m_setAllActive = true;
	m_intersectMode = IntersectMode::NONE;
	if(instance->IsSubD())
	{
		ClearIntersectBuffer(pd3dImmediateContext, instance);

		V_RETURN(IntersectOSDBatch(pd3dImmediateContext, instance,1));
		
	}
	m_setAllActive = false;
	return hr;
}

EffectRegistryIntersect::ConfigType * EffectRegistryIntersect::_CreateDrawConfig( DescType const & desc, SourceConfigType const * sconfig, ID3D11Device1 * pd3dDevice, ID3D11InputLayout ** ppInputLayout, D3D11_INPUT_ELEMENT_DESC const * pInputElementDescs, int numInputElements ) const  // make const
{
	ConfigType * config = DrawRegistryBase::_CreateDrawConfig(desc.value, sconfig, pd3dDevice, ppInputLayout, pInputElementDescs, numInputElements);
	assert(config);

	return config;
}

EffectRegistryIntersect::SourceConfigType * EffectRegistryIntersect::_CreateDrawSourceConfig( DescType const & desc, ID3D11Device1 * pd3dDevice ) const// make const
{
	IntersectConfig effect = desc;

	SourceConfigType * sconfig = DrawRegistryBase::_CreateDrawSourceConfig(desc.value, pd3dDevice);
	assert(sconfig);


	sconfig->commonConfig.common_render_hash = effect.value;
	sconfig->value = effect.value;

	const wchar_t* shaderFileOSD = L"shader/IntersectOSDCS.hlsl";

	std::ostringstream ss;

	if (effect.face_mode == (UINT)IntersectFaceType::OSD)
	{
		sconfig->computeShader.sourceFile = shaderFileOSD;

		if (effect.isct_mode == (UINT)IntersectMode::Brush)
		{
			sconfig->computeShader.AddDefine("INTERSECT_BRUSH");

		}
		else if (effect.isct_mode == (UINT)IntersectMode::OBB)
		{
			sconfig->computeShader.AddDefine("INTERSECT_OBB");
		}


		if (effect.patch_type == (UINT)IntersectPatchType::GREGORY)
		{
			ss.str("");	ss << effect.max_valence;
			sconfig->computeShader.AddDefine("OSD_MAX_VALENCE", ss.str());
			sconfig->computeShader.entry = "IntersectGregoryCS";
			sconfig->computeShader.AddDefine("TYPE_GREGORY");

		}
		else
		{
			sconfig->computeShader.entry = "IntersectRegularCS";
		}

		{
			ss.str("");	ss << effect.batch_size;
			sconfig->computeShader.AddDefine("BATCH_SIZE", ss.str());
		}

		if (effect.all_active)
			sconfig->computeShader.AddDefine("SET_ALL_ACTIVE");

		if(effect.use_maxdisp)
		{
			sconfig->computeShader.AddDefine("WITH_DYNAMIC_MAX_DISP");
		}
	}

	return sconfig;
}

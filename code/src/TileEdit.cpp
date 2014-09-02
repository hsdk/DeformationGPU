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

#include "TileEdit.h"
#include "MemoryManager.h"
#include "IntersectPatches.h"
#include "TileOverlapUpdater.h"

#include <SDX/DXShaderManager.h>
#include "utils/MathHelpers.h"
#include "scene/DXSubDModel.h"
#include "scene/DXModel.h"
#include "scene/ModelInstance.h"

#include <sstream>

//Henry: has to be last header
#include "utils/DbgNew.h"

using namespace DirectX;

#define COLOR_DISPATCH_TILE_SIZE 16
#define DISPLACEMENT_DISPATCH_TILE_SIZE 16
#define WORK_GROUP_SIZE_EXTRAORDINARY 128

TileEdit					g_deformation;

EffectRegistryPaintDeform g_paintDeformEffectRegistry;

TileEdit::TileEdit()
{		
	m_intersectModelCB = NULL;
	m_VoxelGridCB = NULL;
	m_osdCB = NULL;
	m_dispatchIndirectBUF = NULL;
	m_samplerBilinear = NULL;
	m_samplerNearest = NULL;
}

TileEdit::~TileEdit()
{
	Destroy();
}

HRESULT TileEdit::Create( ID3D11Device1* pd3dDevice )
{
	HRESULT hr = S_OK;

	// constant buffers	
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_IntersectModel)		, D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, m_intersectModelCB));
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_VoxelGrid),		  D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, m_VoxelGridCB));
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_OSDConfig),		  D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, m_osdCB));


	UINT NUM_BLOCKS_COLOR =  g_app.g_colorTileSize / COLOR_DISPATCH_TILE_SIZE;
	UINT NUM_BLOCKS_DISP =  g_app.g_displacementTileSize / DISPLACEMENT_DISPATCH_TILE_SIZE;
	UINT indirectInitialState[] = {
		0, NUM_BLOCKS_COLOR*NUM_BLOCKS_COLOR, 1, 1, // COLOR
		0, NUM_BLOCKS_DISP*NUM_BLOCKS_DISP, 1, 1,
	};
	V_RETURN(DXCreateBuffer(DXUTGetD3D11Device(),	0,
							 2*sizeof(UINT)*4, 0,	D3D11_USAGE_DEFAULT,
							 m_dispatchIndirectBUF, indirectInitialState, D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS, sizeof(UINT)*4));
	DXUT_SetDebugName(m_dispatchIndirectBUF, "m_dispatchIndirectBUF");

	D3D11_SAMPLER_DESC sdesc;
	ZeroMemory(&sdesc,sizeof(sdesc));
	sdesc.AddressU	= D3D11_TEXTURE_ADDRESS_WRAP;
	sdesc.AddressV	= D3D11_TEXTURE_ADDRESS_WRAP;
	sdesc.AddressW	= D3D11_TEXTURE_ADDRESS_WRAP;
	sdesc.Filter	= D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;	
	V_RETURN(DXUTGetD3D11Device()->CreateSamplerState(&sdesc, &m_samplerBilinear));
	DXUT_SetDebugName(m_samplerBilinear, "BrushEdit Sampler (m_sampler)");

	sdesc.Filter	= D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	V_RETURN(DXUTGetD3D11Device()->CreateSamplerState(&sdesc, &m_samplerNearest));
	DXUT_SetDebugName(m_samplerNearest, "BrushEdit Sampler (m_samplerNearest)");

	return hr;
}

void TileEdit::Destroy()
{
	SAFE_RELEASE(m_intersectModelCB);
	SAFE_RELEASE(m_VoxelGridCB);
	SAFE_RELEASE(m_osdCB);

	SAFE_RELEASE(m_dispatchIndirectBUF);

	SAFE_RELEASE(m_samplerBilinear);
	SAFE_RELEASE(m_samplerNearest);
	
	g_paintDeformEffectRegistry.Reset();
}

void TileEdit::SetVoxelGridCB(ID3D11DeviceContext1* pd3dImmediateContext, Voxelizable* penetratorVoxelization, ModelInstance* deformableInstance) const
{
	const VoxelGridDefinition& gridDef = penetratorVoxelization->GetVoxelGridDefinition();
	//XMMATRIX mModel2World = XMMatrixIdentity(); //XMLoadFloat4x4A(&mesh->GetModelMatrix());	// checkme set model matrix from bullet?	
	XMMATRIX mModel2World = deformableInstance->GetModelMatrix(); //XMLoadFloat4x4A(&mesh->GetModelMatrix());	// checkme set model matrix from bullet?	
	XMMATRIX mWorld2Voxel = gridDef.m_MatrixWorldToVoxel;				// checkme
	XMMATRIX model2Voxel =  mModel2World * mWorld2Voxel;

	XMMATRIX normalMatrix = XMMatrixTranspose(XMMatrixInverse(NULL, model2Voxel));

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map( m_VoxelGridCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CB_VoxelGrid* pCB = ( CB_VoxelGrid* )MappedResource.pData;					
	pCB->m_matModelToProj = normalMatrix; // set normal matrix here
	pCB->m_matModelToVoxel = model2Voxel;
	pCB->m_stride[0] = gridDef.m_StrideX;
	pCB->m_stride[1] = gridDef.m_StrideY;
	pCB->m_stride[2] = 0;
	pCB->m_stride[3] = 0;
	pCB->m_gridSize = gridDef.m_VoxelGridSize;
	//pCB->padding = 0;
	pd3dImmediateContext->Unmap( m_VoxelGridCB, 0 );
	DXUTGetD3D11DeviceContext()->CSSetConstantBuffers( CB_LOC::VOXELGRID, 1, &m_VoxelGridCB );
}

HRESULT TileEdit::Apply(ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* targetInstance, IntersectGPU* intersect, uint32_t batchIdx, ModelInstance* penetratorVoxelization)
{
	HRESULT hr = S_OK;

	pd3dImmediateContext->CSSetSamplers(0, 1, &m_samplerBilinear);

	// set modelToWorld and max displacement cb
	{
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		pd3dImmediateContext->Map( m_intersectModelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );

		CB_IntersectModel* pCB = ( CB_IntersectModel* )MappedResource.pData;		
		pCB->modelToWorld = targetInstance->GetModelMatrix();
		pCB->g_displacementScale = g_app.g_fDisplacementScalar; 
		pCB->padding = XMFLOAT3(0,0,0);

		pd3dImmediateContext->Unmap( m_intersectModelCB, 0 );
	}
	pd3dImmediateContext->CSSetConstantBuffers( CB_LOC::INTERSECT, 1, &m_intersectModelCB );
		
	PaintDeformConfig config;	
	config.patch_type = (UINT)EditPatchType::REGULAR;	
	config.num_vertex_components = targetInstance->GetOSDMesh()->GetNumVertexElements();
	config.max_valence = intersect->GetMaxValenceLastIntersected();
	config.voxel_multisampling = g_app.g_voxelDeformationMultiSampling;
	config.with_culling = g_app.g_useCullingForRayCast ? 1 : 0;
	config.continuous = g_app.g_enableOffsetUV ? 1: 0;
	config.with_constraints = g_app.g_useDisplacementConstraints ? 1 : 0;
	config.displacement_tile_size = log2Integer(g_app.g_displacementTileSize);	

	config.update_max_disp = true; // CHECKME hardcoded


	if(g_app.g_useCullingForRayCast)
	{		
		// REGULAR
		{
			PERF_EVENT_SCOPED(perf,L"Brush Edit Regular");	
			pd3dImmediateContext->CopyStructureCount(m_dispatchIndirectBUF, 4 * sizeof(UINT), intersect->GetIntersectedPatchesOSDRegularUAV(batchIdx));
			BindShaders(pd3dImmediateContext, config, targetInstance, intersect, penetratorVoxelization, batchIdx);
			pd3dImmediateContext->DispatchIndirect(m_dispatchIndirectBUF, sizeof(UINT)*4);
		}

		// Gregory
		{
			PERF_EVENT_SCOPED(perf,L"Brush Edit Gregory");	
		
			config.patch_type = (UINT)EditPatchType::GREGORY;			
			pd3dImmediateContext->CopyStructureCount(m_dispatchIndirectBUF, 4 * sizeof(UINT), intersect->GetIntersectedPatchesOSDGregoryUAV(batchIdx));			
			BindShaders(pd3dImmediateContext, config, targetInstance, intersect, penetratorVoxelization, batchIdx);
			pd3dImmediateContext->DispatchIndirect(m_dispatchIndirectBUF, sizeof(UINT)*4);
		}

		if(g_app.g_useDisplacementConstraints)
		{		
			g_overlapUpdater.UpdateOverlapDisplacementConstraints(pd3dImmediateContext, targetInstance);
			config.with_constraints = 2;
			{		
				PERF_EVENT_SCOPED(perf,L"Brush Edit Regular");	
				config.patch_type = (UINT)EditPatchType::REGULAR;			
				pd3dImmediateContext->CopyStructureCount(m_dispatchIndirectBUF, 4 * sizeof(UINT), intersect->GetIntersectedPatchesOSDRegularUAV(batchIdx));

				BindShaders(pd3dImmediateContext, config, targetInstance, intersect, penetratorVoxelization, batchIdx);

				pd3dImmediateContext->DispatchIndirect(m_dispatchIndirectBUF, sizeof(UINT)*4);
			}

			// Gregory
			{
				PERF_EVENT_SCOPED(perf,L"Brush Edit Gregory");	

				config.patch_type = (UINT)EditPatchType::GREGORY;
				pd3dImmediateContext->CopyStructureCount(m_dispatchIndirectBUF, 4 * sizeof(UINT), intersect->GetIntersectedPatchesOSDGregoryUAV(batchIdx));

				BindShaders(pd3dImmediateContext, config, targetInstance, intersect, penetratorVoxelization, batchIdx);

				pd3dImmediateContext->DispatchIndirect(m_dispatchIndirectBUF, sizeof(UINT)*4);
			}
		}

		pd3dImmediateContext->CSSetShaderResources(0, 11, g_ppSRVNULL);
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 3, g_ppUAVNULL, NULL);
	}
	else
	{
		DXOSDMesh* mesh = targetInstance->GetOSDMesh();
		auto osdMesh = mesh->GetMesh();	
		UINT numLevels = log2Integer(g_app.g_displacementTileSize)+1;	
		UINT strides = mesh->GetNumVertexElements();

		const auto& patches = osdMesh->GetDrawContext()->patchArrays;
		for(const auto& patch : patches)
		{
			auto numPatches = patch.GetNumPatches();

			if(numPatches == 0) continue;

			if(patch.GetDescriptor().GetType() == OpenSubdiv::OPENSUBDIV_VERSION::FarPatchTables::REGULAR)			
			{
				config.patch_type = static_cast<uint32_t>(EditPatchType::REGULAR);	
			}
			else if(patch.GetDescriptor().GetType() == OpenSubdiv::OPENSUBDIV_VERSION::FarPatchTables::GREGORY)
			{
				config.patch_type = static_cast<uint32_t>(EditPatchType::GREGORY);	
			}
			else
				continue;

			// Problem 1: we dont have end regular patches in opensubdiv
			// opensubdiv packs patches in arrays by type(e.g. regular, boundary,...), pattern (no transition, transition 1..) and subpatches in patterns
			// workaround: run deformation for each pattern only on subpatch 0
			if(patch.GetDescriptor().GetSubPatch() > 0) continue;

			// Update config state	
			{	
				assert(m_osdCB);

				D3D11_MAPPED_SUBRESOURCE MappedResource;
				pd3dImmediateContext->Map(m_osdCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				CB_OSDConfig * pData = ( CB_OSDConfig* )MappedResource.pData;

				//pData->TessLevel = static_cast<float>(int(g_app.g_fTessellationFactor+0.5f));
				pData->GregoryQuadOffsetBase = patch.GetQuadOffsetIndex();
				pData->PrimitiveIdBase		 = patch.GetPatchIndex();			// patch id for ptex/far table access
				pData->IndexStart			 = patch.GetVertIndex();			// index of first control vertex in global index array			
				pData->NumVertexComponents   = mesh->GetNumVertexElements();	// for stride in vertex buffer			
				pData->NumIndicesPerPatch	 = patch.GetDescriptor().GetNumControlVertices(); // how many indices per patch, todo set using define in shader
				pData->NumPatches			 = numPatches; 	
				pd3dImmediateContext->Unmap( m_osdCB, 0 );
				pd3dImmediateContext->CSSetConstantBuffers( CB_LOC::OSD_DEFORMATION_CONFIG, 1, &m_osdCB );
			}

			
			config.num_vertex_components = mesh->GetNumVertexElements();
			config.max_valence = intersect->GetMaxValenceLastIntersected();


			BindShaders(pd3dImmediateContext, config, targetInstance, intersect, penetratorVoxelization, batchIdx);

			UINT NUM_BLOCKS_DISP =  g_app.g_displacementTileSize / DISPLACEMENT_DISPATCH_TILE_SIZE;
			pd3dImmediateContext->Dispatch(patch.GetNumPatches(),NUM_BLOCKS_DISP*NUM_BLOCKS_DISP,1);

			pd3dImmediateContext->CSSetShaderResources(0, 11, g_ppSRVNULL);
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, g_ppUAVNULL, NULL);
		}	
	}
	return hr;
}


void TileEdit::BindShaders(ID3D11DeviceContext1* pd3dImmediateContext, const PaintDeformConfig effect, ModelInstance* instance, IntersectGPU* intersect, ModelInstance* penetratorVoxelization, uint32_t batchIdx)
{
	const D3D11_INPUT_ELEMENT_DESC hInElementDesc[] = { { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };

	ID3D11InputLayout* tmp = NULL;
	EffectRegistryIntersect::ConfigType * config = g_paintDeformEffectRegistry.GetDrawConfig(effect, DXUTGetD3D11Device(), &(const_cast<ID3D11InputLayout*>(tmp)), hInElementDesc, ARRAYSIZE(hInElementDesc));
	SAFE_RELEASE(tmp);

	DXOSDMesh* mesh = instance->GetOSDMesh();

	auto osdMesh = mesh->GetMesh();
	UINT strides = mesh->GetNumVertexElements();

	ID3D11ShaderResourceView* ppSRV[] = {
		instance->GetVisibility()->SRV,							// t0 Visibility
		osdMesh->GetDrawContext()->vertexBufferSRV,				// t1 vertex buffer, float, numcomponents set in cb
		osdMesh->GetDrawContext()->patchIndexBufferSRV,			// t2 index buffer			
		osdMesh->GetDrawContext()->ptexCoordinateBufferSRV,		// t3 tile rotation info and patch to tile mapping, CHECKME            
		osdMesh->GetDrawContext()->vertexValenceBufferSRV,		// new t4		    
		osdMesh->GetDrawContext()->quadOffsetBufferSRV,			// new t5			
		intersect->GetIntersectedPatchesOSDRegularSRV(batchIdx),// t6 PATCHDATA HACK
		intersect->GetIntersectedPatchesOSDGregorySRV(batchIdx),// t7 PATCHDATA HACK
		instance->GetDisplacementTileLayout()->SRV,				// t8 tile layout info
		instance->GetColorTileLayout()->SRV,						// t9
	};

	ID3D11UnorderedAccessView* ppUAV[] = {
		effect.with_constraints == 1 ? g_memoryManager.GetDisplacementConstraintsUAV() : g_memoryManager.GetDisplacementDataUAV()				// u0
		, g_memoryManager.GetColorDataUAV()						// u1 debug write colors
		, effect.update_max_disp > 0 ? instance->GetMaxDisplacement()->UAV : NULL
	};

	ID3D11ShaderResourceView* ppUVSRV[] = { osdMesh->GetDrawContext()->fvarDataBufferSRV };

	pd3dImmediateContext->CSSetShaderResources(0, 10, ppSRV);

	ID3D11ShaderResourceView* ppVoxelSRV[] = { penetratorVoxelization->GetVoxelizationSRV(),	// t10 voxelization
		NULL			// t11 constraints if enabled - TODO						
	};
	pd3dImmediateContext->CSSetShaderResources(10, 2, ppVoxelSRV);

	if (effect.with_constraints == 2) // eval constraints mode
	{
		ID3D11ShaderResourceView* ppBrushSRV[] = { g_memoryManager.GetDisplacementConstraintsSRV() }; // t10 BRUSH TEXTURE	
		pd3dImmediateContext->CSSetShaderResources(10, 1, ppBrushSRV);
	}

	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 3, ppUAV, NULL);

	pd3dImmediateContext->CSSetShader(config->computeShader->Get(), NULL, 0);
	
}


HRESULT TileEdit::VoxelDeformOSD( ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* deformable, ModelInstance* penetratorVoxelization, IntersectGPU* intersect, uint32_t batchIdx )
{
	HRESULT hr = S_OK;
	if(! deformable->IsSubD()) return hr;

	SetVoxelGridCB(pd3dImmediateContext, penetratorVoxelization, deformable);
	pd3dImmediateContext->CSSetConstantBuffers( CB_LOC::MATERIAL, 1, &penetratorVoxelization->GetMaterial()->_cbMat);

	if (g_app.g_bTimingsEnabled) 
	{
		g_app.WaitForGPU();
		g_app.g_TimingLog.m_dRayCastTotal -= GetTimeMS()/(double)g_app.g_TimingLog.m_uNumRuns;
		for (UINT i = 0; i < g_app.g_TimingLog.m_uNumRuns; i++)
		{
			Apply(pd3dImmediateContext, deformable, intersect, batchIdx, penetratorVoxelization);
		}

		g_app.WaitForGPU();
		g_app.g_TimingLog.m_dRayCastTotal += GetTimeMS()/(double)g_app.g_TimingLog.m_uNumRuns;
		g_app.g_TimingLog.m_uRayCastCount++;
	} 
	else 
	{
		Apply(pd3dImmediateContext, deformable, intersect, batchIdx, penetratorVoxelization);
	}

	deformable->GetOSDMesh()->SetRequiresOverlapUpdate();
	//UpdateOverlap(mesh);	//is called by main

	return hr;
}

EffectRegistryPaintDeform::ConfigType * EffectRegistryPaintDeform::_CreateDrawConfig( DescType const & desc, SourceConfigType const * sconfig, ID3D11Device1 * pd3dDevice, ID3D11InputLayout ** ppInputLayout, D3D11_INPUT_ELEMENT_DESC const * pInputElementDescs, int numInputElements ) const  // make const
{

	//g_shaderManager.SetPrecompiledShaders(false); // for nsight debugging, compile all shaders after this point without using shaders from disk

	ConfigType * config = DrawRegistryBase::_CreateDrawConfig(desc.value, sconfig, pd3dDevice, ppInputLayout, pInputElementDescs, numInputElements);
	assert(config);

	//g_shaderManager.SetPrecompiledShaders(true); // for nsight debugging, compile all shaders after this point without using shaders from disk

	return config;
}

EffectRegistryPaintDeform::SourceConfigType * EffectRegistryPaintDeform::_CreateDrawSourceConfig( DescType const & desc, ID3D11Device1 * pd3dDevice ) const// make const
{
	PaintDeformConfig effect = desc;

	std::cout << effect.value << std::endl;

	SourceConfigType * sconfig = DrawRegistryBase::_CreateDrawSourceConfig(desc.value, pd3dDevice);
	assert(sconfig);

	sconfig->commonConfig.common_render_hash = effect.value;
	sconfig->value = effect.value;
	
	
	std::ostringstream ss;


	std::cout << "subd" << std::endl;
	ss.str("");	ss << effect.num_vertex_components;
	sconfig->computeShader.AddDefine("OSD_NUM_ELEMENTS", ss.str());

	{
		ss.str("");	ss << g_app.g_displacementTileSize;
		sconfig->computeShader.AddDefine("TILE_SIZE", ss.str());

		ss.str("");	ss << DISPLACEMENT_DISPATCH_TILE_SIZE;
		sconfig->computeShader.AddDefine("DISPLACEMENT_DISPATCH_TILE_SIZE", ss.str());
	}

	if (effect.continuous == 1)
	{
		sconfig->computeShader.AddDefine("CONTINUOUS_TEX");
		std::cout << "continuous mode" << std::endl;

	}

	if (effect.with_culling)
	{
		sconfig->computeShader.AddDefine("WITH_CULLING");
	}

	if (effect.with_constraints == 2)
	{
		sconfig->computeShader.sourceFile	= L"shader/TileEdit.hlsl";
		sconfig->computeShader.entry		= "ApplyConstraintsOSDCulledCS";
	}
	else
	{
		sconfig->computeShader.sourceFile	= L"shader/TileEdit.hlsl";
		sconfig->computeShader.entry		= "TileEditCS";
		sconfig->computeShader.AddDefine("USE_VOXELDEFORM");

		if (effect.voxel_multisampling)
			sconfig->computeShader.AddDefine("WITH_MULTISAMPLING");
	}



	if (effect.patch_type == (UINT)EditPatchType::REGULAR)
	{
		sconfig->commonShader.AddDefine("REGULAR");
	}

	if (effect.patch_type == (UINT)EditPatchType::GREGORY)
	{
		sconfig->commonShader.AddDefine("GREGORY");
		ss.str("");	ss << effect.max_valence;
		sconfig->computeShader.AddDefine("OSD_MAX_VALENCE", ss.str());
	}

	if (effect.with_constraints != 0)
		sconfig->commonShader.AddDefine("WITH_CONSTRAINTS");

	if (effect.update_max_disp != 0)
		sconfig->computeShader.AddDefine("UPDATE_MAX_DISPLACEMENT");

	
	return sconfig;
}


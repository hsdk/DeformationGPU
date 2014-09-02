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
#include "rendering/RendererSubD.h"
#include "scene/DXSubDModel.h"
#include "scene/ModelInstance.h"
#include "MemoryManager.h"

#include <sstream>
//Henry: has to be last header
#include "utils/DbgNew.h"

//static
RendererSubD	   g_rendererSubD;
EffectDrawRegistry g_osdEffectRegistry;

EffectDrawRegistry::ConfigType * EffectDrawRegistry::_CreateDrawConfig( DescType const & desc, SourceConfigType const * sconfig, ID3D11Device1* pd3dDevice, ID3D11InputLayout ** ppInputLayout, D3D11_INPUT_ELEMENT_DESC const * pInputElementDescs, int numInputElements )
{
	//g_shaderManager.SetPrecompiledShaders(false); // for nsight debugging, compile all shaders after this point without using shaders from disk

	ConfigType * config = BaseRegistry::_CreateDrawConfig(desc.first, sconfig, pd3dDevice, ppInputLayout, pInputElementDescs, numInputElements);
	assert(config);
	
	//g_shaderManager.SetPrecompiledShaders(false); // for nsight debugging, compile all shaders after this point without using shaders from disk

	return config;
}

EffectDrawRegistry::SourceConfigType * EffectDrawRegistry::_CreateDrawSourceConfig( DescType const & desc, ID3D11Device1* pd3dDevice )
{
	OSDEffect effect = desc.second;
	const wchar_t* shaderFile = L"shader/OSDRender.hlsl";
	
	SourceConfigType * sconfig = BaseRegistry::_CreateDrawSourceConfig(desc.first, pd3dDevice);
	assert(sconfig);

	sconfig->value = effect.value;
	std::ostringstream ss;
	// override default shader config
	bool quad = true;
	if (desc.first.GetType() == OpenSubdiv::FarPatchTables::QUADS ||
		desc.first.GetType() == OpenSubdiv::FarPatchTables::TRIANGLES) {
			sconfig->vertexShader.sourceFile = shaderFile;
			sconfig->vertexShader.target = "vs_5_0";
			sconfig->vertexShader.entry = "vs_main";
			if (effect.displacement) 
			{
				sconfig->geometryShader.AddDefine("FLAT_NORMALS");				
			}
			effect.quad = 1;
	} else {
		quad = false;
		sconfig->domainShader.sourceFile = shaderFile;// checkme include correct file depending on patch type set by create draw config; //g_shaderSource + sconfig->domainShader.source;
		if (effect.displacement and (not effect.normal))
		{
			sconfig->geometryShader.AddDefine("FLAT_NORMALS");
			effect.quad = 0;
		}
	}

	sconfig->commonConfig.common_render_hash = effect.value;

	// todo tri/quad flag for common

	if (effect.patchCull)	
	{		
		sconfig->commonShader.AddDefine("OSD_ENABLE_PATCH_CULL");		
	}
	if (effect.screenSpaceTess)
	{
		sconfig->commonShader.AddDefine("OSD_ENABLE_SCREENSPACE_TESSELLATION");
	}
	if (effect.fractionalSpacing)
	{
		sconfig->commonShader.AddDefine("OSD_FRACTIONAL_ODD_SPACING");		
	}
	


	sconfig->geometryShader.sourceFile = shaderFile;
	sconfig->geometryShader.target = "gs_5_0";
	sconfig->geometryShader.entry = "gs_main";

	sconfig->pixelShader.sourceFile = shaderFile;
	sconfig->pixelShader.target = "ps_5_0";
	sconfig->pixelShader.entry = "ps_main";
	sconfig->pixelShader.AddDefine("PIXEL_SHADING");

	switch (effect.color) {
	case RendererSubD::COLOR_NONE:
		break;
	case RendererSubD::COLOR_PTEX_NEAREST:
		sconfig->pixelShader.AddDefine("COLOR_PTEX_NEAREST");
		break;
	case RendererSubD::COLOR_PTEX_HW_BILINEAR:
		sconfig->pixelShader.AddDefine("COLOR_PTEX_HW_BILINEAR");
		break;
	case RendererSubD::COLOR_PTEX_BILINEAR:
		sconfig->pixelShader.AddDefine("COLOR_PTEX_BILINEAR");
		break;
	case RendererSubD::COLOR_PTEX_BIQUADRATIC:
		sconfig->pixelShader.AddDefine("COLOR_PTEX_BIQUADRATIC");
		break;
	case RendererSubD::COLOR_PATCHTYPE:
		sconfig->pixelShader.AddDefine("COLOR_PATCHTYPE");
		break;
	case RendererSubD::COLOR_PATCHCOORD:
		sconfig->pixelShader.AddDefine("COLOR_PATCHCOORD");
		break;
	case RendererSubD::COLOR_NORMAL:
		sconfig->pixelShader.AddDefine("COLOR_NORMAL");
		break;
	}

	if(effect.colorUV == 1)
		sconfig->pixelShader.AddDefine("COLOR_UV_TEX");

	switch (effect.displacement) {
	case RendererSubD::DISPLACEMENT_NONE:
		break;
	case RendererSubD::DISPLACEMENT_HW_BILINEAR:
		sconfig->commonShader.AddDefine("DISPLACEMENT_HW_BILINEAR");
		break;
	case RendererSubD::DISPLACEMENT_BILINEAR:
		sconfig->commonShader.AddDefine("DISPLACEMENT_BILINEAR");
		break;
	case RendererSubD::DISPLACEMENT_BIQUADRATIC:
		sconfig->commonShader.AddDefine("DISPLACEMENT_BIQUADRATIC");
		break;
	case RendererSubD::DISPLACEMENT_UV_TEX:
		sconfig->commonShader.AddDefine("DISPLACEMENT_UV_TEX");
		break;
	}

	switch (effect.normal) {
	case RendererSubD::NORMAL_FACET:
		sconfig->commonShader.AddDefine("NORMAL_FACET");
		break;
	case RendererSubD::NORMAL_HW_SCREENSPACE:
		sconfig->commonShader.AddDefine("NORMAL_HW_SCREENSPACE");
		break;
	case RendererSubD::NORMAL_SCREENSPACE:
		sconfig->commonShader.AddDefine("NORMAL_SCREENSPACE");
		break;
	case RendererSubD::NORMAL_BIQUADRATIC:
		sconfig->commonShader.AddDefine("NORMAL_BIQUADRATIC");
		break;
	case RendererSubD::NORMAL_BIQUADRATIC_WG:
		sconfig->commonShader.AddDefine("OSD_COMPUTE_NORMAL_DERIVATIVES");
		sconfig->commonShader.AddDefine("NORMAL_BIQUADRATIC_WG");
		break;
	case RendererSubD::NORMAL_UV_TEX:
		sconfig->commonShader.AddDefine("NORMAL_UV_TEX");
		break;
	}

	if (effect.occlusion)	sconfig->pixelShader.AddDefine("USE_PTEX_OCCLUSION");
	if (effect.specular)	sconfig->pixelShader.AddDefine("USE_PTEX_SPECULAR");
	if (effect.ibl)			sconfig->pixelShader.AddDefine("USE_IBL");

	if (effect.showisct)	sconfig->pixelShader.AddDefine("DEBUG_SHOW_INTERSECTION");

	if (quad) 
	{
		//sconfig->commonShader.AddDefine("PRIM_QUAD");
		sconfig->geometryShader.AddDefine("PRIM_QUAD");
		sconfig->pixelShader.AddDefine("PRIM_QUAD");
	} 
	else 
	{	
		//sconfig->commonShader.AddDefine("PRIM_TRI");	
		sconfig->geometryShader.AddDefine("PRIM_TRI");
		sconfig->pixelShader.AddDefine("PRIM_TRI");
	}

	if (effect.seamless) 	sconfig->commonShader.AddDefine("SEAMLESS_MIPMAP");
	

	if (effect.wire == 0) 
	{
		//sconfig->geometryShader.AddDefine("GEOMETRY_OUT_WIRE");
		//sconfig->pixelShader.AddDefine("GEOMETRY_OUT_WIRE");
	}
	else if (effect.wire == 1) 
	{
		sconfig->domainShader.AddDefine("GEOMETRY_OUT_FILL");
		sconfig->geometryShader.AddDefine("GEOMETRY_OUT_FILL");
		sconfig->pixelShader.AddDefine("GEOMETRY_OUT_FILL");
	}
	else if (effect.wire == 2) 
	{
		sconfig->domainShader.AddDefine("GEOMETRY_OUT_LINE");
		sconfig->geometryShader.AddDefine("GEOMETRY_OUT_LINE");
		sconfig->pixelShader.AddDefine("GEOMETRY_OUT_LINE");
	}

	if(effect.showAllocs == 1)
	{
		sconfig->pixelShader.AddDefine("SHOW_ALLOCS");			
	}

	if(effect.ssao == 1)
		sconfig->pixelShader.AddDefine("WITH_SSAO");

	if(effect.shadow == RendererSubD::ShadowType::SHADOW_GENERATE)
	{	
		sconfig->commonShader.AddDefine("GEN_SHADOW");
		sconfig->hullShader.AddDefine("TESSADAPTIVE_DIFFERENT_CAM");
		sconfig->pixelShader.entry = "ps_genShadow";			
	}
	else if(effect.shadow == RendererSubD::SHADOW_GENERATE_GS)
	{	
		sconfig->commonShader.AddDefine("GEN_SHADOW");
		sconfig->hullShader.AddDefine("TESSADAPTIVE_DIFFERENT_CAM");
		
		ss.str(""); ss << NUM_CASCADES;
		sconfig->geometryShader.AddDefine("NUM_CASCADES", ss.str());
		sconfig->geometryShader.AddDefine("GEN_SHADOW_FAST");	
		sconfig->geometryShader.entry = "gs_mainShadow";

		sconfig->pixelShader.AddDefine("GEN_SHADOW_FAST");	
		sconfig->pixelShader.entry = "ps_genShadowFast";		
	}
	else if(effect.shadow == RendererSubD::SHADOW_ENABLED)
	{
		sconfig->commonShader.AddDefine("USE_SHADOW");
		ss.str(""); ss << NUM_CASCADES;
		sconfig->pixelShader.AddDefine("NUM_CASCADES", ss.str());		
	}
		
	if(effect.picking == RendererSubD::PICKING_ENABLED)
	{			
		//sconfig->pixelShader.AddDefine("WITH_PICKING");
		sconfig->commonShader.AddDefine("WITH_PICKING");
	}
	if(effect.colorUV || effect.normal == RendererSubD::NORMAL_UV_TEX || effect.displacement == RendererSubD::DISPLACEMENT_UV_TEX)
	{	
		sconfig->commonShader.AddDefine("WITH_UV");	
	}

	if(effect.voxelize != RendererSubD::VOXELIZATION_NONE)
	{
		sconfig->hullShader.AddDefine("TESSADAPTIVE_DIFFERENT_CAM");
		sconfig->pixelShader.entry = "PS_VoxelizeSolid";	
		sconfig->pixelShader.AddDefine("VOXELIZE");

		if(effect.voxelize == RendererSubD::VOXELIZATION_BACKWARD)
			sconfig->pixelShader.AddDefine("VOXELIZE_BACKWARD");
	}

	return sconfig;
}

RendererSubD::RendererSubD()
{
	_pcbTessellation = NULL;	

	_bGenShadow			= false;
	_bGenShadowFast		= false;
	_bUseShadow			= false;
	_bGenVoxelization	= false;
	_bWithSSAO			= false;
	_bWireFrameOverlay	= false;	
	_voxelizeMode		= false;
	_inputLayout = NULL;
	_inputLayoutTexCoords = NULL;
}

RendererSubD::~RendererSubD()
{

}

HRESULT RendererSubD::Create( ID3D11Device1* pd3dDevice )
{
	HRESULT hr = S_OK;

	// constant buffers
	DXCreateBuffer(pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CBOSDTessellation), D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, _pcbTessellation);
	DXCreateBuffer(pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CBOSDConfig)		, D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, _pcbConfig);

	// sampler for fast ptex lookup
	D3D11_SAMPLER_DESC samlerDesc = 
	{
		D3D11_FILTER_MIN_MAG_MIP_LINEAR	,// D3D11_FILTER Filter;
		//D3D11_TEXTURE_ADDRESS_CLAMP,	//D3D11_TEXTURE_ADDRESS_MODE AddressU;
		//D3D11_TEXTURE_ADDRESS_CLAMP,	//D3D11_TEXTURE_ADDRESS_MODE AddressV;
		D3D11_TEXTURE_ADDRESS_WRAP,	//D3D11_TEXTURE_ADDRESS_MODE AddressU;
		D3D11_TEXTURE_ADDRESS_WRAP,	//D3D11_TEXTURE_ADDRESS_MODE AddressV;
		D3D11_TEXTURE_ADDRESS_BORDER,	//D3D11_TEXTURE_ADDRESS_MODE AddressW;
		0,//FLOAT MipLODBias;
		0,//UINT MaxAnisotropy;
		D3D11_COMPARISON_NEVER , //D3D11_COMPARISON_FUNC ComparisonFunc;
		0.0,0.0,0.0,0.0,//FLOAT BorderColor[ 4 ];
		0,//FLOAT MinLOD;
		0//FLOAT MaxLOD;   
	};

	V_RETURN( pd3dDevice->CreateSamplerState( &samlerDesc, &_samplerLinear) );
	DXUT_SetDebugName( _samplerLinear, "DXOSDRenderer Linear Sampler" );
	return hr;
}

void RendererSubD::Destroy()
{
	g_osdEffectRegistry.Reset();
	SAFE_RELEASE(_pcbTessellation);
	SAFE_RELEASE(_pcbConfig);
	SAFE_RELEASE(_samplerLinear);
	SAFE_RELEASE(_inputLayout);
	SAFE_RELEASE(_inputLayoutTexCoords);
	
}

void RendererSubD::BindShaders(  ID3D11DeviceContext1* pd3dImmediateContext, OSDEffect effect, DXOSDMesh* mesh, OpenSubdiv::OsdDrawContext::PatchArray const & patch, ModelInstance* instance ) const
{
	OSDEffectDesc effectDesc(patch.GetDescriptor(), effect);

	//checkme: we dont provide normals for subd models
	const D3D11_INPUT_ELEMENT_DESC hInElementDesc[] = {
		 { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		//,{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 4*3, D3D11_INPUT_PER_VERTEX_DATA, 0 }	// checkme and remove
	};

	const D3D11_INPUT_ELEMENT_DESC hInElementDescTC[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 4*2, D3D11_INPUT_PER_VERTEX_DATA, 0 }	// checkme and remove
	};

	EffectDrawRegistry::ConfigType* config = config = g_osdEffectRegistry.GetDrawConfig( effectDesc, DXUTGetD3D11Device(),
																				&const_cast<ID3D11InputLayout*>(mesh->GetHasTexcoords()?_inputLayoutTexCoords : _inputLayout),// mesh->GetInputLayoutRef(),	
																				mesh->GetHasTexcoords()?hInElementDescTC: hInElementDesc,
																				mesh->GetHasTexcoords()?ARRAYSIZE(hInElementDescTC):ARRAYSIZE(hInElementDesc));
	if(mesh->GetHasTexcoords())
		assert(_inputLayoutTexCoords);
	else
		assert(_inputLayout);
	


	//if(mesh->GetInputLayout() ==NULL)
	//assert(mesh->GetInputLayout());
	// setup camera constant buffer -  should have been done in main
	// CHECKME cb in shaders

	// Update tessellation state	
	{	
		assert(_pcbTessellation);

		D3D11_MAPPED_SUBRESOURCE MappedResource;
		pd3dImmediateContext->Map(_pcbTessellation, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
		CBOSDTessellation * pData = ( CBOSDTessellation* )MappedResource.pData;

		if(_voxelizeMode)
			pData->TessLevel = static_cast<float>(int(32));  // HENRY CHECKME: we use fixed tess factor for voxelization
		else
			pData->TessLevel = static_cast<float>(int(g_app.g_fTessellationFactor+0.5f));
		pData->GregoryQuadOffsetBase = patch.GetQuadOffsetIndex();
		pData->PrimitiveIdBase = patch.GetPatchIndex();

		pd3dImmediateContext->Unmap( _pcbTessellation, 0 );
	}


	// Update config state	
	{
		assert(_pcbConfig);

		D3D11_MAPPED_SUBRESOURCE MappedResource;
		pd3dImmediateContext->Map(_pcbConfig, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
		CBOSDConfig * pData = ( CBOSDConfig* )MappedResource.pData;

		pData->displacementScale = g_app.g_fDisplacementScalar;
		pData->mipmapBias		 = g_app.g_fMipMapBias;

		pd3dImmediateContext->Unmap( _pcbConfig, 0 );
	}

	if(effect.color == COLOR_PTEX_HW_BILINEAR || effect.displacement == DISPLACEMENT_HW_BILINEAR || effect.colorUV)
	{
		pd3dImmediateContext->DSSetSamplers(0, 1, &_samplerLinear);
		pd3dImmediateContext->PSSetSamplers(0, 1, &_samplerLinear);
	}

	//pd3dImmediateContext->IASetInputLayout(mesh->GetInputLayout());
	if(mesh->GetHasTexcoords())
		pd3dImmediateContext->IASetInputLayout(_inputLayoutTexCoords);
	else
		pd3dImmediateContext->IASetInputLayout(_inputLayout);

	pd3dImmediateContext->VSSetShader(config->vertexShader->Get(), NULL, 0);
	// TODO check per frame buffer layout for opensubdiv
	//pd3dImmediateContext->VSSetConstantBuffers(0, 1, &g_pcbPerFrame); // should have been set in main
	
	pd3dImmediateContext->HSSetShader(config->hullShader->Get(), NULL, 0);
	//pd3dImmediateContext->HSSetConstantBuffers(0, 1, &g_pcbPerFrame); // should have been set in main
	pd3dImmediateContext->HSSetConstantBuffers(1, 1, &_pcbTessellation);
	
	pd3dImmediateContext->DSSetShader(config->domainShader->Get(), NULL, 0);
	//pd3dImmediateContext->DSSetConstantBuffers(0, 1, &_pcbPerFrame);  // should have been set in main
	pd3dImmediateContext->DSSetConstantBuffers(3, 1, &_pcbConfig);

	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	//CHECKME we dont want geometry shader except for wireframe rendering
	if(effect.wire == DISPLAY_SHADED)
		pd3dImmediateContext->GSSetShader(NULL, NULL, 0);	
	else
	{
		pd3dImmediateContext->GSSetShader(config->geometryShader->Get(), NULL, 0);	
		//std::cout << "GS: " << config->geometryShader->Get() << " is active for type " << patch.GetDescriptor().GetType() << std::endl;
		//pd3dImmediateContext->GS
		
	}

	if(effect.shadow == SHADOW_GENERATE_GS)	
	{
		pd3dImmediateContext->GSSetShader(config->geometryShader->Get(), NULL, 0);	
		//g_pd3dDeviceContext->GSSetConstantBuffers(0, 1, &g_pcbPerFrame);  // should have been set in main
	}
	pd3dImmediateContext->PSSetShader(config->pixelShader->Get(), NULL, 0);
	//pd3dImmediateContext->PSSetConstantBuffers(0, 1, &g_pcbPerFrame);   // should have been set in main
	//pd3dImmediateContext->PSSetConstantBuffers(2, 1, &g_pcbLighting);	// should have been set in main, TODO check cb loc and data
	pd3dImmediateContext->PSSetConstantBuffers(3, 1, &_pcbConfig);

	const auto osdMesh = mesh->GetMesh();
	if (osdMesh->GetDrawContext()->vertexBufferSRV) {
		pd3dImmediateContext->VSSetShaderResources(0, 1, &osdMesh->GetDrawContext()->vertexBufferSRV);
		//pd3dImmediateContext->HSSetShaderResources(0, 1, &osdMesh->GetDrawContext()->vertexBufferSRV);
	}
	if (osdMesh->GetDrawContext()->vertexValenceBufferSRV) {
		pd3dImmediateContext->VSSetShaderResources(1, 1, &osdMesh->GetDrawContext()->vertexValenceBufferSRV);
		pd3dImmediateContext->HSSetShaderResources(1, 1, &osdMesh->GetDrawContext()->vertexValenceBufferSRV);
	}
	if (osdMesh->GetDrawContext()->quadOffsetBufferSRV) {
		pd3dImmediateContext->HSSetShaderResources(2, 1, &osdMesh->GetDrawContext()->quadOffsetBufferSRV);
	}

	if (osdMesh->GetDrawContext()->ptexCoordinateBufferSRV) {
		pd3dImmediateContext->HSSetShaderResources(3, 1, &osdMesh->GetDrawContext()->ptexCoordinateBufferSRV);
		pd3dImmediateContext->DSSetShaderResources(3, 1, &osdMesh->GetDrawContext()->ptexCoordinateBufferSRV);
	}

	// check if memory managed, if so apply correct buffer from memory manager, precomputed layout from mesh
	if(instance->GetHasDynamicDisplacement())
	{			
			ID3D11ShaderResourceView* ppDisplSRVS[] = { g_memoryManager.GetDisplacementDataSRV(),
														instance->GetDisplacementTileLayout()->SRV };
			pd3dImmediateContext->DSSetShaderResources(6, 2, ppDisplSRVS);	// 6 data, 7 layout				
			pd3dImmediateContext->PSSetShaderResources(6, 2, ppDisplSRVS);	// 6 data, 7 layout		
	}
	
	// check if memory managed, if so apply correct buffer from memory manager, precomputed layout from mesh
	if(instance->GetHasDynamicColor())
	{
		ID3D11ShaderResourceView* ppDisplSRVS[] = { g_memoryManager.GetColorDataSRV(),
			instance->GetColorTileLayout()->SRV };
		pd3dImmediateContext->PSSetShaderResources(4, 2, ppDisplSRVS);	// 4 data, 5 layout	
	}

	if(effect.colorUV || effect.normal == NORMAL_UV_TEX || effect.displacement == DISPLACEMENT_UV_TEX)
	{			
		ID3D11ShaderResourceView* ppUVSRV[] = { osdMesh->GetDrawContext()->fvarDataBufferSRV};
		
		pd3dImmediateContext->HSSetShaderResources(8, 1, ppUVSRV);	
		pd3dImmediateContext->DSSetShaderResources(8, 1, ppUVSRV);	
		pd3dImmediateContext->PSSetShaderResources(8, 1, ppUVSRV);	

		const auto* mat = mesh->GetMaterial();
		if(mat->HasDisplacementTex())
		{
			ID3D11ShaderResourceView* ppDisplSRVS[] = { mat->GetDisplacementTexSRV()};			
			pd3dImmediateContext->DSSetShaderResources(9, 1,  ppDisplSRVS);
		}

		if(mat->HasNormalsTex())
		{
			ID3D11ShaderResourceView* ppNormalSRVS[] = { mat->GetNormalsTexSRV()};			
			pd3dImmediateContext->PSSetShaderResources(10, 1,  ppNormalSRVS);
		}

		if(mat->HasDiffuseTex())
		{
			ID3D11ShaderResourceView* ppDiffuseSRVS[] = { mat->GetDiffuseTexSRV()};			
			pd3dImmediateContext->PSSetShaderResources(11, 1,  ppDiffuseSRVS);			
		}
	}

	if(effect.showisct)
	{
		ID3D11ShaderResourceView* ppVisSRV[] = { g_app.g_useCompactedVisibilityOverlap ? instance->GetVisibilityAll()->SRV : instance->GetVisibility()->SRV };
		pd3dImmediateContext->PSSetShaderResources(9, 1, ppVisSRV);	// ps color	
	}

	//else if( i
	//g_pd3dDeviceContext->PSSetShaderResources(4, 1, g_osdPTexImage->GetTexelsSRV());
	//g_pd3dDeviceContext->PSSetShaderResources(5, 1, g_osdPTexImage->GetLayoutSRV());


	//if (g_osdPTexDisplacement) {
	//	g_pd3dDeviceContext->DSSetShaderResources(6, 1, g_osdPTexDisplacement->GetTexelsSRV());
	//	g_pd3dDeviceContext->DSSetShaderResources(7, 1, g_osdPTexDisplacement->GetLayoutSRV());
	//	g_pd3dDeviceContext->PSSetShaderResources(6, 1, g_osdPTexDisplacement->GetTexelsSRV());
	//	g_pd3dDeviceContext->PSSetShaderResources(7, 1, g_osdPTexDisplacement->GetLayoutSRV());
	//}

	//if (g_osdPTexOcclusion) {
	//	g_pd3dDeviceContext->PSSetShaderResources(8, 1, g_osdPTexOcclusion->GetTexelsSRV());
	//	g_pd3dDeviceContext->PSSetShaderResources(9, 1, g_osdPTexOcclusion->GetLayoutSRV());
	//}

	//if (g_osdPTexSpecular) {
	//	g_pd3dDeviceContext->PSSetShaderResources(10, 1, g_osdPTexSpecular->GetTexelsSRV());
	//	g_pd3dDeviceContext->PSSetShaderResources(11, 1, g_osdPTexSpecular->GetLayoutSRV());
	//}


	// TODO config for shadowgen, shadow eval, ssao
	// TODO config for memory managed displacement, color
	// TODO make config evaluate material

}




//HRESULT DXOSDRenderer::FrameRender( ID3D11DeviceContext1* pd3dImmediateContext, DXOSDMesh* mesh )
//{
//	HRESULT hr = S_OK;
//
//	if (g_app.m_bTimingsEnabled) {
//		g_app.WaitForGPU();
//		g_app.m_TimingLog.m_dDrawSubDTotal -= GetTimeMS()/(double)(g_app.m_TimingLog.m_uNumRuns+1);
//		for (UINT i = 0; i < g_app.m_TimingLog.m_uNumRuns+1; i++) { //needs to be an odd number
//			//g_app.SetCameraConstantBuffersAllStages(pd3dImmediateContext);
//			V(FrameRenderInternal(pd3dImmediateContext, mesh));
//			if(FAILED(hr))
//			{
//				g_app.m_TimingLog.m_dDrawSubDTotal += GetTimeMS()/(double)(g_app.m_TimingLog.m_uNumRuns+1);
//				g_app.m_TimingLog.m_uDrawSubDCount++;
//				break;
//			}
//		}
//		g_app.WaitForGPU();
//		g_app.m_TimingLog.m_dDrawSubDTotal += GetTimeMS()/(double)(g_app.m_TimingLog.m_uNumRuns+1);
//		g_app.m_TimingLog.m_uDrawSubDCount++;
//	} else {
//		V(FrameRenderInternal(pd3dImmediateContext, mesh));
//	}
//
//
//	return hr;
//
//}

HRESULT RendererSubD::FrameRender( ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance ) const
{
	HRESULT hr =S_OK;
	if(!instance->IsSubD()) return S_OK;

	
	if (g_app.g_bTimingsEnabled) {
		g_app.WaitForGPU();
		g_app.g_TimingLog.m_dDrawSubDTotal -= GetTimeMS()/(double)(g_app.g_TimingLog.m_uNumRuns+1);
		for (UINT i = 0; i < g_app.g_TimingLog.m_uNumRuns+1; i++) { //needs to be an odd number
			//g_app.SetCameraConstantBuffersAllStages(pd3dImmediateContext);
			V(FrameRenderInternal(pd3dImmediateContext, instance));
			if(FAILED(hr))
			{
				g_app.g_TimingLog.m_dDrawSubDTotal += GetTimeMS()/(double)(g_app.g_TimingLog.m_uNumRuns+1);
				g_app.g_TimingLog.m_uDrawSubDCount++;
				break;
			}
		}
		g_app.WaitForGPU();
		g_app.g_TimingLog.m_dDrawSubDTotal += GetTimeMS()/(double)(g_app.g_TimingLog.m_uNumRuns+1);
		g_app.g_TimingLog.m_uDrawSubDCount++;
	} else {
		V_RETURN(FrameRenderInternal(pd3dImmediateContext, instance));
	}

	return hr;
}



HRESULT RendererSubD::FrameRenderInternal( ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance ) const
{
	HRESULT hr = S_OK;
	DXOSDMesh* mesh =  instance->GetOSDMesh();

	auto osdMesh = mesh->GetMesh();
	ID3D11Buffer* vertexBuffer = osdMesh->BindVertexBuffer();
	assert(vertexBuffer);

	UINT strides = mesh->GetNumVertexElements()*sizeof(float);
	UINT offsets = 0;

	pd3dImmediateContext->IASetVertexBuffers(0, 1, &vertexBuffer, &strides, &offsets);
	pd3dImmediateContext->IASetIndexBuffer(osdMesh->GetDrawContext()->patchIndexBuffer, DXGI_FORMAT_R32_UINT,0);

	if(_bGenShadow)
	{
		//g_app.SetGenShadowConstantBuffersHSDSGS(pd3dImmediateContext);
	}
	else pd3dImmediateContext->PSSetConstantBuffers( CB_LOC::MATERIAL, 1, &mesh->GetMaterial()->_cbMat);
	
	const auto& patches = osdMesh->GetDrawContext()->patchArrays;

	float tmpTess = g_app.g_fTessellationFactor;
	// draw patches
	for (const auto& patch : patches)
	{
		D3D11_PRIMITIVE_TOPOLOGY topology;
		//if (patch.GetDescriptor().GetType() != OpenSubdiv::FarPatchTables::REGULAR) continue; // render only regular test
		//if (patch.GetDescriptor().GetType() == OpenSubdiv::FarPatchTables::REGULAR) continue; // render only regular test
		//if (patch.GetDescriptor().GetType() != OpenSubdiv::FarPatchTables::JENS_FULL_REGULAR) continue; // render only regular test
		//if (patch.GetDescriptor().GetType() != OpenSubdiv::FarPatchTables::NON_PATCH) continue; // render only regular test
		//if (patch.GetDescriptor().GetType() != OpenSubdiv::FarPatchTables::REGULAR) continue; // render only regular test
		//if (patch.GetDescriptor().GetSubPatch() > 0) continue; // debug test
		//if(patch.GetDescriptor().GetPattern() != 0) continue;
		//if(patch.GetDescriptor().GetRotation() != 0) continue;
		//std::cout << (int)patch.GetDescriptor().GetRotation() << std::endl;
		/*if (patch.GetDescriptor().GetSubPatch() > 0) continue;*/
		if (osdMesh->GetDrawContext()->IsAdaptive())
		{
			switch (patch.GetDescriptor().GetNumControlVertices()) {
			case 4:
				topology = D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
				break;
			case 9:
				topology = D3D11_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST;
				break;
			case 12:
				topology = D3D11_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST;
				break;
			case 16:
				topology = D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST;
				break;
			default:
				assert(false);
				break;
			}
		}
		else
		{
			//return S_FALSE; // for now render only quads
			topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
		}

		//if		( //(topology != D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST) && 
		//	(topology != D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST))
		//	continue;

		
		pd3dImmediateContext->IASetPrimitiveTopology(topology);

		// TODO bind shaders and data
		// todo use shader config from opensubdiv
		// we need shader configs for two modes: 
		// 1. standard rendering with (procedural/texture) color
		// 2. shadow rendering: with displacement, without colors, output eye space depth and depth^2 for variance shadow maps

		OSDEffect config;
		config.value = 0; // resets all 
	
		config.displacement = DISPLACEMENT_NONE;
			
		if(instance->GetHasDynamicDisplacement())
		{
			//config.displacement = DISPLACEMENT_BILINEAR;
			//config.displacement = DISPLACEMENT_HW_BILINEAR;
			config.displacement = DISPLACEMENT_BIQUADRATIC;			
		}
			
		config.occlusion = false;
		config.normal = NORMAL_BIQUADRATIC_WG;
		//config.normal = NORMAL_BIQUADRATIC;
		//config.normal = NORMAL_SCREENSPACE;

		config.specular = false;
		config.patchCull = true;
		//config.screenSpaceTess = true;  
		config.screenSpaceTess = g_app.g_adaptiveTessellation;  
		config.fractionalSpacing = true;
		config.ibl = false; // checkme

		config.seamless = false;

		if(_voxelizeMode)
		{
			config.patchCull = true;
			config.screenSpaceTess = false;  
			
			config.color = COLOR_NONE;
			config.wire = DISPLAY_SHADED;
			config.voxelize = instance->GetVoxelGridDefinition().m_bFillSolidBackward ? VOXELIZATION_BACKWARD : VOXELIZATION_FORWARD;

			g_app.SetGenShadowConstantBuffersHSDSGS(pd3dImmediateContext);
		}
		else
		{
			
			config.showAllocs = (g_app.g_showAllocated?1:0);
			config.wire = DISPLAY_SHADED;

			
			config.wire = _bWireFrameOverlay ? DISPLAY_WIRE_ON_SHADED: DISPLAY_SHADED;

			if(!_bGenShadow)
			{
				if(g_app.g_showAllocated)
					config.wire = DISPLAY_WIRE_ON_SHADED;					
			}


			if(instance->GetHasDynamicColor())
			{
				//config.color = COLOR_PTEX_NEAREST;
				config.color = COLOR_PTEX_BILINEAR;
				//config.color = COLOR_PTEX_HW_BILINEAR;
				//config.color = COLOR_UV_TEX;
			}
			//config.color = COLOR_PATCHCOORD;
			//config.color = COLOR_NORMAL;	

			if(!_bGenShadow)
			{
				// DISABLE COLOR UV TEXTURE
				if(mesh->GetMaterial()->HasDiffuseTex())
				{
					//config.colorUV = 1;					
				}
				//config.displacement = DISPLACEMENT_UV_TEX;
				//config.normal = NORMAL_UV_TEX;			
				config.ssao = _bWithSSAO;
			}


			if(_bGenShadow)
			{
				config.color = COLOR_NONE;		
				config.numShadowCascades = NUM_CASCADES == 4 ? 1 : 0;

				//config.screenSpaceTess = false;
				//g_app.g_fTessellationFactor = 6;

				if(_bGenShadowFast)
				{
					config.shadow = SHADOW_GENERATE_GS;	
				}
				else
				{
					config.shadow = SHADOW_GENERATE;	
				}
			}
			else if(_bUseShadow)
			{		
				
				config.numShadowCascades = NUM_CASCADES == 4 ? 1 : 0;
				config.shadow = SHADOW_ENABLED;
			}


			//debug show intersected patches
			if (g_app.g_showIntersections && !_bGenShadow )
			{				
				config.showisct = 1;					
				config.wire = DISPLAY_WIRE_ON_SHADED;
			}			

		}

		BindShaders(pd3dImmediateContext, config, mesh, patch, instance);

		pd3dImmediateContext->DrawIndexed(	patch.GetNumIndices(),
											patch.GetVertIndex(),
											0);

		//if(config.color == COLOR_PTEX_HW_BILINEAR || config.displacement == DISPLACEMENT_HW_BILINEAR || config.color == COLOR_UV_TEX )
		{
			ID3D11SamplerState* ppSSNULL[] = {NULL, NULL, NULL};
			pd3dImmediateContext->DSSetSamplers(0, 1, ppSSNULL);
			pd3dImmediateContext->PSSetSamplers(0, 1, ppSSNULL);
		}

		// unbind dynamic data srvs to enable writing in other stages
		//ID3D11ShaderResourceView* ppSRVNULL[] = {NULL, NULL, NULL};
		pd3dImmediateContext->DSSetShaderResources(6, 2, g_ppSRVNULL);	// ds displacement
		pd3dImmediateContext->PSSetShaderResources(6, 2, g_ppSRVNULL);	// ps displacement
		pd3dImmediateContext->PSSetShaderResources(4, 2, g_ppSRVNULL);	// ps color	
		pd3dImmediateContext->PSSetShaderResources(9, 3, g_ppSRVNULL);	// ps color	
		

		//debug show intersected patches
		if(!_bGenShadow)
		{
			pd3dImmediateContext->PSSetShaderResources(9, 1, g_ppSRVNULL);	// ps color	
		}
	}
	g_app.g_fTessellationFactor = tmpTess;
	return hr;
}

HRESULT RendererSubD::Voxelize( ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance, bool withCulling )
{	
	//m_voxelizeBackward	=  instance->GetVoxelGridDefinition().m_bFillSolidBackward;
	HRESULT hr =S_OK;
	if(!instance->IsSubD()) return S_OK;
	
	_voxelizeMode		= true;

	if (g_app.g_bTimingsEnabled) 
	{
		g_app.WaitForGPU();
		g_app.g_TimingLog.m_dVoxelizationTotal -= GetTimeMS()/(double)(g_app.g_TimingLog.m_uNumRuns+1);
		for (UINT i = 0; i < g_app.g_TimingLog.m_uNumRuns+1; i++) //needs to be an odd number			
		{ 
			V(FrameRenderInternal(pd3dImmediateContext, instance));
			if(FAILED(hr))
			{
				g_app.g_TimingLog.m_dVoxelizationTotal += GetTimeMS()/(double)(g_app.g_TimingLog.m_uNumRuns+1);
				g_app.g_TimingLog.m_uVoxelizationCount++;
				break;
			}
		}
		g_app.WaitForGPU();
		g_app.g_TimingLog.m_dVoxelizationTotal += GetTimeMS()/(double)(g_app.g_TimingLog.m_uNumRuns+1);
		g_app.g_TimingLog.m_uVoxelizationCount++;
	}
	else 
	{
		V_RETURN(FrameRenderInternal(pd3dImmediateContext, instance));
	}
		
	_voxelizeMode = false;
	return hr;
}

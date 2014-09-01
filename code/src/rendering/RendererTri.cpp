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
#include "rendering/RendererTri.h"
#include "scene/Scene.h"

#include "scene/DXModel.h"
#include "App.h"
#include "scene/ModelInstance.h"

#include <vector>
#include <sstream>
//Henry: has to be last header
#include "utils/DbgNew.h"

using namespace DirectX;

RenderTri					g_renderTriMeshes;


EffectDrawRegistryTriangle g_triEffectRegistry;

RenderTri::RenderTri()
{
	//m_VoxelizeMode = false;
	m_renderLines = false;
	m_UseSSAO = false;
	m_UseShadows = false;
	m_GenShadows = false;
	m_GenShadowFast = false;
	m_UseWireframeOverlay = false;

	m_AnisotropicWrapSampler = NULL;
	m_inputLayout = NULL;
}

RenderTri::~RenderTri()
{
	Destroy();
}

HRESULT RenderTri::Create(ID3D11Device1* pd3dDevice)
{
	HRESULT hr = S_OK;
		
	// init shader
	ID3DBlob* pBlob = NULL;
	
	m_SkydomeVS		= g_shaderManager.AddVertexShader(L"shader/renderTri.hlsl", "VS_Skydome", "vs_5_0", &pBlob);
	m_colorVS		 = g_shaderManager.AddVertexShader(L"shader/renderTri.hlsl", "VS_Std", "vs_5_0", &pBlob); 		

		
	const D3D11_INPUT_ELEMENT_DESC attributeLayout[] = 
	{
		{"POSITION" , 0, DXGI_FORMAT_R32G32B32A32_FLOAT	, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL"	, 0, DXGI_FORMAT_R32G32B32A32_FLOAT	, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD" , 0, DXGI_FORMAT_R32G32_FLOAT		, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	//V_RETURN(DXUTGetD3D11Device()->CreateInputLayout(attributeLayout,ARRAYSIZE(attributeLayout), pBlob->GetBufferPointer(), pBlob->GetBufferSize(), &m_inputLayout));

	m_SkydomePS		= g_shaderManager.AddPixelShader(L"shader/renderTri.hlsl", "PS_Skydome", "ps_5_0", &pBlob);
	g_shaderManager.SetPrecompiledShaders(false); 
	m_PSVoxelizeSolidBackward = g_shaderManager.AddPixelShader(L"shader/renderTri.hlsl", "PS_VoxelizeSolidBackward", "ps_5_0", &pBlob);
	m_PSVoxelizeSolidForward = g_shaderManager.AddPixelShader(L"shader/renderTri.hlsl", "PS_VoxelizeSolidForward", "ps_5_0", &pBlob);
	SAFE_RELEASE(pBlob);
	g_shaderManager.SetPrecompiledShaders(true); 
	
	D3D11_SAMPLER_DESC sdesc;
	ZeroMemory(&sdesc,sizeof(sdesc));
	sdesc.AddressU	= D3D11_TEXTURE_ADDRESS_WRAP;
	sdesc.AddressV	= D3D11_TEXTURE_ADDRESS_WRAP;
	sdesc.AddressW	= D3D11_TEXTURE_ADDRESS_WRAP;
	sdesc.Filter	= D3D11_FILTER_ANISOTROPIC;
	//sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;	

	V_RETURN(DXUTGetD3D11Device()->CreateSamplerState(&sdesc, &m_AnisotropicWrapSampler));
	DXUT_SetDebugName(m_AnisotropicWrapSampler, "RenderStd::m_LinearSampler");
	return hr;
}

HRESULT RenderTri::Destroy()
{
	HRESULT hr = S_OK;
	g_triEffectRegistry.Reset();
	SAFE_RELEASE(m_inputLayout);	
	SAFE_RELEASE(m_AnisotropicWrapSampler);
	//SAFE_RELEASE(m_envMapSampler);

	return hr;
}

HRESULT RenderTri::FrameRenderSkydome( ID3D11DeviceContext1* pd3dImmediateContext, const DXModel* model ) const
{
	HRESULT hr = S_OK;

	pd3dImmediateContext->HSSetShader(NULL, 0, 0);
	pd3dImmediateContext->DSSetShader(NULL, 0, 0);
	pd3dImmediateContext->GSSetShader(NULL, 0, 0);

	// setup vertex buffers
	DXUTGetD3D11DeviceContext()->IASetInputLayout( m_inputLayout );
	const UINT Offset0 = 0;
	const UINT Stride0 = sizeof(XMFLOAT4A);	
	const UINT Stride1 = sizeof(XMFLOAT4A);	
	const UINT Stride2 = sizeof(XMFLOAT2A);	

	pd3dImmediateContext->IASetVertexBuffers( 0, 1, &model->_vertexBuffer,	&Stride0, &Offset0 );
	pd3dImmediateContext->IASetVertexBuffers( 1, 1, &model->_normalsBuffer, &Stride1, &Offset0 );
	pd3dImmediateContext->IASetVertexBuffers( 2, 1, &model->_texcoordBuffer,&Stride2, &Offset0 );

	ID3D11ShaderResourceView* ppSRVNULL[] =  { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
	pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	for( const auto& submesh : model->submeshes) 
	{
		pd3dImmediateContext->VSSetShader(m_SkydomeVS->Get(), NULL, 0);		
		pd3dImmediateContext->PSSetShader(m_SkydomePS->Get(), NULL, 0);		

		pd3dImmediateContext->IASetIndexBuffer(	submesh._indexBuffer, DXGI_FORMAT_R32_UINT, 0 );
		pd3dImmediateContext->DrawIndexed(submesh._numFaces * 3, 0, 0);


	}
	pd3dImmediateContext->PSSetShaderResources(0, 8, ppSRVNULL);

	return hr;
}

//HRESULT RenderStd::FrameRender( ID3D11DeviceContext1* pd3dImmediateContext, const DXModel* model ) const
//{
//	HRESULT hr = S_OK;
//
//	pd3dImmediateContext->HSSetShader(NULL, 0, 0);
//	pd3dImmediateContext->DSSetShader(NULL, 0, 0);
//	pd3dImmediateContext->GSSetShader(NULL, 0, 0);
//
//	// setup vertex buffers
//	DXUTGetD3D11DeviceContext()->IASetInputLayout( m_inputLayout );
//	const UINT Offset0 = 0;
//	const UINT Stride0 = sizeof(XMFLOAT4A);	
//	const UINT Stride1 = sizeof(XMFLOAT4A);	
//	const UINT Stride2 = sizeof(XMFLOAT2A);	
//
//	pd3dImmediateContext->IASetVertexBuffers( 0, 1, &model->_vertexBuffer,	&Stride0, &Offset0 );
//	pd3dImmediateContext->IASetVertexBuffers( 1, 1, &model->_normalsBuffer, &Stride1, &Offset0 );
//	pd3dImmediateContext->IASetVertexBuffers( 2, 1, &model->_texcoordBuffer,&Stride2, &Offset0 );
//
//	ID3D11ShaderResourceView* ppSRVNULL[] =  { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
//	pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//	for( const auto& v : model->submeshes)
//	{		
//		if (g_app.g_bTimingsEnabled) 
//		{
//			g_app.WaitForGPU();
//			g_app.g_TimingLog.m_dDrawTotal -= GetTimeMS()/(double)(g_app.g_TimingLog.m_uNumRuns+1);
//			for (UINT i = 0; i < g_app.g_TimingLog.m_uNumRuns+1; i++) { //needs to be an odd number
//				//g_app.SetCameraConstantBuffersAllStages(pd3dImmediateContext);
//				FrameRenderInternal(pd3dImmediateContext, &v);
//			}
//			g_app.WaitForGPU();
//			g_app.g_TimingLog.m_dDrawTotal += GetTimeMS()/(double)(g_app.g_TimingLog.m_uNumRuns+1);
//			g_app.g_TimingLog.m_uDrawCount++;
//
//		}
//		else 
//		{
//			FrameRenderInternal(pd3dImmediateContext, &v);
//		}
//
//	}
//	pd3dImmediateContext->PSSetShaderResources(0, 8, ppSRVNULL);
//
//	return hr;
//}

HRESULT RenderTri::FrameRender( ID3D11DeviceContext1* pd3dImmediateContext, const ModelInstance* instance )  const
{
	HRESULT hr = S_OK;

	if(instance->IsSubD()) return S_OK;

	pd3dImmediateContext->HSSetShader(NULL, 0, 0);
	pd3dImmediateContext->DSSetShader(NULL, 0, 0);
	pd3dImmediateContext->GSSetShader(NULL, 0, 0);



	// checkme does not work with a single tri model, TODO check scene for number of tri models
	static 	DXModel* m_currBaseModel = NULL;
	//if(m_currBaseModel != instance->GetTriangleMesh() )
	{		
		
		m_currBaseModel = instance->GetTriangleMesh();
		const UINT Offset0 = 0;
		const UINT Stride0 = sizeof(XMFLOAT4A);	
		const UINT Stride1 = sizeof(XMFLOAT4A);	
		const UINT Stride2 = sizeof(XMFLOAT2A);	

	
		pd3dImmediateContext->IASetVertexBuffers( 0, 1, &m_currBaseModel->_vertexBuffer,  &Stride0, &Offset0 );
		pd3dImmediateContext->IASetVertexBuffers( 1, 1, &m_currBaseModel->_normalsBuffer, &Stride1, &Offset0 );
		pd3dImmediateContext->IASetVertexBuffers( 2, 1, &m_currBaseModel->_texcoordBuffer,&Stride2, &Offset0 );
			
	}

	// setup vertex buffers
	//pd3dImmediateContext->IASetInputLayout( m_inputLayout );

	if (g_app.g_bTimingsEnabled) 
	{
		g_app.WaitForGPU();
		g_app.g_TimingLog.m_dDrawTotal -= GetTimeMS()/(double)(g_app.g_TimingLog.m_uNumRuns+1);
		for (UINT i = 0; i < g_app.g_TimingLog.m_uNumRuns+1; i++) { //needs to be an odd number
			FrameRenderInternal(pd3dImmediateContext, instance);
		}
		g_app.WaitForGPU();
		g_app.g_TimingLog.m_dDrawTotal += GetTimeMS()/(double)(g_app.g_TimingLog.m_uNumRuns+1);
		g_app.g_TimingLog.m_uDrawCount++;

	}
	else 
	{
		FrameRenderInternal(pd3dImmediateContext, instance);
	}

	
	pd3dImmediateContext->PSSetShaderResources(0, 8, g_ppSRVNULL);

	return hr;
}

HRESULT RenderTri::FrameRenderInternal( ID3D11DeviceContext1* pd3dImmediateContext, const ModelInstance* instance ) const
{
	HRESULT hr = S_OK;
		
	const TriangleSubmesh* submesh = instance->GetTriangleSubMesh();
	const DXMaterial* mat = submesh->GetMaterial();

	if(g_app.g_bShowVoxelization && instance->IsCollider())
		return hr;

	Effect effect;

	if(m_GenShadows)
	{	
		effect.shadow = UINT(m_GenShadowFast ? TriShadowMode::GENERATE_GS : TriShadowMode::GENERATE);
		effect.numShadowCascades = NUM_CASCADES == 4 ? 1 : 0; // for shader precompile
		if(mat->HasAlphaTex())			effect.alpha = 1;
	}
	else
	{
		effect.wire = m_UseWireframeOverlay ? DISPLAY_WIRE_ON_SHADED: DISPLAY_SHADED;
		if(mat->HasDiffuseTex())		effect.color = UINT(TriColorMode::TEX);
		if(mat->HasDisplacementTex())	effect.displacement = UINT(TriDisplacementMode::TEX);
		if(mat->HasNormalsTex())		effect.normal = 1;
		if(mat->HasAlphaTex())			effect.alpha = 1;
		if(mat->HasSpecularTex())		effect.specular = 1;
		if(g_app.g_envMapSRV)			effect.envmap =1;
				
		if(m_UseSSAO)					effect.ssao = 1;
		if(m_UseShadows)				effect.shadow = UINT(TriShadowMode::RENDER);	
		effect.numShadowCascades = NUM_CASCADES == 4 ? 1 : 0; // for shader precompile
	}

	pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	BindShaders(pd3dImmediateContext, effect, instance, mat);

	pd3dImmediateContext->IASetIndexBuffer(	submesh->_indexBuffer, DXGI_FORMAT_R32_UINT, 0 );
	pd3dImmediateContext->IASetInputLayout(m_inputLayout);
	pd3dImmediateContext->DrawIndexed(submesh->_numFaces * 3, 0, 0);

	
	pd3dImmediateContext->HSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->DSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);

	return hr;
}

HRESULT RenderTri::Voxelize( ID3D11DeviceContext1* pd3dImmediateContext, const ModelInstance* instance ) const
{
	HRESULT hr = S_OK;

	if(instance->IsSubD()) return hr;

	if (g_app.g_bTimingsEnabled) {
		g_app.WaitForGPU();

		g_app.g_TimingLog.m_dVoxelizationTotal -= GetTimeMS()/(double)(g_app.g_TimingLog.m_uNumRuns+1);

		for (UINT i = 0; i < g_app.g_TimingLog.m_uNumRuns+1; i++)  //needs to be an odd number
		{
			VoxelizeInternal(pd3dImmediateContext, instance);
		}

		g_app.WaitForGPU();
		g_app.g_TimingLog.m_dVoxelizationTotal += GetTimeMS()/(double)(g_app.g_TimingLog.m_uNumRuns+1);
		g_app.g_TimingLog.m_uVoxelizationCount++;
	}
	else 
	{
		VoxelizeInternal(pd3dImmediateContext, instance);
	}	

	return hr;
}

HRESULT RenderTri::VoxelizeInternal( ID3D11DeviceContext1* pd3dImmediateContext, const ModelInstance* instance ) const
{
	HRESULT hr = S_OK;
	pd3dImmediateContext->HSSetShader(NULL, 0, 0);
	pd3dImmediateContext->DSSetShader(NULL, 0, 0);
	pd3dImmediateContext->GSSetShader(NULL, 0, 0);


	// setup vertex buffers
	DXUTGetD3D11DeviceContext()->IASetInputLayout( m_inputLayout );
	const UINT Offset0 = 0;
	const UINT Stride0 = sizeof(XMFLOAT4A);	
	const UINT Stride1 = sizeof(XMFLOAT4A);	
	const UINT Stride2 = sizeof(XMFLOAT2A);	

	// setup shared attributes
	pd3dImmediateContext->IASetVertexBuffers( 0, 1, &instance->GetTriangleMesh()->_vertexBuffer,   &Stride0, &Offset0 );
	pd3dImmediateContext->IASetVertexBuffers( 1, 1, &instance->GetTriangleMesh()->_normalsBuffer,  &Stride1, &Offset0 );
	pd3dImmediateContext->IASetVertexBuffers( 2, 1, &instance->GetTriangleMesh()->_texcoordBuffer, &Stride2, &Offset0 );


	ID3D11ShaderResourceView* ppSRVNULL[] =  { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

	pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	const bool voxelizeBackward = instance->GetVoxelGridDefinition().m_bFillSolidBackward;	
	pd3dImmediateContext->VSSetShader(m_colorVS->Get(), NULL, 0);

	if(voxelizeBackward)	pd3dImmediateContext->PSSetShader(m_PSVoxelizeSolidBackward->Get(), NULL, 0);
	else					pd3dImmediateContext->PSSetShader(m_PSVoxelizeSolidForward->Get(), NULL, 0);	


	pd3dImmediateContext->IASetIndexBuffer(	instance->GetTriangleSubMesh()->_indexBuffer, DXGI_FORMAT_R32_UINT, 0 );
	pd3dImmediateContext->DrawIndexed(instance->GetTriangleSubMesh()->_numFaces * 3, 0, 0);
	pd3dImmediateContext->PSSetShaderResources(0, 8, ppSRVNULL);

	return hr;
}

void RenderTri::BindShaders( ID3D11DeviceContext1* pd3dImmediateContext, const Effect effect, const ModelInstance* instance, const DXMaterial* material) const
{
	const D3D11_INPUT_ELEMENT_DESC hInElementDesc[] = 
	{
		{"POSITION" , 0, DXGI_FORMAT_R32G32B32A32_FLOAT	, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL"	, 0, DXGI_FORMAT_R32G32B32A32_FLOAT	, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD" , 0, DXGI_FORMAT_R32G32_FLOAT		, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
		
	EffectDrawRegistryTriangle::ConfigType * config = g_triEffectRegistry.GetDrawConfig( effect, DXUTGetD3D11Device(),
																					  &(const_cast<ID3D11InputLayout*>(m_inputLayout)), hInElementDesc, ARRAYSIZE(hInElementDesc));

	assert(m_inputLayout);
	
	pd3dImmediateContext->VSSetShader(config->vertexShader->Get(),NULL, 0);
	pd3dImmediateContext->DSSetShader(NULL,NULL, 0);
	pd3dImmediateContext->HSSetShader(NULL,NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL,NULL, 0);
	//if(effect.shadow ==0)
	{
		pd3dImmediateContext->PSSetShader(config->pixelShader->Get(),NULL, 0);
		pd3dImmediateContext->PSSetConstantBuffers( CB_LOC::MATERIAL, 1, &material->_cbMat );	
		pd3dImmediateContext->PSSetSamplers(0, 1, &m_AnisotropicWrapSampler);
	}
	//else pd3dImmediateContext->PSSetSamplers(NULL,NULL, 0);
		
	if(effect.wire == DISPLAY_SHADED)
	{
		pd3dImmediateContext->GSSetShader(NULL, NULL, 0);			
	}
	else
	{			
		pd3dImmediateContext->GSSetShader(config->geometryShader->Get(), NULL, 0);		
	}

	if(effect.color != (UINT) TriColorMode::NONE || effect.normal == 1)
	{
		pd3dImmediateContext->PSSetSamplers(0, 1, &m_AnisotropicWrapSampler);
	}

	if(effect.color == (UINT)TriColorMode::TEX)
	{
		ID3D11ShaderResourceView* ppSRV[] = { material->GetDiffuseTexSRV()};
		pd3dImmediateContext->PSSetShaderResources(0, 1, ppSRV);
	}
	else if( effect.color == (UINT)TriColorMode::MULTIRESATTR)
	{
		std::cout << "todo" << std::endl;
	}

	if(effect.normal == 1)
	{
		ID3D11ShaderResourceView* ppSRV[] = { material->GetNormalsTexSRV()};
		pd3dImmediateContext->PSSetShaderResources(1, 1, ppSRV);		
	}

	if(effect.displacement == UINT(TriDisplacementMode::TEX))
	{
		ID3D11ShaderResourceView* ppSRV[] = { material->GetDisplacementTexSRV()};
		pd3dImmediateContext->PSSetShaderResources(2, 1, ppSRV);		
	}

	if(effect.alpha == 1)
	{
		ID3D11ShaderResourceView* ppSRV[] = { material->GetAlphaTexSRV()};
		pd3dImmediateContext->PSSetShaderResources(3, 1, ppSRV);		
	}

	if(effect.specular == 1)
	{
		ID3D11ShaderResourceView* ppSRV[] = { material->GetSpecularTexSRV()};
		pd3dImmediateContext->PSSetShaderResources(4, 1, ppSRV);		
	}

	if(effect.shadow == (UINT)TriShadowMode::GENERATE_GS)
	{
		pd3dImmediateContext->GSSetShader(config->geometryShader->Get(),NULL, 0);
	}


	if(g_app.g_envMapSRV)
	{
		ID3D11ShaderResourceView* ppSRV[] = { g_app.g_envMapSRV};
		pd3dImmediateContext->PSSetShaderResources(5, 1, ppSRV);		
	}
}


EffectDrawRegistryTriangle::ConfigType * EffectDrawRegistryTriangle::_CreateDrawConfig( DescType const & desc, SourceConfigType const * sconfig, ID3D11Device1 * pd3dDevice, ID3D11InputLayout ** ppInputLayout, D3D11_INPUT_ELEMENT_DESC const * pInputElementDescs, int numInputElements ) const  // make const
{
	ConfigType * config = DrawRegistryBase::_CreateDrawConfig(desc.value, sconfig, pd3dDevice, ppInputLayout, pInputElementDescs, numInputElements);
	assert(config);

	return config;
}

EffectDrawRegistryTriangle::SourceConfigType * EffectDrawRegistryTriangle::_CreateDrawSourceConfig( DescType const & desc, ID3D11Device1 * pd3dDevice ) const// make const
{
	Effect effect = desc;
	
	SourceConfigType * sconfig = DrawRegistryBase::_CreateDrawSourceConfig(desc.value, pd3dDevice);
	assert(sconfig);
		

	sconfig->commonConfig.common_render_hash = effect.value;
	sconfig->value = effect.value;

	const wchar_t* shaderFileTri = L"shader/renderTri.hlsl";

	std::ostringstream ss;

	if(effect.multiresAttributes == 1)
	{
		std::cout << "todo" << std::endl;
	}

	sconfig->vertexShader.sourceFile = shaderFileTri;	
	sconfig->pixelShader.sourceFile  = shaderFileTri;


	sconfig->vertexShader.entry = "VS_Std";
	sconfig->pixelShader.entry  = "PS_Std";
	sconfig->pixelShader.AddDefine("PIXEL_SHADING");
	
	if(effect.ssao    == 1)								sconfig->pixelShader.AddDefine("WITH_SSAO");	
	if(effect.color != (UINT)TriColorMode::NONE)
	{
		sconfig->commonShader.AddDefine("WITH_TEXTURE");
		if(effect.normal == 1)								sconfig->pixelShader.AddDefine("WITH_NORMAL_TEX");
		if(effect.displacement == (UINT)TriDisplacementMode::TEX)		sconfig->commonShader.AddDefine("WITH_DISPLACEMENT_TEX");
		if(effect.specular == 1)							sconfig->pixelShader.AddDefine("WITH_SPECULAR_TEX");	
	}
	if(effect.alpha == 1)								sconfig->commonShader.AddDefine("WITH_ALPHA_TEX");
	
	

	if(effect.wire != TriDisplayType::DISPLAY_SHADED)
	{			
		sconfig->geometryShader.sourceFile = shaderFileTri;
		sconfig->geometryShader.target = "gs_5_0";

		sconfig->geometryShader.entry = "gs_main";

		if(effect.wire == TriDisplayType::DISPLAY_WIRE)
		{
			sconfig->geometryShader.AddDefine("GEOMETRY_OUT_WIRE");
			sconfig->pixelShader.AddDefine("GEOMETRY_OUT_WIRE");
		}
		else
		{			
			sconfig->geometryShader.AddDefine("GEOMETRY_OUT_LINE");
			sconfig->pixelShader.AddDefine("GEOMETRY_OUT_LINE");
		}
	}

	if(effect.shadow == (UINT)TriShadowMode::RENDER || effect.shadow == (UINT)TriShadowMode::NONE)
	{
		sconfig->commonShader.AddDefine("WITH_WS_SHADING");
		if(effect.envmap == 1)		
		sconfig->pixelShader.AddDefine("WITH_ENVMAP");
	
	}


	if(effect.shadow == (UINT)TriShadowMode::RENDER)	
	{
		sconfig->commonShader.AddDefine("WITH_SHADOW");
		ss.str("");	ss << NUM_CASCADES;
		sconfig->commonShader.AddDefine("NUM_CASCADES", ss.str());
	}
	else if(effect.shadow == (UINT)TriShadowMode::GENERATE)
	{
		sconfig->vertexShader.entry = "VS_Solid";
		sconfig->pixelShader.entry =  "PS_GenVarianceShadow";
	}
	else if(effect.shadow == (UINT)TriShadowMode::GENERATE_GS)
	{
		sconfig->vertexShader.entry		=  "VS_GenVarianceShadowFast";
		sconfig->geometryShader.entry	=  "GS_GenVarianceShadowFast";
		sconfig->pixelShader.entry		=  "PS_GenVarianceShadowFast";
				
		ss.str("");	ss << NUM_CASCADES;
		sconfig->geometryShader.AddDefine("NUM_CASCADES", ss.str());
		sconfig->geometryShader.sourceFile = shaderFileTri;
	}


	return sconfig;
}

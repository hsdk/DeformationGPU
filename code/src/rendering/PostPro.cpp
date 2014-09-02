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

#include "PostPro.h"
#include "App.h"
#include <SDX/DXTextureCache.h>
#include <SDX/DXBuffer.h>
//Henry: has to be last header
#include "utils/DbgNew.h"

using namespace DirectX;

static float bgColor[4] = {0,0,0,1};

PostPro::PostPro()
{
	m_fsQuadInputLayout					= NULL;
	m_fsQuadVertexBuffer				= NULL;

	m_fsQuadVS							= NULL;
	m_fsQuadPS							= NULL;

	m_ssaoPS							= NULL;
	m_ssoaBlurVerticalPS				= NULL;
	m_ssaoBlurHorizontalPS				= NULL;
	m_ssaoComposePS						= NULL;

	m_blur4HPS							= NULL;
	m_blur4VPS							= NULL;
	
	m_blur16HPS							= NULL;
	m_blur16VPS							= NULL;
	
	m_blur32HPS							= NULL;
	m_blur32VPS							= NULL;
	
	m_bokeh1PS							= NULL;
	m_bokeh2PS							= NULL;
	
	m_streaksPS							= NULL;
	m_mergePS							= NULL;

	m_noiseTextureSRV					= NULL;

	m_samplerStateWrap					= NULL;
	m_samplerStateClamp					= NULL;

	m_depthStencilStateDisableDepth		= NULL;

	m_ColorTEX							= NULL;
	m_ColorSRV							= NULL;
	m_ColorRTV							= NULL;

	m_ColorMSAATEX						= NULL;
	m_ColorMSAARTV						= NULL;
	m_ColorMSAASRV						= NULL;

	m_Color1TEX							= NULL;
	m_Color1SRV							= NULL;
	m_Color1RTV							= NULL;

	m_Color1MSAATEX						= NULL;
	m_Color1MSAARTV						= NULL;
	m_Color1MSAASRV						= NULL;

	m_FBO0Tex							= NULL;
	m_FBO0RTV							= NULL;
	m_FBO0SRV							= NULL;

	m_FBO1Tex							= NULL;
	m_FBO1RTV							= NULL;
	m_FBO1SRV							= NULL;

	m_RTT0Tex_1							= NULL;
	m_RTT0RTV_1							= NULL;
	m_RTT0SRV_1							= NULL;

	m_RTT1Tex_1							= NULL;
	m_RTT1RTV_1							= NULL;
	m_RTT1SRV_1							= NULL;

	m_RTT1bTex_1						= NULL;
	m_RTT1bRTV_1						= NULL;
	m_RTT1bSRV_1						= NULL;
	
	m_RTT0Tex_4							= NULL;
	m_RTT0RTV_4							= NULL;
	m_RTT0SRV_4							= NULL;

	m_RTT1Tex_4							= NULL;
	m_RTT1RTV_4							= NULL;
	m_RTT1SRV_4							= NULL;

	m_RTT0Tex_16						= NULL;
	m_RTT0RTV_16						= NULL;
	m_RTT0SRV_16						= NULL;
			  							
	m_RTT1Tex_16						= NULL;
	m_RTT1RTV_16						= NULL;
	m_RTT1SRV_16						= NULL;
										
	m_RTT0Tex_32						= NULL;
	m_RTT0RTV_32						= NULL;
	m_RTT0SRV_32						= NULL;
			  							
	m_RTT1Tex_32						= NULL;
	m_RTT1RTV_32						= NULL;
	m_RTT1SRV_32						= NULL;

	m_DepthStencilTEX					= NULL;
	m_DepthStencilView					= NULL;
	m_DepthStencilViewSRV				= NULL;

	m_cbPostProcessing					= NULL;
	m_bokehFocus = 60.0f;

	m_width = 0;
	m_height = 0;

}

PostPro::~PostPro()
{
	Destroy();
}

HRESULT PostPro::Create( ID3D11Device1* pd3dDevice )
{
	HRESULT hr = S_OK;
	ID3DBlob* pBlob = NULL;

	// Full screen quad
	m_fsQuadVS = g_shaderManager.AddVertexShader(L"shader/SSAO.hlsl", "fsQuadVS", "vs_5_0",  &pBlob);
	const D3D11_INPUT_ELEMENT_DESC inputElementDescs[] = {
		"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0,
		"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0,
	};

	V_RETURN(pd3dDevice->CreateInputLayout(inputElementDescs, ARRAYSIZE(inputElementDescs), pBlob->GetBufferPointer(), pBlob->GetBufferSize(), &m_fsQuadInputLayout));
	m_fsQuadPS = g_shaderManager.AddPixelShader( L"shader/SSAO.hlsl", "fsQuadPS", "ps_5_0", &pBlob);

	//D3D_SHADER_MACRO bokehPS1_MSAA_macro[]	= { { "WITH_MSAA", "1"}, { 0 } };
						


	// SSAO
	m_ssaoPS				= g_shaderManager.AddPixelShader(	L"shader/SSAO.hlsl", "ssaoPS",		"ps_5_0", &pBlob);
	m_ssoaBlurVerticalPS	= g_shaderManager.AddPixelShader(	L"shader/SSAO.hlsl", "blurVPS",		"ps_5_0", &pBlob);
	m_ssaoBlurHorizontalPS	= g_shaderManager.AddPixelShader(	L"shader/SSAO.hlsl", "blurHPS",		"ps_5_0", &pBlob);	
	m_ssaoComposePS			= g_shaderManager.AddPixelShader(	L"shader/SSAO.hlsl", "composePS",	"ps_5_0", &pBlob);

	m_blur4HPS				= g_shaderManager.AddPixelShader(	L"shader/PostProcessing.hlsl", "blur4HPS",	"ps_5_0", &pBlob);
	m_blur4VPS				= g_shaderManager.AddPixelShader(	L"shader/PostProcessing.hlsl", "blur4VPS",	"ps_5_0", &pBlob);
	
	m_blur16HPS				= g_shaderManager.AddPixelShader(	L"shader/PostProcessing.hlsl", "blur16HPS",	"ps_5_0", &pBlob);
	m_blur16VPS				= g_shaderManager.AddPixelShader(	L"shader/PostProcessing.hlsl", "blur16VPS",	"ps_5_0", &pBlob);
	
	m_blur32HPS				= g_shaderManager.AddPixelShader(	L"shader/PostProcessing.hlsl", "blur32HPS",	"ps_5_0", &pBlob);
	m_blur32VPS				= g_shaderManager.AddPixelShader(	L"shader/PostProcessing.hlsl", "blur32VPS",	"ps_5_0", &pBlob);
	
	m_bokeh1PS				= g_shaderManager.AddPixelShader(	L"shader/PostProcessing.hlsl", "bokeh1PS",	"ps_5_0", &pBlob);
	//m_bokeh1PSMSAA			= g_shaderManager.AddPixelShader(	L"shader/PostProcessing.hlsl", "bokeh1PS",	"ps_5_0", &pBlob, bokehPS1_MSAA_macro);
	m_bokeh2PS				= g_shaderManager.AddPixelShader(	L"shader/PostProcessing.hlsl", "bokeh2PS",	"ps_5_0", &pBlob);
	
	m_streaksPS				= g_shaderManager.AddPixelShader(	L"shader/PostProcessing.hlsl", "streaksPS",	"ps_5_0", &pBlob);
	m_mergePS				= g_shaderManager.AddPixelShader(	L"shader/PostProcessing.hlsl", "mergePS",	"ps_5_0", &pBlob);


	SAFE_RELEASE( pBlob );


	CD3D11_DEFAULT d;
	CD3D11_DEPTH_STENCIL_DESC dsDesc(d);
	//ZeroMemory(&dsDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	dsDesc.DepthEnable = FALSE;
	//dsDesc.StencilEnable = FALSE;
	//dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	V_RETURN(DXUTGetD3D11Device()->CreateDepthStencilState(&dsDesc, &m_depthStencilStateDisableDepth));


	D3D11_SAMPLER_DESC SamDescShad =  
	{
		//D3D11_FILTER_ANISOTROPIC,		// D3D11_FILTER Filter;
		D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		D3D11_TEXTURE_ADDRESS_WRAP,		//D3D11_TEXTURE_ADDRESS_MODE AddressU;
		D3D11_TEXTURE_ADDRESS_WRAP,		//D3D11_TEXTURE_ADDRESS_MODE AddressV;
		D3D11_TEXTURE_ADDRESS_BORDER,	//D3D11_TEXTURE_ADDRESS_MODE AddressW;
		0,								//FLOAT MipLODBias;
		2,								//UINT MaxAnisotropy;
		D3D11_COMPARISON_NEVER ,		//D3D11_COMPARISON_FUNC ComparisonFunc;
		0.0,0.0,0.0,0.0,				//FLOAT BorderColor[ 4 ];
		0,								//FLOAT MinLOD;
		0								//FLOAT MaxLOD;   
	};
	SamDescShad.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SamDescShad.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	V_RETURN(DXUTGetD3D11Device()->CreateSamplerState(&SamDescShad, &m_samplerStateWrap));


	SamDescShad.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamDescShad.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	V_RETURN(DXUTGetD3D11Device()->CreateSamplerState(&SamDescShad, &m_samplerStateClamp));
	
	m_noiseTextureSRV = g_textureManager.AddTexture("../../media/textures/noise.dds");
	
	V_RETURN(CreateFullScreenQuad(pd3dDevice));


	V_RETURN(DXCreateBuffer( pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_POSTPROCESSING), D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, m_cbPostProcessing));
	
	return hr;
}

void PostPro::Destroy()
{
	SAFE_RELEASE(m_fsQuadInputLayout			);
	SAFE_RELEASE(m_fsQuadVertexBuffer			);
	
	//SAFE_RELEASE(m_noiseTextureSRV				);
	
	SAFE_RELEASE(m_samplerStateWrap				);
	SAFE_RELEASE(m_samplerStateClamp			);
	
	SAFE_RELEASE(m_depthStencilStateDisableDepth);
	
	SAFE_RELEASE(m_ColorTEX						);
	SAFE_RELEASE(m_ColorSRV						);
	SAFE_RELEASE(m_ColorRTV						);
	
	SAFE_RELEASE(m_ColorMSAATEX				);
	SAFE_RELEASE(m_ColorMSAARTV				);
	SAFE_RELEASE(m_ColorMSAASRV				);
	
	SAFE_RELEASE(m_Color1TEX					);
	SAFE_RELEASE(m_Color1SRV					);
	SAFE_RELEASE(m_Color1RTV					);
	
	SAFE_RELEASE(m_Color1MSAATEX				);
	SAFE_RELEASE(m_Color1MSAARTV				);
	SAFE_RELEASE(m_Color1MSAASRV				);
	
	SAFE_RELEASE(m_FBO0Tex						);
	SAFE_RELEASE(m_FBO0RTV						);
	SAFE_RELEASE(m_FBO0SRV						);
	
	SAFE_RELEASE(m_FBO1Tex						);
	SAFE_RELEASE(m_FBO1RTV						);
	SAFE_RELEASE(m_FBO1SRV						);


	SAFE_RELEASE(m_RTT0Tex_1					);
	SAFE_RELEASE(m_RTT0RTV_1					);
	SAFE_RELEASE(m_RTT0SRV_1					);

	SAFE_RELEASE(m_RTT1Tex_1					);
	SAFE_RELEASE(m_RTT1RTV_1					);
	SAFE_RELEASE(m_RTT1SRV_1					);

	SAFE_RELEASE(m_RTT1bTex_1					);
	SAFE_RELEASE(m_RTT1bRTV_1					);
	SAFE_RELEASE(m_RTT1bSRV_1					);

	SAFE_RELEASE(m_RTT0Tex_4					);
	SAFE_RELEASE(m_RTT0RTV_4					);
	SAFE_RELEASE(m_RTT0SRV_4					);

	SAFE_RELEASE(m_RTT1Tex_4					);
	SAFE_RELEASE(m_RTT1RTV_4					);
	SAFE_RELEASE(m_RTT1SRV_4					);

	SAFE_RELEASE(m_RTT0Tex_16					);
	SAFE_RELEASE(m_RTT0RTV_16					);
	SAFE_RELEASE(m_RTT0SRV_16					);

	SAFE_RELEASE(m_RTT1Tex_16					);
	SAFE_RELEASE(m_RTT1RTV_16					);
	SAFE_RELEASE(m_RTT1SRV_16					);

	SAFE_RELEASE(m_RTT0Tex_32					);
	SAFE_RELEASE(m_RTT0RTV_32					);
	SAFE_RELEASE(m_RTT0SRV_32					);

	SAFE_RELEASE(m_RTT1Tex_32					);
	SAFE_RELEASE(m_RTT1RTV_32					);
	SAFE_RELEASE(m_RTT1SRV_32					);
	
	SAFE_RELEASE(m_DepthStencilTEX				);
	SAFE_RELEASE(m_DepthStencilView				);
	SAFE_RELEASE(m_DepthStencilViewSRV			);

	SAFE_RELEASE(m_cbPostProcessing);
}

HRESULT PostPro::Resize( ID3D11Device1* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
{
	HRESULT hr = S_OK;
	SAFE_RELEASE(m_ColorTEX);
	SAFE_RELEASE(m_ColorRTV);
	SAFE_RELEASE(m_ColorSRV);

	SAFE_RELEASE(m_ColorMSAATEX);
	SAFE_RELEASE(m_ColorMSAASRV);
	SAFE_RELEASE(m_ColorMSAARTV);
	

	SAFE_RELEASE(m_Color1TEX);
	SAFE_RELEASE(m_Color1SRV);
	SAFE_RELEASE(m_Color1RTV);
	

	SAFE_RELEASE(m_Color1MSAATEX);
	SAFE_RELEASE(m_Color1MSAASRV);
	SAFE_RELEASE(m_Color1MSAARTV);
	

	SAFE_RELEASE(m_FBO0Tex);
	SAFE_RELEASE(m_FBO0SRV);
	SAFE_RELEASE(m_FBO0RTV);
	

	SAFE_RELEASE(m_FBO1Tex);
	SAFE_RELEASE(m_FBO1RTV);
	SAFE_RELEASE(m_FBO1SRV);

	SAFE_RELEASE(m_RTT0Tex_1);
	SAFE_RELEASE(m_RTT0RTV_1);
	SAFE_RELEASE(m_RTT0SRV_1);

	SAFE_RELEASE(m_RTT1Tex_1);
	SAFE_RELEASE(m_RTT1RTV_1);
	SAFE_RELEASE(m_RTT1SRV_1);

	SAFE_RELEASE(m_RTT1bTex_1);
	SAFE_RELEASE(m_RTT1bRTV_1);
	SAFE_RELEASE(m_RTT1bSRV_1);

	SAFE_RELEASE(m_RTT0Tex_4);
	SAFE_RELEASE(m_RTT0RTV_4);
	SAFE_RELEASE(m_RTT0SRV_4);

	SAFE_RELEASE(m_RTT1Tex_4);
	SAFE_RELEASE(m_RTT1RTV_4);
	SAFE_RELEASE(m_RTT1SRV_4);

	SAFE_RELEASE(m_RTT0Tex_16);
	SAFE_RELEASE(m_RTT0RTV_16);
	SAFE_RELEASE(m_RTT0SRV_16);

	SAFE_RELEASE(m_RTT1Tex_16);
	SAFE_RELEASE(m_RTT1RTV_16);
	SAFE_RELEASE(m_RTT1SRV_16);

	SAFE_RELEASE(m_RTT0Tex_32);
	SAFE_RELEASE(m_RTT0RTV_32);
	SAFE_RELEASE(m_RTT0SRV_32);

	SAFE_RELEASE(m_RTT1Tex_32);
	SAFE_RELEASE(m_RTT1RTV_32);
	SAFE_RELEASE(m_RTT1SRV_32);


	m_width   = pBackBufferSurfaceDesc->Width;
	m_height  = pBackBufferSurfaceDesc->Height;
	std::cout << "resize ssao" << m_width << "," << m_height << std::endl;

	D3D11_TEXTURE2D_DESC texDesc;
	//ZeroMemory(&texDesc, sizeof(D3D11_TEXTURE2D_DESC));
	texDesc.Width              = m_width;
	texDesc.Height             = m_height;
	texDesc.ArraySize          = 1;
	texDesc.MiscFlags          = 0;
	texDesc.MipLevels          = 1;
	texDesc.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.Usage              = D3D11_USAGE_DEFAULT;
	texDesc.CPUAccessFlags     = NULL;

	// Allocate MSAA color buffer
	texDesc.SampleDesc.Count   = DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count;//g_MSAADesc[g_MSAACurrentSettings].samples;
	texDesc.SampleDesc.Quality = DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Quality;//g_MSAADesc[g_MSAACurrentSettings].quality;
	texDesc.Format             = DXGI_FORMAT_R32G32B32A32_FLOAT;	//pBackBufferSurfaceDesc->Format;

	V_RETURN(pd3dDevice->CreateTexture2D(			&texDesc,			NULL, &m_ColorMSAATEX));
	V_RETURN(pd3dDevice->CreateRenderTargetView(	m_ColorMSAATEX,	NULL, &m_ColorMSAARTV));
	V_RETURN(pd3dDevice->CreateShaderResourceView(	m_ColorMSAATEX,	NULL, &m_ColorMSAASRV));

	V_RETURN(pd3dDevice->CreateTexture2D(&texDesc, NULL,					&m_Color1MSAATEX));
	V_RETURN(pd3dDevice->CreateRenderTargetView(m_Color1MSAATEX, NULL,	&m_Color1MSAARTV));
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_Color1MSAATEX, NULL,	&m_Color1MSAASRV));

	texDesc.SampleDesc.Count   = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Format             = DXGI_FORMAT_R32G32B32A32_FLOAT;

	// flip flop textures and rtvs
	V_RETURN(pd3dDevice->CreateTexture2D(&texDesc, NULL, &m_ColorTEX));
	V_RETURN(pd3dDevice->CreateRenderTargetView(m_ColorTEX, NULL, &m_ColorRTV));
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_ColorTEX, NULL, &m_ColorSRV));

	V_RETURN(pd3dDevice->CreateTexture2D(&texDesc, NULL, &m_Color1TEX));
	V_RETURN(pd3dDevice->CreateRenderTargetView(m_Color1TEX, NULL, &m_Color1RTV));
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_Color1TEX, NULL, &m_Color1SRV));

	// Allocate non-MSAA color buffer


	V_RETURN(pd3dDevice->CreateTexture2D(			&texDesc, NULL,			&m_FBO0Tex));
	V_RETURN(pd3dDevice->CreateRenderTargetView(	m_FBO0Tex, NULL,		&m_FBO0RTV));
	V_RETURN(pd3dDevice->CreateShaderResourceView(	m_FBO0Tex, NULL,		&m_FBO0SRV));

	V_RETURN(pd3dDevice->CreateTexture2D(			&texDesc, NULL,			&m_FBO1Tex));
	V_RETURN(pd3dDevice->CreateRenderTargetView(	m_FBO1Tex, NULL,		&m_FBO1RTV));
	V_RETURN(pd3dDevice->CreateShaderResourceView(	m_FBO1Tex, NULL,		&m_FBO1SRV));

	{
		// Render targets for PostProcessing Pipeline
		D3D11_TEXTURE2D_DESC texDesc;
		//ZeroMemory(&texDesc, sizeof(D3D11_TEXTURE2D_DESC));
		texDesc.ArraySize          = 1;
		texDesc.MipLevels          = 0;
		texDesc.MiscFlags          = 1;//D3D11_RESOURCE_MISC_GENERATE_MIPS;
		texDesc.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		texDesc.Usage              = D3D11_USAGE_DEFAULT;
		texDesc.CPUAccessFlags     = NULL;
		texDesc.SampleDesc.Count   = 1;//g_MSAADesc[g_MSAACurrentSettings].samples;
		texDesc.SampleDesc.Quality = 0;//g_MSAADesc[g_MSAACurrentSettings].quality;
		texDesc.Format             = DXGI_FORMAT_R32G32B32A32_FLOAT;//pBackBufferSurfaceDesc->Format;

		texDesc.Width              = m_width;
		texDesc.Height             = m_height;
		V_RETURN(pd3dDevice->CreateTexture2D(			&texDesc, NULL,			&m_RTT0Tex_1));
		V_RETURN(pd3dDevice->CreateRenderTargetView(	m_RTT0Tex_1, NULL,		&m_RTT0RTV_1));
		V_RETURN(pd3dDevice->CreateShaderResourceView(	m_RTT0Tex_1, NULL,		&m_RTT0SRV_1));
		
		V_RETURN(pd3dDevice->CreateTexture2D(			&texDesc, NULL,			&m_RTT1Tex_1));
		V_RETURN(pd3dDevice->CreateRenderTargetView(	m_RTT1Tex_1, NULL,		&m_RTT1RTV_1));
		V_RETURN(pd3dDevice->CreateShaderResourceView(	m_RTT1Tex_1, NULL,		&m_RTT1SRV_1));		

		V_RETURN(pd3dDevice->CreateTexture2D(			&texDesc, NULL,			&m_RTT1bTex_1));
		V_RETURN(pd3dDevice->CreateRenderTargetView(	m_RTT1bTex_1, NULL,		&m_RTT1bRTV_1));
		V_RETURN(pd3dDevice->CreateShaderResourceView(	m_RTT1bTex_1, NULL,		&m_RTT1bSRV_1));		

		texDesc.Width              = m_width/4;
		texDesc.Height             = m_height/4;
		V_RETURN(pd3dDevice->CreateTexture2D(			&texDesc, NULL,			&m_RTT0Tex_4));
		V_RETURN(pd3dDevice->CreateRenderTargetView(	m_RTT0Tex_4, NULL,		&m_RTT0RTV_4));
		V_RETURN(pd3dDevice->CreateShaderResourceView(	m_RTT0Tex_4, NULL,		&m_RTT0SRV_4));
																						   
		V_RETURN(pd3dDevice->CreateTexture2D(			&texDesc, NULL,			&m_RTT1Tex_4));
		V_RETURN(pd3dDevice->CreateRenderTargetView(	m_RTT1Tex_4, NULL,		&m_RTT1RTV_4));
		V_RETURN(pd3dDevice->CreateShaderResourceView(	m_RTT1Tex_4, NULL,		&m_RTT1SRV_4));

		texDesc.Width              = m_width/16;
		texDesc.Height             = m_height/16;
		V_RETURN(pd3dDevice->CreateTexture2D(			&texDesc, NULL,			&m_RTT0Tex_16));
		V_RETURN(pd3dDevice->CreateRenderTargetView(	m_RTT0Tex_16, NULL,		&m_RTT0RTV_16));
		V_RETURN(pd3dDevice->CreateShaderResourceView(	m_RTT0Tex_16, NULL,		&m_RTT0SRV_16));
																						  
		V_RETURN(pd3dDevice->CreateTexture2D(			&texDesc, NULL,			&m_RTT1Tex_16));
		V_RETURN(pd3dDevice->CreateRenderTargetView(	m_RTT1Tex_16, NULL,		&m_RTT1RTV_16));
		V_RETURN(pd3dDevice->CreateShaderResourceView(	m_RTT1Tex_16, NULL,		&m_RTT1SRV_16));
		
		texDesc.Width              = m_width/32;
		texDesc.Height             = m_height/32;
		V_RETURN(pd3dDevice->CreateTexture2D(			&texDesc, NULL,			&m_RTT0Tex_32));
		V_RETURN(pd3dDevice->CreateRenderTargetView(	m_RTT0Tex_32, NULL,		&m_RTT0RTV_32));
		V_RETURN(pd3dDevice->CreateShaderResourceView(	m_RTT0Tex_32, NULL,		&m_RTT0SRV_32));
																						  
		V_RETURN(pd3dDevice->CreateTexture2D(			&texDesc, NULL,			&m_RTT1Tex_32));
		V_RETURN(pd3dDevice->CreateRenderTargetView(	m_RTT1Tex_32, NULL,		&m_RTT1RTV_32));
		V_RETURN(pd3dDevice->CreateShaderResourceView(	m_RTT1Tex_32, NULL,		&m_RTT1SRV_32));

		/*
	SAFE_RELEASE(m_RTT0Tex_1);
	SAFE_RELEASE(m_RTT0RTV_1);
	SAFE_RELEASE(m_RTT0SRV_1);

	SAFE_RELEASE(m_RTT1Tex_1);
	SAFE_RELEASE(m_RTT1RTV_1);
	SAFE_RELEASE(m_RTT1SRV_1);

	SAFE_RELEASE(m_RTT0Tex_4);
	SAFE_RELEASE(m_RTT0RTV_4);
	SAFE_RELEASE(m_RTT0SRV_4);

	SAFE_RELEASE(m_RTT1Tex_4);
	SAFE_RELEASE(m_RTT1RTV_4);
	SAFE_RELEASE(m_RTT1SRV_4);

	SAFE_RELEASE(m_RTT0Tex_16);
	SAFE_RELEASE(m_RTT0RTV_16);
	SAFE_RELEASE(m_RTT0SRV_16);

	SAFE_RELEASE(m_RTT1Tex_16);
	SAFE_RELEASE(m_RTT1RTV_16);
	SAFE_RELEASE(m_RTT1SRV_16);

	SAFE_RELEASE(m_RTT0Tex_32);
	SAFE_RELEASE(m_RTT0RTV_32);
	SAFE_RELEASE(m_RTT0SRV_32);

	SAFE_RELEASE(m_RTT1Tex_32);
	SAFE_RELEASE(m_RTT1RTV_32);
	SAFE_RELEASE(m_RTT1SRV_32);

*/

	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	///// DEPTH
	SAFE_RELEASE(m_DepthStencilTEX);
	SAFE_RELEASE(m_DepthStencilView);
	SAFE_RELEASE(m_DepthStencilViewSRV);

	//D3D11_TEXTURE2D_DESC texDesc;
	texDesc.ArraySize          = 1;
	texDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags     = NULL;
	texDesc.Width              = m_width;
	texDesc.Height             = m_height;
	texDesc.MipLevels          = 1;
	texDesc.MiscFlags          = NULL;
	texDesc.SampleDesc.Count   = DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count;//g_MSAADesc[g_MSAACurrentSettings].samples;
	texDesc.SampleDesc.Quality = DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Quality;//g_MSAADesc[g_MSAACurrentSettings].quality;
	texDesc.Usage              = D3D11_USAGE_DEFAULT;
	texDesc.Format             = DXGI_FORMAT_R24G8_TYPELESS;
	V_RETURN(DXUTGetD3D11Device()->CreateTexture2D(&texDesc, NULL, &m_DepthStencilTEX));

	// Create a depth-stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
	ZeroMemory(&descSRV, sizeof(descSRV));

	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descSRV.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

	//if (g_MSAADesc[g_MSAACurrentSettings].samples > 1)
	//{
	//	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
	//}
	//else
	//{
	dsvDesc.ViewDimension = (DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count <= 1) ? D3D11_DSV_DIMENSION_TEXTURE2D : D3D11_DSV_DIMENSION_TEXTURE2DMS;
	descSRV.ViewDimension = (DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count <= 1) ? D3D11_SRV_DIMENSION_TEXTURE2D : D3D11_SRV_DIMENSION_TEXTURE2DMS;//D3D11_SRV_DIMENSION_TEXTURE2D;
	//}
	dsvDesc.Texture2D.MipSlice = 0;
	dsvDesc.Flags = 0; 
	
	descSRV.Texture2D.MipLevels = 1;
	descSRV.Texture2D.MostDetailedMip = 0;

	V_RETURN(DXUTGetD3D11Device()->CreateDepthStencilView(m_DepthStencilTEX, &dsvDesc, &m_DepthStencilView));
	V_RETURN(DXUTGetD3D11Device()->CreateShaderResourceView(m_DepthStencilTEX, &descSRV, &m_DepthStencilViewSRV));

	return hr;
}

void PostPro::BeginOffscreenRendering(ID3D11DeviceContext1* pd3dImmediateContext)
{
	// CHECKME

	pd3dImmediateContext->ClearDepthStencilView(m_DepthStencilView, D3D11_CLEAR_DEPTH, 1.0, 0);
	
	//DXUTGetD3D11DeviceContext()->OMSetRenderTargets(1, &m_ColorRTV, m_DepthStencilView);

	ID3D11RenderTargetView* ppRTV[] = {m_ColorRTV, m_Color1RTV};

	if (DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count > 1) {
		ppRTV[0] = m_ColorMSAARTV;
		ppRTV[1] = m_Color1MSAARTV;
		pd3dImmediateContext->ClearRenderTargetView(m_ColorMSAARTV, &bgColor[0]);
		pd3dImmediateContext->ClearRenderTargetView(m_Color1MSAARTV, &bgColor[0]);
	} else {
		pd3dImmediateContext->ClearRenderTargetView(m_ColorRTV, &bgColor[0]);
		pd3dImmediateContext->ClearRenderTargetView(m_Color1RTV, &bgColor[0]);
	}

	pd3dImmediateContext->OMSetRenderTargets(2, ppRTV, m_DepthStencilView);	
}

void PostPro::EndOffscreenRendering(ID3D11DeviceContext1* pd3dImmediateContext)
{
	ID3D11RenderTargetView* stdRTV[] = {DXUTGetD3D11RenderTargetView()};
	pd3dImmediateContext->OMSetRenderTargets(1, stdRTV, DXUTGetD3D11DepthStencilView());
		if (DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count > 1) {
			pd3dImmediateContext->ResolveSubresource(m_ColorTEX, 0, m_ColorMSAATEX, 0, DXGI_FORMAT_R32G32B32A32_FLOAT);
			pd3dImmediateContext->ResolveSubresource(m_Color1TEX, 0, m_Color1MSAATEX, 0, DXGI_FORMAT_R32G32B32A32_FLOAT);
	}
}

void PostPro::ApplySSAO(ID3D11DeviceContext1* pd3dImmediateContext) const
{
	if (0) {
		ID3D11ShaderResourceView* ppSRVNULL[] = {NULL, NULL, NULL, NULL};	
		ID3D11SamplerState* ssNULL[] = {NULL, NULL,NULL};

		pd3dImmediateContext->OMSetDepthStencilState(m_depthStencilStateDisableDepth, 1);
		pd3dImmediateContext->ClearDepthStencilView(m_DepthStencilView, D3D11_CLEAR_DEPTH, 1.0, 0);

	
		ID3D11ShaderResourceView* texturesRT0[] = {m_FBO0SRV};
		ID3D11ShaderResourceView* texturesRT1[] = {m_FBO1SRV};
	
		// TODO update sampelers in ssao shader

		ID3D11SamplerState* ppSampler[] = {m_samplerStateWrap, m_samplerStateClamp};
		pd3dImmediateContext->PSSetSamplers(0, 2, ppSampler);
	

		ID3D11RenderTargetView* ppRTV0[] = {m_FBO0RTV};
		ID3D11RenderTargetView* ppRTV1[] = {m_FBO1RTV};

		//SSAO compute pass
		pd3dImmediateContext->OMSetRenderTargets(1, ppRTV0, m_DepthStencilView);		
	
		ID3D11ShaderResourceView* ppSRVSSAO[] = {m_ColorSRV, m_Color1SRV, m_noiseTextureSRV};
		pd3dImmediateContext->PSSetShaderResources(0, 3, ppSRVSSAO);		
		pd3dImmediateContext->PSSetShader(m_ssaoPS->Get(), NULL, 0);
		RenderFullScreenQuad(pd3dImmediateContext);
		pd3dImmediateContext->PSSetShaderResources(0, 3, ppSRVNULL);

		// BLUR horizontal pass
		pd3dImmediateContext->OMSetRenderTargets(1, ppRTV1, m_DepthStencilView);		
		pd3dImmediateContext->PSSetShaderResources(0, 1, texturesRT0);
		pd3dImmediateContext->PSSetShader(m_ssaoBlurHorizontalPS->Get(), NULL, 0);
		RenderFullScreenQuad(pd3dImmediateContext);
		pd3dImmediateContext->PSSetShaderResources(0, 1, ppSRVNULL);
	
		// BLUR vertical pass
		pd3dImmediateContext->OMSetRenderTargets(1, ppRTV0, m_DepthStencilView);		
		pd3dImmediateContext->PSSetShaderResources(0, 1, texturesRT1);
		pd3dImmediateContext->PSSetShader(m_ssoaBlurVerticalPS->Get(), NULL, 0);
		RenderFullScreenQuad(pd3dImmediateContext);
		pd3dImmediateContext->PSSetShaderResources(0, 1, ppSRVNULL);


		// render result to back buffer	
		ID3D11RenderTargetView* stdRTV[] = {DXUTGetD3D11RenderTargetView(),NULL};
		pd3dImmediateContext->OMSetRenderTargets(1, stdRTV, DXUTGetD3D11DepthStencilView());

		ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
		pd3dImmediateContext->ClearRenderTargetView(pRTV, &bgColor[0]);

		pd3dImmediateContext->ClearDepthStencilView( DXUTGetD3D11DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0 );
		// render compose
		ID3D11ShaderResourceView* texturesCompose[2] = {m_ColorSRV, m_FBO0SRV };
		pd3dImmediateContext->PSSetShaderResources(0, 2, texturesCompose);
		pd3dImmediateContext->PSSetShader(m_ssaoComposePS->Get(), NULL, 0);	
		RenderFullScreenQuad(pd3dImmediateContext);	
		pd3dImmediateContext->PSSetShaderResources(0, 2, ppSRVNULL);
		// end compose

		pd3dImmediateContext->PSSetSamplers(0,3,ssNULL);
	} else {
		ID3D11ShaderResourceView* ppSRVNULL[] = {NULL, NULL, NULL, NULL, NULL};	
		ID3D11SamplerState* ssNULL[] = {NULL, NULL, NULL};
		ID3D11SamplerState* ppSampler[] = {m_samplerStateWrap, m_samplerStateClamp,  m_samplerStateClamp};
		pd3dImmediateContext->OMSetDepthStencilState(m_depthStencilStateDisableDepth, 1);
		pd3dImmediateContext->ClearDepthStencilView(m_DepthStencilView, D3D11_CLEAR_DEPTH, 1.0, 0);

		pd3dImmediateContext->PSSetSamplers(0, 3, ppSampler);
		
		D3D11_MAPPED_SUBRESOURCE mappedBuf;
		pd3dImmediateContext->Map(m_cbPostProcessing, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedBuf);
		CB_POSTPROCESSING* cbPostProcessing = reinterpret_cast<CB_POSTPROCESSING*>(mappedBuf.pData);
		cbPostProcessing->bokehFocus = m_bokehFocus;
		pd3dImmediateContext->Unmap(m_cbPostProcessing, 0);
		ID3D11Buffer* constantBuffers[] = { m_cbPostProcessing };
		pd3dImmediateContext->PSSetConstantBuffers(CB_LOC::MATERIAL, ARRAYSIZE(constantBuffers), constantBuffers);

		{// Bokeh pass 1
			pd3dImmediateContext->ClearRenderTargetView(m_RTT1RTV_1, &bgColor[0]);
			pd3dImmediateContext->ClearRenderTargetView(m_RTT1bRTV_1, &bgColor[0]);
			pd3dImmediateContext->ClearDepthStencilView(m_DepthStencilView, D3D11_CLEAR_DEPTH, 1.0, 0);

			ID3D11ShaderResourceView* ppInputTextures[] = {m_ColorSRV, m_Color1SRV};
			ID3D11RenderTargetView* ppBokeh1RTV[] = {m_RTT1RTV_1, m_RTT1bRTV_1};
			pd3dImmediateContext->PSSetShaderResources(0, 2, ppInputTextures);
			pd3dImmediateContext->OMSetRenderTargets(2, ppBokeh1RTV, NULL);
			pd3dImmediateContext->PSSetShader(m_bokeh1PS->Get(), NULL, 0);	

			RenderFullScreenQuad(pd3dImmediateContext);
			pd3dImmediateContext->PSSetShaderResources(0, 3, ppSRVNULL);

		}
		if (1) {// Bokeh pass 2
			ID3D11ShaderResourceView* ppInputTextures[] = {m_RTT1SRV_1, m_RTT1bSRV_1};
			ID3D11RenderTargetView* ppBokeh0RTV[] = {m_RTT0RTV_1};
			//{
			//	ID3D11RenderTargetView* stdRTV[] = {DXUTGetD3D11RenderTargetView(), NULL};
			//	pd3dImmediateContext->OMSetRenderTargets(2, stdRTV, DXUTGetD3D11DepthStencilView());
			//	ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
			//	pd3dImmediateContext->ClearRenderTargetView(pRTV, &bgColor[0]);
			//	pd3dImmediateContext->ClearDepthStencilView( DXUTGetD3D11DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0 );
			//}
			pd3dImmediateContext->OMSetRenderTargets(1, ppBokeh0RTV, NULL);
			
			if (1){
				ID3D11RenderTargetView* stdRTV[] = {DXUTGetD3D11RenderTargetView(), NULL};
				pd3dImmediateContext->OMSetRenderTargets(2, stdRTV, DXUTGetD3D11DepthStencilView());
				ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
				pd3dImmediateContext->ClearRenderTargetView(pRTV, &bgColor[0]);
				pd3dImmediateContext->ClearDepthStencilView( DXUTGetD3D11DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0 );
							D3D11_VIEWPORT vp;
				vp.Width = (FLOAT)DXUTGetDXGIBackBufferSurfaceDesc()->Width;
				vp.Height = (FLOAT)DXUTGetDXGIBackBufferSurfaceDesc()->Height;
				vp.MinDepth = 0;
				vp.MaxDepth = 1;
				vp.TopLeftX = 0;
				vp.TopLeftY = 0;
				pd3dImmediateContext->RSSetViewports( 1, &vp );
			}
			

			pd3dImmediateContext->PSSetShaderResources(0, 2, ppInputTextures);
			pd3dImmediateContext->PSSetShader(m_bokeh2PS->Get(), NULL, 0);	

			RenderFullScreenQuad(pd3dImmediateContext);
			pd3dImmediateContext->PSSetShaderResources(0, 3, ppSRVNULL);
		}
		//if (1) { // Blur Pass 1 & 2
		//	DXUTGetD3D11DeviceContext()->GenerateMips(m_RTT0SRV_1);
		//	ID3D11RenderTargetView* ppBlur1RTV[] = {m_RTT1RTV_4};
		//	ID3D11RenderTargetView* ppBlur2RTV[] = {m_RTT0RTV_4};
		//	ID3D11ShaderResourceView* ppInputTextures1[] = {m_RTT0SRV_1};
		//	ID3D11ShaderResourceView* ppInputTextures2[] = {m_RTT1SRV_4};
		//
		//
		//	pd3dImmediateContext->OMSetRenderTargets(1, ppBlur1RTV, NULL);
		//	
		//	D3D11_VIEWPORT viewport;
		//	viewport.TopLeftX = 0.0f;
		//	viewport.TopLeftY = 0.0f;
		//	viewport.Width = 1280/4;
		//	viewport.Height = 720/4;
		//	viewport.MinDepth = D3D11_MIN_DEPTH;
		//	viewport.MaxDepth = D3D11_MAX_DEPTH;
		//	pd3dImmediateContext->PSSetShader(m_blur4HPS->Get(), NULL, 0);
		//	pd3dImmediateContext->RSSetViewports(1, &viewport);
		//
		//	pd3dImmediateContext->PSSetShaderResources(0, 1, ppInputTextures1);
		//	RenderFullScreenQuad(pd3dImmediateContext);
		//	pd3dImmediateContext->PSSetShaderResources(0, 3, ppSRVNULL);
		//
		//	pd3dImmediateContext->OMSetRenderTargets(1, ppBlur2RTV, NULL);
		//	pd3dImmediateContext->PSSetShader(m_blur4VPS->Get(), NULL, 0);
		//
		//	pd3dImmediateContext->PSSetShaderResources(0, 1, ppInputTextures2);
		//	RenderFullScreenQuad(pd3dImmediateContext);
		//	pd3dImmediateContext->PSSetShaderResources(0, 3, ppSRVNULL);
		//}
		//
		//if (1) { // Blur Pass 3 & 4
		//	DXUTGetD3D11DeviceContext()->GenerateMips(m_RTT0SRV_4);
		//	ID3D11RenderTargetView* ppBlur1RTV[] = {m_RTT1RTV_16};
		//	ID3D11RenderTargetView* ppBlur2RTV[] = {m_RTT0RTV_16};
		//	ID3D11ShaderResourceView* ppInputTextures1[] = {m_RTT0SRV_4};
		//	ID3D11ShaderResourceView* ppInputTextures2[] = {m_RTT1SRV_16};
		//
		//
		//	pd3dImmediateContext->OMSetRenderTargets(1, ppBlur1RTV, NULL);
		//	
		//	D3D11_VIEWPORT viewport;
		//	viewport.TopLeftX = 0.0f;
		//	viewport.TopLeftY = 0.0f;
		//	viewport.Width = 1280/16;
		//	viewport.Height = 720/16;
		//	viewport.MinDepth = D3D11_MIN_DEPTH;
		//	viewport.MaxDepth = D3D11_MAX_DEPTH;
		//	pd3dImmediateContext->PSSetShader(m_blur16HPS->Get(), NULL, 0);
		//	pd3dImmediateContext->RSSetViewports(1, &viewport);
		//
		//	pd3dImmediateContext->PSSetShaderResources(0, 1, ppInputTextures1);
		//	RenderFullScreenQuad(pd3dImmediateContext);
		//	pd3dImmediateContext->PSSetShaderResources(0, 3, ppSRVNULL);
		//
		//	pd3dImmediateContext->OMSetRenderTargets(1, ppBlur2RTV, NULL);
		//	pd3dImmediateContext->PSSetShader(m_blur16VPS->Get(), NULL, 0);
		//
		//	pd3dImmediateContext->PSSetShaderResources(0, 1, ppInputTextures2);
		//	RenderFullScreenQuad(pd3dImmediateContext);
		//	pd3dImmediateContext->PSSetShaderResources(0, 3, ppSRVNULL);
		//}
		//
		//if (1) { // Blur Pass 5 & 6
		//	DXUTGetD3D11DeviceContext()->GenerateMips(m_RTT0SRV_16);
		//	ID3D11RenderTargetView* ppBlur1RTV[] = {m_RTT1RTV_32};
		//	ID3D11RenderTargetView* ppBlur2RTV[] = {m_RTT0RTV_32};
		//	ID3D11ShaderResourceView* ppInputTextures1[] = {m_RTT0SRV_16};
		//	ID3D11ShaderResourceView* ppInputTextures2[] = {m_RTT1SRV_32};
		//
		//
		//	pd3dImmediateContext->OMSetRenderTargets(1, ppBlur1RTV, NULL);
		//	
		//	D3D11_VIEWPORT viewport;
		//	viewport.TopLeftX = 0.0f;
		//	viewport.TopLeftY = 0.0f;
		//	viewport.Width = 1280/32;
		//	viewport.Height = 720/32;
		//	viewport.MinDepth = D3D11_MIN_DEPTH;
		//	viewport.MaxDepth = D3D11_MAX_DEPTH;
		//	pd3dImmediateContext->PSSetShader(m_blur32HPS->Get(), NULL, 0);
		//	pd3dImmediateContext->RSSetViewports(1, &viewport);
		//
		//	pd3dImmediateContext->PSSetShaderResources(0, 1, ppInputTextures1);
		//	RenderFullScreenQuad(pd3dImmediateContext);
		//	pd3dImmediateContext->PSSetShaderResources(0, 3, ppSRVNULL);
		//
		//
		//	pd3dImmediateContext->OMSetRenderTargets(1, ppBlur2RTV, NULL);
		//	pd3dImmediateContext->PSSetShader(m_blur32VPS->Get(), NULL, 0);
		//
		//	pd3dImmediateContext->PSSetShaderResources(0, 1, ppInputTextures2);
		//	RenderFullScreenQuad(pd3dImmediateContext);
		//	pd3dImmediateContext->PSSetShaderResources(0, 3, ppSRVNULL);
		//
		//	DXUTGetD3D11DeviceContext()->GenerateMips(m_RTT0SRV_32);
		//}
		//
		//if (1) { // Streaks
		//
		//	ID3D11RenderTargetView* ppStreakRTV[] = {m_RTT1RTV_1};
		//	ID3D11ShaderResourceView* ppInputTextures1[] = {m_RTT0SRV_4};
		//
		//	if (0){
		//		ID3D11RenderTargetView* stdRTV[] = {DXUTGetD3D11RenderTargetView(), NULL};
		//		pd3dImmediateContext->OMSetRenderTargets(2, stdRTV, DXUTGetD3D11DepthStencilView());
		//		ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
		//		pd3dImmediateContext->ClearRenderTargetView(pRTV, &bgColor[0]);
		//		pd3dImmediateContext->ClearDepthStencilView( DXUTGetD3D11DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0 );
		//					D3D11_VIEWPORT vp;
		//		vp.Width = (FLOAT)DXUTGetDXGIBackBufferSurfaceDesc()->Width;
		//		vp.Height = (FLOAT)DXUTGetDXGIBackBufferSurfaceDesc()->Height;
		//		vp.MinDepth = 0;
		//		vp.MaxDepth = 1;
		//		vp.TopLeftX = 0;
		//		vp.TopLeftY = 0;
		//		pd3dImmediateContext->RSSetViewports( 1, &vp );
		//	} else
		//	pd3dImmediateContext->OMSetRenderTargets(1, ppStreakRTV, NULL);
		//	
		//	D3D11_VIEWPORT viewport;
		//	viewport.TopLeftX = 0.0f;
		//	viewport.TopLeftY = 0.0f;
		//	viewport.Width = 1280;
		//	viewport.Height = 720;
		//	viewport.MinDepth = D3D11_MIN_DEPTH;
		//	viewport.MaxDepth = D3D11_MAX_DEPTH;
		//
		//	pd3dImmediateContext->PSSetShader(m_streaksPS->Get(), NULL, 0);
		//	pd3dImmediateContext->RSSetViewports(1, &viewport);
		//
		//	pd3dImmediateContext->PSSetShaderResources(0, 1, ppInputTextures1);
		//	RenderFullScreenQuad(pd3dImmediateContext);
		//	pd3dImmediateContext->PSSetShaderResources(0, 3, ppSRVNULL);
		//}
		//
		//if (1) { // Merge
		//
		//	ID3D11ShaderResourceView* ppInputTextures1[] = {m_RTT1SRV_1, m_RTT0SRV_1,  m_RTT0SRV_4,  m_RTT0SRV_16,  m_RTT0SRV_32};
		//
		//	if (1){
		//		ID3D11RenderTargetView* stdRTV[] = {DXUTGetD3D11RenderTargetView(), NULL};
		//		pd3dImmediateContext->OMSetRenderTargets(2, stdRTV, DXUTGetD3D11DepthStencilView());
		//		ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
		//		pd3dImmediateContext->ClearRenderTargetView(pRTV, &bgColor[0]);
		//		pd3dImmediateContext->ClearDepthStencilView( DXUTGetD3D11DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0 );
		//					D3D11_VIEWPORT vp;
		//		vp.Width = (FLOAT)DXUTGetDXGIBackBufferSurfaceDesc()->Width;
		//		vp.Height = (FLOAT)DXUTGetDXGIBackBufferSurfaceDesc()->Height;
		//		vp.MinDepth = 0;
		//		vp.MaxDepth = 1;
		//		vp.TopLeftX = 0;
		//		vp.TopLeftY = 0;
		//		pd3dImmediateContext->RSSetViewports( 1, &vp );
		//	}
		//	
		//
		//	pd3dImmediateContext->PSSetShader(m_mergePS->Get(), NULL, 0);
		//
		//	pd3dImmediateContext->PSSetShaderResources(0, 5, ppInputTextures1);
		//	RenderFullScreenQuad(pd3dImmediateContext);
		//	pd3dImmediateContext->PSSetShaderResources(0, 5, ppSRVNULL);
		//}

		//pd3dImmediateContext->OMSetDepthStencilState(m_depthStencilStateDisableDepth, 1);
		//pd3dImmediateContext->ClearDepthStencilView(m_DepthStencilView, D3D11_CLEAR_DEPTH, 1.0, 0);
		//pd3dImmediateContext->PSSetSamplers(0,3,ssNULL);
	}
}

void PostPro::RenderFullScreenQuad(ID3D11DeviceContext1* pd3dImmediateContext) const
{
	DXUTGetD3D11DeviceContext()->VSSetShader(m_fsQuadVS->Get(), NULL, 0);
	DXUTGetD3D11DeviceContext()->HSSetShader(NULL, NULL, 0);
	DXUTGetD3D11DeviceContext()->DSSetShader(NULL, NULL, 0);
	
	const UINT strides[1] = { UINT(m_uBytesPerQuadVertex) };
	const UINT offsets[1] = { 0 };
	DXUTGetD3D11DeviceContext()->IASetVertexBuffers(0, 1, &m_fsQuadVertexBuffer, strides, offsets);

	DXUTGetD3D11DeviceContext()->IASetInputLayout(m_fsQuadInputLayout);
	DXUTGetD3D11DeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	DXUTGetD3D11DeviceContext()->Draw(4, 0);
}

HRESULT PostPro::CreateFullScreenQuad(ID3D11Device1* pd3dDevice)
{
	HRESULT hr = S_OK;

	struct VertexQuad {
		XMFLOAT4A position;
		XMFLOAT2A texcoord;
	};

	VertexQuad vertices[4] = {
		XMFLOAT4A(-1.0f, -1.0f, 0.5f, 1.f), XMFLOAT2A(0.0f, 1.0f),		
		XMFLOAT4A(-1.0f,  1.0f, 0.5f, 1.f), XMFLOAT2A(0.0f, 0.0f),
		XMFLOAT4A( 1.0f, -1.0f, 0.5f, 1.f), XMFLOAT2A(1.0f, 1.0f),
		XMFLOAT4A( 1.0f,  1.0f, 0.5f, 1.f), XMFLOAT2A(1.0f, 0.0f),
	};

	m_uBytesPerQuadVertex = sizeof(VertexQuad);
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_VERTEX_BUFFER,m_uBytesPerQuadVertex*4, 0, D3D11_USAGE_IMMUTABLE, m_fsQuadVertexBuffer, &vertices[0]));

	return hr;
}

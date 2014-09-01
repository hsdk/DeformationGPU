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

//! Screen Space Ambient Occlusion 
#pragma once

#include <DXUT.h>
#include <SDX/DXShaderManager.h>


class PostPro
{
public:
	PostPro();
	~PostPro();

	HRESULT Create(ID3D11Device1* pd3dDevice);
	void Destroy();

	HRESULT Resize(ID3D11Device1* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);

	void BeginOffscreenRendering(ID3D11DeviceContext1* pd3dImmediateContext);
	void EndOffscreenRendering(ID3D11DeviceContext1* pd3dImmediateContext);

	void ApplySSAO(ID3D11DeviceContext1* pd3dImmediateContext) const;


	void SetBokehFocus(float x) { m_bokehFocus = x; }

	ID3D11DepthStencilView* GetDepthStencilView() { return m_DepthStencilView;}
	ID3D11ShaderResourceView* GetDepthStencilViewSRV() { return m_DepthStencilViewSRV; }
	
	UINT GetWidth()		const {return m_width;}
	UINT GetHeight()	const {return m_height;}

private:
	void	RenderFullScreenQuad(ID3D11DeviceContext1* pd3dImmediateContext) const;
	HRESULT CreateFullScreenQuad(ID3D11Device1* pd3dDevice);
		
	ID3D11InputLayout*				m_fsQuadInputLayout;
	ID3D11Buffer*					m_fsQuadVertexBuffer;
	unsigned int					m_uBytesPerQuadVertex;		


	Shader<ID3D11VertexShader>*		m_fsQuadVS;
	Shader<ID3D11PixelShader>*		m_fsQuadPS;

	Shader<ID3D11PixelShader>*		m_ssaoPS;
	Shader<ID3D11PixelShader>*		m_ssoaBlurVerticalPS;
	Shader<ID3D11PixelShader>*		m_ssaoBlurHorizontalPS;
	Shader<ID3D11PixelShader>*		m_ssaoComposePS;
	
	Shader<ID3D11PixelShader>*		m_blur4HPS;
	Shader<ID3D11PixelShader>*		m_blur4VPS;

	Shader<ID3D11PixelShader>*		m_blur16HPS;
	Shader<ID3D11PixelShader>*		m_blur16VPS;

	Shader<ID3D11PixelShader>*		m_blur32HPS;
	Shader<ID3D11PixelShader>*		m_blur32VPS;

	Shader<ID3D11PixelShader>*		m_bokeh1PS;
	Shader<ID3D11PixelShader>*		m_bokeh1PSMSAA;
	Shader<ID3D11PixelShader>*		m_bokeh2PS;

	Shader<ID3D11PixelShader>*		m_streaksPS;
	Shader<ID3D11PixelShader>*		m_mergePS;

	ID3D11ShaderResourceView*		m_noiseTextureSRV;
		
	ID3D11SamplerState*				m_samplerStateWrap;
	ID3D11SamplerState*				m_samplerStateClamp;

	ID3D11DepthStencilState*		m_depthStencilStateDisableDepth;

	ID3D11Texture2D*				m_ColorTEX;
	ID3D11ShaderResourceView*		m_ColorSRV;
	ID3D11RenderTargetView*			m_ColorRTV;
	
	ID3D11Texture2D*				m_ColorMSAATEX;
	ID3D11RenderTargetView*			m_ColorMSAARTV;
	ID3D11ShaderResourceView*		m_ColorMSAASRV;

	ID3D11Texture2D*				m_Color1TEX;
	ID3D11ShaderResourceView*		m_Color1SRV;
	ID3D11RenderTargetView*			m_Color1RTV;
	
	ID3D11Texture2D*				m_Color1MSAATEX;
	ID3D11RenderTargetView*			m_Color1MSAARTV;
	ID3D11ShaderResourceView*		m_Color1MSAASRV;

	ID3D11Texture2D*				m_FBO0Tex;
	ID3D11RenderTargetView*			m_FBO0RTV;
	ID3D11ShaderResourceView*		m_FBO0SRV;

	ID3D11Texture2D*				m_FBO1Tex;
	ID3D11RenderTargetView*			m_FBO1RTV;
	ID3D11ShaderResourceView*		m_FBO1SRV;

	ID3D11Texture2D*				m_RTT0Tex_1;
	ID3D11RenderTargetView*			m_RTT0RTV_1;
	ID3D11ShaderResourceView*		m_RTT0SRV_1;
	
	ID3D11Texture2D*				m_RTT1Tex_1;
	ID3D11RenderTargetView*			m_RTT1RTV_1;
	ID3D11ShaderResourceView*		m_RTT1SRV_1;

	ID3D11Texture2D*				m_RTT1bTex_1;
	ID3D11RenderTargetView*			m_RTT1bRTV_1;
	ID3D11ShaderResourceView*		m_RTT1bSRV_1;

	ID3D11Texture2D*				m_RTT0Tex_4;
	ID3D11RenderTargetView*			m_RTT0RTV_4;
	ID3D11ShaderResourceView*		m_RTT0SRV_4;
	
	ID3D11Texture2D*				m_RTT1Tex_4;
	ID3D11RenderTargetView*			m_RTT1RTV_4;
	ID3D11ShaderResourceView*		m_RTT1SRV_4;

	ID3D11Texture2D*				m_RTT0Tex_16;
	ID3D11RenderTargetView*			m_RTT0RTV_16;
	ID3D11ShaderResourceView*		m_RTT0SRV_16;
											  
	ID3D11Texture2D*				m_RTT1Tex_16;
	ID3D11RenderTargetView*			m_RTT1RTV_16;
	ID3D11ShaderResourceView*		m_RTT1SRV_16;

	ID3D11Texture2D*				m_RTT0Tex_32;
	ID3D11RenderTargetView*			m_RTT0RTV_32;
	ID3D11ShaderResourceView*		m_RTT0SRV_32;
											  
	ID3D11Texture2D*				m_RTT1Tex_32;
	ID3D11RenderTargetView*			m_RTT1RTV_32;
	ID3D11ShaderResourceView*		m_RTT1SRV_32;

	ID3D11Texture2D*				m_DepthStencilTEX;
	ID3D11DepthStencilView*			m_DepthStencilView;
	ID3D11ShaderResourceView*		m_DepthStencilViewSRV;

	//ID3D11Texture2D*				m_DepthStencilTEXMSAA;
	//ID3D11DepthStencilView*			m_DepthStencilViewMSAA;
	//ID3D11ShaderResourceView*		m_DepthStencilViewSRVMSAA;

	UINT m_width;
	UINT m_height;

	// Constant buffers
	ID3D11Buffer*					m_cbPostProcessing;

	float m_bokehFocus;

};
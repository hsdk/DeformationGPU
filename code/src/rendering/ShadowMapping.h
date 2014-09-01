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
#include "App.h"

//#define SHADOW_DEBUG

class CModelViewerCamera;
class CBaseCamera;
class Scene;
class AdaptiveRenderer;
class RenderTri;
class RendererSubD;

class ShadowMapping
{
public:
	ShadowMapping();
	~ShadowMapping();
	HRESULT Create(ID3D11Device1* pd3dDevice);
	void Destroy();

	HRESULT RenderShadowMap(ID3D11DeviceContext1* pd3dImmediateContext, 
							CBaseCamera* renderCam,
							CModelViewerCamera* lightCam,
							const DirectX::XMFLOAT3& lightDir,
							Scene* scene,
							RenderTri* triRenderer,
							RendererSubD* osdRenderer);
	void DebugRenderCascade(ID3D11DeviceContext1* pd3dImmediateContext, ID3D11RenderTargetView* outputRTV) const;

	// compute cascade settings, run once per frame before render shadows to offscreen buffers
	void ComputeCascades(CBaseCamera* renderCam, CModelViewerCamera* lightCam, Scene* scene);

	HRESULT UpdateShadowCB(ID3D11DeviceContext1* pd3dImmediateContext, DirectX::XMMATRIX matCameraView, DirectX::XMMATRIX matCameraProj);
	HRESULT UpdateShadowCascadesCB(ID3D11DeviceContext1* pd3dImmediateContext);
	HRESULT RenderShadowsForAllCascades(ID3D11DeviceContext1* pd3dDeviceContext, DirectX::XMMATRIX lightViewMatrix, Scene* scene,
										RenderTri* triRenderer, RendererSubD* osdRenderer );


	ID3D11ShaderResourceView*	GetShadowSRV()		const	{ return m_pVarianceShadowArraySRV;	}
	ID3D11SamplerState*			GetShadowSampler()	const   { return m_pSamShadowAnisotropic;					}

	const DirectX::XMFLOAT3*	GetShadowCamFrustum() const { return m_shadowCamFrustumPoints;}
	DirectX::XMMATRIX GetCascadeProjMatrix(int cascadeIndex) const;


	void SetShowCascades(bool b) { m_showCascades = b;}
	
	bool& GetUseSinglePassShadowRef()	{ return m_useSinglePassShadow;}
	bool& GetUseBlurCSRef()				{ return m_useBlurCS;}	
	bool& GetUseBlurSharedMemCSRef()	{ return m_useBlurSharedMemCS;}
	bool& GetUseAutomaticCascadesRef()  { return m_useDepthReduction; }
	DirectX::XMFLOAT2& GetDepthMinMaxRef() {return m_depthMinMax; }
	
	
	
	HRESULT RunDepthPrepass(ID3D11DeviceContext1* pd3dImmediateContext, CModelViewerCamera* camera, Scene* scene, RenderTri* triRenderer, RendererSubD* osdRenderer);
	HRESULT RunDepthReduction( ID3D11DeviceContext1* pd3dImmediateContext, CModelViewerCamera* camera, ID3D11DepthStencilView* dsv, ID3D11ShaderResourceView* depthSRV, UINT depthTexWidth, UINT depthTexHeight );

	DirectX::XMFLOAT3					m_sceneAABBCorners[8];
	DirectX::XMFLOAT3					m_viewFrustumCorners[NUM_CASCADES][8];
	DirectX::XMFLOAT3					m_cascadeFrustumCorners[NUM_CASCADES][8];
protected:
	
	
	

	Shader<ID3D11PixelShader>*		m_visShadowPS;


	// cascade shadow maps
	float                               m_fCascadePartitionsMax;
	float                               m_fCascadePartitionsMin;
	float                               m_fCascadePartitionsFrustum[NUM_CASCADES]; // Values are  between near and far
	float								m_fSplitDistance[NUM_CASCADES]; // Values are 0 to 1 and represent a percent of the frustum	
	float                               m_fBlurBetweenCascadesAmount;
	bool                                m_bMoveLightTexelSize;

	ID3D11Buffer*						m_cbLight;


	enum class FIT_PROJECTION_TO 
	{
		CASCADES,
		SCENE
	};

	enum class FIT_NEAR_FAR 
	{
		ZERO_ONE,
		AABB,
		SCENE_AABB
	};

	enum class CASCADE_SELECTION 
	{
		MAP,
		INTERVAL
	};

	enum class PartitionMode 
	{
		MANUAL  = 0,
		LOGARTIHMIC		= 1,
		PSSM	= 2
	};

	DirectX::XMFLOAT3			m_lightDir;
	bool						m_showCascades;

	FIT_PROJECTION_TO			m_eSelectedCascadesFit;
	FIT_NEAR_FAR				m_eSelectedNearFarFit;
	CASCADE_SELECTION			m_eSelectedCascadeSelection;
	PartitionMode				m_ePartitionMode;

	float						m_pssmLambda;
	
	DirectX::XMMATRIX           m_matShadowProj[NUM_CASCADES]; 
	DirectX::XMFLOAT3			m_cascadeScales[NUM_CASCADES];
	DirectX::XMFLOAT3			m_cascadeOffsets[NUM_CASCADES];
	DirectX::XMMATRIX			m_lightViewMatrix;
	DirectX::XMMATRIX           m_matShadowView;

	// D3D11 variables
	Shader<ID3D11VertexShader>* m_pvsQuadBlur;
	Shader<ID3D11PixelShader>*  m_ppsQuadBlurX;
	Shader<ID3D11PixelShader>*  m_ppsQuadBlurY;

	Shader<ID3D11ComputeShader>* m_blurXCS;
	Shader<ID3D11ComputeShader>* m_blurYCS;

	Shader<ID3D11ComputeShader>* m_blurXSharedMemCS;
	Shader<ID3D11ComputeShader>* m_blurYSharedMemCS;
	
	ID3D11InputLayout*          m_pVertexLayoutMesh;
	ID3D11VertexShader*         m_pvsRenderScene[NUM_CASCADES];
	ID3D11PixelShader*          m_ppsRenderSceneAllShaders[NUM_CASCADES];

	ID3D11Texture2D*            m_pVarianceShadowArrayTEX;
	ID3D11RenderTargetView*		m_pVarianceShadowArrayRTV;
	ID3D11RenderTargetView*     m_pVarianceShadowArraySliceRTV[NUM_CASCADES];
	ID3D11ShaderResourceView*   m_pVarianceShadowArraySliceSRV[NUM_CASCADES];
	ID3D11ShaderResourceView*   m_pVarianceShadowArraySRV;
	ID3D11UnorderedAccessView*  m_pVarianceShadowArraySliceUAV[NUM_CASCADES];

	ID3D11Texture2D*            m_pDepthBufferTextureArray;
	ID3D11DepthStencilView*     m_pDepthBufferDSV;
	ID3D11DepthStencilView*		m_pDepthBufferArrayDSV;
	ID3D11ShaderResourceView*   m_pDepthBufferSRV;
	
	ID3D11Texture2D*            m_pShadowTempTEX;
	ID3D11RenderTargetView*     m_pShadowTempRTV;
	ID3D11ShaderResourceView*   m_pShadowTempSRV;
	ID3D11UnorderedAccessView*  m_pShadowTempUAV;

	//ID3D11Buffer*               m_pcbGlobalConstantBuffer; // All VS and PS constants are in the same buffer.  
	// An actual title would break this up into multiple 
	// buffers updated based on frequency of variable changes

	ID3D11RasterizerState		*m_prsShadow;
	D3D11_VIEWPORT				 m_RenderOneTileVP;
	D3D11_VIEWPORT				 m_RenderPrepassVP;

	ID3D11SamplerState			*m_pSamLinear;
	ID3D11SamplerState			*m_pSamShadowPoint;
	ID3D11SamplerState			*m_pSamShadowLinear;
	ID3D11SamplerState			*m_pSamShadowLinearClamp;
	ID3D11SamplerState			*m_pSamShadowAnisotropic;
	
	///////////////////////////////////////////////////
	DirectX::XMFLOAT3			 m_shadowCamFrustumPoints[8];

	ID3D11Buffer				*m_shadowCascadesProjCB;


	// depth reduction scan buffers
	bool m_useDepthReduction;	
	UINT m_depthReductionFrameCounter;
	DirectX::XMFLOAT2			m_depthMinMax;
	ID3D11Buffer				*m_depthReductionResultStagingBUF[3];		
	ID3D11Buffer				*m_depthReductionResultBUF[3];				
	ID3D11UnorderedAccessView	*m_depthReductionResultUAV[3];				

	//ID3D11Buffer				*m_depthReductionResultReductionAtomicBUF[3];				
	ID3D11UnorderedAccessView	*m_depthReductionResultReductionAtomicUAV[3];		

	ID3D11Buffer				*m_depthScanBucketsBUF;					// pass 1 and 5
	ID3D11Buffer				*m_depthScanBucketsResultsBUF;			// pass 2,3,4,5
	ID3D11Buffer				*m_depthScanBucketsBlockResultsBUF;		// pass 3 and 4

	ID3D11ShaderResourceView	*m_depthScanBucketsSRV;					// pass 1 and 5
	ID3D11ShaderResourceView	*m_depthScanBucketsResultsSRV;			// pass 2,3,4,5
	ID3D11ShaderResourceView	*m_depthScanBucketsBlockResultsSRV;		// pass 3 and 4

	ID3D11UnorderedAccessView	*m_depthScanBucketsUAV;					// pass 1 and 5
	ID3D11UnorderedAccessView	*m_depthScanBucketsResultsUAV;			// pass 2,3,4,5
	ID3D11UnorderedAccessView	*m_depthScanBucketsBlockResultsUAV;		// pass 3 and 4

	ID3D11Buffer				*m_depthReductionCB;

	Shader<ID3D11ComputeShader> *m_depthScanBucketCS				;		// depth min/max pass 1 up sweep
	Shader<ID3D11ComputeShader> *m_depthScanBucketResultsCS			;		// depth min/max pass 2 up sweep
	Shader<ID3D11ComputeShader> *m_depthScanBucketBlockResultsCS	;		// depth min/max pass 3 
	Shader<ID3D11ComputeShader> *m_depthScanWriteResultCS;

	Shader<ID3D11ComputeShader> *m_depthReductionAtomicCS;
	Shader<ID3D11ComputeShader> *m_depthReductionAtomicMSAACS;
	
	UINT						 m_scanBucketSize;
	UINT						 m_scanBucketBlockSize;
	UINT						 m_numScanBuckets;	
	UINT						 m_numScanBucketBlocks;


	// 
	bool m_useSinglePassShadow;
	bool m_useBlurCS;
	bool m_useBlurSharedMemCS;
	bool m_stabilizeCascades;

};
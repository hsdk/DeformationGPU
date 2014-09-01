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

#pragma warning(disable:4324)

#include <DXUT.h>
#include <SDX/DXShaderManager.h>
#include <SDX/DXObjectOrientedBoundingBox.h>


// fwd decls
class ModelInstance;


const UINT g_StaticVoxelGridSize = 256;

const UINT g_maxVoxelGridSize =  256;
const UINT g_maxVoxelGridSizeX = g_maxVoxelGridSize;
const UINT g_maxVoxelGridSizeY = g_maxVoxelGridSize;
const UINT g_maxVoxelGridSizeZ = g_maxVoxelGridSize;

#define MAX_NUM_COLLISIONS 100

struct CollisionHit 
{
	DirectX::XMFLOAT4 vPosition;
	DirectX::XMFLOAT3 vNormal;
	DirectX::XMFLOAT2 vUV;
	UINT uPatchID;
};

struct VoxelGridDefinition {
	DirectX::XMUINT3 m_VoxelGridSize;
	UINT m_StrideX;
	UINT m_StrideY;
	UINT m_DataSize;
	ID3D11Buffer*					m_bufVoxelization;
	ID3D11UnorderedAccessView*		m_uavVoxelization;
	ID3D11ShaderResourceView*		m_srvVoxelization;
	DXObjectOrientedBoundingBox	m_OOBB;
	DirectX::XMMATRIX				m_MatrixWorldToVoxel;
	DirectX::XMMATRIX				m_MatrixVoxelToWorld;
	DirectX::XMMATRIX				m_MatrixWorldToVoxelProj;
	DirectX::XMMATRIX				m_VoxelView;
	DirectX::XMMATRIX				m_VoxelProj;
	DirectX::XMMATRIX				m_WorldMatrix;
	bool							m_bFillSolidBackward;

	VoxelGridDefinition() {
		m_VoxelGridSize = DirectX::XMUINT3(g_StaticVoxelGridSize, g_StaticVoxelGridSize, g_StaticVoxelGridSize);
		m_StrideX = m_StrideY = m_DataSize = 0;
		m_bufVoxelization = NULL;
		m_uavVoxelization = NULL;
		m_srvVoxelization = NULL;
		m_MatrixWorldToVoxel = DirectX::XMMatrixIdentity();
		m_MatrixVoxelToWorld = DirectX::XMMatrixIdentity();		
		m_MatrixWorldToVoxelProj = DirectX::XMMatrixIdentity();
		m_VoxelProj = DirectX::XMMatrixIdentity();
		m_WorldMatrix = DirectX::XMMatrixIdentity();
		m_VoxelView = DirectX::XMMatrixIdentity();
	}

	HRESULT Create(ID3D11Device1* pd3dDevice);
	void Destroy();

	void ComputeAdaptiveVoxelSize(const DXObjectOrientedBoundingBox& voxelOOBB, float voxelResolutionScale = 1.0f ) 
	{
		DirectX::XMFLOAT3 extent = voxelOOBB.GetExtent();

		float lcX = extent.x; 
		float lcY = extent.y;
		float lcZ = extent.z;

		// TODO make voxel scaler global
		float scale = 5.0f;

		// TODO check that x and y multiple is 2 does not break code
		UINT gsX = UINT((lcX * scale) /  2.f + 0.5f) *  2;
		UINT gsY = UINT((lcY * scale) /  2.f + 0.5f) *  2;
		UINT gsZ = UINT((lcZ * scale) / 32.f + 0.5f) * 32;

		// checkme size constraints violated if voxelResScale is float?
		gsX = (UINT)((float)gsX * voxelResolutionScale);
		gsY = (UINT)((float)gsY * voxelResolutionScale);
		gsZ = (UINT)((float)gsZ * voxelResolutionScale);

		// clamping and set		
		m_VoxelGridSize.x = DirectX::XMMax( 2u,		DirectX::XMMin( g_maxVoxelGridSizeX, gsX));
		m_VoxelGridSize.y = DirectX::XMMax( 2u,		DirectX::XMMin( g_maxVoxelGridSizeY, gsY));
		m_VoxelGridSize.z = DirectX::XMMax( 32u,	DirectX::XMMin( g_maxVoxelGridSizeZ, gsZ));
	} 



};

// todo move to separate file, or define binding location app.h

class Voxelizable
{
public:
	Voxelizable();
	virtual ~Voxelizable();

	HRESULT Create(ID3D11Device1* pd3dDevice);
	void Destroy();

	VoxelGridDefinition&		GetVoxelGridDefinition()			{ return m_VoxelGridDefinition;	}
	const VoxelGridDefinition&	GetVoxelGridDefinition()	const	{ return m_VoxelGridDefinition;	}
	ID3D11ShaderResourceView*	GetVoxelizationSRV()		const	{ return m_VoxelGridDefinition.m_srvVoxelization; } 
	ID3D11UnorderedAccessView*	GetVoxelizationUAV()		const	{ return m_VoxelGridDefinition.m_uavVoxelization; } 


protected:	
	VoxelGridDefinition				m_VoxelGridDefinition;
};



class VoxelizationRenderer 
{
public:

	VoxelizationRenderer();
	~VoxelizationRenderer();

	//! Performs a solid voxelization of the object
	void StartVoxelizeSolid(ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* voxelizable, const DXObjectOrientedBoundingBox& bb, bool useCullInfo = true) const;

	// reset viewport and rtv
	void EndVoxelizeSolid(ID3D11DeviceContext1* pd3dImmediateContext) const;

	//! Visualizes the created voxelization (that must be valid)
	void RenderVoxelizationViaRaycasting(ID3D11DeviceContext1* pd3dImmediateContext, Voxelizable* voxelData, const DirectX::XMFLOAT3* cameraEye) const;
	
	
	HRESULT Create(ID3D11Device1* pd3dDevice);
	void Destroy();
	

	void SetUseAdaptiveVoxelization(bool b)					  { s_bAdaptiveVoxelization = b; }
	bool GetUseAdaptiveVoxelization()						const { return s_bAdaptiveVoxelization; }

private:
	void    SetAndMapVoxelGridDefinitionAndViewportAndRS( ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* model, const DXObjectOrientedBoundingBox &bboxVoxelGrid, DirectX::XMMATRIX *customWorldToVoxel = NULL ) const;
	void    RestoreOldRTAndDSVAndViewportAndRS( ID3D11DeviceContext1* pd3dImmediateContext ) const;
	//HRESULT CreateFullscreenQuad(ID3D11Device1* pd3dDevice);

	//ID3D11InputLayout*	s_InputLayout;
	//ID3D11SamplerState*	s_LinearSampler;

	//ID3D11Buffer*					s_ResultBuffer;
	//ID3D11UnorderedAccessView*		s_ResultBufferUAV;
	//ID3D11Buffer*					s_ResultBufferStaging;

	ID3D11Texture2D*				s_texVoxelizationDummy;
	ID3D11RenderTargetView*			s_voxelizationDummyRTV;

	ID3D11Buffer*					s_cbVoxelGrid;
	ID3D11RasterizerState*			s_rastNoCull;
	ID3D11RasterizerState*			s_rastBFCull;

	ID3D11Buffer*					m_emptyVoxelGridBUF;

	bool							s_bShowVoxelBorderLines;
	ID3D11Buffer*					s_cbRaycasting;
	Shader<ID3D11VertexShader>*		s_VertexShaderRenderVoxelizationRaycasting;
	Shader<ID3D11PixelShader>*		s_PixelShaderRenderVoxelizationRaycasting;
	bool							s_bAdaptiveVoxelization;
	
};

extern VoxelizationRenderer g_voxelization;


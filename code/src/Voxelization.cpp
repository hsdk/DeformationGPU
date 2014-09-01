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

#include <DXUT.h>
#include <DXUTcamera.h>
#include <iostream>

#include "App.h"

#include "scene/ModelInstance.h"
#include "scene/DXModel.h"
#include "Voxelization.h"
#include <SDX/StringConversion.h>

#include "utils/DbgNew.h" // has to be last include

using namespace DirectX;


// global voxelization renderer
VoxelizationRenderer g_voxelization;


VoxelizationRenderer::VoxelizationRenderer()
{
	s_voxelizationDummyRTV	= NULL;

	s_cbVoxelGrid			= NULL;
	s_rastNoCull			= NULL;
	s_rastBFCull			= NULL;

	s_bShowVoxelBorderLines = true;
	s_cbRaycasting			= NULL;
	s_VertexShaderRenderVoxelizationRaycasting = NULL;
	s_PixelShaderRenderVoxelizationRaycasting = NULL;
	s_bAdaptiveVoxelization = false;
}


VoxelizationRenderer::~VoxelizationRenderer(void)
{
}

HRESULT VoxelizationRenderer::Create(ID3D11Device1* pd3dDevice) 
{
	HRESULT hr = S_OK;

	// create texture to serve as render target for triggering fragment generation during voxelization
	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = g_maxVoxelGridSizeX;
	texDesc.Height = g_maxVoxelGridSizeY;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateTexture2D(&texDesc, NULL, &s_texVoxelizationDummy));
	DXUT_SetDebugName(s_texVoxelizationDummy,"s_texVoxelizationDummy");

	// create render target view
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = texDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	V_RETURN(pd3dDevice->CreateRenderTargetView(s_texVoxelizationDummy, &rtvDesc, &s_voxelizationDummyRTV));
	DXUT_SetDebugName(s_voxelizationDummyRTV,"s_voxelizationDummyRTV");
	


	// constant buffers
	D3D11_BUFFER_DESC bufDesc;
	bufDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufDesc.MiscFlags = 0;
	bufDesc.StructureByteStride = 0;

	bufDesc.ByteWidth = sizeof(CB_VoxelGrid);
	V_RETURN(pd3dDevice->CreateBuffer(&bufDesc, NULL, &s_cbVoxelGrid));
	DXUT_SetDebugName(s_cbVoxelGrid,"s_cbVoxelGrid");

	bufDesc.ByteWidth = sizeof(CB_Raycasting);
	V_RETURN(pd3dDevice->CreateBuffer(&bufDesc, NULL, &s_cbRaycasting));
	DXUT_SetDebugName(s_cbRaycasting,"s_cbRaycasting");


	CD3D11_RASTERIZER_DESC rastDesc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
	rastDesc.FrontCounterClockwise = true;
	rastDesc.CullMode = D3D11_CULL_NONE;
	V_RETURN(pd3dDevice->CreateRasterizerState(&rastDesc, &s_rastNoCull));
	DXUT_SetDebugName(s_rastNoCull,"s_rastNoCull");
	rastDesc.CullMode = D3D11_CULL_BACK;
	V_RETURN(pd3dDevice->CreateRasterizerState(&rastDesc, &s_rastBFCull));
	DXUT_SetDebugName(s_rastBFCull,"s_rastBFCull");


	const D3D11_INPUT_ELEMENT_DESC inputElementDescsQuad[] = {
		"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0,
		"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,     0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0,
	};

	ID3DBlob* pBlob = NULL;
	s_VertexShaderRenderVoxelizationRaycasting = g_shaderManager.AddVertexShader(L"shader/Raycasting.hlsl", "VS_RenderVoxelizationRaycasting", "vs_5_0", &pBlob);
		
	s_PixelShaderRenderVoxelizationRaycasting = g_shaderManager.AddPixelShader( L"shader/Raycasting.hlsl", "PS_RenderVoxelizationRaycasting", "ps_5_0", &pBlob);
	SAFE_RELEASE( pBlob );

	// create clear voxelization buffer

	UINT StrideX = (g_StaticVoxelGridSize + 31) / 32;
	UINT StrideY = StrideX * g_StaticVoxelGridSize;
	UINT DataSize = StrideY * g_StaticVoxelGridSize;

	UINT *myData = new UINT[DataSize];
	for (UINT i = 0; i < DataSize; i++)	
	{
		//myData[i] = UINT_MAX;
		myData[i] = 0;
	}

	DXUTCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE, DataSize*4, 0, D3D11_USAGE_IMMUTABLE,  m_emptyVoxelGridBUF, &myData[0]);
	DXUT_SetDebugName(m_emptyVoxelGridBUF,"m_emptyVoxelGridBUF");
	SAFE_DELETE_ARRAY(myData);

	return hr;
}

void VoxelizationRenderer::Destroy()
{
	SAFE_RELEASE(s_texVoxelizationDummy);
	SAFE_RELEASE(s_voxelizationDummyRTV);

	SAFE_RELEASE(s_cbVoxelGrid);
	SAFE_RELEASE(s_cbRaycasting);
	SAFE_RELEASE(s_rastNoCull);
	SAFE_RELEASE(s_rastBFCull);

	SAFE_RELEASE(m_emptyVoxelGridBUF);
}

void VoxelizationRenderer::StartVoxelizeSolid( ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* model, const DXObjectOrientedBoundingBox& voxelizationOBB, bool useCullInfo /*= true*/ ) const
{
	SetAndMapVoxelGridDefinitionAndViewportAndRS(pd3dImmediateContext, model, voxelizationOBB);

	const float colClear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	
	pd3dImmediateContext->ClearRenderTargetView(s_voxelizationDummyRTV, colClear);

	// CHECKME clear is slow, better use own clear compute shader
	// or create cleared buffer and copy to voxel data buffer
	if(1)
	{
		const UINT clearValue[4] = { 0u, 0u, 0u, 0u };
		//const UINT clearValue[4] = { -1u,-1u,-1u,-1u };
		pd3dImmediateContext->ClearUnorderedAccessViewUint(model->GetVoxelGridDefinition().m_uavVoxelization, clearValue);
	}
	else
	{
		pd3dImmediateContext->CopyResource(model->GetVoxelGridDefinition().m_bufVoxelization, m_emptyVoxelGridBUF);
	}
	

	// rasterize into dummy render target of size gridSizeX x gridSizeY but write into voxelization buffer
	pd3dImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &s_voxelizationDummyRTV, NULL, 1, 1, &model->GetVoxelGridDefinition().m_uavVoxelization, NULL);
	
	ID3D11Buffer* constantBuffers[1] = {		s_cbVoxelGrid,	};
	
	pd3dImmediateContext->DSSetConstantBuffers(CB_LOC::VOXELGRID, 1, constantBuffers);
	pd3dImmediateContext->PSSetConstantBuffers(CB_LOC::VOXELGRID, 1, constantBuffers);

	// the mesh renderer is responsible for defining and using voxelization shader
	// remember to set forward, backward fill shader correctly in renderer 	
}




void VoxelizationRenderer::EndVoxelizeSolid( ID3D11DeviceContext1* pd3dImmediateContext ) const
{
	RestoreOldRTAndDSVAndViewportAndRS(pd3dImmediateContext);	
}

// set render to voxel grid defined by bboxVoxelGrid, also sets camara to transform from models obb space to voxel space defined by bboxVoxelGrid
void VoxelizationRenderer::SetAndMapVoxelGridDefinitionAndViewportAndRS( ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* model, const DXObjectOrientedBoundingBox &voxelizationOBB, XMMATRIX* customWorldToVoxel /*= NULL */ ) const
{
	HRESULT hr = S_OK;

	const DXObjectOrientedBoundingBox* meshOBB = &model->GetOBBWorld();
	
	VoxelGridDefinition& gridDef = model->GetVoxelGridDefinition();
	if (!customWorldToVoxel) 
	{
		if (s_bAdaptiveVoxelization) 
			gridDef.ComputeAdaptiveVoxelSize(voxelizationOBB, g_app.g_adaptiveVoxelizationScale);
		else
		{
			gridDef.m_VoxelGridSize = XMUINT3(g_StaticVoxelGridSize, g_StaticVoxelGridSize, g_StaticVoxelGridSize);
		}
	}

	gridDef.m_StrideX  = (gridDef.m_VoxelGridSize.z + 31) / 32;
	gridDef.m_StrideY  = gridDef.m_StrideX * gridDef.m_VoxelGridSize.x;
	gridDef.m_DataSize = gridDef.m_StrideY * gridDef.m_VoxelGridSize.y;

	//check correct voxelization order
	XMFLOAT3 frontZ[4];
	voxelizationOBB.GetFaceZFront(frontZ);
	gridDef.m_bFillSolidBackward =  meshOBB->TestFace(frontZ);	//zPlane must lie outside the OOBB of the model

	XMFLOAT3 backZ[4];
	voxelizationOBB.GetFaceZBack(backZ);	
	assert(customWorldToVoxel || !meshOBB->TestFace(frontZ) || !meshOBB->TestFace(backZ));	//make sure at least one face is outside of the bounding box (required for a correct voxelization)

	gridDef.m_OOBB = voxelizationOBB;	


	XMMATRIX matScale, matTrafo;
	matScale = XMMatrixScaling(2.0f, 2.0f, 1.0f);
	matTrafo = XMMatrixTranslation(-1.0f, -1.0f, 0.0f);

	//transforms to [-1;1]x[-1;1]x[0;1];
	//matWorldToVoxelProj = bboxVoxelGrid.getWorldToOOBB() * matScale * matTrafo;	
	XMMATRIX voxelProj = matScale * matTrafo;
	XMMATRIX matWorldToVoxelProj = voxelizationOBB.getWorldToOOBB() * voxelProj;// matScale * matTrafo;	
	gridDef.m_VoxelProj = voxelProj;
	gridDef.m_WorldMatrix = model->GetModelMatrix();

	//tranform to [0;sizeX]x[0;sizeY]x[0;sizeZ];
	matScale = XMMatrixScaling((float)gridDef.m_VoxelGridSize.x, (float)gridDef.m_VoxelGridSize.y, (float)gridDef.m_VoxelGridSize.z);
	XMMATRIX matWorldToVoxel = voxelizationOBB.getWorldToOOBB() * matScale;	
	
	gridDef.m_VoxelView = voxelizationOBB.getWorldToOOBB();

	if (!customWorldToVoxel) {	//do not update the voxelgriddefinition matrices if we have a render and check pass
		gridDef.m_MatrixWorldToVoxel = matWorldToVoxel;// * matScale;
				
		gridDef.m_MatrixVoxelToWorld = XMMatrixInverse(NULL, gridDef.m_MatrixWorldToVoxel );		
	}

	// TODO , use bullet for dynamic objects to compute current modeltoworld matrix in model
	// or define matrix on load for static objects or objects without physics
	// for now use identity
	
	//XMMATRIX matModelToWorld = XMMatrixIdentity();//->GetObjectToWorldMatrix();
	XMMATRIX matModelToWorld = model->GetModelMatrix(); //->GetObjectToWorldMatrix();
	//matModelToWorld = XMMatrixTranspose(matModelToWorld);

	D3D11_MAPPED_SUBRESOURCE mappedBuf;
	V(pd3dImmediateContext->Map(s_cbVoxelGrid, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedBuf));
	CB_VoxelGrid* cbVoxelGrid = reinterpret_cast<CB_VoxelGrid*>(mappedBuf.pData);
	//if (customWorldToVoxel)	cbVoxelGrid->m_matModelToProj = *customWorldToVoxel * matModelToWorld;	//in that case use a different projection
	//else					cbVoxelGrid->m_matModelToProj = matWorldToVoxelProj * matModelToWorld;
	//cbVoxelGrid->m_matModelToVoxel = m_VoxelGridDefinition.m_MatrixWorldToVoxel * matModelToWorld;

	// checkme, 
	cbVoxelGrid->m_matModelToProj = matModelToWorld  * matWorldToVoxelProj;
	if (customWorldToVoxel)	
	{
		cbVoxelGrid->m_matModelToVoxel =  *customWorldToVoxel * matModelToWorld;		
		gridDef.m_MatrixWorldToVoxelProj = matModelToWorld * matWorldToVoxelProj;
	}
	else 
	{
		cbVoxelGrid->m_matModelToVoxel = matModelToWorld * gridDef.m_MatrixWorldToVoxel;
		gridDef.m_MatrixWorldToVoxelProj = matModelToWorld * matWorldToVoxelProj;
	}


	cbVoxelGrid->m_stride[0] = gridDef.m_StrideX * 4;
	cbVoxelGrid->m_stride[1] = gridDef.m_StrideY * 4;
	cbVoxelGrid->m_gridSize = gridDef.m_VoxelGridSize;
	pd3dImmediateContext->Unmap(s_cbVoxelGrid, 0);
	
	// set viewport to render into voxel grid (x,y dim)
	D3D11_VIEWPORT viewport;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = float(gridDef.m_VoxelGridSize.x);
	viewport.Height = float(gridDef.m_VoxelGridSize.y);
	viewport.MinDepth = D3D11_MIN_DEPTH;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	pd3dImmediateContext->RSSetViewports(1, &viewport);	
	pd3dImmediateContext->RSSetState(s_rastNoCull);
}

// reset render to voxel grid
void VoxelizationRenderer::RestoreOldRTAndDSVAndViewportAndRS( ID3D11DeviceContext1* pd3dImmediateContext ) const
{
	D3D11_VIEWPORT viewport;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = float(DXUTGetDXGIBackBufferSurfaceDesc()->Width);
	viewport.Height = float(DXUTGetDXGIBackBufferSurfaceDesc()->Height);
	viewport.MinDepth = D3D11_MIN_DEPTH;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	pd3dImmediateContext->RSSetViewports(1, &viewport);


	ID3D11RenderTargetView* oldRTV = DXUTGetD3D11RenderTargetView();
	ID3D11DepthStencilView* oldDSV = DXUTGetD3D11DepthStencilView();	
	//ID3D11RasterizerState* oldRS = s_rastBFCull;
	pd3dImmediateContext->OMSetRenderTargets(1, &oldRTV, oldDSV);
	//pd3dImmediateContext->RSSetState(oldRS);

	ID3D11ShaderResourceView* srvsNULL [] = {NULL, NULL, NULL, NULL, NULL, NULL};
	pd3dImmediateContext->PSSetShaderResources(0, 6, srvsNULL);
}


void VoxelizationRenderer::RenderVoxelizationViaRaycasting( ID3D11DeviceContext1* pd3dImmediateContext, Voxelizable* voxelData, const DirectX::XMFLOAT3* cameraEye ) const
{
	XMMATRIX matWorld = XMMatrixIdentity();
	
	XMMATRIX matWorldViewProj = g_app.GetViewProjectionMatrix();
	
	XMFLOAT4X4 mView;
	XMStoreFloat4x4(&mView, g_app.GetViewMatrix());

	XMVECTOR cameraPos = XMLoadFloat3(cameraEye);

	XMVECTOR vLightPos = XMVectorAdd(cameraPos, (XMVectorSet(mView._12, mView._22, mView._32, 1.0) * 0.2f));
	
	const VoxelGridDefinition& gridDef = voxelData->GetVoxelGridDefinition();

	XMMATRIX matQuadToWorld = XMMatrixInverse(NULL, matWorldViewProj);	
	XMMATRIX matQuadToVoxel =  matQuadToWorld * gridDef.m_MatrixWorldToVoxel; 			
	XMMATRIX matVoxelToWorld = XMMatrixInverse( NULL, gridDef.m_MatrixWorldToVoxel);

	// unit cube to voxel grid scale and trafo
	XMMATRIX mTrafo, mScale, mat12;
	mTrafo = XMMatrixTranslation(1.f, 1.f, 0.f);
	mScale = XMMatrixScaling(float(DXUTGetDXGIBackBufferSurfaceDesc()->Width) * 0.5f, float(DXUTGetDXGIBackBufferSurfaceDesc()->Height) * 0.5f, 1.0f);
	mat12 = mTrafo * mScale;

	XMMATRIX matVoxelToScreen = matVoxelToWorld * matWorldViewProj * mat12;

	XMMATRIX matWorldInv = XMMatrixInverse(NULL, matWorld);

	cameraPos	 = XMVector3TransformCoord(cameraPos, matWorldInv);	
	XMVECTOR rayOrigin	 = XMVector3TransformCoord(cameraPos, gridDef.m_MatrixWorldToVoxel);
	XMVECTOR voxLightPos = XMVector3TransformCoord(vLightPos, gridDef.m_MatrixWorldToVoxel);


	D3D11_MAPPED_SUBRESOURCE mappedBuf;
	pd3dImmediateContext->Map(s_cbRaycasting, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedBuf);
	CB_Raycasting* cbRaycasting = reinterpret_cast<CB_Raycasting*>(mappedBuf.pData);
	cbRaycasting->mQuadToVoxel		= matQuadToVoxel;
	cbRaycasting->mVoxelToScreen	= matVoxelToScreen;
	XMStoreFloat3(&cbRaycasting->rayOrigin,rayOrigin);
	XMStoreFloat3(&cbRaycasting->voxLightPos,voxLightPos);
	cbRaycasting->m_stride[0]		= gridDef.m_StrideX;
	cbRaycasting->m_stride[1]		= gridDef.m_StrideY;
	cbRaycasting->m_gridSize[0]		= gridDef.m_VoxelGridSize.x;
	cbRaycasting->m_gridSize[1]		= gridDef.m_VoxelGridSize.y;
	cbRaycasting->m_gridSize[2]		= gridDef.m_VoxelGridSize.z;
	cbRaycasting->m_showLines		= s_bShowVoxelBorderLines;

	pd3dImmediateContext->Unmap(s_cbRaycasting, 0);


	pd3dImmediateContext->IASetInputLayout(NULL);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	pd3dImmediateContext->VSSetConstantBuffers(CB_LOC::RAYCAST_VOXELIZATION, 1, &s_cbRaycasting);
	pd3dImmediateContext->PSSetConstantBuffers(CB_LOC::RAYCAST_VOXELIZATION, 1, &s_cbRaycasting);

	pd3dImmediateContext->PSSetShaderResources(0, 1, &gridDef.m_srvVoxelization);

	pd3dImmediateContext->VSSetShader(s_VertexShaderRenderVoxelizationRaycasting->Get(), NULL, 0);
	pd3dImmediateContext->HSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->DSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(s_PixelShaderRenderVoxelizationRaycasting->Get(), NULL, 0);
	pd3dImmediateContext->Draw(4, 0);

	ID3D11ShaderResourceView* srvNULL[] = {NULL};
	pd3dImmediateContext->PSSetShaderResources(0, 1, srvNULL);
}


//VOXEGRID DEFINITION BELOW
HRESULT VoxelGridDefinition::Create( ID3D11Device1* pd3dDevice )
{
	HRESULT hr = S_OK;

	std::cout << "Create and init voxel grid def" << std::endl;
 	//TODO make this dynamic
	m_StrideX = (m_VoxelGridSize.z + 31) / 32;
	m_StrideY = m_StrideX * m_VoxelGridSize.x;
	m_DataSize = m_StrideY * m_VoxelGridSize.y;

	std::cout << "data size voxelization:  " << m_DataSize << std::endl;
	// create voxelization buffer (one bit per voxel)
	D3D11_BUFFER_DESC bufDesc;
	bufDesc.ByteWidth = m_DataSize * 4;
	bufDesc.Usage = D3D11_USAGE_DEFAULT;
	bufDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	bufDesc.CPUAccessFlags = 0;
	bufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	bufDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA initData;
	UINT *myData = new UINT[m_DataSize];
	for (UINT i = 0; i < m_DataSize; i++)	
	{
		//myData[i] = UINT_MAX;
		myData[i] = 0;
	}
	initData.pSysMem = myData;

	V_RETURN(pd3dDevice->CreateBuffer(&bufDesc, &initData, &m_bufVoxelization));
	DXUT_SetDebugName(m_bufVoxelization,"m_bufVoxelization");
	SAFE_DELETE_ARRAY(myData);

	// create unordered access view for voxelization buffer
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = bufDesc.ByteWidth / 4;
	uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_bufVoxelization, &uavDesc, &m_uavVoxelization));
	DXUT_SetDebugName(m_uavVoxelization,"m_uavVoxelization");

	// create shader resource view for voxelization buffer
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_R32_UINT;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.ElementOffset = 0;
	srvDesc.Buffer.ElementWidth = bufDesc.ByteWidth / 4;
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_bufVoxelization, &srvDesc, &m_srvVoxelization));
	DXUT_SetDebugName(m_srvVoxelization,"m_srvVoxelization");

	return hr;
}

void VoxelGridDefinition::Destroy()
{
	SAFE_RELEASE(m_bufVoxelization);
	SAFE_RELEASE(m_uavVoxelization);
	SAFE_RELEASE(m_srvVoxelization);
}


Voxelizable::Voxelizable()
{
}

Voxelizable::~Voxelizable()
{		
	Destroy();
}

HRESULT Voxelizable::Create(ID3D11Device1* pd3dDevice)
{
	HRESULT hr =S_OK;
	m_VoxelGridDefinition.Create(pd3dDevice);
	return hr;
}

void Voxelizable::Destroy()
{
	m_VoxelGridDefinition.Destroy();
}


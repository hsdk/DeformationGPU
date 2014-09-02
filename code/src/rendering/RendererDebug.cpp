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
#include "RendererDebug.h"

#include "App.h"

#include "scene/Scene.h"
#include "scene/DXModel.h"
#include "scene/ModelInstance.h"
#include "scene/DXSubDModel.h"
#include <SDX/DXObjectOrientedBoundingBox.h>

#include <vector>
#include <btBulletCollisionCommon.h>
#include <btBulletDynamicsCommon.h>
#include <btBulletCollisionCommon.h>

#include "utils/DbgNew.h" // has to be last include

using namespace DirectX;



RendererBoundingGeometry::RendererBoundingGeometry()
{

}

RendererBoundingGeometry::~RendererBoundingGeometry()
{
	Destroy();
}

HRESULT RendererBoundingGeometry::Create(ID3D11Device1* pd3dDevice)
{
	HRESULT hr = S_OK;

	// init unit cube vertex and index data

	// init shader
	ID3DBlob* pBlob = NULL;
	_frustumVS = g_shaderManager.AddVertexShader(L"shader/RenderDebug.hlsl", "VS_FRUSTUM", "vs_5_0", &pBlob); 
	_controlCageVS= g_shaderManager.AddVertexShader(L"shader/RenderDebug.hlsl", "VS_CAGE", "vs_5_0", &pBlob); 
	_aabbVS = g_shaderManager.AddVertexShader(L"shader/RenderDebug.hlsl", "VS_AABB", "vs_5_0", &pBlob); 	

	const D3D11_INPUT_ELEMENT_DESC attributeLayout[] = 
	{
		{"POSITION" , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	V_RETURN(pd3dDevice->CreateInputLayout(attributeLayout, ARRAYSIZE(attributeLayout), pBlob->GetBufferPointer(), pBlob->GetBufferSize(), &_inputLayout));

	_aabbPS = g_shaderManager.AddPixelShader(L"shader/RenderDebug.hlsl", "PS_AABB", "ps_5_0", &pBlob);
	_frustumPS = g_shaderManager.AddPixelShader(L"shader/RenderDebug.hlsl", "PS_FRUSTUM", "ps_5_0", &pBlob);

	SAFE_RELEASE(pBlob);

	V_RETURN(initCube());
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_MODEL_MATRIX), D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, _cbModelMatrix));


	// create dynamic buffers
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_VERTEX_BUFFER, 8 * sizeof(XMFLOAT3), D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, _vertexBufferDynamic));
	V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_INDEX_BUFFER, 24 * sizeof(UINT), D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, _indexBufferDynamic));
	return hr;
}

HRESULT RendererBoundingGeometry::Destroy()
{
	HRESULT hr = S_OK;

	SAFE_RELEASE(_vertexBufferStatic);
	SAFE_RELEASE(_solidIndicesBuffer);	
	SAFE_RELEASE(_linesIndicesBuffer);

	SAFE_RELEASE(_vertexBufferDynamic);
	SAFE_RELEASE(_indexBufferDynamic);

	SAFE_RELEASE(_inputLayout);
	SAFE_RELEASE(_cbModelMatrix);

	return hr;
}

HRESULT RendererBoundingGeometry::RenderPhysicsAABB( ModelGroup* group, bool lines /*= true*/)
{
	HRESULT hr = S_OK;

	UINT Offset = 0;
	UINT Stride = sizeof(XMFLOAT3);	

	btVector3 bmin;
	btVector3 bmax;
	XMMATRIX mModel;
	D3D11_MAPPED_SUBRESOURCE MappedResource;


	if(group->GetPhysicsObject() != NULL)
	{
		exit(1);
		btTransform trafo = group->GetPhysicsObject()->getWorldTransform();
		group->GetPhysicsObject()->getCollisionShape()->getAabb(trafo,bmin, bmax);
		//[i].getCollisionShape()->getAabb(bmin, bmax);

		btVector3 axisExtend = bmax - bmin;		
		std::cout << axisExtend.x() << ", " << axisExtend.y() << ", " << axisExtend.z() << std::endl;
		DXUTGetD3D11DeviceContext()->Map( _cbModelMatrix, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
		CB_MODEL_MATRIX* pCB = ( CB_MODEL_MATRIX* )MappedResource.pData;			
		mModel = XMMatrixScaling(axisExtend.x(), axisExtend.y(), axisExtend.z()) * XMMatrixTranslation(bmin.x(), bmin.y(), bmin.z()); 				
		pCB->mModel = mModel;
		DXUTGetD3D11DeviceContext()->Unmap( _cbModelMatrix, 0 );	

		DXUTGetD3D11DeviceContext()->VSSetConstantBuffers( CB_LOC::MODEL_MATRIX, 1, &_cbModelMatrix );	

		DXUTGetD3D11DeviceContext()->IASetInputLayout( _inputLayout );
		DXUTGetD3D11DeviceContext()->IASetVertexBuffers( 0, 1, &_vertexBufferStatic, &Stride, &Offset );
		DXUTGetD3D11DeviceContext()->IASetPrimitiveTopology( lines ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DXUTGetD3D11DeviceContext()->IASetIndexBuffer(		 lines ? _linesIndicesBuffer : _solidIndicesBuffer, DXGI_FORMAT_R32_UINT, 0 );

		DXUTGetD3D11DeviceContext()->VSSetShader(_aabbVS->Get(), NULL, 0);
		DXUTGetD3D11DeviceContext()->HSSetShader(NULL, 0, 0);
		DXUTGetD3D11DeviceContext()->DSSetShader(NULL, 0, 0);
		DXUTGetD3D11DeviceContext()->GSSetShader(NULL, 0, 0);
		DXUTGetD3D11DeviceContext()->PSSetShader(_aabbPS->Get(), NULL, 0);

		DXUTGetD3D11DeviceContext()->DrawIndexed(lines ? 24 : 36, 0, 0);

		
	}
	return hr;
}

HRESULT RendererBoundingGeometry::RenderPhysicsAABB( Scene* scene, bool lines /*= true*/)
{
	HRESULT hr = S_OK;
		
	UINT Offset = 0;
	UINT Stride = sizeof(XMFLOAT3);	
	
	btVector3 bmin;
	btVector3 bmax;
	XMMATRIX mModel;
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	
	
if(1)
{
	int N = scene->GetPhysics()->GetDynamicsWorld()->getNumCollisionObjects();
	for(int i = 0; i < N; ++i)
	{	
		
		btTransform trafo = scene->GetPhysics()->GetDynamicsWorld()->getCollisionObjectArray()[i]->getWorldTransform();
		scene->GetPhysics()->GetDynamicsWorld()->getCollisionObjectArray()[i]->getCollisionShape()->getAabb(trafo,bmin, bmax);
			//[i].getCollisionShape()->getAabb(bmin, bmax);

		btVector3 axisExtend = bmax - bmin;		

		DXUTGetD3D11DeviceContext()->Map( _cbModelMatrix, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
		CB_MODEL_MATRIX* pCB = ( CB_MODEL_MATRIX* )MappedResource.pData;			
		mModel = XMMatrixScaling(axisExtend.x(), axisExtend.y(), axisExtend.z()) * XMMatrixTranslation(bmin.x(), bmin.y(), bmin.z()); 				
		pCB->mModel = mModel;
		DXUTGetD3D11DeviceContext()->Unmap( _cbModelMatrix, 0 );	

		DXUTGetD3D11DeviceContext()->VSSetConstantBuffers( CB_LOC::MODEL_MATRIX, 1, &_cbModelMatrix );	

		DXUTGetD3D11DeviceContext()->IASetInputLayout( _inputLayout );
		DXUTGetD3D11DeviceContext()->IASetVertexBuffers( 0, 1, &_vertexBufferStatic, &Stride, &Offset );
		DXUTGetD3D11DeviceContext()->IASetPrimitiveTopology( lines ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DXUTGetD3D11DeviceContext()->IASetIndexBuffer(		 lines ? _linesIndicesBuffer : _solidIndicesBuffer, DXGI_FORMAT_R32_UINT, 0 );

		DXUTGetD3D11DeviceContext()->VSSetShader(_aabbVS->Get(), NULL, 0);
		DXUTGetD3D11DeviceContext()->HSSetShader(NULL, 0, 0);
		DXUTGetD3D11DeviceContext()->DSSetShader(NULL, 0, 0);
		DXUTGetD3D11DeviceContext()->GSSetShader(NULL, 0, 0);
		DXUTGetD3D11DeviceContext()->PSSetShader(_aabbPS->Get(), NULL, 0);

		DXUTGetD3D11DeviceContext()->DrawIndexed(lines ? 24 : 36, 0, 0);

	}
}
else
{

	for(const auto &model : scene->GetModelGroups())
	{	
		// only render physics aabbs
		if(!model->_physicsObject)
			continue;

		model->_physicsObject->getAabb(bmin, bmax);

		btVector3 axisExtend = bmax - bmin;		

		DXUTGetD3D11DeviceContext()->Map( _cbModelMatrix, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
		CB_MODEL_MATRIX* pCB = ( CB_MODEL_MATRIX* )MappedResource.pData;			
		mModel = XMMatrixScaling(axisExtend.x(), axisExtend.y(), axisExtend.z()) * XMMatrixTranslation(bmin.x(), bmin.y(), bmin.z()); 				
		pCB->mModel = mModel;
		DXUTGetD3D11DeviceContext()->Unmap( _cbModelMatrix, 0 );	

		DXUTGetD3D11DeviceContext()->VSSetConstantBuffers( CB_LOC::MODEL_MATRIX, 1, &_cbModelMatrix );	

		DXUTGetD3D11DeviceContext()->IASetInputLayout( _inputLayout );
		DXUTGetD3D11DeviceContext()->IASetVertexBuffers( 0, 1, &_vertexBufferStatic, &Stride, &Offset );
		DXUTGetD3D11DeviceContext()->IASetPrimitiveTopology( lines ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DXUTGetD3D11DeviceContext()->IASetIndexBuffer(		 lines ? _linesIndicesBuffer : _solidIndicesBuffer, DXGI_FORMAT_R32_UINT, 0 );

		DXUTGetD3D11DeviceContext()->VSSetShader(_aabbVS->Get(), NULL, 0);
		DXUTGetD3D11DeviceContext()->HSSetShader(NULL, 0, 0);
		DXUTGetD3D11DeviceContext()->DSSetShader(NULL, 0, 0);
		DXUTGetD3D11DeviceContext()->GSSetShader(NULL, 0, 0);
		DXUTGetD3D11DeviceContext()->PSSetShader(_aabbPS->Get(), NULL, 0);

		DXUTGetD3D11DeviceContext()->DrawIndexed(lines ? 24 : 36, 0, 0);

	}
}
	return hr;
}

HRESULT RendererBoundingGeometry::RenderOBB( const DXObjectOrientedBoundingBox* obb, bool isIntersectingOBB /*= false*/ ) const
{
	HRESULT hr = S_OK;

	// update dynamic box buffers
	std::vector<XMFLOAT3> verts;
	std::vector<UINT>	indices;
	obb->GetObjectVerticesAndIndices(verts,indices);

	if(verts.size() > 0)
	{	

	D3D11_MAPPED_SUBRESOURCE resource;
	// update vertex buffer
	DXUTGetD3D11DeviceContext()->Map(_vertexBufferDynamic, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);		
	memcpy(resource.pData, &verts[0], sizeof( XMFLOAT3 ) * verts.size() );
	DXUTGetD3D11DeviceContext()->Unmap(_vertexBufferDynamic, 0);

	// update index buffer
	DXUTGetD3D11DeviceContext()->Map(_indexBufferDynamic, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);	
	memcpy(resource.pData, &indices[0], sizeof( UINT ) * indices.size() );
	DXUTGetD3D11DeviceContext()->Unmap(_indexBufferDynamic, 0);

	UINT Offset = 0;
	UINT Stride = sizeof(XMFLOAT3);	


	// set model matrix
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	DXUTGetD3D11DeviceContext()->Map( _cbModelMatrix, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CB_MODEL_MATRIX* pCB = ( CB_MODEL_MATRIX* )MappedResource.pData;	

	// TODO use physics model matrix, skinning?
	pCB->mModel = XMMatrixIdentity();
	DXUTGetD3D11DeviceContext()->Unmap( _cbModelMatrix, 0 );	
		
	DXUTGetD3D11DeviceContext()->VSSetConstantBuffers( CB_LOC::MODEL_MATRIX, 1, &_cbModelMatrix );	

	// render

	DXUTGetD3D11DeviceContext()->IASetInputLayout( _inputLayout );
	DXUTGetD3D11DeviceContext()->IASetVertexBuffers( 0, 1, &_vertexBufferDynamic, &Stride, &Offset );
	DXUTGetD3D11DeviceContext()->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_LINELIST );
	DXUTGetD3D11DeviceContext()->IASetIndexBuffer(		_indexBufferDynamic, DXGI_FORMAT_R32_UINT, 0 );

	DXUTGetD3D11DeviceContext()->VSSetShader(_aabbVS->Get(), NULL, 0);
	DXUTGetD3D11DeviceContext()->HSSetShader(NULL, 0, 0);
	DXUTGetD3D11DeviceContext()->DSSetShader(NULL, 0, 0);
	DXUTGetD3D11DeviceContext()->GSSetShader(NULL, 0, 0);
	DXUTGetD3D11DeviceContext()->PSSetShader(_aabbPS->Get(), NULL, 0);

	DXUTGetD3D11DeviceContext()->DrawIndexed(24, 0, 0);
	}

	return hr;
}

HRESULT RendererBoundingGeometry::RenderFrustum( const XMFLOAT3 cornerPoints[8] ) const
{
	HRESULT hr = S_OK;

	UINT Offset = 0;
	UINT Stride = sizeof(XMFLOAT3);	

	
	// set model matrix
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	DXUTGetD3D11DeviceContext()->Map( _cbModelMatrix, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CB_MODEL_MATRIX* pCB = ( CB_MODEL_MATRIX* )MappedResource.pData;	
	pCB->mModel = XMMatrixIdentity();
	DXUTGetD3D11DeviceContext()->Unmap( _cbModelMatrix, 0 );	
	DXUTGetD3D11DeviceContext()->VSSetConstantBuffers( CB_LOC::MODEL_MATRIX, 1, &_cbModelMatrix );	


	// update vertex buffer
	DXUTGetD3D11DeviceContext()->Map(_vertexBufferDynamic, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);		
	memcpy(MappedResource.pData, &cornerPoints[0], sizeof( XMFLOAT3 ) * 8 );
	DXUTGetD3D11DeviceContext()->Unmap(_vertexBufferDynamic, 0);


	DXUTGetD3D11DeviceContext()->IASetInputLayout( _inputLayout );
	DXUTGetD3D11DeviceContext()->IASetVertexBuffers( 0, 1, &_vertexBufferDynamic, &Stride, &Offset );
	DXUTGetD3D11DeviceContext()->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_LINELIST );
	DXUTGetD3D11DeviceContext()->IASetIndexBuffer(	_linesIndicesBuffer, DXGI_FORMAT_R32_UINT, 0 );

	DXUTGetD3D11DeviceContext()->VSSetShader(_frustumVS->Get(), NULL, 0);
	DXUTGetD3D11DeviceContext()->HSSetShader(NULL, 0, 0);
	DXUTGetD3D11DeviceContext()->DSSetShader(NULL, 0, 0);
	DXUTGetD3D11DeviceContext()->GSSetShader(NULL, 0, 0);
	DXUTGetD3D11DeviceContext()->PSSetShader(_frustumPS->Get(), NULL, 0);

	DXUTGetD3D11DeviceContext()->DrawIndexed(24, 0, 0);



	return hr;
}

HRESULT RendererBoundingGeometry::initCube()
{
	HRESULT hr = S_OK;

	//   7------6
	//  /|     /|
	// 4-+----5 |
	// | 3----|-2
	// |/     |/
	// 0----- 1 

	XMFLOAT3 vertices[8] =  {
		XMFLOAT3( 0.f,  0.f,  0.f),//, 1.f),
		XMFLOAT3( 1.f,  0.f,  0.f),//, 1.f),
		XMFLOAT3( 1.f,  1.f,  0.f),//, 1.f),
		XMFLOAT3( 0.f,  1.f,  0.f),//, 1.f),
								  
		XMFLOAT3( 0.f,  0.f,  1.f),//, 1.f),
		XMFLOAT3( 1.f,  0.f,  1.f),//, 1.f),
		XMFLOAT3( 1.f,  1.f,  1.f),//, 1.f),
		XMFLOAT3( 0.f,  1.f,  1.f),//, 1.f),
	};

	UINT indicesLines[24] = { 0, 1,		1, 2,	2, 3,	3, 0, 
							  4, 5,		5, 6,	6, 7,	7, 4,
							  0, 4,		1, 5,	2, 6,	3, 7};
	
	UINT indicesSolid[36] = {
							  0, 3, 1,		1, 3, 2,	// floor		
							  5, 6, 4,		6, 7, 4,	// ceiling		
							  1, 2, 5,		5, 2, 6,	// right wall		
							  0, 4, 3,		3, 4, 7,	// left wall		
							  2, 3, 6,		6, 3, 7,	// back wall		
							  1, 5, 0,		0, 5, 4, };	// front wall
	
	V_RETURN(DXCreateBuffer(DXUTGetD3D11Device(), D3D11_BIND_VERTEX_BUFFER, 8 * sizeof(XMFLOAT3), 0, D3D11_USAGE_IMMUTABLE, _vertexBufferStatic, &vertices[0]));
	V_RETURN(DXCreateBuffer(DXUTGetD3D11Device(), D3D11_BIND_INDEX_BUFFER, 24 * sizeof(UINT), 0, D3D11_USAGE_IMMUTABLE, _linesIndicesBuffer, &indicesLines[0]));
	V_RETURN(DXCreateBuffer(DXUTGetD3D11Device(), D3D11_BIND_INDEX_BUFFER, 36 * sizeof(UINT), 0, D3D11_USAGE_IMMUTABLE, _solidIndicesBuffer, &indicesSolid[0]));




	return hr;
}

HRESULT RendererBoundingGeometry::RenderControlCage( ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance ) const
{
	HRESULT hr = S_OK;

	UINT Offset = 0;
	UINT Stride = sizeof(XMFLOAT3);	
	
	//// set model matrix
	//D3D11_MAPPED_SUBRESOURCE MappedResource;
	//DXUTGetD3D11DeviceContext()->Map( _cbModelMatrix, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	//CB_MODEL_MATRIX* pCB = ( CB_MODEL_MATRIX* )MappedResource.pData;	

	//// TODO use physics model matrix, skinning?
	//pCB->mModel = XMMatrixIdentity();
	//DXUTGetD3D11DeviceContext()->Unmap( _cbModelMatrix, 0 );	

	//DXUTGetD3D11DeviceContext()->VSSetConstantBuffers( CB_LOC::MODEL_MATRIX, 1, &_cbModelMatrix );	

	// render

	pd3dImmediateContext->IASetInputLayout( _inputLayout );
	ID3D11Buffer* vb[] = { instance->GetOSDMesh()->GetControlCageEdgeVertices()};
	pd3dImmediateContext->IASetVertexBuffers( 0, 1, vb, &Stride, &Offset );
	pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_LINELIST );
	//DXUTGetD3D11DeviceContext()->IASetIndexBuffer(		_indexBufferDynamic, DXGI_FORMAT_R32_UINT, 0 );	

	pd3dImmediateContext->VSSetShader(_controlCageVS->Get(), NULL, 0);
	pd3dImmediateContext->HSSetShader(NULL, 0, 0);
	pd3dImmediateContext->DSSetShader(NULL, 0, 0);
	pd3dImmediateContext->GSSetShader(NULL, 0, 0);
	pd3dImmediateContext->PSSetShader(_aabbPS->Get(), NULL, 0);

	//DXUTGetD3D11DeviceContext()->DrawIndexed(instance->GetOSDMesh()->GetNumCageEdgeVertices(), 0, 0);
	pd3dImmediateContext->Draw(instance->GetOSDMesh()->GetNumCageEdgeVertices(), 0);

	return hr;
}

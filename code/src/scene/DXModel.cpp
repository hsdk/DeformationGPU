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
#include "DXModel.h"
#include "ModelLoader.h"
#include <SDX/DXBuffer.h>
#include <vector>

#include "utils/DbgNew.h" // has to be last include

using namespace DirectX;


TriangleSubmesh::TriangleSubmesh()	
{
	_baseVertex		= 0;
	_numFaces		= 0;
	_numVertices	= 0;		
	_material		= NULL;
	_indexBuffer = NULL;
}

TriangleSubmesh::~TriangleSubmesh()
{
	// cleanup is done in ~DXModel

}

HRESULT TriangleSubmesh::Create( ID3D11Device1* pd3dDevice, const SubMeshData* data, const std::vector<XMFLOAT4A>& vertices )
{	
	_baseVertex		= data->baseVertex;
	_numFaces		= static_cast<unsigned int>(data->indicesTri.size());
	_numVertices	= data->numVertices;		
	_material		= data->material;

	HRESULT hr = S_OK;

	V_RETURN(DXUTCreateBuffer(pd3dDevice, D3D11_BIND_INDEX_BUFFER , _numFaces * sizeof(XMUINT3), 0, D3D11_USAGE_IMMUTABLE, _indexBuffer, (void*)&data->indicesTri[0]));
	DXUT_SetDebugName(_indexBuffer,"Triangle Submesh index buffer");
	
	// init oriented bounding boxes	
	if (!m_obbObject.IsValid()) 
	{

		std::vector<XMFLOAT3> controlPoints(_numVertices);
		//for( UINT i = _baseVertex; i < _baseVertex+_numVertices; ++i)		
		for (UINT i = 0; i < _numVertices; ++i)
		{
			const auto& v = vertices[_baseVertex + i];
			controlPoints[i] = XMFLOAT3(v.x, v.y, v.z);
		}
		m_obbObject.ComputeFromPCA(controlPoints);		
	}
	
	if(!m_obbObject.IsValid())
	{
		MessageBoxA(0, "invalid obb", "ERROR", MB_OK);			
	}
	return hr;
}

void TriangleSubmesh::Destroy()
{
	SAFE_RELEASE(_indexBuffer);
}

DXModel::DXModel()
	//: Voxelizable(), MovableObject()
{
	_vertexBuffer	= NULL;
	_normalsBuffer	= NULL;
	_texcoordBuffer = NULL;
	_numVertices = 0;

	// FOR SKINNING
	g_pVertexBufferSRV = NULL;
	g_pVertexBufferSRV4Components = NULL;
	g_pVertexBufferUAV = NULL;

	g_pVertexBufferBasePose = NULL;
	g_pVertexBufferBasePoseSRV = NULL;
	g_pVertexBufferBasePoseSRV4Components = NULL;


	g_pNormalsBufferSRV = NULL;
	g_pNormalsBufferSRV4Components = NULL;
	g_pNormalsBufferUAV = NULL;

	g_pNormalsBufferBasePose = NULL;
	g_pNormalsBufferBasePoseSRV = NULL;
	g_pNormalsBufferBasePoseSRV4Components = NULL;

	//m_skinningMeshAnimationManager = NULL;

	//_baseVertex = 0;
}

DXModel::~DXModel()
{
	for(auto submeshes : submeshes)
	{
		submeshes.Destroy();
	}
	submeshes.clear();

	SAFE_RELEASE(_vertexBuffer);
	SAFE_RELEASE(_normalsBuffer);
	SAFE_RELEASE(_texcoordBuffer);

	//SAFE_DELETE(m_skinningMeshAnimationManager);
	

	//SAFE_RELEASE(g_pVertexBuffer);
	// skinning...
	SAFE_RELEASE(g_pVertexBufferSRV);
	SAFE_RELEASE(g_pVertexBufferSRV4Components);
	SAFE_RELEASE(g_pVertexBufferUAV);

	SAFE_RELEASE(g_pVertexBufferBasePose);
	SAFE_RELEASE(g_pVertexBufferBasePoseSRV);
	SAFE_RELEASE(g_pVertexBufferBasePoseSRV4Components);

	SAFE_RELEASE(g_pNormalsBufferSRV);
	SAFE_RELEASE(g_pNormalsBufferSRV4Components);
	SAFE_RELEASE(g_pNormalsBufferUAV);

	SAFE_RELEASE(g_pNormalsBufferBasePose);
	SAFE_RELEASE(g_pNormalsBufferBasePoseSRV);
	SAFE_RELEASE(g_pNormalsBufferBasePoseSRV4Components);
	//_name.clear();
}

HRESULT DXModel::Create( ID3D11Device1* pd3dDevice, 
						MeshData* meshData)
{
	HRESULT hr = S_OK;
	assert(meshData->vertices.size() > 0);	

	//V_RETURN(Voxelizable::Create(pd3dDevice));
	
	_numVertices = static_cast<unsigned int>(meshData->vertices.size());	

	//bool withSkinning = meshData->skinning != NULL;
	bool withSkinning = meshData->withSkinning != NULL;
	//m_skinningMeshAnimationManager = meshData->skinning; 
	

	V_RETURN(DXUTCreateBuffer(pd3dDevice,	D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE | (withSkinning ? D3D11_BIND_UNORDERED_ACCESS : 0),
			_numVertices * sizeof(XMFLOAT4A), 0, (withSkinning ? D3D11_USAGE_DEFAULT : D3D11_USAGE_IMMUTABLE), _vertexBuffer,  &meshData->vertices[0]));
		
	std::vector<XMFLOAT4A> N(_numVertices);
	if(meshData->normals.size() > 0)
	{
		assert(meshData->normals.size() == _numVertices);
		// todo make float3!!
		for (UINT i = 0; i < _numVertices; ++i)
		{
			N[i] = XMFLOAT4A(meshData->normals[i].x, meshData->normals[i].y, meshData->normals[i].z, 0.0f);
		}

		V_RETURN(DXUTCreateBuffer(pd3dDevice, D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE | (withSkinning ? D3D11_BIND_UNORDERED_ACCESS : 0), _numVertices * sizeof(XMFLOAT4A),
			0, (withSkinning ? D3D11_USAGE_DEFAULT : D3D11_USAGE_IMMUTABLE), _normalsBuffer,  &N[0]));
	}

	if(meshData->texcoords.size() > 0)
	{
		assert(meshData->texcoords.size() == _numVertices);
		V_RETURN(DXUTCreateBuffer(pd3dDevice, D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE, _numVertices * sizeof(XMFLOAT2A), 0, D3D11_USAGE_IMMUTABLE, _texcoordBuffer, &meshData->texcoords[0]));
	}

	// process submeshes
	for(const auto& subdata : meshData->meshes)
	{		
		TriangleSubmesh mesh;
		std::cout << "init model " << subdata.name << std::endl;
		mesh.Create(pd3dDevice, &subdata, meshData->vertices);
		mesh.SetName(subdata.name);	
		submeshes.push_back(mesh);


	}

	

	// SKINNING
	// ---- skinning stuff begin (if animated?)
	//if (m_skinningMeshAnimationManager) {
	if (meshData->withSkinning) {
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		ZeroMemory( &SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC) );		
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		SRVDesc.Buffer.FirstElement = 0;

		SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
		SRVDesc.Buffer.NumElements = 4 * _numVertices;
		V_RETURN(pd3dDevice->CreateShaderResourceView( _vertexBuffer, &SRVDesc, &g_pVertexBufferSRV));
		DXUT_SetDebugName(g_pVertexBufferSRV,"g_pVertexBufferSRV");


		SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;				
		SRVDesc.Buffer.NumElements = _numVertices;
		V_RETURN(pd3dDevice->CreateShaderResourceView( _vertexBuffer, &SRVDesc, &g_pVertexBufferSRV4Components));
		DXUT_SetDebugName(g_pVertexBufferSRV4Components,"g_pVertexBufferSRV4Components");


		D3D11_UNORDERED_ACCESS_VIEW_DESC DescUAV;
		ZeroMemory( &DescUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC) );		
		DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		DescUAV.Buffer.FirstElement = 0;
		DescUAV.Format = DXGI_FORMAT_R32_FLOAT;
		DescUAV.Buffer.NumElements = 4 * _numVertices;
		V_RETURN( pd3dDevice->CreateUnorderedAccessView( _vertexBuffer, &DescUAV, &g_pVertexBufferUAV ));	
		DXUT_SetDebugName(g_pVertexBufferUAV,"g_pVertexBufferUAV");

		// creating the vertex buffer and it's views
		V_RETURN(DXUTCreateBuffer(DXUTGetD3D11Device(),	D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,	_numVertices * sizeof(XMFLOAT4A), 0,	D3D11_USAGE_DEFAULT,
			g_pVertexBufferBasePose, &meshData->vertices[0], 0, sizeof(XMFLOAT4A)));
		DXUT_SetDebugName(g_pVertexBufferBasePose,"g_pVertexBufferBasePose");

		SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
		SRVDesc.Buffer.NumElements = 4 * _numVertices;
		V_RETURN(pd3dDevice->CreateShaderResourceView( g_pVertexBufferBasePose, &SRVDesc, &g_pVertexBufferBasePoseSRV));
		DXUT_SetDebugName(g_pVertexBufferBasePoseSRV,"g_pVertexBufferBasePoseSRV");


		SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		SRVDesc.Buffer.NumElements = _numVertices;
		V_RETURN(pd3dDevice->CreateShaderResourceView( g_pVertexBufferBasePose, &SRVDesc, &g_pVertexBufferBasePoseSRV4Components));
		DXUT_SetDebugName(g_pVertexBufferBasePoseSRV4Components,"g_pVertexBufferBasePoseSRV4Components");

		
		// normals
				
		SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
		SRVDesc.Buffer.NumElements = 4 * _numVertices;
		V_RETURN(pd3dDevice->CreateShaderResourceView( _normalsBuffer, &SRVDesc, &g_pNormalsBufferSRV));
		DXUT_SetDebugName(g_pNormalsBufferSRV,"g_pNormalsBufferSRV");


		SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;		
		SRVDesc.Buffer.NumElements = _numVertices;
		V_RETURN(pd3dDevice->CreateShaderResourceView( _normalsBuffer, &SRVDesc, &g_pNormalsBufferSRV4Components));
		DXUT_SetDebugName(g_pNormalsBufferSRV4Components,"g_pNormalsBufferSRV4Components");


		DescUAV.Format = DXGI_FORMAT_R32_FLOAT;
		DescUAV.Buffer.NumElements = 4 * _numVertices;
		V_RETURN( pd3dDevice->CreateUnorderedAccessView( _normalsBuffer, &DescUAV, &g_pNormalsBufferUAV ));	
		DXUT_SetDebugName(g_pNormalsBufferUAV,"g_pNormalsBufferUAV");

		// create the vertex buffer and it's views
		V_RETURN(DXUTCreateBuffer(DXUTGetD3D11Device(),	D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
			_numVertices * sizeof(XMFLOAT4A), 0,	D3D11_USAGE_DEFAULT,
			g_pNormalsBufferBasePose, (void*)&N[0], 0, sizeof(XMFLOAT4A)));
		DXUT_SetDebugName(g_pNormalsBufferBasePose,"g_pNormalsBufferBasePose");
				
		SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
		SRVDesc.Buffer.NumElements = 4 * _numVertices;
		V_RETURN(pd3dDevice->CreateShaderResourceView( g_pNormalsBufferBasePose, &SRVDesc, &g_pNormalsBufferBasePoseSRV));
		DXUT_SetDebugName(g_pNormalsBufferBasePoseSRV,"g_pNormalsBufferBasePoseSRV");


		SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;						
		SRVDesc.Buffer.NumElements = _numVertices;
		V_RETURN(pd3dDevice->CreateShaderResourceView( g_pNormalsBufferBasePose, &SRVDesc, &g_pNormalsBufferBasePoseSRV4Components));
		DXUT_SetDebugName(g_pNormalsBufferBasePoseSRV4Components,"g_pNormalsBufferBasePoseSRV4Components");
		

	}
	return hr;
}

void DXModel::UpdateOBBs( const XMMATRIX& modelMatrix, DXObjectOrientedBoundingBox& obbWorldResult )
{
	for(auto m : submeshes)
		m.m_obbObject.Transform(modelMatrix, obbWorldResult);
}


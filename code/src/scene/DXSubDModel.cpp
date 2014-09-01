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
#include "scene/DXSubDModel.h"
#include <SDX/DXBuffer.h>

using namespace OpenSubdiv;
using namespace DirectX;

//Henry: has to be last header
#include "utils/DbgNew.h"

DXOSDMesh::DXOSDMesh()
{
	_model = NULL;
	_material = NULL;
	_name = "noname";
	_bbMax = XMFLOAT4A(-FLT_MAX,-FLT_MAX,-FLT_MAX, 1.f);
	_bbMin = XMFLOAT4A( FLT_MAX, FLT_MAX, FLT_MAX, 1.f);
	_bsRadius = 0;	
	_numOSDVertexElements = 0;
	_inputLayout = NULL;

	m_ptexNeighDataBUF = NULL;
	m_ptexNeighDataSRV = NULL;

	m_extraordinaryInfoBUF=NULL;
	m_extraordinaryInfoSRV=NULL;

	m_extraordinaryDataBUF=NULL;
	m_extraordinaryDataSRV=NULL;

	m_cageEdgeVertices=NULL;
	//m_cageEdgeVerticesSRV=NULL;
	
	m_hasTexCoords = false;

	m_numExtraordinary = 0;

	m_numCageEdgeVertices = 0;
}

DXOSDMesh::~DXOSDMesh()
{
	Destroy();
}

HRESULT DXOSDMesh::Create( ID3D11Device1* pd3dDevice, HbrMesh<OsdVertex> *hbrMesh, const std::vector<XMFLOAT4A>& verticesF4, bool hasTexcoords, bool isDeformable )
{
	HRESULT hr = S_OK;
	//if(!g_app.g_osdSubdivider)
	//	return S_FALSE;

	if(!_material)
	{
		std::cerr << "material has not been set yet before DXOSDMesh::Create" << std::endl;
		return S_FALSE;
	}

	// control cage - edge vertex assignment has to be done before creating osdmesh, otherwise num faces reports subdivided mesh faces
	{	
		std::vector<XMFLOAT3> cageEdges;
		UINT numFaces = hbrMesh->GetNumFaces();
		std::cout << "num Faces" << numFaces << std::endl;
		for (unsigned int i = 0; i < numFaces; ++i)
		{
			const OsdHbrFace *face = hbrMesh->GetFace(i);
			int nv = face->GetNumVertices();
			for(int j = 0; j < nv; ++j)
			{
				auto eStart = face->GetVertex(j)->GetID();
				auto eEnd = face->GetVertex((j+1)%nv)->GetID();
				const auto &vStart = verticesF4[eStart];
				const auto &vEnd = verticesF4[eEnd];

				cageEdges.push_back(XMFLOAT3(vStart.x, vStart.y, vStart.z));
				cageEdges.push_back(XMFLOAT3(vEnd.x, vEnd.y, vEnd.z));			
	
			}
		}
				
		V_RETURN(DXUTCreateBuffer(pd3dDevice,	D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE ,	static_cast<unsigned int>(cageEdges.size()*sizeof(XMFLOAT3)),
								  0,D3D11_USAGE_IMMUTABLE, m_cageEdgeVertices,  &cageEdges[0].x));

		m_numCageEdgeVertices = static_cast<unsigned int>(cageEdges.size());
	}


	// topology has been stored in hbr mesh, now create GPU mesh, vertex data will be added later
	OsdMeshBitset configBits;
	configBits.set(MeshAdaptive, true); // we want adaptive subdivision

	if(isDeformable)
		configBits.set(MeshPtexData, true);	// checkme

	if(hasTexcoords)
		configBits.set(OpenSubdiv::MeshFVarData, 1);
		
	m_hasTexCoords = false;
	_numOSDVertexElements =  3;		// only position data, uvs are in FVarData																		
	int numVaryingElements = 0;

	// create the gpu mesh	
	
	_model = new OsdMesh<OsdD3D11VertexBuffer,
						OsdD3D11ComputeController,
						OsdD3D11DrawContext> 
			(	
				g_app.g_osdSubdivider,
				hbrMesh,
				_numOSDVertexElements,
				numVaryingElements,
				g_app.g_maxSubdivisions,
				configBits, DXUTGetD3D11DeviceContext()
			);
	

	// opensubdiv expects float array with vertex positions xyz
	UINT numV = static_cast<UINT>(verticesF4.size());

	std::vector<float> verticesFloatArray;
	verticesFloatArray.reserve(numV*_numOSDVertexElements);

	XMVECTOR vMeshCenter = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	for(UINT i = 0; i < numV; ++i)
	{
		const XMFLOAT4A& v = verticesF4[i];
		verticesFloatArray.push_back(v.x);
		verticesFloatArray.push_back(v.y);
		verticesFloatArray.push_back(v.z);
		
		XMVECTOR curr = XMLoadFloat4A(&verticesF4[i]);
		vMeshCenter += curr; 
	}
	vMeshCenter /= (float)numV;
	XMStoreFloat3A(&_meshCenter, vMeshCenter);


	// upload vertex positions to gpu and run subdivision kernel
	_model->UpdateVertexBuffer(&verticesFloatArray[0], 0, numV);
	_model->Refine();
	_model->Synchronize();	


	// update bounding geometry
	XMVECTOR bmin = XMLoadFloat4A(&_bbMin);
	XMVECTOR bmax = XMLoadFloat4A(&_bbMax);
	for(UINT i = 0; i < numV; ++i)
	{		
		XMVECTOR curr = XMLoadFloat4A(&verticesF4[i]);
		bmin = XMVectorMin(bmin, curr);
		bmax = XMVectorMax(bmax, curr);		
		_bsRadius = XMMax(_bsRadius, XMVectorGetX(XMVector3Length(curr-vMeshCenter)));
	}

	XMStoreFloat4A(&_bbMin, bmin);
	XMStoreFloat4A(&_bbMax, bmax);

	if (!_obb.IsValid()) 
	{
		_obb.ComputeFromPCA((XMFLOAT3*)&verticesFloatArray[0], numV);	
	}

	// ptex neigh data buffers
	if(!m_ptexNeighDataCPU.empty())
	{	
		UINT numElements = static_cast<UINT>(m_ptexNeighDataCPU.size()); // num ptex faces
		//std::cout << "sizeof ptexneighdata struct" << sizeof(PtexNeighborData) << "num uint elems " << sizeof(PtexNeighborData) / sizeof(UINT) << std::endl;
#if 0
		const auto testEntry = m_ptexNeighDataCPU[233];
		std::cout << "tile 233: " << std::endl
								  << "e0: ptexID: " << testEntry.ptexIDNeighbor[0] << ", 
								  " << testEntry.neighEdgeID[0] << std::endl
								  << "e1: ptexID: " << testEntry.ptexIDNeighbor[1] << ", we are edge " << testEntry.neighEdgeID[1] << std::endl
								  << "e2: ptexID: " << testEntry.ptexIDNeighbor[2] << ", we are edge " << testEntry.neighEdgeID[2] << std::endl
								  << "e3: ptexID: " << testEntry.ptexIDNeighbor[3] << ", we are edge " << testEntry.neighEdgeID[3] << std::endl;

#endif
		V_RETURN(DXUTCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE, numElements*sizeof(SPtexNeighborData), 0, D3D11_USAGE_IMMUTABLE, m_ptexNeighDataBUF,  &m_ptexNeighDataCPU[0].ptexIDNeighbor[0]));

		D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
		ZeroMemory(&descSRV, sizeof(descSRV));
		descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		descSRV.Format = DXGI_FORMAT_R32_SINT;		
		descSRV.Buffer.FirstElement = 0;	
		descSRV.Buffer.NumElements = numElements*sizeof(SPtexNeighborData)/sizeof(UINT);
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_ptexNeighDataBUF, &descSRV, &m_ptexNeighDataSRV));

				
		// extraordinary info		
		if(!m_extraordinaryInfoCPU.empty())
		{		
			std::cout << "sizeof extraordinary info struct" << sizeof(SExtraordinaryInfo) << "num uint elems " << sizeof(SExtraordinaryInfo) / sizeof(UINT) << std::endl;
			std::cout << "num elems in extraordinary info " << m_extraordinaryInfoCPU.size() << std::endl;
			numElements = static_cast<UINT>(m_extraordinaryInfoCPU.size()); // should have been set by loader
			descSRV.Format = DXGI_FORMAT_R32_UINT;		

			V_RETURN(DXUTCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE, numElements*sizeof(SExtraordinaryInfo), 0, D3D11_USAGE_IMMUTABLE, m_extraordinaryInfoBUF,  &m_extraordinaryInfoCPU[0].startIndex));

			descSRV.Buffer.NumElements = numElements*sizeof(SExtraordinaryInfo)/sizeof(UINT);
			V_RETURN(pd3dDevice->CreateShaderResourceView(m_extraordinaryInfoBUF, &descSRV, &m_extraordinaryInfoSRV));
		}

		// extraordinary data
		if(!m_extraordinaryDataCPU.empty())
		{		
			std::cout << "sizeof extraordinary data struct" << sizeof(SExtraordinaryData) << "num uint elems " << sizeof(SExtraordinaryData) / sizeof(UINT) << std::endl;
			std::cout << "num elems in extraordinary data " << m_extraordinaryDataCPU.size() << std::endl;
			numElements = static_cast<UINT>(m_extraordinaryDataCPU.size()); // should have been set by loader
			descSRV.Format = DXGI_FORMAT_R32_UINT;		

			V_RETURN(DXUTCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE, numElements*sizeof(SExtraordinaryData), 0, D3D11_USAGE_IMMUTABLE, m_extraordinaryDataBUF,  &m_extraordinaryDataCPU[0].ptexFaceID));

			descSRV.Buffer.NumElements =numElements*sizeof(SExtraordinaryData)/sizeof(UINT);
			V_RETURN(pd3dDevice->CreateShaderResourceView(m_extraordinaryDataBUF, &descSRV, &m_extraordinaryDataSRV));
		}
	}




	

	if(g_app.g_withPaintSculptTimings)
	{
		UINT totalNumPatches = 0;
		const auto& patches = _model->GetDrawContext()->patchArrays;

		// draw patches
		for(const auto& patch : patches)
		{

			//D3D11_PRIMITIVE_TOPOLOGY topology;
			//if (patch.GetDescriptor().GetType() != OpenSubdiv::FarPatchTables::REGULAR) continue; // render only regular test
			//if (patch.GetDescriptor().GetType() != OpenSubdiv::FarPatchTables::NON_PATCH) continue; // render only regular test
			//if (patch.GetDescriptor().GetSubPatch() > 0) continue; // debug test
			//if(patch.GetDescriptor().GetPattern() != 0) continue;
			//if(patch.GetDescriptor().GetRotation() != 0) continue;
			//std::cout << (int)patch.GetDescriptor().GetRotation() << std::endl;

			totalNumPatches+= patch.GetNumPatches();
		}

		std::cout << "total num patches: " << totalNumPatches << std::endl;
		std::cout << "num coarse faces: " << _model->GetNumPTexFaces() << std::endl;
	}
	
	return hr;
}

void DXOSDMesh::Destroy()
{
	std::cerr << "delete osd model" << std::endl;
	delete _model;
	
	SAFE_RELEASE(_inputLayout);

	SAFE_RELEASE(m_ptexNeighDataBUF);
	SAFE_RELEASE(m_ptexNeighDataSRV);

	SAFE_RELEASE(m_extraordinaryInfoBUF);
    SAFE_RELEASE(m_extraordinaryInfoSRV);
	
    SAFE_RELEASE(m_extraordinaryDataBUF);
	SAFE_RELEASE(m_extraordinaryDataSRV);

	SAFE_RELEASE(m_cageEdgeVertices);
	//SAFE_RELEASE(m_cageEdgeVerticesSRV);

	m_ptexNeighDataCPU.clear();
	m_extraordinaryDataCPU.clear();
	m_extraordinaryInfoCPU.clear();
	
}
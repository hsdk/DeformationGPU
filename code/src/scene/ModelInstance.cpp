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
#include "stdafx.h"
#include "ModelInstance.h"
#include "DXModel.h"
#include "DXMaterial.h"
#include "DXSubDModel.h"
#include "MemoryManager.h"
#include "IntersectPatches.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"

#include "utils/DbgNew.h" // has to be last include


using namespace DirectX;

ModelGroup::ModelGroup()
{
	_physicsObject		 = NULL;
	_ghostObject		 = NULL;
	_modelMatrix		 = XMMatrixIdentity();
	_physicsScaleMatrix  = XMMatrixIdentity();
	_containsSubD		 = false;
	_containsDeformables = false;
	_name				 = "unnamed";
	_special			 = false;
	_physicsScalingFactor = 1.0f;
	_originialPhysicsScaling = btVector3(0,0,0);	
}

ModelGroup::~ModelGroup()
{
	for(auto instance : modelInstances) 	delete instance;	
	modelInstances.clear();
}


void ModelGroup::UpdateModelMatrix()
{
	if(_physicsObject)
	{		
		// variant 2: with interpolation
		btTransform trafo;
		btMotionState* ms=_physicsObject->getMotionState();		
		ms->getWorldTransform(trafo);

		// set models rendering trafo
		// variant 1: dx quaternion, translation
		btVector3 o = trafo.getOrigin();
		btQuaternion q = trafo.getRotation();
		_modelMatrix = _physicsScaleMatrix * XMMatrixRotationQuaternion(XMVectorSet(q.x(), q.y(), q.z(), q.w())) * XMMatrixTranslation(o.x(), o.y(), o.z()) ;
	}

	if(_ghostObject)
	{
		const btTransform& trafo = _ghostObject->getWorldTransform();
		const btVector3 &o = trafo.getOrigin();
		const btQuaternion &q = trafo.getRotation();  
		_modelMatrix = /*XMMatrixTranslation(0.0f, 0.0f, 2.0f)   * */ XMMatrixRotationZ(-float(M_PI)*0.5f) * XMMatrixRotationQuaternion(DirectX::XMVectorSet(q.x(), q.y(), q.z(), -q.w())) * XMMatrixTranslation(o.x(), o.y(), o.z()) ;
	}


	for(auto& instance : modelInstances)
	{
		instance->UpdateOBBWorld(_modelMatrix);
	}
}



ModelInstance::ModelInstance( DXModel* model, TriangleSubmesh* subMesh )
{
	m_globalInstanceID = 0;
	m_isSubD = false;
	m_isDeformable = false;
	m_isCollider = false;
	m_isPickable = true;
	m_isTool = false;
	m_triangleMesh = model;
	m_triangleSubmesh = subMesh;
	m_group = NULL;

	m_materialRef = subMesh->GetMaterial();

	m_obbWorld = DXObjectOrientedBoundingBox(subMesh->GetModelOBB());

	m_hasDynamicTileColor = false;
	m_hasDynamicTileDisplacement = false;
}

ModelInstance::ModelInstance( DXOSDMesh* osdMesh )
{
	m_globalInstanceID=0;
	m_isSubD = true;
	m_isDeformable = false;
	m_isCollider = false;
	m_isPickable = true;
	m_isTool = false;
	m_group = NULL;

	m_osdMesh = osdMesh;
	m_materialRef = osdMesh->GetMaterial();
	m_obbWorld = DXObjectOrientedBoundingBox(osdMesh->GetModelOBB());
	m_triangleMesh = NULL;
	m_triangleSubmesh = NULL;

	m_hasDynamicTileColor = false;
	m_hasDynamicTileDisplacement = false;
}

ModelInstance::~ModelInstance()
{
	Voxelizable::Destroy();
	Destroy();
}

void ModelInstance::UpdateOBBWorld( const DirectX::XMMATRIX& modelMatrix )
{
	m_modelMatrix = modelMatrix;

	if(IsSubD())
	{
		m_osdMesh->GetModelOBB().Transform(m_modelMatrix, m_obbWorld);
	}
	else
	{		
		m_triangleSubmesh->GetModelOBB().Transform(m_modelMatrix, m_obbWorld);
	}
	
}

HRESULT ModelInstance::EnableDynamicDisplacement()
{
	HRESULT hr = S_OK;
	auto pd3dDevice = DXUTGetD3D11Device();
	if(!m_hasDynamicTileDisplacement)
	{
		if(m_isSubD)
		{
			int numTiles = m_osdMesh->GetNumPTexFaces(); 		
			V_RETURN(g_memoryManager.CreateLocalTileInfo(pd3dDevice, numTiles, g_app.g_displacementTileSize, m_tileLayoutDisplacement.BUF, m_tileLayoutDisplacement.SRV, m_tileLayoutDisplacement.UAV));

			// create visibility/brush intersect buffer
			if(!m_visibility.BUF) 
			{
				std::cout << " create visibility buffer for " << m_osdMesh->GetName() << std::endl;
				V_RETURN(g_intersectGPU.CreateVisibilityBuffer(pd3dDevice, numTiles, m_visibility.BUF, m_visibility.SRV, m_visibility.UAV));
				V_RETURN(g_intersectGPU.CreateVisibilityBuffer(pd3dDevice, numTiles, m_visibilityAll.BUF, m_visibilityAll.SRV, m_visibilityAll.UAV));
				V_RETURN(g_intersectGPU.CreateCompactedVisibilityBuffer(pd3dDevice, numTiles, m_visibilityAppend.BUF, m_visibilityAppend.SRV, m_visibilityAppend.UAV));
			}

			{
				std::vector<float> initMaxDisplacement(numTiles, 0);
				V_RETURN(DXCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, numTiles * sizeof(float), 0, D3D11_USAGE_DEFAULT, m_maxDisplacement.BUF, &initMaxDisplacement[0]));
								
				{
					D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
					ZeroMemory(&descSRV, sizeof(descSRV));					
					descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
					descSRV.Format = DXGI_FORMAT_R32_FLOAT;
					descSRV.Buffer.FirstElement = 0;
					descSRV.Buffer.NumElements = numTiles;
					V_RETURN(pd3dDevice->CreateShaderResourceView(m_maxDisplacement.BUF, &descSRV, &m_maxDisplacement.SRV));
				}

				{
					D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
					ZeroMemory(&descUAV, sizeof(descUAV));
					descUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
					descUAV.Format = DXGI_FORMAT_R32_UINT;
					descUAV.Buffer.FirstElement = 0;
					descUAV.Buffer.NumElements = numTiles;
					V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_maxDisplacement.BUF, &descUAV, &m_maxDisplacement.UAV));
				}
			}

			if(g_app.g_memDebugDoPrealloc )
			{			
				std::cerr << " ModelInstance::EnableDynamicDisplacement calls prealloc memory for displacement REMOVE ME once management is working" << std::endl;
				std::cerr << " ModelInstance::EnableDynamicDisplacement prealloc only a subset test" << std::endl;
				// debug: alloc only a subset 
				V_RETURN(g_memoryManager.PreallocateDisplacementTiles(DXUTGetD3D11DeviceContext(), numTiles, m_tileLayoutDisplacement.UAV));
			}

			m_hasDynamicTileDisplacement = true;

			#if 0 // debug: TODO copy layout buffer to cpu and display content
			V_RETURN(g_memoryManager.PrintTileInfo(DXUTGetD3D11DeviceContext(), numFaces, m_tileLayoutDisplacementBUF ));
			#endif		
			//exit(1);
		}				
	}

	return hr;
}

HRESULT ModelInstance::EnableDynamicColor()
{	
	HRESULT hr = S_OK;

	// create default layout, pages and uv offsets are created by memory manager
	if(!m_hasDynamicTileColor)
	{
		if(m_isSubD)
		{
			int numFaces = m_osdMesh->GetNumPTexFaces(); 		
			V_RETURN(g_memoryManager.CreateLocalTileInfo(DXUTGetD3D11Device(), numFaces, g_app.g_colorTileSize, m_tileLayoutColor.BUF, m_tileLayoutColor.SRV, m_tileLayoutColor.UAV));

			// create visibility/brush intersect buffer
			if(!m_visibility.BUF) 
			{
				std::cout << " create visibility buffer for " << m_osdMesh->GetName() << std::endl;
				V_RETURN(g_intersectGPU.CreateVisibilityBuffer(DXUTGetD3D11Device(), numFaces, m_visibility.BUF, m_visibility.SRV, m_visibility.UAV));
				V_RETURN(g_intersectGPU.CreateVisibilityBuffer(DXUTGetD3D11Device(), numFaces, m_visibilityAll.BUF, m_visibilityAll.SRV, m_visibilityAll.UAV));
				V_RETURN(g_intersectGPU.CreateCompactedVisibilityBuffer(DXUTGetD3D11Device(), numFaces, m_visibilityAppend.BUF, m_visibilityAppend.SRV, m_visibilityAppend.UAV));
			}

			if(g_app.g_memDebugDoPrealloc )
			{			
				std::cerr << " ModelInstance::EnableDynamicColor calls prealloc memory for color REMOVE ME once management is working" << std::endl;
				std::cerr << " ModelInstance::EnableDynamicColor prealloc only a subset test" << std::endl;
				// debug: alloc only a subset 
				V_RETURN(g_memoryManager.PreallocateColorTiles(DXUTGetD3D11DeviceContext(), numFaces, m_tileLayoutColor.UAV));
			}

			m_hasDynamicTileColor = true;

			#if 0 // debug: TODO copy layout buffer to cpu and display content
			V_RETURN(g_memoryManager.PrintTileInfo(DXUTGetD3D11DeviceContext(), numFaces, m_tileLayoutColorBUF ));
			#endif		
			//exit(1);
		}	
	}

	return hr;

}

void ModelInstance::Destroy()
{
	m_tileLayoutDisplacement.Destroy();
	m_tileLayoutColor.Destroy();
	m_visibility.Destroy();
	m_visibilityAll.Destroy();
	m_visibilityAppend.Destroy();
	m_maxDisplacement.Destroy();
}


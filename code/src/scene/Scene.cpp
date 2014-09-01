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
#include "Scene.h"
#include "DXModel.h"
#include "DXSubDModel.h"
#include "ModelInstance.h"

#include "dynamics/AnimationGroup.h"
#include "dynamics/SkinningAnimation.h"
#include "rendering/RendererTri.h"
#include "rendering/RendererSubD.h"

#include "utils/DbgNew.h" // has to be last include


using namespace DirectX;

Scene::Scene()
{
	m_sceneAABBMin =  XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
	m_sceneAABBMax =  XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
}

Scene::~Scene()
{
	std::cerr << "Scene Destructor" << std::endl;
	Destroy();
}

void Scene::Destroy()
{
	for(auto m : _models)		{ SAFE_DELETE(m);	}
	for(auto m : _osdmodels)	{ SAFE_DELETE(m);	}
	for(auto g : _modelGroups)	{ SAFE_DELETE(g);		}
	for(auto m : _skinningAnimationManagers) {	SAFE_DELETE(m);	}

	_skinningAnimationManagers.clear();
	_models.clear();	
	_modelGroups.clear();
	_skinningAnimationManagers.clear();
}

HRESULT Scene::AddModel( DXModel* model )
{
	HRESULT hr = S_OK;
	_models.push_back(model);
	return hr;
}


HRESULT Scene::AddModel( DXOSDMesh* model )
{
	HRESULT hr = S_OK;
	_osdmodels.push_back(model);
	return hr;
}

HRESULT Scene::AddGroup( ModelGroup* model )
{
	HRESULT hr = S_OK;
	_modelGroups.push_back(model);
	return hr;
}

HRESULT Scene::AddGroup( AnimationGroup* group )
{
	HRESULT hr = S_OK;

	if(group == NULL) return S_FALSE;

	group->GetAnimationManager()->setupBuffers();
	//_skinningAnimationManagers.push_back(group->m_skinningMgr);
	_modelGroups.push_back(group);
	_animGroups.push_back(group);

	return S_OK;
}

const void Scene::UpdateAABB(bool updateEachFrame) 
{		
	static bool firstRun = true;
	if(firstRun || updateEachFrame)
	{
		firstRun = false;
	
		XMVECTOR b_min = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
		XMVECTOR b_max = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX);
		for(auto m : _modelGroups)
		{
			for( auto instance : m->modelInstances)
			{
				XMFLOAT3 points[8];
				instance->GetOBBWorld().GetCornerPoints(points);
				for(int i = 0; i < 8; ++i)
				{
					XMVECTOR corner = XMLoadFloat3(&points[i]);
					b_min = XMVectorMin(b_min, corner);
					b_max = XMVectorMax(b_max, corner);
				}
			}
		}
			
		XMStoreFloat3(&m_sceneAABBMin, b_min);
		XMStoreFloat3(&m_sceneAABBMax, b_max);
	}
}

HRESULT Scene::RenderSceneObjects( ID3D11DeviceContext1* pd3dImmediateContext, const RenderTri* triRenderer, RendererSubD* osdRenderer ) const
{
	HRESULT hr = S_OK;
	
	for (const auto* group : GetModelGroups()) 	
	{	
		g_app.UpdateCameraCBForModel(pd3dImmediateContext, group->GetModelMatrix());
		g_app.SetCameraConstantBuffersAllStages(pd3dImmediateContext);
				
		for(auto* instance : group->modelInstances)
		{
			if(instance->IsSubD())
			{
				V_RETURN(osdRenderer->FrameRender (pd3dImmediateContext, instance));
			}
			else
			{			
				V_RETURN(triRenderer->FrameRender (pd3dImmediateContext, instance));
			}
		}
	}
	return hr;
}

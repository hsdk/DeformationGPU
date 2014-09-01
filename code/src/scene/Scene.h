#pragma once

#include "dynamics/Physics.h"
#include "DXMaterialCache.h"
#include <SDX/DXTextureCache.h>
#include <vector>

// fwd decls
struct DXModel;
class MovableObject;
class ModelGroup;
class AnimationGroup;
class RenderTri;
class DXOSDMesh;
class RendererSubD;
class ModelInstance;

namespace SkinningAnimationClasses {
class SkinningMeshAnimationManager;
}
class Scene{
public:
	Scene();
	~Scene();

	void Destroy();	

	HRESULT AddModel(DXModel* model);
	HRESULT AddModel(DXOSDMesh* model);

	HRESULT AddGroup(ModelGroup* group);
	HRESULT AddGroup(AnimationGroup* group);

	void AddStadiumCamPosition(DirectX::XMFLOAT3 camPos)
	{
		m_stadiumCamPositions.push_back(camPos);
	}

	const std::vector<DirectX::XMFLOAT3>& GetStadiumCamPositions() const { return m_stadiumCamPositions;}

	void SetPhysics(Physics* physicsHandler) 
	{ 
		_physics = physicsHandler;
	}

	const Physics* GetPhysics() const
	{
		return _physics;
	}

	const std::vector<ModelGroup*>& GetModelGroups() const
	{
		return _modelGroups;
	}

	const std::vector<DXModel*>& GetModels() const
	{
		return _models;
	}

	const std::vector<DXOSDMesh*>& GetOSDModels() const
	{
		return _osdmodels;
	}

	const DXMaterialCache& GetMaterials() const {
		return _materials;
	}

	DXMaterialCache& GetMaterials(){
		return _materials;
	}

	const DXTextureCache& GetTextures() const {
		return _textures;
	}

	DXTextureCache& GetTextures(){
		return _textures;
	}

	std::vector<ModelInstance*>& GetGlobalIDToInstanceMap() {
		return _globalIDToInstanceMap;
	}

	std::vector<SkinningAnimationClasses::SkinningMeshAnimationManager*> GetSkinningAnimationManagers() {
		return _skinningAnimationManagers;
	}

	const std::vector<AnimationGroup*>& GetAnimatedModels() const
	{
		return _animGroups;
	}

	const void UpdateAABB(bool updateEachFrame);
	const DirectX::XMFLOAT3& GetSceneAABBMin() const { return m_sceneAABBMin;}
	const DirectX::XMFLOAT3& GetSceneAABBMax() const { return m_sceneAABBMax;}

	HRESULT RenderSceneObjects(ID3D11DeviceContext1* pd3dImmediateContext, const RenderTri* triRenderer, RendererSubD* osdRenderer = NULL) const;

private:
	std::vector<SkinningAnimationClasses::SkinningMeshAnimationManager*> _skinningAnimationManagers;
	std::vector<DXModel*>			_models;		// scene models	
	std::vector<DXOSDMesh*>			_osdmodels;		// opensubdiv models
	std::vector<ModelGroup*>		_modelGroups;	// scene models
	std::vector<AnimationGroup*>	_animGroups;	// groups with animated models
	DXTextureCache					_textures;		// reuse already loaded textures
	DXMaterialCache					_materials;		// reuse materials
	Physics*						_physics;		// optional physics component

	std::vector<ModelInstance*>		_globalIDToInstanceMap; // for picking

	DirectX::XMFLOAT3 m_sceneAABBMin;
	DirectX::XMFLOAT3 m_sceneAABBMax;

	std::vector<DirectX::XMFLOAT3> m_stadiumCamPositions;
};
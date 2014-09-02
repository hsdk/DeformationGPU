#pragma once

#include "App.h"
#include "Voxelization.h"
#include <SDX/DXBuffer.h>

// fwd decls
struct DXModel;
class  TriangleSubmesh;
class  btPairCachingGhostObject;
class  DXOSDMesh;
struct DXMaterial;

class ModelGroup;

class ModelInstance : public Voxelizable
{
public:
	ModelInstance(DXOSDMesh*    osdMesh);
	ModelInstance(DXModel* model, TriangleSubmesh* subMesh);

	virtual ~ModelInstance();

	void UpdateOBBWorld(const DirectX::XMMATRIX& modelMatrix);
	bool IsSubD()				const { return m_isSubD; }
	bool IsDeformable()			const { return m_isDeformable; }
	void SetIsDeformable(bool b)	  { m_isDeformable = b; }
	bool IsPickable()			const { return m_isPickable; }
	void SetIsPickable(bool b)		  { m_isPickable = b; }

	bool IsCollider()			const { return m_isCollider; }
	void SetIsCollider(bool b)		  { m_isCollider = b; }

	bool IsTool()				const { return m_isTool; }
	void SetIsTool(bool b)			  { m_isTool = b; }

	// for memory management
	HRESULT EnableDynamicDisplacement();		// create tile info/layout buffer
	HRESULT EnableDynamicColor();				// create tile info/layout buffer

	bool GetHasDynamicDisplacement() { return m_hasDynamicTileDisplacement; }
	bool GetHasDynamicColor()		 { return m_hasDynamicTileColor; }

	DirectX::DXBufferSRVUAV*	GetDisplacementTileLayout() { return &m_tileLayoutDisplacement; }
	
	DirectX::DXBufferSRVUAV*	GetColorTileLayout() { return &m_tileLayoutColor; }
	
	DirectX::DXBufferSRVUAV*	GetVisibility() { return &m_visibility; }
	
	DirectX::DXBufferSRVUAV*	GetVisibilityAll() { return &m_visibilityAll; }
	
	DirectX::DXBufferSRVUAV*  GetCompactedVisibility(){ return &m_visibilityAppend; }


	DirectX::DXBufferSRVUAV* GetMaxDisplacement() { return &m_maxDisplacement; }

	__forceinline void SetGroup(ModelGroup* group) { m_group = group; }
	__forceinline ModelGroup* GetGroup() { return m_group; }

	void Destroy();

	void SetGlobalInstanceID(UINT globalID)
	{
		m_globalInstanceID = globalID;
	}

	UINT GetGlobalInstanceID() const {return m_globalInstanceID; }
	// mesh access
	DXOSDMesh*	     GetOSDMesh()			const { return m_osdMesh;			}
	DXModel*		 GetTriangleMesh()		const { return m_triangleMesh;		}
	TriangleSubmesh* GetTriangleSubMesh()	const { return m_triangleSubmesh;	}
	DXMaterial*		 GetMaterial()			const { return m_materialRef;		}

	const DirectX::XMMATRIX& GetModelMatrix()			const		{ return m_modelMatrix;	}

	const DXObjectOrientedBoundingBox& GetOBBWorld() const		{ return m_obbWorld;	}
	DXObjectOrientedBoundingBox& GetOBBWorld()					{ return m_obbWorld;	}

protected:
	ModelInstance();	
	DXOSDMesh*						m_osdMesh;
	DXModel*						m_triangleMesh;
	TriangleSubmesh*				m_triangleSubmesh;
	DXMaterial*						m_materialRef;

	DXObjectOrientedBoundingBox		m_obbWorld;
	DirectX::XMMATRIX				m_modelMatrix;

	// ptex/tile based for dynamic memory management
	DirectX::DXBufferSRVUAV		m_tileLayoutDisplacement;
	
	DirectX::DXBufferSRVUAV		m_tileLayoutColor;
	
	// for storing intersection with brush/ voxelization (culling)
	DirectX::DXBufferSRVUAV		m_visibility;
	
	DirectX::DXBufferSRVUAV		m_visibilityAll;
	
	DirectX::DXBufferSRVUAV		m_visibilityAppend;
	
	DirectX::DXBufferSRVUAV		m_maxDisplacement;	
	
	ModelGroup* m_group;

	bool m_isTool;
	bool m_isSubD;
	bool m_isDeformable;
	bool m_isCollider;
	bool m_isPickable;
	
	bool m_hasDynamicTileColor;
	bool m_hasDynamicTileDisplacement;

	UINT m_globalInstanceID;
};

class ModelGroup
{
public:
	ModelGroup();
	virtual ~ModelGroup();
	
	void UpdateModelMatrix();



	__forceinline void SetPhysicsObject(btRigidBody* physicsObject)
	{
		_physicsObject = physicsObject;
		if (_physicsObject)
			_originialPhysicsScaling = _physicsObject->getCollisionShape()->getLocalScaling();
	}
	void SetGhostObject(btPairCachingGhostObject* physicsObject) 			{	_ghostObject   = physicsObject;	}

	void SetModelMatrix			(const DirectX::XMMATRIX& mModel)			{	_modelMatrix = mModel;			}	
	void SetPhysicsScaleMatrix	(const DirectX::XMMATRIX& mScale)			{	_physicsScaleMatrix = mScale;	}	

	
		  DirectX::XMMATRIX& GetModelMatrix()				{	return _modelMatrix;			}
	const DirectX::XMMATRIX& GetModelMatrix()	const		{	return _modelMatrix;			}
	const btRigidBody*	   GetPhysicsObject()	const		{	return _physicsObject;			}
	btRigidBody*	   GetPhysicsObject()					{	return _physicsObject;			}
	void ResetPhysicsObject()								{	_physicsObject = NULL;			}

	__forceinline bool HasDeformables() {return _containsDeformables; }
	__forceinline void SetHasDeformables(bool isDeformable) { _containsDeformables = isDeformable; }

	__forceinline bool GetHasSubD()				{ return _containsSubD; }
	__forceinline void SetHasSubD(bool isSubD)	{ _containsSubD = isSubD; }

	__forceinline const std::string& GetName() { return _name; };
	__forceinline void SetName(const std::string& name ) { _name = name; };
	
	__forceinline void SetPhysicsScalingFactor(float f)
	{
		_physicsScalingFactor = f;
		if (_physicsObject)
			_physicsObject->getCollisionShape()->setLocalScaling(_originialPhysicsScaling * _physicsScalingFactor);
	}

	__forceinline float GetPhysicsScalingFactor() const { return _physicsScalingFactor; }


	void addTriangleMesh(DXModel* mesh);

	bool IsSpecial(){return _special;} // enable for car or other special physics objects

	std::vector<DXModel*>		triModels;
	std::vector<DXOSDMesh*>		osdModels;

	std::vector<ModelInstance*>	modelInstances;

	bool _containsDeformables;
	bool _containsSubD;
	std::string _name;
	bool _special;
	
	btVector3 _originialPhysicsScaling;
	float _physicsScalingFactor;
	

	btRigidBody*				_physicsObject;
	btPairCachingGhostObject*	_ghostObject;
	DirectX::XMMATRIX			_modelMatrix;
	DirectX::XMMATRIX			_physicsScaleMatrix;
};


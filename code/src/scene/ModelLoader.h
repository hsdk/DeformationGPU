#pragma once

// switch between maddi subdiv and opensubdiv
#define USE_OPENSUBDIV

#include <DXUT.h>
#include <string>
#include <vector>

// fwd decls
class Scene;
struct DXMaterial;
class AnimationGroup;
class ModelGroup;

namespace SkinningAnimationClasses
{
	class SkinningMeshAnimationManager;
}


struct SubMeshData
{
	SubMeshData()
	{
		baseVertex = 0;
		numVertices = 0;
		material = NULL;
	}

	UINT baseVertex;
	UINT numVertices;
	std::vector<DirectX::XMUINT3> indicesTri;
	std::vector<DirectX::XMUINT4> indicesQuad;
	DXMaterial* material;
	std::string name;
};

struct MeshData
{
	MeshData()
	{
		numVertices = 0;
		//skinning = NULL;
		withSkinning = false;
		name = "";
	}

	UINT numVertices;		
	std::vector<DirectX::XMFLOAT4A> vertices;
	std::vector<DirectX::XMFLOAT3A> normals;
	std::vector<DirectX::XMFLOAT2A> texcoords;
	std::vector<SubMeshData> meshes;
	//SkinningAnimationClasses::SkinningMeshAnimationManager* skinning;
	bool withSkinning;
	std::string name;
};

class ModelLoader
{
public:
	static HRESULT Load(std::string fileName, Scene* scene, AnimationGroup* animGroup = NULL, ModelGroup* loadToGroup = NULL);
};
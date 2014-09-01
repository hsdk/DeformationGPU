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

//! Class to apply skinning animation to control mesh

#pragma once

#define SKINNING_NUM_MAX_VERTICES (1024*1024)

#include <map>
#include <DXUT.h>
#include <SDX/DXShaderManager.h>

// fwd decls
class ControlMesh;
struct DXModel;
class AnimationGroup;

namespace SkinningAnimationClasses {

#define INDEX_NOT_FOUND ((UINT)-1)
class Node {
public:
	Node(std::string nodeName);

	Node(const Node& node);

	~Node();

	const std::string&	getNodeName() const {	return m_nodeName;	}

	void setBoneID(UINT boneID) {		m_boneID = boneID;	}
	const int			getBoneID()   const {	return m_boneID;	}

	void setAnimationAndNodeAnimationIDs(UINT animationID, UINT nodeAnimationID);

	void setAnimationID(UINT animationID)			{	m_animationID = animationID;	}
	void setNodeAnimationID(UINT nodeAnimationID)	{	m_nodeAnimationID = nodeAnimationID;	}

	UINT getAnimationID()		const {	return m_animationID;		}
	UINT getNodeAnimationID()	const {	return m_nodeAnimationID;	}


	Node*& addChild(Node* child);
	
	const std::vector<Node*>& getChildren()	const {	return m_children;		}
	const Node& getChild(UINT i)				const {	return *m_children[i];	}

	UINT getNumChildren()						const {	return m_numChildren;	}

	void					 setTransformation(const DirectX::XMMATRIX& transformation)		  {	m_transformation = transformation;	}
	const DirectX::XMMATRIX& getTransformation()										const {	return m_transformation;			}

protected:
	std::string m_nodeName;
	UINT m_boneID;
	bool m_hasBone;
	UINT m_numChildren;

	UINT m_animationID;
	UINT m_nodeAnimationID;

	DirectX::XMMATRIX m_transformation;

	std::vector<Node*> m_children;
};


using namespace DirectX;
class AnimationKey 
{
public:
	AnimationKey() : m_time(0.0f) {}
	AnimationKey(const float& time) : m_time(time) {}

	void SetTime(const float& time) {	m_time = time;	}
	const float& GetTime() const	{	return m_time;	}

protected:
	float m_time;
};

class RotationKey : public AnimationKey 
{
public:
	RotationKey() 
		: AnimationKey() , m_rotation(DirectX::XMVECTOR()) {}

	RotationKey(const float& time, const XMVECTOR& rotation) 
		: AnimationKey(time) , m_rotation(rotation) {}

	void SetRotation(const XMVECTOR& rotation) {	m_rotation = rotation;	}
	const XMVECTOR& GetRotation() const		{	return m_rotation;	}

protected:
	XMVECTOR m_rotation;
};


class TranslationKey  : public AnimationKey  
{
public:

	TranslationKey() 
		: AnimationKey(), m_translation(XMVECTOR()) {}

	TranslationKey(const float& time, const XMVECTOR& translation) 
		: AnimationKey(time), m_translation(translation) {}

	void			 SetTranslation(const XMVECTOR& translation)			{	m_translation = translation;	}
	const XMVECTOR& GetTranslation()								const	{	return m_translation;			}

protected:
	XMVECTOR m_translation;
};

class ScalingKey  : public AnimationKey 
{
public:
	ScalingKey() 
		: AnimationKey(), m_scaling(XMVECTOR()) {}

	ScalingKey(const float& time, const XMVECTOR& scaling)
		: AnimationKey(time) , m_scaling(scaling) {}

	void			 SetScaling(const XMVECTOR& scaling)			{	m_scaling = scaling;	}
	const XMVECTOR& GetScaling()							const	{	return m_scaling;		}

protected:
	XMVECTOR m_scaling;
};

class NodeAnimation {
public:	

	NodeAnimation() : m_nodeName("") {}
	NodeAnimation(const std::string& nodeName) : m_nodeName(nodeName) {}

	void				SetNodeName(const std::string& nodeName)		{		m_nodeName = nodeName;	}
	const std::string&	GetNodeName()							const	{		return m_nodeName;		}

	void AddRotation	(const RotationKey& rotation) 		{	m_rotations.push_back(rotation);		}
	void AddTranslation	(const TranslationKey& translation) {	m_translations.push_back(translation);	}
	void AddScaling		(const ScalingKey& scaling) 		{	m_scalings.push_back(scaling);			}

	size_t GetNumRotations()	const {	return m_rotations.size();		}
	size_t GetNumTranslations()	const {	return m_translations.size();	}
	size_t GetNumScalings()		const {	return m_scalings.size();		}

	const std::vector<RotationKey>&		GetRotations()	  const {	return m_rotations;		}
	const std::vector<TranslationKey>&	GetTranslations() const {	return m_translations;	}
	const std::vector<ScalingKey>&		GetScalings()	  const {	return m_scalings;		}

protected:
	std::string m_nodeName;
	const Node* m_node;

	std::vector<RotationKey>	m_rotations;
	std::vector<TranslationKey> m_translations;
	std::vector<ScalingKey>		m_scalings;
};

class Animation 
{
public:	
	Animation();
	~Animation();

	void  setTicksPerSecond(float ticksPerSecond);
	float getTicksPerSecond() const;

	void  setDuration(float duration);	
	float getDuration() const;

	void setNumChannels(UINT numChannels);
	UINT getNumChannels();

	const NodeAnimation* getChannel(UINT i) const;
	NodeAnimation*& getChannelRef(UINT i);

protected:
	float m_ticksPerSecond;
	float m_duration;
	UINT  m_numChannels;
	std::vector<NodeAnimation*> m_channels;
};

#define NUM_MAX_BONES_PER_VERTEX 4

struct VertexBoneData {
	VertexBoneData();
	void setBoneData(int id, float weight);

	UINT boneIDs[NUM_MAX_BONES_PER_VERTEX];
	float boneWeights[NUM_MAX_BONES_PER_VERTEX];
};

class SkinningMeshAnimationManager 
{
public:
	SkinningMeshAnimationManager();
	~SkinningMeshAnimationManager();

	const std::vector<Node*>&			GetRootNodesRef()	const			{	return m_roots;					}
	std::vector<Node*>&					GetRootNodesRef()					{	return m_roots;					}
	const std::vector<Animation*>&		GetAnimationsRef()		const		{	return m_animations;			}
	std::vector<Animation*>&			GetAnimationsRef()					{	return m_animations;			}

	const std::map<std::string, UINT>&	GetBoneMappingRef() 		const	{	return m_boneMapping;			}
	std::map<std::string, UINT>&		GetBoneMappingRef() 				{	return m_boneMapping;			}
	const std::vector<XMMATRIX>&		boneOffsetsRef()			const	{	return m_boneOffsets;			}
	std::vector<XMMATRIX>&				boneOffsetsRef()					{	return m_boneOffsets;			}
	const std::vector<XMMATRIX>&		GetBoneTransformationsRef()	const	{	return m_boneTransformations;	}
	std::vector<XMMATRIX>&				GetBoneTransformationsRef()			{	return m_boneTransformations;	}
	const std::vector<VertexBoneData>&	GetBoneDataRef()			const	{	return m_boneData;				}
	std::vector<VertexBoneData>&		GetBoneDataRef()					{	return m_boneData;				}
	const std::vector<UINT>&			GetBaseVertexIndicesRef()	const	{	return m_baseVertexIndices;		}
	std::vector<UINT>&					GetBaseVertexIndicesRef()			{	return m_baseVertexIndices;		}
	const XMMATRIX&						globalInverseTransformationRef()const{	return m_globalInverseTransformation;	}
	XMMATRIX&							globalInverseTransformationRef()	{	return m_globalInverseTransformation;	}
	const std::vector<XMFLOAT4A>&		GetBoneWeightsRef()				const{	return m_boneWeights;			}
	std::vector<XMFLOAT4A>&				GetBoneWeightsRef()					{	return m_boneWeights;			}
	const std::vector<XMUINT4>&			GetBoneIDsRef()					const{	return m_boneIDs;				}
	std::vector<XMUINT4>&				GetBoneIDsRef()						{	return m_boneIDs;				}




	UINT addBone(std::string boneName, const XMMATRIX& boneOffset) 
	{
		UINT boneID = INDEX_NOT_FOUND;
		std::map<std::string, UINT>::const_iterator iterator = m_boneMapping.find(boneName);
		if (iterator == m_boneMapping.end()) 
		{
			boneID = m_numBones++;
			// Store the bone
			m_boneMapping[boneName] = boneID;
			m_boneOffsets.push_back(boneOffset);
		} else {
			// The bone has already been processed - fetch the index
			boneID = iterator->second;
		}
		return boneID;	
	}

	void printBones() {
		if (m_numBones == 0) return;
		for (auto it = m_boneMapping.begin(); it != m_boneMapping.end(); ++it)
			fprintf(stderr, "BONE[%d]: %s\n", it->second, it->first.c_str());
	}

	UINT findNodeAnimationID(UINT animationID, const Node& node)			const;
	UINT findBoneID			(const Node& node)								const;

	UINT getRotationID		(const NodeAnimation& nodeAnimation, float t)	const;
	UINT getTranslationID	(const NodeAnimation& nodeAnimation, float t)	const;
	UINT getScalingID		(const NodeAnimation& nodeAnimation, float t)	const;

	XMVECTOR slerpRotation(const NodeAnimation& nodeAnimation, float t)	const;
	XMVECTOR lerpTranslation(const NodeAnimation& nodeAnimation, float t)	const;
	XMVECTOR lerpScaling(const NodeAnimation& nodeAnimation, float t)		const;

	void traverseAndAnimateNodeHierachy(const Node& node, std::vector<XMMATRIX>& boneTransformations, const XMMATRIX& parentTransformation, float t) const;
	void computeAnimation(int animationID, float t);

	HRESULT setupBuffers();

public:
	ID3D11Buffer*					g_pBoneIDBuffer;
	ID3D11ShaderResourceView*		g_pBoneIDBufferSRV;

	ID3D11Buffer*					g_pBoneWeightBuffer;
	ID3D11ShaderResourceView*		g_pBoneWeightBufferSRV;
private:
	UINT m_numBones;
	std::vector<VertexBoneData> m_boneData;
	std::vector<XMFLOAT4A>		m_boneWeights;
	std::vector<XMUINT4>		m_boneIDs;
	std::map<std::string, UINT> m_boneMapping;
	std::vector<XMMATRIX>		m_boneOffsets;
	std::vector<XMMATRIX>		m_boneTransformations;
	XMMATRIX					m_globalInverseTransformation;

	std::vector<UINT>			m_baseVertexIndices;
	
	// Per animation data
	// Node structure
	std::vector<Node*> m_roots;
	// Animation channels
	std::vector<Animation*> m_animations;
};

} // SkinningAnimationClasses

class SkinningAnimation
{
public:
	SkinningAnimation();
	~SkinningAnimation();

	HRESULT Create(ID3D11Device1* pd3dDevice);
	void	Destroy();

	//HRESULT ApplySkinning(ID3D11DeviceContext1* pd3dImmediateContext, ControlMesh* mesh, float fTime);
	//HRESULT ComputeOBB(ID3D11DeviceContext1* pd3dImmediateContext, ControlMesh* mesh);

	HRESULT ApplySkinning(ID3D11DeviceContext1* pd3dImmediateContext, DXModel* mesh, float fTime);
	HRESULT ComputeOBB(ID3D11DeviceContext1* pd3dImmediateContext, DXModel* mesh);

	HRESULT ApplySkinning( ID3D11DeviceContext1* pd3dImmediateContext, AnimationGroup* group, float fTime);

protected:

	Shader<ID3D11ComputeShader>*	s_skinningCS;	

	Shader<ID3D11ComputeShader>*	s_centroidBlockCS;	
	Shader<ID3D11ComputeShader>*	s_centroidCS;	

	Shader<ID3D11ComputeShader>*	s_covarianceBlockCS;
	Shader<ID3D11ComputeShader>*	s_covarianceCS;

	Shader<ID3D11ComputeShader>*	s_minMaxBlockCS;	
	Shader<ID3D11ComputeShader>*	s_minMaxCS;	

	// Constant buffers
	ID3D11Buffer*					m_cbSkinning;

	// Temporary buffers for OBB stuff.
	ID3D11Buffer*					m_pCentroidTmpBuffer;
	ID3D11ShaderResourceView*		m_pCentroidTmpBufferSRV4Components;
	ID3D11UnorderedAccessView*		m_pCentroidTmpBufferUAV4Components;

	ID3D11Buffer*					m_pCentroidBuffer;
	ID3D11ShaderResourceView*		m_pCentroidBufferSRV4Components;
	ID3D11UnorderedAccessView*		m_pCentroidBufferUAV4Components;

	ID3D11Buffer*					m_pDTmpBuffer;
	ID3D11ShaderResourceView*		m_pDTmpSRV4Components;
	ID3D11UnorderedAccessView*		m_pDTmpUAV4Components;

	ID3D11Buffer*					m_pLTmpBuffer;
	ID3D11ShaderResourceView*		m_pLTmpSRV4Components;
	ID3D11UnorderedAccessView*		m_pLTmpUAV4Components;

	ID3D11Buffer*					m_pONBBuffer;
	ID3D11ShaderResourceView*		m_pONBSRV4Components;
	ID3D11UnorderedAccessView*		m_pONBUAV4Components;

	ID3D11Buffer*					m_pMinTmpBuffer;
	ID3D11ShaderResourceView*		m_pMinTmpSRV4Components;
	ID3D11UnorderedAccessView*		m_pMinTmpUAV4Components;

	ID3D11Buffer*					m_pMaxTmpBuffer;
	ID3D11ShaderResourceView*		m_pMaxTmpSRV4Components;
	ID3D11UnorderedAccessView*		m_pMaxTmpUAV4Components;	
};

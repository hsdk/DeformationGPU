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

#include "App.h"
// add constant buffer structs in app.h

#include "scene/DXModel.h"

#include "dynamics/SkinningAnimation.h"
#include "dynamics/AnimationGroup.h"

#include <SDX/StringConversion.h>

//Henry: has to be last header
#include "utils/DbgNew.h"


#define GET_THREAD_GROUP_COUNT(THREADS, BLOCKSIZE) (((THREADS)+(BLOCKSIZE)-1)/((BLOCKSIZE)))

using namespace DirectX;
namespace SkinningAnimationClasses {

Node::Node(std::string nodeName) : m_nodeName(nodeName), m_boneID(INDEX_NOT_FOUND), m_numChildren(0), m_animationID(INDEX_NOT_FOUND), m_nodeAnimationID(INDEX_NOT_FOUND) {
	m_children.clear();
}

Node::Node(const Node& node) {
	m_nodeName = node.m_nodeName;
	m_boneID = node.m_boneID;
	m_numChildren = 0;
	m_children.clear();
	m_animationID = INDEX_NOT_FOUND;
	m_nodeAnimationID = INDEX_NOT_FOUND;
}

Node::~Node() 
{
	for(auto child : m_children)	
		SAFE_DELETE(child);

	m_children.clear();
}



void Node::setAnimationAndNodeAnimationIDs(UINT animationID, UINT nodeAnimationID) 
{
	setAnimationID(animationID);
	setNodeAnimationID(nodeAnimationID);
}

Node*& Node::addChild(Node* child) 
{
	m_children.push_back(child);
	return m_children[m_numChildren++];
}


Animation::Animation() : m_numChannels(0) {}

Animation::~Animation() {
	for (UINT i = 0; i < m_numChannels; ++i)
		if (m_channels[i] != NULL) delete m_channels[i];
}

void Animation::setTicksPerSecond(float ticksPerSecond) {
	m_ticksPerSecond = ticksPerSecond;
}

float Animation::getTicksPerSecond() const {
	return m_ticksPerSecond;
}

void Animation::setDuration(float duration) {
	m_duration = duration;
}

float Animation::getDuration() const {
	return m_duration;
}

void Animation::setNumChannels(UINT numChannels) {
	m_channels.resize(m_numChannels = numChannels, NULL);
}

UINT Animation::getNumChannels() {
	return m_numChannels;
}

const NodeAnimation* Animation::getChannel(UINT i) const {
	if (i == INDEX_NOT_FOUND)
		return NULL;
	return m_channels[i];
}

NodeAnimation*& Animation::getChannelRef(UINT i) {
	return m_channels[i];
}

VertexBoneData::VertexBoneData() {
	for (UINT i = 0; i < NUM_MAX_BONES_PER_VERTEX; ++i) 
	{
		boneIDs[i] = INDEX_NOT_FOUND;
		boneWeights[i] = 0.0f;
	}
}

void VertexBoneData::setBoneData(int id, float weight) {
	for (UINT i = 0; i < NUM_MAX_BONES_PER_VERTEX; ++i) 
	{
		if (boneIDs[i] == id)	return;

		if (boneWeights[i] == 0.0f) 
		{
			boneIDs[i] = id;
			boneWeights[i] = weight;
			return;
		}
	}
	fprintf(stderr, "setBoneData(id = %d, weight = %f) failed", id, weight);
}

SkinningMeshAnimationManager::SkinningMeshAnimationManager() 
{
	m_numBones = 0;
	m_baseVertexIndices.clear();

	g_pBoneIDBuffer = NULL;
	g_pBoneIDBufferSRV = NULL;
	
	g_pBoneWeightBuffer = NULL;
	g_pBoneWeightBufferSRV = NULL;
}

SkinningMeshAnimationManager::~SkinningMeshAnimationManager() 
{
	static bool ONCE = true;

	if (ONCE) {
		ONCE = false; 
		SAFE_RELEASE(g_pBoneIDBuffer);
		SAFE_RELEASE(g_pBoneIDBufferSRV);

		SAFE_RELEASE(g_pBoneWeightBuffer);
		SAFE_RELEASE(g_pBoneWeightBufferSRV);
	}
	for (UINT i = 0; i < m_animations.size(); ++i)
		delete m_animations[i];
	for (UINT i = 0; i < m_roots.size(); ++i)
		delete m_roots[i];
}



UINT SkinningMeshAnimationManager::findNodeAnimationID(UINT animationID, const Node& node) const 
{
	Animation* animation = m_animations[animationID];
	for (UINT i = 0; i < animation->getNumChannels(); ++i) {
		const NodeAnimation* nodeAnimation = animation->getChannel(i);
		if (!node.getNodeName().compare(nodeAnimation->GetNodeName())) {
			return i;
		}
	}
	return INDEX_NOT_FOUND;
}
	
UINT SkinningMeshAnimationManager::findBoneID(const Node& node) const
{
	std::map<std::string, UINT>::const_iterator iterator = m_boneMapping.find(node.getNodeName());
	if (iterator == m_boneMapping.end())
		return INDEX_NOT_FOUND;
	return iterator->second;
}

UINT SkinningMeshAnimationManager::getRotationID(const NodeAnimation& nodeAnimation, float t) const 
{
	const std::vector<RotationKey>& rotations = nodeAnimation.GetRotations();
	for (UINT i = 0; i < nodeAnimation.GetNumRotations()-1; ++i)
		if (t >= (float)rotations[i].GetTime() && t < (float)rotations[i+1].GetTime())
			return i;
	
	//LogError("findRotationID() failed - t = %f", t);
	return 0;//nodeAnimation.getNumRotations()-1;
}

UINT SkinningMeshAnimationManager::getTranslationID(const NodeAnimation& nodeAnimation, float t) const 
{
	const std::vector<TranslationKey>& translations = nodeAnimation.GetTranslations();
	for (UINT i = 0; i < nodeAnimation.GetNumTranslations()-1; ++i)
		if (t >= (float)translations[i].GetTime() && t < (float)translations[i+1].GetTime())
			return i;
	
	//LogError("findTranslationID() failed - t = %f", t);
	return 0;//nodeAnimation.getNumTranslations()-1;
}

UINT SkinningMeshAnimationManager::getScalingID(const NodeAnimation& nodeAnimation, float t) const 
{
	const std::vector<ScalingKey>& scalings = nodeAnimation.GetScalings();
	for (UINT i = 0; i < nodeAnimation.GetNumScalings()-1; ++i)
		if (t >= (float)scalings[i].GetTime() && t < (float)scalings[i+1].GetTime())
			return i;
	
	//LogError("findScalingID() failed - t = %f", t);
	return 0;//nodeAnimation.getNumScalings()-1;
}

XMVECTOR SkinningMeshAnimationManager::slerpRotation(const NodeAnimation& nodeAnimation, float t) const 
{
	if (nodeAnimation.GetNumRotations() == 1) {
		return nodeAnimation.GetRotations()[0].GetRotation();
	}
	UINT i = getRotationID(nodeAnimation, t);

	const RotationKey& a = nodeAnimation.GetRotations()[i];
	const RotationKey& b = nodeAnimation.GetRotations()[(i+1) % nodeAnimation.GetNumRotations()];

	float delta = (float)fabs(b.GetTime() - a.GetTime());
	float x = (t - (float)a.GetTime()) / delta;

	//if (x < 0.0f || x > 1.0f) LogError("slerpRotation(): x < 0.0f || x > 1.0f - x = %f & t = %f", x, t);
	//return glm::normalize(glm::slerp(a.getRotation(), b.getRotation(), x));
	//XMFLOAT4A result;
	//XMStoreFloat4A(&result, XMQuaternionSlerp(XMLoadFloat4(&a.GetRotation()), XMLoadFloat4(&b.GetRotation()), x));
	//return result;

	return  XMQuaternionSlerp(a.GetRotation(), b.GetRotation(), x);
}


XMVECTOR SkinningMeshAnimationManager::lerpTranslation(const NodeAnimation& nodeAnimation, float t) const 
{
	if (nodeAnimation.GetNumTranslations() == 1) {
		return nodeAnimation.GetTranslations()[0].GetTranslation();
	}
	UINT i = getTranslationID(nodeAnimation, t);

	const TranslationKey& a = nodeAnimation.GetTranslations()[i];
	const TranslationKey& b = nodeAnimation.GetTranslations()[(i+1) % nodeAnimation.GetNumTranslations()];

	float delta = (float)fabs(b.GetTime() - a.GetTime());
	float x = (t - (float)a.GetTime()) / delta;

	//if (x < 0.0f || x > 1.0f) LogError("lerpTranslation(): x < 0.0f || x > 1.0f - x = %f & t = %f", x, t);
	//return glm::mix(a.getTranslation(), b.getTranslation(), x);
	//return XMFLOAT3A();
	//XMFLOAT3A result;
	//XMStoreFloat3A(&result, XMVectorLerp(XMLoadFloat3A(&a.GetTranslation()), XMLoadFloat3A(&b.GetTranslation()), x));
	//return result;

	return XMVectorLerp(a.GetTranslation(), b.GetTranslation(), x);

}

XMVECTOR SkinningMeshAnimationManager::lerpScaling(const NodeAnimation& nodeAnimation, float t) const 
{
	if (nodeAnimation.GetNumScalings() == 1) 
	{
		return nodeAnimation.GetScalings()[0].GetScaling();
	}
	UINT i = getTranslationID(nodeAnimation, t);

	const ScalingKey& a = nodeAnimation.GetScalings()[i];
	const ScalingKey& b = nodeAnimation.GetScalings()[(i+1) % nodeAnimation.GetNumScalings()];

	float delta = (float)fabs(b.GetTime() - a.GetTime());
	float x = (t - (float)a.GetTime()) / delta;

	//if (x < 0.0f || x > 1.0f) LogError("lerpScaling(): x < 0.0f || x > 1.0f - x = %f & t = %f", x, t);
	//return glm::mix(a.getScaling(), b.getScaling(), x);

	//XMFLOAT3A result;
	//XMStoreFloat3A(&result, XMVectorLerp(XMLoadFloat3A(&a.GetScaling()), XMLoadFloat3A(&b.GetScaling()), x));	
	//return result;

	return XMVectorLerp(a.GetScaling(), b.GetScaling(), x);
}

void SkinningMeshAnimationManager::traverseAndAnimateNodeHierachy(const Node& node, std::vector<XMMATRIX>& boneTransformations, const XMMATRIX& parentTransformation, float t) const
{
	const Animation* animation = m_animations[node.getAnimationID()];
	const NodeAnimation* nodeAnimation = animation->getChannel(node.getNodeAnimationID());

	XMMATRIX nodeTransformation;

	if (nodeAnimation) {
		XMVECTOR translation	= lerpTranslation(*nodeAnimation, t);
		XMVECTOR rotation		= slerpRotation(*nodeAnimation, t);
		XMVECTOR scaling		= lerpScaling(*nodeAnimation, t);
				
		XMMATRIX T = XMMatrixTranslationFromVector(translation);		
		XMMATRIX R = XMMatrixInverse(NULL, XMMatrixRotationQuaternion(rotation));
		XMMATRIX S = XMMatrixInverse(NULL, XMMatrixScalingFromVector(scaling));
		
		nodeTransformation = XMMatrixTranspose(T) * R * S ;

	} else {
		// No animation for the current node found
		nodeTransformation = node.getTransformation();
	}

	XMMATRIX globalTransformation = XMMatrixMultiply(parentTransformation, nodeTransformation);


	// Set the bone transformation matrix
	const int boneID = node.getBoneID();
	if (boneID != INDEX_NOT_FOUND) 
	{
		boneTransformations[boneID] = XMMatrixMultiply(m_globalInverseTransformation, XMMatrixMultiply(globalTransformation, m_boneOffsets[boneID]));
	}

	for (const auto& childNode : node.getChildren())
	{
		traverseAndAnimateNodeHierachy(*childNode, boneTransformations, globalTransformation, t);
	}
}

void SkinningMeshAnimationManager::computeAnimation(int animationID, float t) 
{
	if (m_animations.size() == 0) return;
	float ticksPerSecond = (float)m_animations[animationID]->getTicksPerSecond();
	ticksPerSecond <= 0.0f ? 25.0f : ticksPerSecond;

	float animationTime = fmod(t * ticksPerSecond, (float)m_animations[animationID]->getDuration());
	if (t * ticksPerSecond >= (float)m_animations[animationID]->getDuration())
	{
		//animationTime = (float)m_animations[animationID]->getDuration()-0.1f;
		//animationTime = (float)m_animations[animationID]->getDuration()-0.1f;
	}
	//traverseAndAnimateNodeHierachy(*m_roots[animationID], m_boneTransformations, XMFLOAT4X4A(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1), animationTime);
	//fprintf(stderr, "%f\n", animationTime);
	traverseAndAnimateNodeHierachy(*m_roots[animationID], m_boneTransformations, XMMatrixIdentity(), animationTime);

	////checkme using elapsed time or uncomment above
	//static double lastPlaying = 0.;
	//static double g_dCurrent =0;
	//std::cout << "\n\n" << t << std::endl;
	//
	//g_dCurrent += t / double(1000) - lastPlaying;
	//std::cout << "\n\n" << g_dCurrent << std::endl;

	//double tps = (float)m_animations[animationID]->getTicksPerSecond() ? (float)m_animations[animationID]->getTicksPerSecond() : 25.f;
	//double time = fmod(g_dCurrent, (float)m_animations[animationID]->getDuration()/tps);
	//traverseAndAnimateNodeHierachy(*m_roots[animationID], m_boneTransformations, XMMatrixIdentity(), time);
	//lastPlaying = g_dCurrent;
	//fprintf(stderr, "%f\n", t);
}

HRESULT SkinningMeshAnimationManager::setupBuffers() 
{
	m_boneWeights.clear();
	m_boneIDs.clear();

	if (m_numBones == 0) return 0;
		
	m_boneTransformations.resize(m_numBones);
		
	for (UINT i = 0; i < m_boneData.size(); ++i) {
		VertexBoneData& b = m_boneData[i];
		m_boneIDs.push_back(XMUINT4(&b.boneIDs[0]));
		m_boneWeights.push_back(XMFLOAT4A(&b.boneWeights[0]));
	}
	HRESULT hr;
	// weights & boneIDs (MAKE D3D11_USAGE_IMMUTABLE)
	V_RETURN(DXCreateBuffer(DXUTGetD3D11Device(),	 D3D11_BIND_SHADER_RESOURCE, (UINT)m_boneData.size() * sizeof(XMUINT4), 0,	D3D11_USAGE_DEFAULT, g_pBoneIDBuffer, &GetBoneIDsRef()[0],	0, sizeof(XMINT4)));
	DXUT_SetDebugName(g_pBoneIDBuffer,"g_pBoneIDBuffer2");

	V_RETURN(DXCreateBuffer(DXUTGetD3D11Device(),	 D3D11_BIND_SHADER_RESOURCE, (UINT)m_boneData.size() * sizeof(XMFLOAT4A), 0, D3D11_USAGE_DEFAULT,g_pBoneWeightBuffer, &GetBoneWeightsRef()[0], 0,	sizeof(XMFLOAT4A)));
	DXUT_SetDebugName(g_pBoneWeightBuffer,"g_pBoneWeightBuffer2");

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	ZeroMemory( &SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC) );
	SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SRVDesc.Buffer.FirstElement = 0;
	SRVDesc.Buffer.NumElements = (UINT)m_boneData.size();
	
	V_RETURN(DXUTGetD3D11Device()->CreateShaderResourceView( g_pBoneIDBuffer, &SRVDesc, &g_pBoneIDBufferSRV));	
	DXUT_SetDebugName(g_pBoneIDBufferSRV, "g_pBoneIDBufferSRV2");
	
	SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	V_RETURN(DXUTGetD3D11Device()->CreateShaderResourceView( g_pBoneWeightBuffer, &SRVDesc, &g_pBoneWeightBufferSRV));
	DXUT_SetDebugName(g_pBoneWeightBufferSRV, "g_pBoneWeightBufferSRV2");

	return hr;
}

}


SkinningAnimation::SkinningAnimation()
{
	m_cbSkinning = NULL;
	//m_cbOBB = NULL;

	m_pCentroidTmpBuffer = NULL;
	m_pCentroidTmpBufferSRV4Components = NULL;
	m_pCentroidTmpBufferUAV4Components = NULL;

	m_pCentroidBuffer = NULL;
	m_pCentroidBufferSRV4Components = NULL;
	m_pCentroidBufferUAV4Components = NULL;

	m_pDTmpBuffer = NULL;
	m_pDTmpSRV4Components = NULL;
	m_pDTmpUAV4Components = NULL;

	m_pLTmpBuffer = NULL;
	m_pLTmpSRV4Components = NULL;
	m_pLTmpUAV4Components = NULL;

	m_pONBBuffer = NULL;
	m_pONBSRV4Components = NULL;
	m_pONBUAV4Components = NULL;
	
}

SkinningAnimation::~SkinningAnimation()
{
	Destroy();
}

HRESULT SkinningAnimation::Create( ID3D11Device1* pd3dDevice )
{
	HRESULT hr = S_OK;
	ID3DBlob* pBlob = NULL;
	s_skinningCS		= g_shaderManager.AddComputeShader(L"shader/SkinningCS.hlsl", "SkinningCS", "cs_5_0", &pBlob); SAFE_RELEASE(pBlob);

	s_centroidBlockCS	= g_shaderManager.AddComputeShader(L"shader/SkinningCentroidBlockCS.hlsl",	 "SkinningCS", "cs_5_0", &pBlob); SAFE_RELEASE(pBlob);
	s_centroidCS		= g_shaderManager.AddComputeShader(L"shader/SkinningCentroidCS.hlsl",		 "SkinningCS", "cs_5_0", &pBlob); SAFE_RELEASE(pBlob);

	s_covarianceBlockCS = g_shaderManager.AddComputeShader(L"shader/SkinningCovarianceBlockCS.hlsl", "SkinningCS", "cs_5_0", &pBlob); SAFE_RELEASE(pBlob);
	s_covarianceCS		= g_shaderManager.AddComputeShader(L"shader/SkinningCovarianceCS.hlsl",		 "SkinningCS", "cs_5_0", &pBlob); SAFE_RELEASE(pBlob);

	s_minMaxBlockCS		= g_shaderManager.AddComputeShader(L"shader/SkinningMinMaxBlockCS.hlsl",	 "SkinningCS", "cs_5_0", &pBlob); SAFE_RELEASE(pBlob);
	s_minMaxCS			= g_shaderManager.AddComputeShader(L"shader/SkinningMinMaxCS.hlsl",			 "SkinningCS", "cs_5_0", &pBlob); SAFE_RELEASE(pBlob);

	V_RETURN(DXCreateBuffer( pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_SKINNING_AND_OBB), D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, m_cbSkinning));

	// Create temporary buffers for the scan operations
	{
		{ // Buffers for the centroid computation
			V_RETURN(DXCreateBuffer(DXUTGetD3D11Device(),	D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
									 SKINNING_NUM_MAX_VERTICES * sizeof(XMFLOAT4A), 0,	D3D11_USAGE_DEFAULT,
									 m_pCentroidTmpBuffer, NULL, 0, sizeof(XMFLOAT4A)));
			DXUT_SetDebugName(m_pCentroidTmpBuffer, "m_pCentroidTmpBuffer");

			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			ZeroMemory( &SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC) );
			SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			SRVDesc.Buffer.FirstElement = 0;
			SRVDesc.Buffer.NumElements = SKINNING_NUM_MAX_VERTICES;
			V_RETURN(pd3dDevice->CreateShaderResourceView(m_pCentroidTmpBuffer, &SRVDesc, &m_pCentroidTmpBufferSRV4Components));
			DXUT_SetDebugName(m_pCentroidTmpBufferSRV4Components, "m_pCentroidTmpBufferSRV4Components");
	
			D3D11_UNORDERED_ACCESS_VIEW_DESC DescUAV;
			ZeroMemory( &DescUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC) );
			DescUAV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			DescUAV.Buffer.FirstElement = 0;
			DescUAV.Buffer.NumElements = SKINNING_NUM_MAX_VERTICES;
			V_RETURN( pd3dDevice->CreateUnorderedAccessView(m_pCentroidTmpBuffer, &DescUAV, &m_pCentroidTmpBufferUAV4Components ));	
			DXUT_SetDebugName(m_pCentroidTmpBufferUAV4Components, "m_pCentroidTmpBufferUAV4Components");

			V_RETURN(DXCreateBuffer(DXUTGetD3D11Device(),	D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
									 8 * sizeof(XMFLOAT4A), 0,	D3D11_USAGE_DEFAULT,
									 m_pCentroidBuffer, NULL, 0, sizeof(XMFLOAT4A)));
			DXUT_SetDebugName(m_pCentroidBuffer, "m_pCentroidBuffer");

			ZeroMemory( &SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC) );
			SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			SRVDesc.Buffer.FirstElement = 0;
			SRVDesc.Buffer.NumElements = 8;
			V_RETURN(pd3dDevice->CreateShaderResourceView(m_pCentroidBuffer, &SRVDesc, &m_pCentroidBufferSRV4Components));
			DXUT_SetDebugName(m_pCentroidBufferSRV4Components, "m_pCentroidBufferSRV4Components");
	
			ZeroMemory( &DescUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC) );
			DescUAV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			DescUAV.Buffer.FirstElement = 0;
			DescUAV.Buffer.NumElements = 8;
			V_RETURN( pd3dDevice->CreateUnorderedAccessView(m_pCentroidBuffer, &DescUAV, &m_pCentroidBufferUAV4Components ));	
			DXUT_SetDebugName(m_pCentroidBufferUAV4Components, "m_pCentroidBufferUAV4Components");	
		}

		{ // Covariance Matrix buffers
			// DIAGONAL
			V_RETURN(DXCreateBuffer(DXUTGetD3D11Device(),	D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
									 SKINNING_NUM_MAX_VERTICES * sizeof(XMFLOAT4A), 0,	D3D11_USAGE_DEFAULT,
									 m_pDTmpBuffer, NULL, 0, sizeof(XMFLOAT4A)));
			DXUT_SetDebugName(m_pDTmpBuffer, "m_pDTmpBuffer");

			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			ZeroMemory( &SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC) );
			SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			SRVDesc.Buffer.FirstElement = 0;
			SRVDesc.Buffer.NumElements = SKINNING_NUM_MAX_VERTICES;
			V_RETURN(pd3dDevice->CreateShaderResourceView(m_pDTmpBuffer, &SRVDesc, &m_pDTmpSRV4Components));
			DXUT_SetDebugName(m_pDTmpSRV4Components, "m_pDTmpSRV4Components");
	
			D3D11_UNORDERED_ACCESS_VIEW_DESC DescUAV;
			ZeroMemory( &DescUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC) );
			DescUAV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			DescUAV.Buffer.FirstElement = 0;
			DescUAV.Buffer.NumElements = SKINNING_NUM_MAX_VERTICES;
			V_RETURN( pd3dDevice->CreateUnorderedAccessView(m_pDTmpBuffer, &DescUAV, &m_pDTmpUAV4Components ));	
			DXUT_SetDebugName(m_pDTmpUAV4Components, "m_pDTmpUAV4Components");

			// LOWER
			V_RETURN(DXCreateBuffer(DXUTGetD3D11Device(),	D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
									 SKINNING_NUM_MAX_VERTICES * sizeof(XMFLOAT4A), 0,	D3D11_USAGE_DEFAULT,
									 m_pLTmpBuffer, NULL, 0, sizeof(XMFLOAT4A)));
			DXUT_SetDebugName(m_pLTmpBuffer, "m_pLTmpBuffer");

			ZeroMemory( &SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC) );
			SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			SRVDesc.Buffer.FirstElement = 0;
			SRVDesc.Buffer.NumElements = SKINNING_NUM_MAX_VERTICES;
			V_RETURN(pd3dDevice->CreateShaderResourceView(m_pLTmpBuffer, &SRVDesc, &m_pLTmpSRV4Components));
			DXUT_SetDebugName(m_pLTmpSRV4Components, "m_pLTmpSRV4Components");
	
			ZeroMemory( &DescUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC) );
			DescUAV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			DescUAV.Buffer.FirstElement = 0;
			DescUAV.Buffer.NumElements = SKINNING_NUM_MAX_VERTICES;
			V_RETURN( pd3dDevice->CreateUnorderedAccessView(m_pLTmpBuffer, &DescUAV, &m_pLTmpUAV4Components ));	
			DXUT_SetDebugName(m_pLTmpUAV4Components, "m_pLTmpUAV4Components");

			// ONB
			V_RETURN(DXCreateBuffer(DXUTGetD3D11Device(),	D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
									 8 * sizeof(XMFLOAT4A), 0,	D3D11_USAGE_DEFAULT,
									 m_pONBBuffer, NULL, 0, sizeof(XMFLOAT4A)));
			DXUT_SetDebugName(m_pONBBuffer, "m_pONBBuffer");

			ZeroMemory( &SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC) );
			SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			SRVDesc.Buffer.FirstElement = 0;
			SRVDesc.Buffer.NumElements = 8;
			V_RETURN(pd3dDevice->CreateShaderResourceView(m_pONBBuffer, &SRVDesc, &m_pONBSRV4Components));
			DXUT_SetDebugName(m_pONBSRV4Components, "m_pONBSRV4Components");
	
			ZeroMemory( &DescUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC) );
			DescUAV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			DescUAV.Buffer.FirstElement = 0;
			DescUAV.Buffer.NumElements = 8;
			V_RETURN( pd3dDevice->CreateUnorderedAccessView(m_pONBBuffer, &DescUAV, &m_pONBUAV4Components ));	
			DXUT_SetDebugName(m_pONBUAV4Components, "m_pONBUAV4Components");
		}

		{ // Min & Max tmp buffers
			// Min
			V_RETURN(DXCreateBuffer(DXUTGetD3D11Device(),	D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
									 SKINNING_NUM_MAX_VERTICES * sizeof(XMFLOAT4A), 0,	D3D11_USAGE_DEFAULT,
									 m_pMinTmpBuffer, NULL, 0, sizeof(XMFLOAT4A)));
			DXUT_SetDebugName(m_pMinTmpBuffer, "m_pMinTmpBuffer");

			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			ZeroMemory( &SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC) );
			SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			SRVDesc.Buffer.FirstElement = 0;
			SRVDesc.Buffer.NumElements = SKINNING_NUM_MAX_VERTICES;
			V_RETURN(pd3dDevice->CreateShaderResourceView(m_pMinTmpBuffer, &SRVDesc, &m_pMinTmpSRV4Components));
			DXUT_SetDebugName(m_pMinTmpSRV4Components, "m_pMinTmpSRV4Components");
	
			D3D11_UNORDERED_ACCESS_VIEW_DESC DescUAV;
			ZeroMemory( &DescUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC) );
			DescUAV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			DescUAV.Buffer.FirstElement = 0;
			DescUAV.Buffer.NumElements = SKINNING_NUM_MAX_VERTICES;
			V_RETURN( pd3dDevice->CreateUnorderedAccessView(m_pMinTmpBuffer, &DescUAV, &m_pMinTmpUAV4Components ));	
			DXUT_SetDebugName(m_pMinTmpUAV4Components, "m_pMinTmpUAV4Components");

			// Max
			V_RETURN(DXCreateBuffer(DXUTGetD3D11Device(),	D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
									 SKINNING_NUM_MAX_VERTICES * sizeof(XMFLOAT4A), 0,	D3D11_USAGE_DEFAULT,
									 m_pMaxTmpBuffer, NULL, 0, sizeof(XMFLOAT4A)));
			DXUT_SetDebugName(m_pMaxTmpBuffer, "m_pMaxTmpBuffer");

			ZeroMemory( &SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC) );
			SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			SRVDesc.Buffer.FirstElement = 0;
			SRVDesc.Buffer.NumElements = SKINNING_NUM_MAX_VERTICES;
			V_RETURN(pd3dDevice->CreateShaderResourceView(m_pMaxTmpBuffer, &SRVDesc, &m_pMaxTmpSRV4Components));
			DXUT_SetDebugName(m_pMaxTmpSRV4Components, "m_pMaxTmpSRV4Components");
	
			ZeroMemory( &DescUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC) );
			DescUAV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			DescUAV.Buffer.FirstElement = 0;
			DescUAV.Buffer.NumElements = SKINNING_NUM_MAX_VERTICES;
			V_RETURN( pd3dDevice->CreateUnorderedAccessView(m_pMaxTmpBuffer, &DescUAV, &m_pMaxTmpUAV4Components ));	
			DXUT_SetDebugName(m_pMaxTmpUAV4Components, "m_pMaxTmpUAV4Components");
		}	
	}
	return hr;
}

void SkinningAnimation::Destroy()
{
	// cleanup here
	// shaders are destroyed by shader manager
	SAFE_RELEASE(m_cbSkinning);

	// cleanup the buffers
	SAFE_RELEASE(m_pCentroidTmpBufferUAV4Components)
	SAFE_RELEASE(m_pCentroidTmpBufferSRV4Components);
	SAFE_RELEASE(m_pCentroidTmpBuffer);

	SAFE_RELEASE(m_pCentroidBufferUAV4Components)
	SAFE_RELEASE(m_pCentroidBufferSRV4Components);
	SAFE_RELEASE(m_pCentroidBuffer);

	SAFE_RELEASE(m_pDTmpUAV4Components)
	SAFE_RELEASE(m_pDTmpSRV4Components);
	SAFE_RELEASE(m_pDTmpBuffer);

	SAFE_RELEASE(m_pLTmpUAV4Components)
	SAFE_RELEASE(m_pLTmpSRV4Components);
	SAFE_RELEASE(m_pLTmpBuffer);

	SAFE_RELEASE(m_pONBUAV4Components)
	SAFE_RELEASE(m_pONBSRV4Components);
	SAFE_RELEASE(m_pONBBuffer);

	SAFE_RELEASE(m_pMinTmpUAV4Components)
	SAFE_RELEASE(m_pMinTmpSRV4Components);
	SAFE_RELEASE(m_pMinTmpBuffer);

	SAFE_RELEASE(m_pMaxTmpUAV4Components)
	SAFE_RELEASE(m_pMaxTmpSRV4Components);
	SAFE_RELEASE(m_pMaxTmpBuffer);
}

//HRESULT SkinningAnimation::ApplySkinning( ID3D11DeviceContext1* pd3dImmediateContext, ControlMesh* mesh, float fTime )
//{
//	if (!mesh->IsSkinned()) return 0;
//
//	mesh->m_skinningMeshAnimationManager->computeAnimation(0, fTime);
//
//	UINT voffset = 0, stride = sizeof(XMFLOAT4A);
//	HRESULT hr = S_OK;
//	ID3D11Buffer* ppBuffer[] = { NULL, NULL, NULL };
//	pd3dImmediateContext->IASetVertexBuffers(0, 3, ppBuffer, &stride, &voffset);
//
//
//	ID3D11ShaderResourceView* ppSRVNULL[] =  { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
//	ID3D11UnorderedAccessView* ppUAVNULL[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
//	pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
//	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);
//
//	D3D11_MAPPED_SUBRESOURCE mappedBuf;
//	pd3dImmediateContext->Map(m_cbSkinning, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedBuf);
//	CB_SKINNING_AND_OBB* cbSkinning = reinterpret_cast<CB_SKINNING_AND_OBB*>(mappedBuf.pData);
//	cbSkinning->mNumOriginalVertices = mesh->m_NumControlMeshVertices;
//	for (UINT i = 0; i < mesh->m_skinningMeshAnimationManager->GetBoneTransformationsRef().size() && i < SKINNING_NUM_MAX_BONES; ++i)
//		cbSkinning->mBoneMatrices[i] =  mesh->m_skinningMeshAnimationManager->GetBoneTransformationsRef()[i]; //XMFLOAT4X4A(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1) ;//
//
//	pd3dImmediateContext->Unmap(m_cbSkinning, 0);
//	
//	ID3D11Buffer* constantBuffers[] = { m_cbSkinning };
//	pd3dImmediateContext->CSSetConstantBuffers(CB_LOC::SKINNING_AND_OBB, ARRAYSIZE(constantBuffers), constantBuffers);
//	
//	// Original vertices, boneIDs per vertex, bone weights per vertex
//	ID3D11ShaderResourceView* ppSRV[] = { mesh->g_pVertexBufferBasePoseSRV, mesh->g_pBoneIDBufferSRV, mesh->g_pBoneWeightBufferSRV };
//	pd3dImmediateContext->CSSetShaderResources(0, 3, ppSRV);
//	
//	ID3D11UnorderedAccessView* ppUAV[] = { mesh->GetVertexBufferUAV() };
//	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);
//	
//	pd3dImmediateContext->CSSetShader(s_skinningCS->Get(), NULL, 0);
//	
//	// todo set num dispatches
//	int numDispatches = GET_THREAD_GROUP_COUNT(mesh->m_NumControlMeshVertices, 128);
//	pd3dImmediateContext->Dispatch(numDispatches, 1, 1);
//
//	// Reset
//	pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
//	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);
//
//	// Compute the OBB
//	ComputeOBB(pd3dImmediateContext, mesh);
//
//	return hr;
//}
//
//HRESULT SkinningAnimation::ComputeOBB(ID3D11DeviceContext1* pd3dImmediateContext, ControlMesh* mesh) {
//	HRESULT hr = S_OK;
//	ID3D11ShaderResourceView* ppSRVNULL[] =  { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
//	ID3D11UnorderedAccessView* ppUAVNULL[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
//	int numDispatches = GET_THREAD_GROUP_COUNT(mesh->m_NumControlMeshVertices, 1024);
//
//	// IMPORTANT: Constant buffer with "uint N" is assumed to be set in "ApplySkinning" to SKINNING_AND_OBB
//
//	// Compute centroid
//	{ // Blockscan 1024
//		// Set SRVs & UAVs
//		
//		ID3D11ShaderResourceView* ppSRV[] = { mesh->GetVertexBufferSRV4Components()	};
//		pd3dImmediateContext->CSSetShaderResources(0, 1, ppSRV);
//		
//		ID3D11UnorderedAccessView* ppUAV[] = { m_pCentroidTmpBufferUAV4Components };
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);
//
//		pd3dImmediateContext->CSSetShader(s_centroidBlockCS->Get(), NULL, 0);
//		pd3dImmediateContext->Dispatch(numDispatches, 1, 1);
//
//		// Reset SRVs & UAVs
//		pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);
//
//		if (0) {
//			float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pCentroidTmpBuffer);
//			fprintf(stderr, "%f %f %f\n", C[0], C[1], C[2]);
//			for (int i = 0; i < 32; ++i)
//				fprintf(stderr, "%f %f %f %f \n", C[i*4+0], C[i*4+1], C[i*4+2], C[i*4+3]);
//			delete [] C;
//		}
//
//	}
//	float centroid[4];
//	{ // Combine
//		// Set SRVs & UAVs
//		ID3D11ShaderResourceView* ppSRV[] = { m_pCentroidTmpBufferSRV4Components };
//		pd3dImmediateContext->CSSetShaderResources(0, 1, ppSRV);
//		
//		ID3D11UnorderedAccessView* ppUAV[] = { m_pCentroidBufferUAV4Components };
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);
//
//		pd3dImmediateContext->CSSetShader(s_centroidCS->Get(), NULL, 0);
//		pd3dImmediateContext->Dispatch(1, 1, 1);
//
//		// Reset SRVs & UAVs
//		pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);
//
//		if (0) {
//			float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pCentroidBuffer);
//			fprintf(stderr, "Centroid %f %f %f\n", C[0], C[1], C[2]);
//			for (int i = 0; i < 4; ++i) centroid[i] = C[i];
//			delete [] C;
//		}
//
//
//	} // Centroid DONE
//	//float* frogVertexData = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, mesh->GetVertexBuffer());
//	//float D[4] = {0.0f, 0.0f, 0.0f, 0.0f};
//	//float L[4] = {0.0f, 0.0f, 0.0f, 0.0f};
//	//int N = mesh->m_NumControlMeshVertices;
//	//for (int i = 0; i < N; ++i) {
//	//	float X[4];
//	//	for (int j = 0; j < 3; ++j) {
//	//		X[j] = (frogVertexData[i*4+j] - centroid[j]) / (float)(N-1);
//	//		D[j] += X[j] * X[j];
//	//	}
//	//	L[0] += X[0] * X[1];
//	//	L[1] += X[0] * X[2];
//	//	L[2] += X[1] * X[2];
//	//}
//	//delete [] frogVertexData;
//
//	// Compute covariance matrix
//	{ // Blockscan 1024
//		// Set SRVs & UAVs
//		
//		ID3D11ShaderResourceView* ppSRV[] = { mesh->GetVertexBufferSRV4Components(), m_pCentroidBufferSRV4Components };
//		pd3dImmediateContext->CSSetShaderResources(0, 2, ppSRV);
//		
//		ID3D11UnorderedAccessView* ppUAV[] = { m_pDTmpUAV4Components, m_pLTmpUAV4Components };
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, ppUAV, NULL);
//
//		pd3dImmediateContext->CSSetShader(s_covarianceBlockCS->Get(), NULL, 0);
//		pd3dImmediateContext->Dispatch(numDispatches, 1, 1);
//
//		// Reset SRVs & UAVs
//		pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);
//
//		if (0) {
//			float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pLTmpBuffer);
//			fprintf(stderr, "%f %f %f\n", C[0], C[1], C[2]);
//			for (int i = 0; i < 32; ++i)
//				fprintf(stderr, "%f %f %f %f \n", C[i*4+0], C[i*4+1], C[i*4+2], C[i*4+3]);
//			delete [] C;
//		}
//
//	}
//	{ // Combine
//		// Set SRVs & UAVs
//		ID3D11ShaderResourceView* ppSRV[] = { m_pDTmpSRV4Components, m_pLTmpSRV4Components };
//		pd3dImmediateContext->CSSetShaderResources(0, 2, ppSRV);
//		
//		ID3D11UnorderedAccessView* ppUAV[] = { m_pONBUAV4Components };
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);
//
//		pd3dImmediateContext->CSSetShader(s_covarianceCS->Get(), NULL, 0);
//		pd3dImmediateContext->Dispatch(1, 1, 1);
//
//		// Reset SRVs & UAVs
//		pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);
//
//		if (0) {
//			float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pONBBuffer);
//			for (int i = 0; i < 5; ++i)
//				fprintf(stderr, "%f %f %f %f\n", C[0+i*4], C[1+i*4], C[2+i*4], C[3+i*4]);
//			delete [] C;
//			//fprintf(stderr, "CPU D: %f %f %f\n", D[0], D[1], D[2]);
//			//fprintf(stderr, "CPU L: %f %f %f\n", L[0], L[1], L[2]);
//		}
//		
//
//
//	} // Covariance Matric & ONB DONE
//
//
//
//	// Min & Max & Extends
//	{ // Blockscan 1024
//		// Set SRVs & UAVs
//		//const float clearValueMin[4] = { 100000.0f, 100000.0f, 100000.0f, 100000.0f };
//		//const float clearValueMax[4]  = { -100000.0f, -100000.0f, -100000.0f, -100000.0f };
//		//const UINT clearValue[4] = { -1u,-1u,-1u,-1u };
//		//pd3dImmediateContext->ClearUnorderedAccessViewUint(meshVoxelData->GetVoxelGridDefinition().m_uavVoxelization, clearValue);
//		//pd3dImmediateContext->ClearUnorderedAccessViewFloat(m_pMinTmpUAV4Components, clearValueMin);
//		//pd3dImmediateContext->ClearUnorderedAccessViewFloat(m_pMaxTmpUAV4Components, clearValueMax);
//
//
//		ID3D11ShaderResourceView* ppSRV[] = { mesh->GetVertexBufferSRV4Components(), m_pONBSRV4Components };
//		pd3dImmediateContext->CSSetShaderResources(0, 2, ppSRV);
//		
//		ID3D11UnorderedAccessView* ppUAV[] = { m_pMinTmpUAV4Components, m_pMaxTmpUAV4Components };
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, ppUAV, NULL);
//
//		pd3dImmediateContext->CSSetShader(s_minMaxBlockCS->Get(), NULL, 0);
//		pd3dImmediateContext->Dispatch(numDispatches, 1, 1);
//
//		// Reset SRVs & UAVs
//		pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);
//
//		if (0) {
//			float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pLTmpBuffer);
//			fprintf(stderr, "%f %f %f\n", C[0], C[1], C[2]);
//			for (int i = 0; i < 32; ++i)
//				fprintf(stderr, "%f %f %f %f \n", C[i*4+0], C[i*4+1], C[i*4+2], C[i*4+3]);
//			delete [] C;
//		}
//
//	}
//	{ // Combine
//		// Set SRVs & UAVs
//		ID3D11ShaderResourceView* ppSRV[] = { m_pONBSRV4Components, m_pMinTmpSRV4Components, m_pMaxTmpSRV4Components };
//		pd3dImmediateContext->CSSetShaderResources(0, 3, ppSRV);
//		
//		ID3D11UnorderedAccessView* ppUAV[] = { m_pCentroidBufferUAV4Components };
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);
//
//		pd3dImmediateContext->CSSetShader(s_minMaxCS->Get(), NULL, 0);
//		pd3dImmediateContext->Dispatch(1, 1, 1);
//
//		// Reset SRVs & UAVs
//		pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);
//
//		if (1) {
//			float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pCentroidBuffer);
//			//for (int i = 0; i < 6; ++i)
//			//	fprintf(stderr, "%d - %f %f %f %f\n", i, C[0+i*4], C[1+i*4], C[2+i*4], C[3+i*4]);
//			D3D11ObjectOrientedBoundingBox& obbRef = mesh->GetModelOBB();
//			obbRef.setAnchorAndScaledAxes(
//				point3d<float>(&C[0]),
//				point3d<float>(&C[4]),
//				point3d<float>(&C[8]),
//				point3d<float>(&C[12]));
//
//			delete [] C;
//		}
//
//
//	} // Covariance Matric & ONB DONE
//
//
//
//	return hr;
//}




//HRESULT SkinningAnimation::ApplySkinning( ID3D11DeviceContext1* pd3dImmediateContext, DXModel* mesh, float fTime )
//{
//	if (!mesh->IsSkinned()) return 0;
//
//	mesh->m_skinningMeshAnimationManager->computeAnimation(0, fTime);
//
//	UINT voffset = 0, stride = sizeof(XMFLOAT4A);
//	HRESULT hr = S_OK;
//	ID3D11Buffer* ppBuffer[] = { NULL, NULL, NULL };
//	pd3dImmediateContext->IASetVertexBuffers(0, 3, ppBuffer, &stride, &voffset);
//
//
//	ID3D11ShaderResourceView* ppSRVNULL[] =  { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
//	ID3D11UnorderedAccessView* ppUAVNULL[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
//	pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
//	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);
//
//	D3D11_MAPPED_SUBRESOURCE mappedBuf;
//	pd3dImmediateContext->Map(m_cbSkinning, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedBuf);
//
//	CB_SKINNING_AND_OBB* cbSkinning = reinterpret_cast<CB_SKINNING_AND_OBB*>(mappedBuf.pData);
//	cbSkinning->mNumOriginalVertices = mesh->_numVertices;
//	//cbSkinning->mBaseVertex = mesh->_baseVertex;
//	for (UINT i = 0; i < mesh->m_skinningMeshAnimationManager->getBoneTransformationsRef().size() && i < SKINNING_NUM_MAX_BONES; ++i)
//		cbSkinning->mBoneMatrices[i] =  mesh->m_skinningMeshAnimationManager->getBoneTransformationsRef()[i]; //XMFLOAT4X4A(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1) ;//
//
//	pd3dImmediateContext->Unmap(m_cbSkinning, 0);
//	
//	ID3D11Buffer* constantBuffers[] = { m_cbSkinning };
//	pd3dImmediateContext->CSSetConstantBuffers(CB_LOC::SKINNING_AND_OBB, ARRAYSIZE(constantBuffers), constantBuffers);
//	
//	// Original vertices, boneIDs per vertex, bone weights per vertex
//	ID3D11ShaderResourceView* ppSRV[] = { mesh->g_pVertexBufferBasePoseSRV,  mesh->g_pNormalsBufferBasePoseSRV, mesh->m_skinningMeshAnimationManager->g_pBoneIDBufferSRV, mesh->m_skinningMeshAnimationManager->g_pBoneWeightBufferSRV };
//	pd3dImmediateContext->CSSetShaderResources(0, 4, ppSRV);
//	
//	ID3D11UnorderedAccessView* ppUAV[] = { mesh->GetVertexBufferUAV(), mesh->GetNormalsBufferUAV() };
//	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, ppUAV, NULL);
//	
//	pd3dImmediateContext->CSSetShader(s_skinningCS->Get(), NULL, 0);
//	
//	// todo set num dispatches
//	int numDispatches = GET_THREAD_GROUP_COUNT(mesh->_numVertices, 128);
//	pd3dImmediateContext->Dispatch(numDispatches, 1, 1);
//
//	// Reset
//	pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
//	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);
//
//	// Compute the OBB
//	// TODO fix me
//	ComputeOBB(pd3dImmediateContext, mesh);
//
//	return hr;
//}

HRESULT SkinningAnimation::ApplySkinning( ID3D11DeviceContext1* pd3dImmediateContext, AnimationGroup* group, float fTime )
{
	HRESULT hr = S_OK;
	if(group == NULL) return S_FALSE;
	if(group->GetAnimationManager() == NULL) return S_FALSE;

	group->GetAnimationManager()->computeAnimation(group->GetCurrentAnimation(), fTime);
	// TODO indepenent anims in group

	for(auto mesh : group->triModels)
	{			
		UINT voffset = 0, stride = sizeof(XMFLOAT4A);
		HRESULT hr = S_OK;
		ID3D11Buffer* ppBuffer[] = { NULL, NULL, NULL };
		pd3dImmediateContext->IASetVertexBuffers(0, 3, ppBuffer, &stride, &voffset);


		ID3D11ShaderResourceView* ppSRVNULL[] =  { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
		ID3D11UnorderedAccessView* ppUAVNULL[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
		pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);

		D3D11_MAPPED_SUBRESOURCE mappedBuf;
		pd3dImmediateContext->Map(m_cbSkinning, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedBuf);
		CB_SKINNING_AND_OBB* cbSkinning = reinterpret_cast<CB_SKINNING_AND_OBB*>(mappedBuf.pData);
		//cbSkinning->mNumOriginalVertices = mesh->_numVertices;

		cbSkinning->mNumOriginalVertices = mesh->_numVertices+1;  // CHECKME something is off by one in minmax, ugly fix here 
		
		//cbSkinning->mBaseVertex = mesh->_baseVertex;
		for (UINT i = 0; i < group->GetAnimationManager()->GetBoneTransformationsRef().size() && i < SKINNING_NUM_MAX_BONES; ++i)
			cbSkinning->mBoneMatrices[i] =  group->GetAnimationManager()->GetBoneTransformationsRef()[i]; //XMFLOAT4X4A(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1) ;//

		pd3dImmediateContext->Unmap(m_cbSkinning, 0);

		ID3D11Buffer* constantBuffers[] = { m_cbSkinning };
		pd3dImmediateContext->CSSetConstantBuffers(CB_LOC::SKINNING_AND_OBB, ARRAYSIZE(constantBuffers), constantBuffers);

		// Original vertices, boneIDs per vertex, bone weights per vertex
		ID3D11ShaderResourceView* ppSRV[] = { mesh->g_pVertexBufferBasePoseSRV,  mesh->g_pNormalsBufferBasePoseSRV,
											  group->GetAnimationManager()->g_pBoneIDBufferSRV, group->GetAnimationManager()->g_pBoneWeightBufferSRV };
		pd3dImmediateContext->CSSetShaderResources(0, 4, ppSRV);

		ID3D11UnorderedAccessView* ppUAV[] = { mesh->GetVertexBufferUAV(), mesh->GetNormalsBufferUAV() };
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, ppUAV, NULL);

		pd3dImmediateContext->CSSetShader(s_skinningCS->Get(), NULL, 0);

		// todo set num dispatches
		int numDispatches = GET_THREAD_GROUP_COUNT(mesh->_numVertices, 128);
		pd3dImmediateContext->Dispatch(numDispatches, 1, 1);

		// Reset
		pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);

		// Compute the OBB
		// TODO fix me
		ComputeOBB(pd3dImmediateContext, mesh);

	}

	return hr;
}

// compute obb object space
HRESULT SkinningAnimation::ComputeOBB(ID3D11DeviceContext1* pd3dImmediateContext, DXModel* mesh) {
	HRESULT hr = S_OK;
	ID3D11ShaderResourceView* ppSRVNULL[] =  { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
	ID3D11UnorderedAccessView* ppUAVNULL[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

	//for(auto& submesh : mesh->submeshes)
	{
		//int numDispatches = GET_THREAD_GROUP_COUNT(submesh._numVertices, 1024);
		int numDispatches = GET_THREAD_GROUP_COUNT(mesh->_numVertices, 1024);
		// todo set cb with numV and startIndex;

		// IMPORTANT: Constant buffer with "uint N" is assumed to be set in "ApplySkinning" to SKINNING_AND_OBB

		// Compute centroid
		{ // Blockscan 1024
			// Set SRVs & UAVs

			ID3D11ShaderResourceView* ppSRV[] = { mesh->GetVertexBufferSRV4Components()	};
			pd3dImmediateContext->CSSetShaderResources(0, 1, ppSRV);

			ID3D11UnorderedAccessView* ppUAV[] = { m_pCentroidTmpBufferUAV4Components };
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);

			pd3dImmediateContext->CSSetShader(s_centroidBlockCS->Get(), NULL, 0);
			pd3dImmediateContext->Dispatch(numDispatches, 1, 1);

			// Reset SRVs & UAVs
			pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);

			if (0) {
				float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pCentroidTmpBuffer);
				fprintf(stderr, "%f %f %f\n", C[0], C[1], C[2]);
				for (int i = 0; i < 32; ++i)
					fprintf(stderr, "%f %f %f %f \n", C[i*4+0], C[i*4+1], C[i*4+2], C[i*4+3]);
				delete [] C;
			}

		}
		float centroid[4];
		{ // Combine
			// Set SRVs & UAVs
			ID3D11ShaderResourceView* ppSRV[] = { m_pCentroidTmpBufferSRV4Components };
			pd3dImmediateContext->CSSetShaderResources(0, 1, ppSRV);

			ID3D11UnorderedAccessView* ppUAV[] = { m_pCentroidBufferUAV4Components };
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);

			pd3dImmediateContext->CSSetShader(s_centroidCS->Get(), NULL, 0);
			pd3dImmediateContext->Dispatch(1, 1, 1);

			// Reset SRVs & UAVs
			pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);

			if (0) {
				float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pCentroidBuffer);
				fprintf(stderr, "Centroid %f %f %f\n", C[0], C[1], C[2]);
				for (int i = 0; i < 4; ++i) centroid[i] = C[i];
				delete [] C;
			}
		} // Centroid DONE

		//float* frogVertexData = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, mesh->GetVertexBuffer());
		//float D[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		//float L[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		//int N = mesh->m_NumControlMeshVertices;
		//for (int i = 0; i < N; ++i) {
		//	float X[4];
		//	for (int j = 0; j < 3; ++j) {
		//		X[j] = (frogVertexData[i*4+j] - centroid[j]) / (float)(N-1);
		//		D[j] += X[j] * X[j];
		//	}
		//	L[0] += X[0] * X[1];
		//	L[1] += X[0] * X[2];
		//	L[2] += X[1] * X[2];
		//}
		//delete [] frogVertexData;

		// Compute covariance matrix
		{ // Blockscan 1024
			// Set SRVs & UAVs

			ID3D11ShaderResourceView* ppSRV[] = { mesh->GetVertexBufferSRV4Components(), m_pCentroidBufferSRV4Components };
			pd3dImmediateContext->CSSetShaderResources(0, 2, ppSRV);

			ID3D11UnorderedAccessView* ppUAV[] = { m_pDTmpUAV4Components, m_pLTmpUAV4Components };
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, ppUAV, NULL);

			pd3dImmediateContext->CSSetShader(s_covarianceBlockCS->Get(), NULL, 0);
			pd3dImmediateContext->Dispatch(numDispatches, 1, 1);

			// Reset SRVs & UAVs
			pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);

			if (0) {
				float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pLTmpBuffer);
				fprintf(stderr, "%f %f %f\n", C[0], C[1], C[2]);
				for (int i = 0; i < 32; ++i)
					fprintf(stderr, "%f %f %f %f \n", C[i*4+0], C[i*4+1], C[i*4+2], C[i*4+3]);
				delete [] C;
			}

		}
		{ // Combine
			// Set SRVs & UAVs
			ID3D11ShaderResourceView* ppSRV[] = { m_pDTmpSRV4Components, m_pLTmpSRV4Components };
			pd3dImmediateContext->CSSetShaderResources(0, 2, ppSRV);

			ID3D11UnorderedAccessView* ppUAV[] = { m_pONBUAV4Components };
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);

			pd3dImmediateContext->CSSetShader(s_covarianceCS->Get(), NULL, 0);
			pd3dImmediateContext->Dispatch(1, 1, 1);

			// Reset SRVs & UAVs
			pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);

			if (0) {
				float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pONBBuffer);
				for (int i = 0; i < 5; ++i)
					fprintf(stderr, "%f %f %f %f\n", C[0+i*4], C[1+i*4], C[2+i*4], C[3+i*4]);
				delete [] C;
				//fprintf(stderr, "CPU D: %f %f %f\n", D[0], D[1], D[2]);
				//fprintf(stderr, "CPU L: %f %f %f\n", L[0], L[1], L[2]);
			}
		} // Covariance Matric & ONB DONE



		// Min & Max & Extends
		{ // Blockscan 1024
			// Set SRVs & UAVs

			ID3D11ShaderResourceView* ppSRV[] = { mesh->GetVertexBufferSRV4Components(), m_pONBSRV4Components };
			pd3dImmediateContext->CSSetShaderResources(0, 2, ppSRV);

			ID3D11UnorderedAccessView* ppUAV[] = { m_pMinTmpUAV4Components, m_pMaxTmpUAV4Components };
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, ppUAV, NULL);

			pd3dImmediateContext->CSSetShader(s_minMaxBlockCS->Get(), NULL, 0);
			pd3dImmediateContext->Dispatch(numDispatches, 1, 1);

			// Reset SRVs & UAVs
			pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);

			if (0) {
				float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pLTmpBuffer);
				fprintf(stderr, "%f %f %f\n", C[0], C[1], C[2]);
				for (int i = 0; i < 32; ++i)
					fprintf(stderr, "%f %f %f %f \n", C[i*4+0], C[i*4+1], C[i*4+2], C[i*4+3]);
				delete [] C;
			}

		}
		{ // Combine
			// Set SRVs & UAVs
			ID3D11ShaderResourceView* ppSRV[] = { m_pONBSRV4Components, m_pMinTmpSRV4Components, m_pMaxTmpSRV4Components };
			pd3dImmediateContext->CSSetShaderResources(0, 3, ppSRV);

			ID3D11UnorderedAccessView* ppUAV[] = { m_pCentroidBufferUAV4Components };
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);

			pd3dImmediateContext->CSSetShader(s_minMaxCS->Get(), NULL, 0);
			pd3dImmediateContext->Dispatch(1, 1, 1);

			// Reset SRVs & UAVs
			pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);

			if (1) {
				float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pCentroidBuffer);
				//for (int i = 0; i < 6; ++i)
				//	fprintf(stderr, "%d - %f %f %f %f\n", i, C[0+i*4], C[1+i*4], C[2+i*4], C[3+i*4]);
				for(auto& submesh : mesh->submeshes)
				{

				
				DXObjectOrientedBoundingBox& obbRef = submesh.GetModelOBB();
				//obbRef.setAnchorAndScaledAxes(
				//	point3d<float>(&C[0]),
				//	point3d<float>(&C[4]),
				//	point3d<float>(&C[8]),
				//	point3d<float>(&C[12]));
				//}
				
				obbRef.setAnchorAndScaledAxes(
					(XMFLOAT3*)&C[0],
					(XMFLOAT3*)&C[4],
					(XMFLOAT3*)&C[8],
					(XMFLOAT3*)&C[12]);
			}
				delete [] C;
			}

		} // Covariance Matric & ONB DONE
	}



	return hr;
}

// update object space obb
//HRESULT SkinningAnimation::ComputeOBB(ID3D11DeviceContext1* pd3dImmediateContext, DXModel* mesh) {
//	HRESULT hr = S_OK;
//	ID3D11ShaderResourceView* ppSRVNULL[] =  { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
//	ID3D11UnorderedAccessView* ppUAVNULL[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
//
//	for(auto& submesh : mesh->submeshes)
//	{
//		int numDispatches = GET_THREAD_GROUP_COUNT(submesh._numVertices, 1024);
//		// todo set cb with numV and startIndex;
//
//		// IMPORTANT: Constant buffer with "uint N" is assumed to be set in "ApplySkinning" to SKINNING_AND_OBB
//
//		// Compute centroid
//		{ // Blockscan 1024
//			// Set SRVs & UAVs
//
//			ID3D11ShaderResourceView* ppSRV[] = { mesh->GetVertexBufferSRV4Components()	};
//			pd3dImmediateContext->CSSetShaderResources(0, 1, ppSRV);
//
//			ID3D11UnorderedAccessView* ppUAV[] = { m_pCentroidTmpBufferUAV4Components };
//			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);
//
//			pd3dImmediateContext->CSSetShader(s_centroidBlockCS->Get(), NULL, 0);
//			pd3dImmediateContext->Dispatch(numDispatches, 1, 1);
//
//			// Reset SRVs & UAVs
//			pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
//			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);
//
//			if (0) {
//				float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pCentroidTmpBuffer);
//				fprintf(stderr, "%f %f %f\n", C[0], C[1], C[2]);
//				for (int i = 0; i < 32; ++i)
//					fprintf(stderr, "%f %f %f %f \n", C[i*4+0], C[i*4+1], C[i*4+2], C[i*4+3]);
//				delete [] C;
//			}
//
//		}
//		float centroid[4];
//		{ // Combine
//			// Set SRVs & UAVs
//			ID3D11ShaderResourceView* ppSRV[] = { m_pCentroidTmpBufferSRV4Components };
//			pd3dImmediateContext->CSSetShaderResources(0, 1, ppSRV);
//
//			ID3D11UnorderedAccessView* ppUAV[] = { m_pCentroidBufferUAV4Components };
//			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);
//
//			pd3dImmediateContext->CSSetShader(s_centroidCS->Get(), NULL, 0);
//			pd3dImmediateContext->Dispatch(1, 1, 1);
//
//			// Reset SRVs & UAVs
//			pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
//			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);
//
//			if (0) {
//				float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pCentroidBuffer);
//				fprintf(stderr, "Centroid %f %f %f\n", C[0], C[1], C[2]);
//				for (int i = 0; i < 4; ++i) centroid[i] = C[i];
//				delete [] C;
//			}
//		} // Centroid DONE
//
//		//float* frogVertexData = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, mesh->GetVertexBuffer());
//		//float D[4] = {0.0f, 0.0f, 0.0f, 0.0f};
//		//float L[4] = {0.0f, 0.0f, 0.0f, 0.0f};
//		//int N = mesh->m_NumControlMeshVertices;
//		//for (int i = 0; i < N; ++i) {
//		//	float X[4];
//		//	for (int j = 0; j < 3; ++j) {
//		//		X[j] = (frogVertexData[i*4+j] - centroid[j]) / (float)(N-1);
//		//		D[j] += X[j] * X[j];
//		//	}
//		//	L[0] += X[0] * X[1];
//		//	L[1] += X[0] * X[2];
//		//	L[2] += X[1] * X[2];
//		//}
//		//delete [] frogVertexData;
//
//		// Compute covariance matrix
//		{ // Blockscan 1024
//			// Set SRVs & UAVs
//
//			ID3D11ShaderResourceView* ppSRV[] = { mesh->GetVertexBufferSRV4Components(), m_pCentroidBufferSRV4Components };
//			pd3dImmediateContext->CSSetShaderResources(0, 2, ppSRV);
//
//			ID3D11UnorderedAccessView* ppUAV[] = { m_pDTmpUAV4Components, m_pLTmpUAV4Components };
//			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, ppUAV, NULL);
//
//			pd3dImmediateContext->CSSetShader(s_covarianceBlockCS->Get(), NULL, 0);
//			pd3dImmediateContext->Dispatch(numDispatches, 1, 1);
//
//			// Reset SRVs & UAVs
//			pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
//			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);
//
//			if (0) {
//				float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pLTmpBuffer);
//				fprintf(stderr, "%f %f %f\n", C[0], C[1], C[2]);
//				for (int i = 0; i < 32; ++i)
//					fprintf(stderr, "%f %f %f %f \n", C[i*4+0], C[i*4+1], C[i*4+2], C[i*4+3]);
//				delete [] C;
//			}
//
//		}
//		{ // Combine
//			// Set SRVs & UAVs
//			ID3D11ShaderResourceView* ppSRV[] = { m_pDTmpSRV4Components, m_pLTmpSRV4Components };
//			pd3dImmediateContext->CSSetShaderResources(0, 2, ppSRV);
//
//			ID3D11UnorderedAccessView* ppUAV[] = { m_pONBUAV4Components };
//			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);
//
//			pd3dImmediateContext->CSSetShader(s_covarianceCS->Get(), NULL, 0);
//			pd3dImmediateContext->Dispatch(1, 1, 1);
//
//			// Reset SRVs & UAVs
//			pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
//			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);
//
//			if (0) {
//				float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pONBBuffer);
//				for (int i = 0; i < 5; ++i)
//					fprintf(stderr, "%f %f %f %f\n", C[0+i*4], C[1+i*4], C[2+i*4], C[3+i*4]);
//				delete [] C;
//				//fprintf(stderr, "CPU D: %f %f %f\n", D[0], D[1], D[2]);
//				//fprintf(stderr, "CPU L: %f %f %f\n", L[0], L[1], L[2]);
//			}
//		} // Covariance Matric & ONB DONE
//
//
//
//		// Min & Max & Extends
//		{ // Blockscan 1024
//			// Set SRVs & UAVs
//
//			ID3D11ShaderResourceView* ppSRV[] = { mesh->GetVertexBufferSRV4Components(), m_pONBSRV4Components };
//			pd3dImmediateContext->CSSetShaderResources(0, 2, ppSRV);
//
//			ID3D11UnorderedAccessView* ppUAV[] = { m_pMinTmpUAV4Components, m_pMaxTmpUAV4Components };
//			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, ppUAV, NULL);
//
//			pd3dImmediateContext->CSSetShader(s_minMaxBlockCS->Get(), NULL, 0);
//			pd3dImmediateContext->Dispatch(numDispatches, 1, 1);
//
//			// Reset SRVs & UAVs
//			pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
//			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);
//
//			if (0) {
//				float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pLTmpBuffer);
//				fprintf(stderr, "%f %f %f\n", C[0], C[1], C[2]);
//				for (int i = 0; i < 32; ++i)
//					fprintf(stderr, "%f %f %f %f \n", C[i*4+0], C[i*4+1], C[i*4+2], C[i*4+3]);
//				delete [] C;
//			}
//
//		}
//		{ // Combine
//			// Set SRVs & UAVs
//			ID3D11ShaderResourceView* ppSRV[] = { m_pONBSRV4Components, m_pMinTmpSRV4Components, m_pMaxTmpSRV4Components };
//			pd3dImmediateContext->CSSetShaderResources(0, 3, ppSRV);
//
//			ID3D11UnorderedAccessView* ppUAV[] = { m_pCentroidBufferUAV4Components };
//			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAV, NULL);
//
//			pd3dImmediateContext->CSSetShader(s_minMaxCS->Get(), NULL, 0);
//			pd3dImmediateContext->Dispatch(1, 1, 1);
//
//			// Reset SRVs & UAVs
//			pd3dImmediateContext->CSSetShaderResources(0, 8, ppSRVNULL);
//			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 8, ppUAVNULL, NULL);
//
//			if (1) {
//				float* C = (float*)CreateAndCopyToDebugBuf(pd3dImmediateContext, m_pCentroidBuffer);
//				//for (int i = 0; i < 6; ++i)
//				//	fprintf(stderr, "%d - %f %f %f %f\n", i, C[0+i*4], C[1+i*4], C[2+i*4], C[3+i*4]);
//				D3D11ObjectOrientedBoundingBox& obbRef = submesh.GetModelOBB();
//				obbRef.setAnchorAndScaledAxes(
//					point3d<float>(&C[0]),
//					point3d<float>(&C[4]),
//					point3d<float>(&C[8]),
//					point3d<float>(&C[12]));
//
//				delete [] C;
//			}
//
//		} // Covariance Matric & ONB DONE
//	}
//
//
//
//	return hr;
//}
#pragma once

#include <DXUT.h>
#include <DirectXMath.h>

#include "utils/TimingLog.h"
#include "utils/timer.h"
#include <SDX/DXPerfEvent.h>
#include <osd/d3d11ComputeController.h>
#include "Voxelization.h"

class DXPicking;

static ID3D11ShaderResourceView*	const g_ppSRVNULL[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static ID3D11UnorderedAccessView*	const g_ppUAVNULL[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static ID3D11SamplerState*			const g_ppSSNULL[]  = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

#define NUM_CASCADES 3
#define DEFORMATION_BATCH_SIZE 6
// constant buffer IDs, make sure to use these IDs in shaders
class CB_LOC
{
public:
	enum {
		PER_FRAME			 =  0,
		MODEL_MATRIX		 =  1,
		MATERIAL			 =  2,
		// unused 3
		CAM_EYE_AND_ZDIST	 =  4,
		GEN_SHADOW			 =  5,
		//PER_LEVEL			 =  5,
		RUNTABLES			 =  6,		// opensubdiv feature adaptive subdivision tables
		OSD_DEFORMATION_CONFIG = 6,
		LIGHT_PROPERTIES	 =  7,
		ANIMATE_KEYFRAME	 =  8,
		PICKING				 =  8,
		VOXELGRID			 =  9,
		RAYCAST_VOXELIZATION = 10,
		CASCADE_PROJECTION   = 10,
		CULLING				 = 11,
		INTERSECT			 = 11,
		MANAGE_TILES		 = 11,
		DEPTH_REDUCTION		 = 11,
		SKINNING_AND_OBB	 = 12,		// Only locally used in SkinningAnimation.cpp/ApplySkinning(), feel free to reuse this slot for "local use"
		UPDATE_OVERLAP_EXTRAORDINARY		 = 12,
		OVERLAP				 = 13
	};
};


// constant buffer data container

// camera, usually set once per frame
struct CB_PER_FRAME
{  
	DirectX::XMMATRIX ModelViewProjection;		// model view projection matrix 
	DirectX::XMMATRIX ModelViewMatrix;			// model view  matrix			
	DirectX::XMMATRIX ProjectionMatrix;			// camera projection matrix		
	DirectX::XMMATRIX WorldMatrix;				// model to world				
	DirectX::XMMATRIX ViewMatrix;				// camera view matrix
	DirectX::XMMATRIX InverseViewMatrix;		// camera view matrix
};


// for physics
struct CB_MODEL_MATRIX
{
	DirectX::XMMATRIX mModel;
};

// parsed material constants, each dxmaterial holds such a cb
struct CB_MATERIAL {
	DirectX::XMFLOAT3 kAmbient;			// constant ambient color
	float magicFlag;					// flag for replacing with magic materials
	DirectX::XMFLOAT3 kDiffuse;			// constant diffuse color
	float smoothness;
	DirectX::XMFLOAT3 kSpecular;		// constant specular color
	float			  Nshininess;		// shininess
};



// additional camera parameters, e.g. for compute tess factor
struct CB_CAMEYE_DIST {
	DirectX::XMFLOAT3 eye;						// CHECKME not found, is it used somewhere @maddi?
	float zfzn;
	float znear;
	float zfar;
	DirectX::XMFLOAT2 padding;
};

// light properties
struct CB_LIGHT
{
	DirectX::XMFLOAT4	lightDirWS;
	DirectX::XMFLOAT4   lightDirES;
	
	DirectX::XMMATRIX	worldToShadowMatrix;
	
	DirectX::XMFLOAT4	cascadeOffset[NUM_CASCADES];

	DirectX::XMFLOAT4	cascadeScale[NUM_CASCADES];

	int					nCascadeLevels;
	int					showCascadeLevels;
	float				minBorderPadding;
	float				maxBorderPadding;

	float				cascadeBlendArea;	// ammount of blend between cascades
	float				texelSize;			// shadow map texel size
	float				nativeTexelSizeInX; // texel size in native map (textures are packed)
	float				padding1;

	float				cascadeFrustumEyeSpaceDepths[4];
};

struct CB_ANIMATE {
	UINT g_NumVertices;
	float g_InterpolateParameter;
	DirectX::XMUINT2 padding;
};


struct CB_VoxelGrid {
	DirectX::XMMATRIX m_matModelToProj;
	DirectX::XMMATRIX m_matModelToVoxel;
	
	UINT m_stride[2+2];

	DirectX::XMUINT3 m_gridSize;
	UINT padding;
};

// checkme padding
// for voxelization debug vis
struct CB_Raycasting {
	DirectX::XMMATRIX mQuadToVoxel;
	DirectX::XMMATRIX mVoxelToScreen;

	DirectX::XMFLOAT3 rayOrigin;
	float padding1;

	DirectX::XMFLOAT3 voxLightPos;
	float padding2;

	UINT m_stride[2];
	DirectX::XMUINT2 padding3;

	UINT m_gridSize[3];
	BOOL m_showLines;
};

#define SKINNING_NUM_MAX_BONES 80

struct CB_SKINNING_AND_OBB {
	UINT mNumOriginalVertices;
	//UINT mBaseVertex
	DirectX::XMUINT3 padding;
	DirectX::XMMATRIX mBoneMatrices[SKINNING_NUM_MAX_BONES];
};

struct CB_GEN_SHADOW
{
	DirectX::XMMATRIX g_SceneModelView;
	DirectX::XMMATRIX g_SceneProj;
};


struct CB_CULLING
{
	DirectX::XMMATRIX mModelToOBB;
	unsigned int mNumPatches;
	float mMaxDisplacement;
	DirectX::XMUINT2 padding;
};


// USES CB_LOC::MATERIAL slot!
struct CB_POSTPROCESSING {
	float bokehFocus;
	float d0, d1, d2;
};

struct CB_PICKING
{
	DirectX::XMFLOAT2 cursorPos;
	DirectX::XMFLOAT2 cursorDir;
	int objectID;
	DirectX::XMFLOAT3 padding;
};


struct CB_CASCADES_PROJECTION
{
	DirectX::XMMATRIX mMatProjCascade[NUM_CASCADES];
};

struct CB_DEPTH_REDUCTION
{
	float	nearPlane;
	float	farPlane;
	UINT	numElements;
	UINT	padding;
	
};

struct SPicking
{
	DirectX::XMFLOAT4X4 TBNP;  // tangent, binormal, normal, pos // sculpting or painting event orientation and position
	float depth;			 // for sorting
	int   objectID;	
};


struct SPickingMatrix
{
	DirectX::XMFLOAT4X4 TBNP;
	DirectX::XMFLOAT4X4 invTBNP;
	DirectX::XMFLOAT4X4 prevTBNP;
	DirectX::XMFLOAT4X4 prevInvTBNP;
};

struct SPtexNeighborData
{
	// data per edge of own ptex id
	int32_t ptexIDNeighbor[4];	// ptex id of neighbor on  edge  0,1,2,3, : entry = -1 == is boundary
	int32_t neighEdgeID[4];		// own edge to neigh edge map: own ptex id is opposite of this edgeID on neighbor ptex
};

struct SExtraordinaryInfo
{
	UINT startIndex;
	UINT valence;
};

struct SExtraordinaryData
{
	UINT ptexFaceID;
	UINT vertexInFace;	// extraordinary is vertex 0, 1, 2, 3
};

__declspec(align(16))
struct CB_IntersectModel {
	DirectX::XMMATRIX modelToWorld;
	float  g_displacementScale;	
	DirectX::XMFLOAT3 padding;
};

__declspec(align(16))
struct CB_IntersectOBBBatch {
	DirectX::XMMATRIX modelToWorld[DEFORMATION_BATCH_SIZE];
	float  g_displacementScale;
};

__declspec(align(16))
struct CB_OSDDisplacement {
	float displacementScale;
	float mipmapBias;
};

__declspec(align(16))
struct CB_OSDConfig {		
	int GregoryQuadOffsetBase;
	int PrimitiveIdBase;
	int IndexStart;		
	unsigned int NumVertexComponents;
	unsigned int NumIndicesPerPatch;
	unsigned int NumPatches;
};

__declspec(align(16))
struct CB_ManageTiles
{
	UINT numTiles;
	DirectX::XMUINT3 padding;

};

__declspec(align(16))
struct CB_UpdateExtarodinary
{
	UINT numExtraordinary;
	DirectX::XMUINT3 padding;

};

//////////////////////////////////////////////////////////////


class App{	
public:
	App()
	{
		g_bRunSimulation = false;
		g_CBCamera = NULL;	
		
		m_camModified			= true;
		g_osdSubdivider			= NULL;
		
		g_bTimingsEnabled = false;	
		
		g_useCulling = false;
		g_useCullingForRayCast = false;
		g_voxelDeformationMultiSampling = false;
		g_withVoxelJittering = false;
		g_withVoxelOBBRotate = false;
			
		g_profilePipelineStages = false;
		g_showIntersections = false;
		g_useCompactedVisibilityOverlap = false;
		g_useDisplacementConstraints = false;
		g_showAllocated = false;
		g_withOverlapUpdate = true;

		g_adaptiveTessellation = true;

		g_displacementTileSize	= 128;
		g_colorTileSize			= 128;

		g_fDisplacementScalar = 1.f;
		g_fDisplacementScalarViewSpace = 1.f; // current displacement scale in view space, gets updated each frame
		g_adaptiveVoxelizationScale = 1.f;
		g_fMipMapBias = 0.f;

		g_fTessellationFactor = 1.f;

		g_memDebugDoPrealloc = false;

		g_memNumColorTiles			= 10000;	   
		g_memNumDisplacementTiles	= 10000;	

		g_memMaxNumTilesPerObject = 50000;

		g_maxSubdivisions = 6u;

		g_qryTimestampStart = NULL;
		g_qryTimestampEnd = NULL;
		g_qryTimestampDisjoint = NULL;
		g_qryTimeActive = false;

		g_withPaintSculptTimings  = false;

		g_pipelineQuery = NULL;

		g_envMapSRV = NULL;

		g_mouseScreenPos	 = DirectX::XMFLOAT2(0, 0);
		g_mousePrevScreenPos = DirectX::XMFLOAT2(0, 0);
		g_mouseDirection	 = DirectX::XMFLOAT2(0, 0);
		g_mousePrevDirection = DirectX::XMFLOAT2(0, 0);

		
	}
	~App()
	{
		Destroy();
	}

	HRESULT Create(ID3D11Device1* pd3dDevice);
	
	void Destroy();

	
	UINT g_displacementTileSize;
	UINT g_colorTileSize	;

	// per frame camera data
	__forceinline void SetViewMatrix(const DirectX::XMMATRIX& viewMatrix)						{		m_ViewMatrix		= viewMatrix; m_inverseViewMatrix = DirectX::XMMatrixInverse(NULL, m_ViewMatrix);		m_camModified = true; }
	__forceinline void SetProjectionMatrix(const DirectX::XMMATRIX& projectionMatrix)			{		m_ProjectionMatrix	= projectionMatrix; m_camModified = true; }
	//__forceinline void SetViewProjectionMatrix(DirectX::XMMATRIX& viewProjectionMatrix)	{		m_ViewProjectionMatrix = viewProjectionMatrix;	}

	__forceinline const DirectX::XMMATRIX& GetViewMatrix()				const {	return m_ViewMatrix;				}
	__forceinline const DirectX::XMMATRIX& GetProjectionMatrix()		const {	return m_ProjectionMatrix;			}
	const DirectX::XMMATRIX& GetViewProjectionMatrix()	{	if(m_camModified) { m_ViewProjectionMatrix = m_ViewMatrix * m_ProjectionMatrix; m_camModified = false;}		return m_ViewProjectionMatrix;		}


	__forceinline void WaitForGPU() const
	{
		DXUTGetD3D11DeviceContext()->End(g_query); while(DXUTGetD3D11DeviceContext()->GetData(g_query, NULL, 0, 0) == S_FALSE) { /*Wait*/ } 
	}

	__forceinline void SetCameraConstantBuffersAllStages(ID3D11DeviceContext1* pd3dImmediateContext) const
	{
		pd3dImmediateContext->CSSetConstantBuffers( CB_LOC::PER_FRAME, 1, &g_CBCamera );
		pd3dImmediateContext->VSSetConstantBuffers( CB_LOC::PER_FRAME, 1, &g_CBCamera );	
		pd3dImmediateContext->DSSetConstantBuffers( CB_LOC::PER_FRAME, 1, &g_CBCamera );
		pd3dImmediateContext->HSSetConstantBuffers( CB_LOC::PER_FRAME, 1, &g_CBCamera );
		pd3dImmediateContext->GSSetConstantBuffers( CB_LOC::PER_FRAME, 1, &g_CBCamera );
		pd3dImmediateContext->PSSetConstantBuffers( CB_LOC::PER_FRAME, 1, &g_CBCamera );
	}

		
	__forceinline void SetGenShadowConstantBuffersHSDSGS(ID3D11DeviceContext1* pd3dImmediateContext) const
	{
		//pd3dImmediateContext->CSSetConstantBuffers( CB_LOC::GEN_SHADOW, 1, &g_CBGenShadow );
		//pd3dImmediateContext->VSSetConstantBuffers( CB_LOC::GEN_SHADOW, 1, &g_CBGenShadow );	
		//pd3dImmediateContext->DSSetConstantBuffers( CB_LOC::GEN_SHADOW, 1, &g_CBGenShadow );
		pd3dImmediateContext->HSSetConstantBuffers( CB_LOC::GEN_SHADOW, 1, &g_CBGenShadow );
		//pd3dImmediateContext->GSSetConstantBuffers( CB_LOC::GEN_SHADOW, 1, &g_CBGenShadow );
		//pd3dImmediateContext->PSSetConstantBuffers( CB_LOC::GEN_SHADOW, 1, &g_CBGenShadow );
	}

	__forceinline void CSSetCameraConstantBuffers(ID3D11DeviceContext1* pd3dImmediateContext) const
	{
		pd3dImmediateContext->CSSetConstantBuffers( CB_LOC::PER_FRAME, 1, &g_CBCamera );
	}

	__forceinline void VSSetCameraConstantBuffers(ID3D11DeviceContext1* pd3dImmediateContext) const
	{
		pd3dImmediateContext->VSSetConstantBuffers( CB_LOC::PER_FRAME, 1, &g_CBCamera );
	}

	__forceinline void HSSetCameraConstantBuffers(ID3D11DeviceContext1* pd3dImmediateContext) const
	{
		pd3dImmediateContext->HSSetConstantBuffers( CB_LOC::PER_FRAME, 1, &g_CBCamera );
	}


	__forceinline void DSetCameraConstantBuffers(ID3D11DeviceContext1* pd3dImmediateContext) const
	{
		pd3dImmediateContext->DSSetConstantBuffers( CB_LOC::PER_FRAME, 1, &g_CBCamera );
	}


	__forceinline void GSSetCameraConstantBuffers(ID3D11DeviceContext1* pd3dImmediateContext) const
	{
		pd3dImmediateContext->GSSetConstantBuffers( CB_LOC::PER_FRAME, 1, &g_CBCamera );
	}


	__forceinline void PSSetCameraConstantBuffers(ID3D11DeviceContext1* pd3dImmediateContext) const
	{
		pd3dImmediateContext->PSSetConstantBuffers( CB_LOC::PER_FRAME, 1, &g_CBCamera );
	}


	void UpdateCameraCBForModel(ID3D11DeviceContext1* pd3dImmediateContext, const DirectX::XMMATRIX& modelToWorldMatrix)
	{
		// Setup the constant buffer for the scene vertex shader
		D3D11_MAPPED_SUBRESOURCE MappedResource;

		pd3dImmediateContext->Map( g_CBCamera, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
		CB_PER_FRAME* pCBcam = ( CB_PER_FRAME* )MappedResource.pData;		
		pCBcam->ModelViewProjection = modelToWorldMatrix * GetViewProjectionMatrix();		
		pCBcam->ModelViewMatrix		= modelToWorldMatrix * m_ViewMatrix;
		pCBcam->ProjectionMatrix	= m_ProjectionMatrix;
		pCBcam->WorldMatrix			= modelToWorldMatrix;
		pCBcam->ViewMatrix			= m_ViewMatrix;
		pCBcam->InverseViewMatrix	= m_inverseViewMatrix;
		pd3dImmediateContext->Unmap( g_CBCamera, 0 );
	}

	// checkme
	void UpdateCameraCBCustom(ID3D11DeviceContext1* pd3dImmediateContext, const DirectX::XMMATRIX& modelViewProjectionMatrix, const DirectX::XMMATRIX& modelToWorldMatrix)
	{
		// Setup the constant buffer for the scene vertex shader
		D3D11_MAPPED_SUBRESOURCE MappedResource;

		pd3dImmediateContext->Map( g_CBCamera, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
		CB_PER_FRAME* pCBcam = ( CB_PER_FRAME* )MappedResource.pData;		
		pCBcam->ModelViewProjection = modelViewProjectionMatrix;		
		pCBcam->ModelViewMatrix		= m_ViewMatrix;
		pCBcam->ProjectionMatrix	= m_ProjectionMatrix;
		pCBcam->WorldMatrix			= modelToWorldMatrix;					// TODO checkme
		pCBcam->ViewMatrix			= m_ViewMatrix;
		pCBcam->InverseViewMatrix	= m_inverseViewMatrix;
		pd3dImmediateContext->Unmap( g_CBCamera, 0 );
	}

	
	//void UpdateCameraCBVoxel(ID3D11DeviceContext1* pd3dImmediateContext, const DirectX::XMMATRIX& modelViewProjectionMatrix, const DirectX::XMMATRIX& modelToWorldMatrix)
	void UpdateCameraCBVoxel(ID3D11DeviceContext1* pd3dImmediateContext, const VoxelGridDefinition& gridDef)
	{
		// Setup the constant buffer for the scene vertex shader
		D3D11_MAPPED_SUBRESOURCE MappedResource;

		pd3dImmediateContext->Map( g_CBCamera, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
		CB_PER_FRAME* pCBcam = ( CB_PER_FRAME* )MappedResource.pData;		
		pCBcam->ModelViewProjection = gridDef.m_MatrixWorldToVoxelProj;		
		pCBcam->ModelViewMatrix		= gridDef.m_WorldMatrix * gridDef.m_VoxelView;//m_ViewMatrix;
		pCBcam->ProjectionMatrix	= gridDef.m_VoxelProj;
		pCBcam->WorldMatrix			= gridDef.m_WorldMatrix;					// TODO checkme
		pCBcam->ViewMatrix			= gridDef.m_VoxelView;
		pCBcam->InverseViewMatrix	= XMMatrixInverse(NULL, gridDef.m_VoxelView);
		pd3dImmediateContext->Unmap( g_CBCamera, 0 );
	}

	void UpdateGenShadowCBCustom(ID3D11DeviceContext1* pd3dImmediateContext, const DirectX::XMMATRIX& mModelView, const DirectX::XMMATRIX& mProjection) {
		// Setup the constant buffer for the scene vertex shader
		D3D11_MAPPED_SUBRESOURCE MappedResource;

		pd3dImmediateContext->Map( g_CBGenShadow, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
		CB_GEN_SHADOW* pCBGenShadow = ( CB_GEN_SHADOW* )MappedResource.pData;		
		pCBGenShadow->g_SceneModelView = mModelView;		
		pCBGenShadow->g_SceneProj = mProjection;		
		pd3dImmediateContext->Unmap( g_CBGenShadow, 0 );

	}

	__forceinline void GPUPerfTimerStart(ID3D11DeviceContext1* pd3dImmediateContext)
	{
		//Sleep(1000);
		//WaitForGPU();
		//if(g_qryTimeActive)		std::cerr << "perf query is active, check your code for nested timings" << std::endl;
		//g_qryTimeActive = true;

		pd3dImmediateContext->Begin(g_qryTimestampDisjoint);		
		pd3dImmediateContext->End(g_qryTimestampStart);
	}

	__forceinline void GPUPerfTimerEnd(ID3D11DeviceContext1* pd3dImmediateContext)
	{
		pd3dImmediateContext->End(g_qryTimestampEnd);
		pd3dImmediateContext->End(g_qryTimestampDisjoint);

		//g_qryTimeActive = false;
	}

	__forceinline double GPUPerfTimerElapsed(ID3D11DeviceContext1* pd3dImmediateContext)
	{
		UINT64 timestampStart;
		UINT64 timestampEnd;

		while (pd3dImmediateContext->GetData(g_qryTimestampDisjoint, NULL, 0, 0) == S_FALSE)
		{
			Sleep(10);       // Wait a bit, but give other threads a chance to run
		}

		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timestampDisjoint;
		while(pd3dImmediateContext->GetData(g_qryTimestampDisjoint, &timestampDisjoint, sizeof(timestampDisjoint), 0) != S_OK);

		while(pd3dImmediateContext->GetData(g_qryTimestampStart, &timestampStart, sizeof(timestampStart), 0) != S_OK);
		while(pd3dImmediateContext->GetData(g_qryTimestampEnd, &timestampEnd, sizeof(timestampEnd), 0) != S_OK);
		
		//pd3dImmediateContext->GetData(g_qryTimestampStart, &timestampStart, sizeof(UINT64), 0);
		//pd3dImmediateContext->GetData(g_qryTimestampEnd, &timestampEnd, sizeof(UINT64), 0) ;


		double elapsed = 0.0;
		if(timestampDisjoint.Disjoint)
			elapsed = -1.0;
		else
			elapsed = double(timestampEnd - timestampStart) / double(timestampDisjoint.Frequency);

		//std::cout << "freq: " << timestampDisjoint.Frequency << std::endl;
		if(g_qryTimeActive)	std::cerr << "perf query is active in GPUPerfTimerElapsed, check your code for nested timings" << std::endl;
		return (float)elapsed*1000.0;		
	}

	void GPUPipelineQueryStart(ID3D11DeviceContext1* pd3dImmediateContext)
	{
		pd3dImmediateContext->Begin(g_pipelineQuery);				
	}

	void GPUPipelineQueryEnd(ID3D11DeviceContext1* pd3dImmediateContext)
	{
		pd3dImmediateContext->End(g_pipelineQuery);				
	}

	unsigned int GPUPipelineQueryResults(ID3D11DeviceContext1* pd3dImmediateContext)
	{
		D3D11_QUERY_DATA_PIPELINE_STATISTICS pipelineQryData;
		while(pd3dImmediateContext->GetData(g_pipelineQuery, &pipelineQryData, sizeof(pipelineQryData), 0) != S_OK);

		unsigned int numPrimitives = (UINT)pipelineQryData.CPrimitives;
		return numPrimitives;
	}

		
	Timer		g_Timer;
	TimingLog	g_TimingLog;
	OpenSubdiv::OsdD3D11ComputeController* g_osdSubdivider;


	// global app settings
	bool		g_bRunSimulation;
	bool		g_bTimingsEnabled;
	bool		g_bShowVoxelization;		
	int			g_editMode;
	bool		g_doEdit;
	
	bool		g_useCulling;
	bool		g_useCullingForRayCast;
	bool		g_voxelDeformationMultiSampling;
	bool		g_withVoxelJittering;
	bool		g_withVoxelOBBRotate;

	bool		g_showIntersections;
	bool		g_adaptiveTessellation;

	float		g_fMipMapBias;
	float		g_fDisplacementScalar;
	float		g_fDisplacementScalarViewSpace;
	float		g_adaptiveVoxelizationScale;	
	
	float		g_fTessellationFactor;

	int			g_memNumColorTiles;	   
	int			g_memNumDisplacementTiles;

	bool		g_memDebugDoPrealloc;

	int			g_memMaxNumTilesPerObject;

	bool		g_withPaintSculptTimings;

	bool		g_profilePipelineStages;
	bool		g_enableOffsetUV;

	bool		g_useCompactedVisibilityOverlap;
	bool		g_useDisplacementConstraints;

	bool		g_showAllocated;
	bool		g_withOverlapUpdate;

	uint32_t	g_maxSubdivisions;

	DirectX::XMFLOAT2 g_mouseScreenPos;
	DirectX::XMFLOAT2 g_mousePrevScreenPos;
	DirectX::XMFLOAT2 g_mouseDirection;
	DirectX::XMFLOAT2 g_mousePrevDirection;

	
	ID3D11ShaderResourceView* g_envMapSRV;

	bool g_qryTimeActive;
	ID3D11Query* g_qryTimestampStart;
	ID3D11Query* g_qryTimestampEnd;
	ID3D11Query* g_qryTimestampDisjoint;

	ID3D11Query* g_pipelineQuery;

private:

	DirectX::XMMATRIX m_ViewProjectionMatrix;	// view projection matrix 
	DirectX::XMMATRIX m_ViewMatrix;				// world to view  matrix			
	DirectX::XMMATRIX m_inverseViewMatrix;		// view  to world matrix	
	DirectX::XMMATRIX m_ProjectionMatrix;		// camera projection matrix	

	ID3D11Buffer*				g_CBCamera;
	ID3D11Buffer*				g_CBGenShadow;
	ID3D11Query*				g_query;
	bool						m_camModified;

	// gpu performance query
	
};

extern App g_app;



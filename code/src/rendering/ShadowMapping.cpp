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
#include "ShadowMapping.h"
#include <DXUTcamera.h>
#include "scene/Scene.h"
#include "utils/Frustum.h"
#include "rendering/RendererTri.h"
#include "rendering/RendererSubD.h"
#include <SDX/DXBuffer.h>
#include <sstream>

#define SHADOW_MAP_RES 2048
#define BLUR_KERNEL_SIZE 3
#define BLUR_BETWEEN_CASCADES_AMOUNT 0.1f
//0.05f

#define SHADOW_MAP_RES_PREPASS SHADOW_MAP_RES
#define WORK_GROUP_SIZE_BLUR 16
#define ROW_TILE_W 256
#define WORKGROUP_SIZE_SCAN 256 
#define SCAN_DISPATCH_THREADS_X 128
// allows depth reduction up to shadow res 4096x4096

// defines for the atomic reduction kernel
#define WORK_GROUP_SIZE_ATOMIC_REDUCTION_X 16
#define WORK_GROUP_SIZE_ATOMIC_REDUCTION_Y 16

using namespace DirectX;

static const XMVECTORF32 g_vFLTMAX = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
static const XMVECTORF32 g_vFLTMIN = { -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX };
static const XMVECTORF32 g_vHalfVector = { 0.5f, 0.5f, 0.5f, 0.5f };
static const XMVECTORF32 g_vMultiplySetzwToZero = { 1.0f, 1.0f, 0.0f, 0.0f };
static const XMVECTORF32 g_vZero = { 0.0f, 0.0f, 0.0f, 0.0f };


static const XMMATRIX g_matTextureScale			= XMMatrixScaling(0.5f, -0.5f, 1.0f );
static const XMMATRIX g_matTextureTranslation	= XMMatrixTranslation(.5f, .5f, 0.f);
static const XMMATRIX g_scaleToTile				= XMMatrixScaling( 1.0f / (float)NUM_CASCADES, 1.0f, 1.0f );

//--------------------------------------------------------------------------------------
// Used to compute an intersection of the orthographic projection and the Scene AABB
//--------------------------------------------------------------------------------------
struct Triangle 
{
	XMVECTOR pt[3];
	BOOL culled;
};

void PrintXMVECTOR_F3(const std::string& msg, FXMVECTOR v)
{
	XMFLOAT4 f3;
	XMStoreFloat4(&f3, v);
	std::cout << msg << " " << f3.x << ", " << f3.y << ", " << f3.z << ", " << f3.w << std::endl;
}

void PrintXMVECTOR_F4(const std::string& msg, FXMVECTOR v)
{
	XMFLOAT4 f3;
	XMStoreFloat4(&f3, v);
	std::cout << msg << " " << f3.x << ", " << f3.y << ", " << f3.z << ", " << f3.w << std::endl;
}

void CreateAABBPoints( XMVECTOR* vAABBPoints, FXMVECTOR vCenter, FXMVECTOR vExtents )
{
	//This map enables us to use a for loop and do vector math.
	static const XMVECTORF32 vExtentsMap[] = 
	{ 
		{ 1.0f,  1.0f, -1.0f, 1.0f}, 
		{-1.0f,  1.0f, -1.0f, 1.0f}, 
		{ 1.0f, -1.0f, -1.0f, 1.0f}, 
		{-1.0f, -1.0f, -1.0f, 1.0f}, 
		{ 1.0f,  1.0f,  1.0f, 1.0f}, 
		{-1.0f,  1.0f,  1.0f, 1.0f}, 
		{ 1.0f, -1.0f,  1.0f, 1.0f}, 
		{-1.0f, -1.0f,  1.0f, 1.0f} 
	};

	for( INT index = 0; index < 8; ++index ) 
	{
		vAABBPoints[index] = XMVectorMultiplyAdd(vExtentsMap[index], vExtents, vCenter ); 
	}

}

void CreateFrustumPointsFromCascadeInterval( float fCascadeIntervalBegin, 
									   FLOAT fCascadeIntervalEnd, 
									   const XMMATRIX &vProjection,
									   XMVECTOR* pvCornerPointsWorld ) 
{

	Frustum vViewFrust;
	ComputeFrustumFromProjection( &vViewFrust, vProjection );
	vViewFrust.Near = fCascadeIntervalBegin;
	vViewFrust.Far = fCascadeIntervalEnd;

	static const XMVECTORU32 vGrabY = {0x00000000,0xFFFFFFFF,0x00000000,0x00000000};
	static const XMVECTORU32 vGrabX = {0xFFFFFFFF,0x00000000,0x00000000,0x00000000};

	XMVECTORF32 vRightTop = {vViewFrust.RightSlope,vViewFrust.TopSlope,1.0f,1.0f};
	XMVECTORF32 vLeftBottom = {vViewFrust.LeftSlope,vViewFrust.BottomSlope,1.0f,1.0f};
	XMVECTORF32 vNear = {vViewFrust.Near,vViewFrust.Near,vViewFrust.Near,1.0f};
	XMVECTORF32 vFar = {vViewFrust.Far,vViewFrust.Far,vViewFrust.Far,1.0f};
	XMVECTOR vRightTopNear = XMVectorMultiply( vRightTop, vNear );
	XMVECTOR vRightTopFar = XMVectorMultiply( vRightTop, vFar );
	XMVECTOR vLeftBottomNear = XMVectorMultiply( vLeftBottom, vNear );
	XMVECTOR vLeftBottomFar = XMVectorMultiply( vLeftBottom, vFar );

	pvCornerPointsWorld[0] = vRightTopNear;
	pvCornerPointsWorld[1] = XMVectorSelect( vRightTopNear, vLeftBottomNear, vGrabX );
	pvCornerPointsWorld[2] = vLeftBottomNear;
	pvCornerPointsWorld[3] = XMVectorSelect( vRightTopNear, vLeftBottomNear,vGrabY );

	pvCornerPointsWorld[4] = vRightTopFar;
	pvCornerPointsWorld[5] = XMVectorSelect( vRightTopFar, vLeftBottomFar, vGrabX );
	pvCornerPointsWorld[6] = vLeftBottomFar;
	pvCornerPointsWorld[7] = XMVectorSelect( vRightTopFar ,vLeftBottomFar, vGrabY );

}

//--------------------------------------------------------------------------------------
// Computing an accurate near and flar plane will decrease surface acne and Peter-panning.
// Surface acne is the term for erroneous self shadowing.  Peter-panning is the effect where
// shadows disappear near the base of an object.
// As offsets are generally used with PCF filtering due self shadowing issues, computing the
// correct near and far planes becomes even more important.
// This concept is not complicated, but the intersection code is.
//--------------------------------------------------------------------------------------
void ComputeNearAndFar( FLOAT& fNearPlane, FLOAT& fFarPlane, FXMVECTOR vLightCameraOrthographicMin, FXMVECTOR vLightCameraOrthographicMax, XMVECTOR* pvPointsInCameraView ) 
{

	// Initialize the near and far planes
	fNearPlane = FLT_MAX;
	fFarPlane = -FLT_MAX;

	Triangle triangleList[16];
	INT iTriangleCnt = 1;

	triangleList[0].pt[0] = pvPointsInCameraView[0];
	triangleList[0].pt[1] = pvPointsInCameraView[1];
	triangleList[0].pt[2] = pvPointsInCameraView[2];
	triangleList[0].culled = false;

	// These are the indices used to tesselate an AABB into a list of triangles.
	static const INT iAABBTriIndexes[] = 
	{
		0,1,2,  1,2,3,
		4,5,6,  5,6,7,
		0,2,4,  2,4,6,
		1,3,5,  3,5,7,
		0,1,4,  1,4,5,
		2,3,6,  3,6,7 
	};

	INT iPointPassesCollision[3];

	// At a high level: 
	// 1. Iterate over all 12 triangles of the AABB.  
	// 2. Clip the triangles against each plane. Create new triangles as needed.
	// 3. Find the min and max z values as the near and far plane.

	//This is easier because the triangles are in camera spacing making the collisions tests simple comparisions.

	float fLightCameraOrthographicMinX = XMVectorGetX( vLightCameraOrthographicMin );
	float fLightCameraOrthographicMaxX = XMVectorGetX( vLightCameraOrthographicMax ); 
	float fLightCameraOrthographicMinY = XMVectorGetY( vLightCameraOrthographicMin );
	float fLightCameraOrthographicMaxY = XMVectorGetY( vLightCameraOrthographicMax );

	for( INT AABBTriIter = 0; AABBTriIter < 12; ++AABBTriIter ) 
	{

		triangleList[0].pt[0] = pvPointsInCameraView[ iAABBTriIndexes[ AABBTriIter*3 + 0 ] ];
		triangleList[0].pt[1] = pvPointsInCameraView[ iAABBTriIndexes[ AABBTriIter*3 + 1 ] ];
		triangleList[0].pt[2] = pvPointsInCameraView[ iAABBTriIndexes[ AABBTriIter*3 + 2 ] ];
		iTriangleCnt = 1;
		triangleList[0].culled = FALSE;

		// Clip each invidual triangle against the 4 frustums.  When ever a triangle is clipped into new triangles, 
		//add them to the list.
		for( INT frustumPlaneIter = 0; frustumPlaneIter < 4; ++frustumPlaneIter ) 
		{

			FLOAT fEdge;
			INT iComponent;

			if( frustumPlaneIter == 0 ) 
			{
				fEdge = fLightCameraOrthographicMinX; // todo make float temp
				iComponent = 0;
			} 
			else if( frustumPlaneIter == 1 ) 
			{
				fEdge = fLightCameraOrthographicMaxX;
				iComponent = 0;
			} 
			else if( frustumPlaneIter == 2 ) 
			{
				fEdge = fLightCameraOrthographicMinY;
				iComponent = 1;
			} 
			else 
			{
				fEdge = fLightCameraOrthographicMaxY;
				iComponent = 1;
			}

			for( INT triIter=0; triIter < iTriangleCnt; ++triIter ) 
			{
				// We don't delete triangles, so we skip those that have been culled.
				if( !triangleList[triIter].culled ) 
				{
					INT iInsideVertCount = 0;
					XMVECTOR tempOrder;
					// Test against the correct frustum plane.
					// This could be written more compactly, but it would be harder to understand.

					if( frustumPlaneIter == 0 ) 
					{
						for( INT triPtIter=0; triPtIter < 3; ++triPtIter ) 
						{
							if( XMVectorGetX( triangleList[triIter].pt[triPtIter] ) >
								XMVectorGetX( vLightCameraOrthographicMin ) ) 
							{ 
								iPointPassesCollision[triPtIter] = 1;
							}
							else 
							{
								iPointPassesCollision[triPtIter] = 0;
							}
							iInsideVertCount += iPointPassesCollision[triPtIter];
						}
					}
					else if( frustumPlaneIter == 1 ) 
					{
						for( INT triPtIter=0; triPtIter < 3; ++triPtIter ) 
						{
							if( XMVectorGetX( triangleList[triIter].pt[triPtIter] ) < 
								XMVectorGetX( vLightCameraOrthographicMax ) )
							{
								iPointPassesCollision[triPtIter] = 1;
							}
							else
							{ 
								iPointPassesCollision[triPtIter] = 0;
							}
							iInsideVertCount += iPointPassesCollision[triPtIter];
						}
					}
					else if( frustumPlaneIter == 2 ) 
					{
						for( INT triPtIter=0; triPtIter < 3; ++triPtIter ) 
						{
							if( XMVectorGetY( triangleList[triIter].pt[triPtIter] ) > 
								XMVectorGetY( vLightCameraOrthographicMin ) ) 
							{
								iPointPassesCollision[triPtIter] = 1;
							}
							else 
							{
								iPointPassesCollision[triPtIter] = 0;
							}
							iInsideVertCount += iPointPassesCollision[triPtIter];
						}
					}
					else 
					{
						for( INT triPtIter=0; triPtIter < 3; ++triPtIter ) 
						{
							if( XMVectorGetY( triangleList[triIter].pt[triPtIter] ) < 
								XMVectorGetY( vLightCameraOrthographicMax ) ) 
							{
								iPointPassesCollision[triPtIter] = 1;
							}
							else 
							{
								iPointPassesCollision[triPtIter] = 0;
							}
							iInsideVertCount += iPointPassesCollision[triPtIter];
						}
					}

					// Move the points that pass the frustum test to the begining of the array.
					if( iPointPassesCollision[1] && !iPointPassesCollision[0] ) 
					{
						tempOrder =  triangleList[triIter].pt[0];   
						triangleList[triIter].pt[0] = triangleList[triIter].pt[1];
						triangleList[triIter].pt[1] = tempOrder;
						iPointPassesCollision[0] = TRUE;            
						iPointPassesCollision[1] = FALSE;            
					}
					if( iPointPassesCollision[2] && !iPointPassesCollision[1] ) 
					{
						tempOrder =  triangleList[triIter].pt[1];   
						triangleList[triIter].pt[1] = triangleList[triIter].pt[2];
						triangleList[triIter].pt[2] = tempOrder;
						iPointPassesCollision[1] = TRUE;            
						iPointPassesCollision[2] = FALSE;                        
					}
					if( iPointPassesCollision[1] && !iPointPassesCollision[0] ) 
					{
						tempOrder =  triangleList[triIter].pt[0];   
						triangleList[triIter].pt[0] = triangleList[triIter].pt[1];
						triangleList[triIter].pt[1] = tempOrder;
						iPointPassesCollision[0] = TRUE;            
						iPointPassesCollision[1] = FALSE;            
					}

					if( iInsideVertCount == 0 ) 
					{ // All points failed. We're done,  
						triangleList[triIter].culled = true;
					}
					else if( iInsideVertCount == 1 ) 
					{// One point passed. Clip the triangle against the Frustum plane
						triangleList[triIter].culled = false;

						// 
						XMVECTOR vVert0ToVert1 = triangleList[triIter].pt[1] - triangleList[triIter].pt[0];
						XMVECTOR vVert0ToVert2 = triangleList[triIter].pt[2] - triangleList[triIter].pt[0];

						// Find the collision ratio.
						FLOAT fHitPointTimeRatio = fEdge - XMVectorGetByIndex( triangleList[triIter].pt[0], iComponent ) ;
						// Calculate the distance along the vector as ratio of the hit ratio to the component.
						FLOAT fDistanceAlongVector01 = fHitPointTimeRatio / XMVectorGetByIndex( vVert0ToVert1, iComponent );
						FLOAT fDistanceAlongVector02 = fHitPointTimeRatio / XMVectorGetByIndex( vVert0ToVert2, iComponent );
						// Add the point plus a percentage of the vector.
						vVert0ToVert1 *= fDistanceAlongVector01;
						vVert0ToVert1 += triangleList[triIter].pt[0];
						vVert0ToVert2 *= fDistanceAlongVector02;
						vVert0ToVert2 += triangleList[triIter].pt[0];

						triangleList[triIter].pt[1] = vVert0ToVert2;
						triangleList[triIter].pt[2] = vVert0ToVert1;

					}
					else if( iInsideVertCount == 2 ) 
					{ // 2 in  // tesselate into 2 triangles


						// Copy the triangle\(if it exists) after the current triangle out of
						// the way so we can override it with the new triangle we're inserting.
						triangleList[iTriangleCnt] = triangleList[triIter+1];

						triangleList[triIter].culled = false;
						triangleList[triIter+1].culled = false;

						// Get the vector from the outside point into the 2 inside points.
						XMVECTOR vVert2ToVert0 = triangleList[triIter].pt[0] - triangleList[triIter].pt[2];
						XMVECTOR vVert2ToVert1 = triangleList[triIter].pt[1] - triangleList[triIter].pt[2];

						// Get the hit point ratio.
						FLOAT fHitPointTime_2_0 =  fEdge - XMVectorGetByIndex( triangleList[triIter].pt[2], iComponent );
						FLOAT fDistanceAlongVector_2_0 = fHitPointTime_2_0 / XMVectorGetByIndex( vVert2ToVert0, iComponent );
						// Calcaulte the new vert by adding the percentage of the vector plus point 2.
						vVert2ToVert0 *= fDistanceAlongVector_2_0;
						vVert2ToVert0 += triangleList[triIter].pt[2];

						// Add a new triangle.
						triangleList[triIter+1].pt[0] = triangleList[triIter].pt[0];
						triangleList[triIter+1].pt[1] = triangleList[triIter].pt[1];
						triangleList[triIter+1].pt[2] = vVert2ToVert0;

						//Get the hit point ratio.
						FLOAT fHitPointTime_2_1 =  fEdge - XMVectorGetByIndex( triangleList[triIter].pt[2], iComponent ) ;
						FLOAT fDistanceAlongVector_2_1 = fHitPointTime_2_1 / XMVectorGetByIndex( vVert2ToVert1, iComponent );
						vVert2ToVert1 *= fDistanceAlongVector_2_1;
						vVert2ToVert1 += triangleList[triIter].pt[2];
						triangleList[triIter].pt[0] = triangleList[triIter+1].pt[1];
						triangleList[triIter].pt[1] = triangleList[triIter+1].pt[2];
						triangleList[triIter].pt[2] = vVert2ToVert1;
						// Cncrement triangle count and skip the triangle we just inserted.
						++iTriangleCnt;
						++triIter;


					}
					else 
					{ // all in
						triangleList[triIter].culled = false;

					}
				}// end if !culled loop            
			}
		}
		for( INT index=0; index < iTriangleCnt; ++index ) 
		{
			if( !triangleList[index].culled ) 
			{
				// Set the near and far plan and the min and max z values respectivly.
				for( int vertind = 0; vertind < 3; ++ vertind ) 
				{
					float fTriangleCoordZ = XMVectorGetZ( triangleList[index].pt[vertind] );
					if( fNearPlane > fTriangleCoordZ ) 
					{
						fNearPlane = fTriangleCoordZ;
					}
					if( fFarPlane  <fTriangleCoordZ ) 
					{
						fFarPlane = fTriangleCoordZ;
					}
				}
			}
		}
	}    

}

ShadowMapping::ShadowMapping()
{
	m_showCascades = false;

	// these settings must match render with shadow settings
	//m_eSelectedCascadesFit = FIT_PROJECTION_TO::SCENE;
	m_eSelectedCascadesFit = FIT_PROJECTION_TO::CASCADES;
	//m_eSelectedNearFarFit = FIT_NEAR_FAR::AABB;
	m_eSelectedNearFarFit = FIT_NEAR_FAR::SCENE_AABB;
	//m_eSelectedNearFarFit = FIT_NEAR_FAR::ZERO_ONE;
	//m_ePartitionMode = PartitionMode::MANUAL;
	m_ePartitionMode = PartitionMode::MANUAL;
	m_bMoveLightTexelSize = true;
	m_stabilizeCascades = false;

	m_pssmLambda = 0.01f;

	m_fBlurBetweenCascadesAmount = BLUR_BETWEEN_CASCADES_AMOUNT;

#if NUM_CASCADES == 3
	m_fSplitDistance[0] = 0.05f;
	m_fSplitDistance[1] = 0.25f;
	m_fSplitDistance[2] = 1.f;
#else
	m_fSplitDistance[0] = 0.05f;
	m_fSplitDistance[1] = 0.15f;
	m_fSplitDistance[2] = 0.5f;
	m_fSplitDistance[3] = 1.0f;
#endif
	m_fCascadePartitionsMax = 1.f;
	m_fCascadePartitionsMin = 0.f;

	m_RenderOneTileVP.Width  = SHADOW_MAP_RES;
	m_RenderOneTileVP.Height = SHADOW_MAP_RES;
	m_RenderOneTileVP.MinDepth = 0.f;
	m_RenderOneTileVP.MaxDepth = 1.f;
	m_RenderOneTileVP.TopLeftX = 0.f;
	m_RenderOneTileVP.TopLeftY = 0.f;

	m_RenderPrepassVP.Width  = SHADOW_MAP_RES_PREPASS;
	m_RenderPrepassVP.Height = SHADOW_MAP_RES_PREPASS;
	m_RenderPrepassVP.MinDepth = 0.f;
	m_RenderPrepassVP.MaxDepth = 1.f;
	m_RenderPrepassVP.TopLeftX = 0.f;
	m_RenderPrepassVP.TopLeftY = 0.f;

	m_ppsQuadBlurX = NULL;
	m_ppsQuadBlurY = NULL;
	m_pvsQuadBlur  = NULL;


	

	
	m_pVarianceShadowArrayTEX = NULL;
	for (int index=0; index < NUM_CASCADES; ++index) 
	{
		m_pVarianceShadowArraySliceRTV[index]  = NULL;
		m_pVarianceShadowArraySliceSRV[index]  = NULL;
		m_pVarianceShadowArraySliceUAV[index]  = NULL;
	}
	m_pVarianceShadowArraySRV = NULL;;

	m_pShadowTempTEX = NULL;
	m_pShadowTempRTV = NULL;
	m_pShadowTempSRV = NULL;
	m_pShadowTempUAV = NULL;

	m_pDepthBufferTextureArray = NULL;
	m_pDepthBufferDSV = NULL;
	m_pDepthBufferSRV = NULL;

	m_pDepthBufferArrayDSV = NULL;
	m_pVarianceShadowArrayRTV = NULL;

	m_pSamShadowPoint		= NULL;
	m_pSamShadowLinear		= NULL;
	m_pSamShadowAnisotropic = NULL;
	m_pSamShadowLinearClamp = NULL;

	m_blurXCS=NULL;
	m_blurYCS=NULL;
	m_blurXSharedMemCS = NULL;
	m_blurYSharedMemCS = NULL;
	m_shadowCascadesProjCB = NULL;

	
	// depth reduction scan
	m_useDepthReduction = true;
	m_depthReductionFrameCounter = 0;
	for(int i = 0 ;i<3;++i)
	{	
		m_depthReductionResultStagingBUF[i]	= NULL;
		m_depthReductionResultBUF[i]		= NULL;
		m_depthReductionResultUAV[i]		= NULL;
	}

	m_depthScanBucketsBUF				= NULL;
	m_depthScanBucketsResultsBUF		= NULL;
	m_depthScanBucketsBlockResultsBUF	= NULL;

	m_depthScanBucketsSRV				= NULL;
	m_depthScanBucketsResultsSRV		= NULL;
	m_depthScanBucketsBlockResultsSRV	= NULL;

	m_depthScanBucketsUAV				= NULL;
	m_depthScanBucketsResultsUAV		= NULL;
	m_depthScanBucketsBlockResultsUAV	= NULL;

	m_depthScanBucketCS					= NULL;	
	m_depthScanBucketResultsCS			= NULL;	
	m_depthScanBucketBlockResultsCS		= NULL;	

	m_depthScanWriteResultCS = NULL;
	m_depthReductionAtomicCS = NULL;
	m_depthReductionAtomicCS = NULL;

	m_depthReductionCB = NULL;

	m_useBlurCS			 = false;
	m_useBlurSharedMemCS = false;
	m_useSinglePassShadow = false;
	

}

ShadowMapping::~ShadowMapping()
{
	Destroy();
}

HRESULT ShadowMapping::Create( ID3D11Device1* pd3dDevice )
{
	HRESULT hr = S_OK;

	V_RETURN(DXUTCreateBuffer(pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_CASCADES_PROJECTION), D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, m_shadowCascadesProjCB));

	DXGI_FORMAT shadowBufferFormat = DXGI_FORMAT_R32G32_FLOAT;

	ID3DBlob* pBlob = NULL;
	
	// blur kernels
	char blurSize[10];
	sprintf_s(blurSize, "%d", BLUR_KERNEL_SIZE);
	std::cout << "blur size: " << BLUR_KERNEL_SIZE << std::endl;
	char shadowRes[10];
	sprintf_s(shadowRes, "%d", SHADOW_MAP_RES);
	D3D_SHADER_MACRO blur_macro[]	= { { "BLUR_KERNEL_SIZE", blurSize}, {"SHADOW_MAP_RES", shadowRes}, { 0 } };	

	m_ppsQuadBlurX = g_shaderManager.AddPixelShader( L"shader/CascadeShadowBlur.hlsl", "PSBlurX", "ps_5_0", &pBlob, blur_macro);
	m_ppsQuadBlurY = g_shaderManager.AddPixelShader( L"shader/CascadeShadowBlur.hlsl", "PSBlurY", "ps_5_0", &pBlob, blur_macro);
	char numcascades[5];
	sprintf_s(numcascades, "%d", NUM_CASCADES);
	D3D_SHADER_MACRO debugcascades_macro[]	= { { "NUM_CASCADES", numcascades}, { 0 } };
	m_visShadowPS  = g_shaderManager.AddPixelShader( L"shader/CascadeShadowBlur.hlsl", "PSDebugShadow", "ps_5_0", &pBlob, debugcascades_macro);
	m_pvsQuadBlur  = g_shaderManager.AddVertexShader(L"shader/CascadeShadowBlur.hlsl", "VSMain",  "vs_5_0", &pBlob);

	char workgroupSize[10];
	sprintf_s(workgroupSize, "%d", WORK_GROUP_SIZE_BLUR);
	D3D_SHADER_MACRO blur_macro_cs[]	= { { "BLUR_KERNEL_SIZE", blurSize}, {"SHADOW_MAP_RES", shadowRes}, {"WORK_GROUP_SIZE_BLUR", workgroupSize}, { 0 } };	
	m_blurXCS = g_shaderManager.AddComputeShader(L"shader/CascadeShadowBlur.hlsl", "ShadowBlurXCS",  "cs_5_0", &pBlob, blur_macro_cs);
	m_blurYCS = g_shaderManager.AddComputeShader(L"shader/CascadeShadowBlur.hlsl", "ShadowBlurYCS",  "cs_5_0", &pBlob, blur_macro_cs);

	char blurLine[10];
	sprintf_s(blurLine, "%d", ROW_TILE_W);
	D3D_SHADER_MACRO blur_macro_shared_cs[]	= { { "BLUR_KERNEL_SIZE", blurSize}, {"SHADOW_MAP_RES", shadowRes}, {"ROW_TILE_W", blurLine}, { 0 } };
	m_blurXSharedMemCS = g_shaderManager.AddComputeShader(L"shader/CascadeShadowBlur.hlsl", "ShadowBlurSharedMemXCS",  "cs_5_0", &pBlob, blur_macro_shared_cs);
	m_blurYSharedMemCS = g_shaderManager.AddComputeShader(L"shader/CascadeShadowBlur.hlsl", "ShadowBlurSharedMemYCS",  "cs_5_0", &pBlob, blur_macro_shared_cs);

	SAFE_RELEASE(pBlob);

	D3D11_RASTERIZER_DESC drd = 
	{
		D3D11_FILL_SOLID,//D3D11_FILL_MODE FillMode;
		D3D11_CULL_BACK,//D3D11_CULL_MODE CullMode;  // CHECKME was NONE
		FALSE,//BOOL FrontCounterClockwise;
		0,//INT DepthBias;
		0.0,//FLOAT DepthBiasClamp;
		0.0,//FLOAT SlopeScaledDepthBias;
		TRUE,//BOOL DepthClipEnable;
		FALSE,//BOOL ScissorEnable;
		TRUE,//BOOL MultisampleEnable;
		FALSE//BOOL AntialiasedLineEnable;   
	};

	
	// Setting the slope scale depth biase greatly decreases surface acne and incorrect self shadowing.
	drd.SlopeScaledDepthBias = 1.0;
	pd3dDevice->CreateRasterizerState( &drd, &m_prsShadow ); 
	//CHECKME TODO backface culling
	DXUT_SetDebugName(m_prsShadow,"VSM Shadow RS");

	DXUTCreateBuffer(pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_LIGHT), D3D11_CPU_ACCESS_WRITE,  D3D11_USAGE_DYNAMIC, m_cbLight);
	DXUT_SetDebugName( m_cbLight, "VSM CB_ALL_SHADOW_DATA" ); 
	

	D3D11_SAMPLER_DESC SamDescShad = 
	{
		D3D11_FILTER_ANISOTROPIC,// D3D11_FILTER Filter;
		D3D11_TEXTURE_ADDRESS_CLAMP, //D3D11_TEXTURE_ADDRESS_MODE AddressU;
		D3D11_TEXTURE_ADDRESS_CLAMP, //D3D11_TEXTURE_ADDRESS_MODE AddressV;
		D3D11_TEXTURE_ADDRESS_BORDER, //D3D11_TEXTURE_ADDRESS_MODE AddressW;
		0,//FLOAT MipLODBias;
		16,//UINT MaxAnisotropy;
		D3D11_COMPARISON_NEVER , //D3D11_COMPARISON_FUNC ComparisonFunc;
		0.0,0.0,0.0,0.0,//FLOAT BorderColor[ 4 ];
		0,//FLOAT MinLOD;
		0//FLOAT MaxLOD;   
	};
	
	SamDescShad.MaxAnisotropy = 16;
	V_RETURN( pd3dDevice->CreateSamplerState( &SamDescShad, &m_pSamShadowAnisotropic) );
	DXUT_SetDebugName( m_pSamShadowAnisotropic, "VSM Shadow Sampler Anisotropic" );

	SamDescShad.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SamDescShad.MaxAnisotropy = 0;
	V_RETURN( pd3dDevice->CreateSamplerState( &SamDescShad, &m_pSamShadowLinear ) );
	DXUT_SetDebugName( m_pSamShadowLinear, "VSM Shadow Linear Sampler" );
	
	SamDescShad.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	V_RETURN( pd3dDevice->CreateSamplerState( &SamDescShad, &m_pSamShadowPoint ) );
	DXUT_SetDebugName( m_pSamShadowPoint, "VSM Shadow Sampler Point" );

	SamDescShad.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SamDescShad.MaxAnisotropy = 0;
	V_RETURN( pd3dDevice->CreateSamplerState( &SamDescShad, &m_pSamShadowLinearClamp ) );
	

	

	// viewport
	m_RenderOneTileVP.Height = (FLOAT)SHADOW_MAP_RES;
	m_RenderOneTileVP.Width = (FLOAT)SHADOW_MAP_RES;
	m_RenderOneTileVP.MaxDepth = 1.0f;
	m_RenderOneTileVP.MinDepth = 0.0f;
	m_RenderOneTileVP.TopLeftX = 0.0f;
	m_RenderOneTileVP.TopLeftY = 0.0f;


	D3D11_TEXTURE2D_DESC depthTexDesc = {
		SHADOW_MAP_RES,				//UINT Width;
		SHADOW_MAP_RES,				//UINT Height;
		1,							//UINT MipLevels;
		NUM_CASCADES,				//UINT ArraySize;
		shadowBufferFormat,			//DXGI_FORMAT Format;
		1,							//DXGI_SAMPLE_DESC SampleDesc;
		0,
		D3D11_USAGE_DEFAULT,		//D3D11_USAGE Usage;
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,//UINT BindFlags;
		0,							//UINT CPUAccessFlags;
		0							//UINT MiscFlags;    
	};
	

	V_RETURN( pd3dDevice->CreateTexture2D( &depthTexDesc, NULL, &m_pVarianceShadowArrayTEX  ) );
	DXUT_SetDebugName( m_pVarianceShadowArrayTEX, "VSM ShadowMap Var Array" );

	depthTexDesc.ArraySize = 1;
	V_RETURN( pd3dDevice->CreateTexture2D( &depthTexDesc, NULL, &m_pShadowTempTEX  ) );
	DXUT_SetDebugName( m_pShadowTempTEX, "VSM ShadowMap Temp Blur" );


	//DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;//DXGI_FORMAT_R32_TYPELESS;
	DXGI_FORMAT depthFormat = DXGI_FORMAT_R32_TYPELESS;
	depthTexDesc.Format = depthFormat; 
	depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL  | D3D11_BIND_SHADER_RESOURCE;
	depthTexDesc.ArraySize = NUM_CASCADES;
	V_RETURN( pd3dDevice->CreateTexture2D( &depthTexDesc, NULL, &m_pDepthBufferTextureArray  ) );
	DXUT_SetDebugName( m_pDepthBufferTextureArray, "VSM Temp ShadowDepth" );


	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilDesc;
	ZeroMemory(&depthStencilDesc, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
	depthStencilDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
	depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;//depthFormat;
	depthStencilDesc.Texture2DArray.FirstArraySlice = 0;
	depthStencilDesc.Texture2DArray.ArraySize = NUM_CASCADES;
	depthStencilDesc.Texture2DArray.MipSlice = 0;
	V_RETURN( pd3dDevice->CreateDepthStencilView( m_pDepthBufferTextureArray, &depthStencilDesc, &m_pDepthBufferArrayDSV ) );
	DXUT_SetDebugName( m_pDepthBufferArrayDSV, "VSM Temp ShadowDepth Array Depth Stencil View" );

	depthStencilDesc.Texture2DArray.ArraySize = 1;
	V_RETURN( pd3dDevice->CreateDepthStencilView( m_pDepthBufferTextureArray, &depthStencilDesc, &m_pDepthBufferDSV ) );
	DXUT_SetDebugName( m_pDepthBufferDSV, "VSM Temp ShadowDepth Single Depth Stencil View" );


	D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
	ZeroMemory(&descUAV, sizeof(descUAV));
	descUAV.Format = shadowBufferFormat;	
	descUAV.ViewDimension =  D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
	descUAV.Texture2DArray.MipSlice = 0;	

	D3D11_RENDER_TARGET_VIEW_DESC descRenderTargets;
	ZeroMemory(&descRenderTargets, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
	descRenderTargets.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	descRenderTargets.Format = shadowBufferFormat;
	descRenderTargets.Texture2DArray.MipSlice = 0;
	descRenderTargets.Texture2DArray.FirstArraySlice = 0;
	descRenderTargets.Texture2DArray.ArraySize = NUM_CASCADES;
	V_RETURN( pd3dDevice->CreateRenderTargetView ( m_pVarianceShadowArrayTEX, &descRenderTargets, &m_pVarianceShadowArrayRTV ) );

	for( int index = 0; index < NUM_CASCADES; ++index ) 
	{
		descRenderTargets.Texture2DArray.FirstArraySlice = index;
		descRenderTargets.Texture2DArray.ArraySize = 1;

		descUAV.Texture2DArray.FirstArraySlice = index;
		descUAV.Texture2DArray.ArraySize = 1;	

		V_RETURN( pd3dDevice->CreateRenderTargetView ( m_pVarianceShadowArrayTEX, &descRenderTargets, &m_pVarianceShadowArraySliceRTV[index] ) );
		V_RETURN( pd3dDevice->CreateUnorderedAccessView(m_pVarianceShadowArrayTEX, &descUAV , &m_pVarianceShadowArraySliceUAV[index]));

		#if defined(_DEBUG) || defined(PROFILE)
		char str[64];
		sprintf_s( str, sizeof(str), "VSM Cascaded Var Array (%d) RTV", index );
		DXUT_SetDebugName( m_pVarianceShadowArraySliceRTV[index], str );
		#endif
	}	


	descRenderTargets.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	descRenderTargets.Texture2D.MipSlice = 0;

	V_RETURN( pd3dDevice->CreateRenderTargetView ( m_pShadowTempTEX, &descRenderTargets,	&m_pShadowTempRTV ) );
	DXUT_SetDebugName( m_pShadowTempRTV, "VSM Cascaded SM Temp Blur RTV" );

	D3D11_SHADER_RESOURCE_VIEW_DESC descSRV = {		shadowBufferFormat,		D3D11_SRV_DIMENSION_TEXTURE2DARRAY,		};
		
	descSRV.Texture2DArray.ArraySize = NUM_CASCADES;
	descSRV.Texture2DArray.FirstArraySlice = 0;
	descSRV.Texture2DArray.MipLevels = 1;
	descSRV.Texture2DArray.MostDetailedMip = 0;

	V_RETURN( pd3dDevice->CreateShaderResourceView( m_pVarianceShadowArrayTEX, &descSRV, &m_pVarianceShadowArraySRV ) );
	DXUT_SetDebugName( m_pVarianceShadowArraySRV, "VSM Cascaded SM Var Array SRV" );

	for( int index = 0; index < NUM_CASCADES; ++index ) 
	{
		descSRV.Texture2DArray.ArraySize = 1;
		descSRV.Texture2DArray.FirstArraySlice = index;
		descSRV.Texture2DArray.MipLevels = 1;
		descSRV.Texture2DArray.MostDetailedMip = 0;
		V_RETURN( pd3dDevice->CreateShaderResourceView( m_pVarianceShadowArrayTEX, 
			&descSRV, &m_pVarianceShadowArraySliceSRV[index] ) );

		#if defined(_DEBUG) || defined(PROFILE)
			char str[64];
			sprintf_s( str, sizeof(str), "VSM Cascaded SM Var Array (%d) SRV", index );
			DXUT_SetDebugName( m_pVarianceShadowArraySliceSRV[index], str );
		#endif
	}

	descSRV.Texture2DArray.ArraySize = 1;
	descSRV.Texture2DArray.FirstArraySlice = 0;
	descSRV.Texture2DArray.MipLevels = 1;
	descSRV.Texture2DArray.MostDetailedMip = 0;
	V_RETURN( pd3dDevice->CreateShaderResourceView( m_pShadowTempTEX, &descSRV, &m_pShadowTempSRV ) );
	DXUT_SetDebugName( m_pShadowTempSRV, "VSM Cascaded SM Temp Blur SRV" );
	
	descSRV.Format = DXGI_FORMAT_R32_FLOAT;//depthFormat;
	V_RETURN( pd3dDevice->CreateShaderResourceView( m_pDepthBufferTextureArray, &descSRV, &m_pDepthBufferSRV ) );
	DXUT_SetDebugName( m_pDepthBufferSRV, "VSM Cascaded m_pDepthBufferSRV" );

	descUAV.Texture2DArray.ArraySize = 1;
	descUAV.Texture2DArray.FirstArraySlice = 0;
	descUAV.Texture2DArray.MipSlice = 0;	
	V_RETURN( pd3dDevice->CreateUnorderedAccessView( m_pShadowTempTEX, &descUAV, &m_pShadowTempUAV ) );
	DXUT_SetDebugName( m_pShadowTempUAV, "VSM Cascaded SM Temp Blur UAV" );
		
	//V_RETURN(DXUTCreateBuffer(pd3dDevice,D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_LIGHT), D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, m_cbLight));
	//DXUT_DEBUG_NAME(m_cbLight, "Light Constant Buffer");


	// depth reduction shaders and data
	if(1){
		m_depthMinMax = XMFLOAT2(0,1);
		m_scanBucketSize = WORKGROUP_SIZE_SCAN;
		m_scanBucketBlockSize = m_scanBucketSize;
		m_numScanBuckets = WORKGROUP_SIZE_SCAN*WORKGROUP_SIZE_SCAN;	
		m_numScanBucketBlocks = WORKGROUP_SIZE_SCAN;

		DXGI_FORMAT formatScan = DXGI_FORMAT_R32G32_FLOAT; 
		
		UINT numElements;
		UINT elemSize = sizeof(XMFLOAT2);

		descSRV.ViewDimension		= D3D11_SRV_DIMENSION_BUFFER;
		descSRV.Format				= formatScan;
		descSRV.Buffer.FirstElement = 0;

		descUAV.ViewDimension		= D3D11_UAV_DIMENSION_BUFFER;
		descUAV.Format				= formatScan;
		descUAV.Buffer.FirstElement = 0;
		descUAV.Buffer.Flags		= 0;

		// aux buffer for scan buckets
		numElements = SHADOW_MAP_RES_PREPASS*SHADOW_MAP_RES_PREPASS;
		UINT byteSize = numElements*elemSize;
		descSRV.Buffer.NumElements = numElements;		
		descUAV.Buffer.NumElements = numElements;	

		V_RETURN(DXUTCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, byteSize, 0, D3D11_USAGE_DEFAULT, m_depthScanBucketsBUF)); 						
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_depthScanBucketsBUF,		&descSRV, &m_depthScanBucketsSRV));		
		V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_depthScanBucketsBUF,	&descUAV, &m_depthScanBucketsUAV));


		// aux buffer for bucket results
		numElements = m_numScanBuckets;
		byteSize = numElements*elemSize;
		descSRV.Buffer.NumElements = numElements;		
		descUAV.Buffer.NumElements = numElements;	
		V_RETURN(DXUTCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, byteSize, 0, D3D11_USAGE_DEFAULT, m_depthScanBucketsResultsBUF)); 						
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_depthScanBucketsResultsBUF,		&descSRV, &m_depthScanBucketsResultsSRV));		
		V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_depthScanBucketsResultsBUF,	&descUAV, &m_depthScanBucketsResultsUAV));

		// aux buffer for bucket block results
		numElements = m_numScanBucketBlocks;
		byteSize = numElements*elemSize;
		descSRV.Buffer.NumElements = numElements;		
		descUAV.Buffer.NumElements = numElements;	
		V_RETURN(DXUTCreateBuffer(pd3dDevice, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, byteSize, 0, D3D11_USAGE_DEFAULT, m_depthScanBucketsBlockResultsBUF)); 						
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_depthScanBucketsBlockResultsBUF,	&descSRV, &m_depthScanBucketsBlockResultsSRV));		
		V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_depthScanBucketsBlockResultsBUF,	&descUAV, &m_depthScanBucketsBlockResultsUAV));


		// aux buffer for reduction result
		numElements = 1;
		byteSize = numElements*elemSize;			
		descUAV.Buffer.NumElements = numElements;
		//descUAV.Format =  DXGI_FORMAT_R32_UINT
		D3D11_UNORDERED_ACCESS_VIEW_DESC descReudctionAtomicUAV = descUAV;
		descReudctionAtomicUAV.Format = DXGI_FORMAT_R32_UINT;
		descReudctionAtomicUAV.Buffer.NumElements = 2;

		XMFLOAT2 initMinMax[] = {XMFLOAT2(0,1)};
		for(int i = 0; i < 3; ++i)
		{		
			V_RETURN(DXUTCreateBuffer(DXUTGetD3D11Device(), 0, byteSize,  D3D11_CPU_ACCESS_READ, D3D11_USAGE_STAGING, m_depthReductionResultStagingBUF[i],&initMinMax[0] ));
			V_RETURN(DXUTCreateBuffer(pd3dDevice, D3D11_BIND_UNORDERED_ACCESS, byteSize, 0, D3D11_USAGE_DEFAULT, m_depthReductionResultBUF[i],&initMinMax[0]));
			V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_depthReductionResultBUF[i],	&descUAV, &m_depthReductionResultUAV[i]));

			//V_RETURN(DXUTCreateBuffer(pd3dDevice, D3D11_BIND_UNORDERED_ACCESS, byteSize, 0, D3D11_USAGE_DEFAULT, m_depthReductionResultReductionAtomicBUF[i]));
			V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_depthReductionResultBUF[i],	&descReudctionAtomicUAV, &m_depthReductionResultReductionAtomicUAV[i]));

		}
		// depth reduction settings cb
		V_RETURN(DXUTCreateBuffer(pd3dDevice, D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_DEPTH_REDUCTION), D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, m_depthReductionCB));


		// shaders
		char workgroup_size_scan[10];
		sprintf_s(workgroup_size_scan, "%d", WORKGROUP_SIZE_SCAN);

		char dispatch_dim_x[10];
		sprintf_s(dispatch_dim_x, "%d", SCAN_DISPATCH_THREADS_X);

		char shadowMapRes[10];
		sprintf_s(shadowMapRes, "%d", SHADOW_MAP_RES_PREPASS);

		D3D_SHADER_MACRO scan_macro[]	= { { "WORKGROUP_SIZE_SCAN", workgroup_size_scan}, {"DISPATCH_THREADS_X", dispatch_dim_x}, {"SHADOW_MAP_RES", shadowMapRes}, { 0 } };	

		m_depthScanBucketCS					= g_shaderManager.AddComputeShader(L"shader/ShadowDepthReduction.hlsl", "ScanBucketCS",  "cs_5_0", &pBlob, scan_macro);	
		m_depthScanBucketResultsCS			= g_shaderManager.AddComputeShader(L"shader/ShadowDepthReduction.hlsl", "ScanBucketResultsCS",  "cs_5_0", &pBlob, scan_macro);	
		m_depthScanBucketBlockResultsCS		= g_shaderManager.AddComputeShader(L"shader/ShadowDepthReduction.hlsl", "ScanBucketBlockResultsCS",  "cs_5_0", &pBlob, scan_macro);	
		m_depthScanWriteResultCS				= g_shaderManager.AddComputeShader(L"shader/ShadowDepthReduction.hlsl", "WriteDepthReductionResultCS",  "cs_5_0", &pBlob, scan_macro);	

		char workgroup_size_x[10];
		sprintf_s(workgroup_size_x, "%d", WORK_GROUP_SIZE_ATOMIC_REDUCTION_X);
		char workgroup_size_y[10];
		sprintf_s(workgroup_size_y, "%d", WORK_GROUP_SIZE_ATOMIC_REDUCTION_Y);


		D3D_SHADER_MACRO reduction_macro[]	= { { "WORK_GROUP_SIZE_ATOMIC_REDUCTION_X", workgroup_size_x},
												{ "WORK_GROUP_SIZE_ATOMIC_REDUCTION_Y", workgroup_size_y},
												{ "SHADOW_MAP_RES", shadowMapRes}, { 0 } };	

		m_depthReductionAtomicCS = g_shaderManager.AddComputeShader(L"shader/ShadowDepthReduction.hlsl", "ReductionAtomic",  "cs_5_0", &pBlob, reduction_macro);	

		D3D_SHADER_MACRO reduction_MSAA_macro[]	= { { "WORK_GROUP_SIZE_ATOMIC_REDUCTION_X", workgroup_size_x},
													{ "WORK_GROUP_SIZE_ATOMIC_REDUCTION_Y", workgroup_size_y},
													{ "WITH_MSAA", "1"},
													{ "SHADOW_MAP_RES", shadowMapRes}, { 0 } };
						
		m_depthReductionAtomicMSAACS = g_shaderManager.AddComputeShader(L"shader/ShadowDepthReduction.hlsl", "ReductionAtomic",  "cs_5_0", &pBlob, reduction_MSAA_macro);

	}

	
	return hr;
}

void ShadowMapping::Destroy()
{
	//SAFE_RELEASE(m_pShadowDSV);
	//SAFE_RELEASE(m_pShadowSRV);
	//SAFE_RELEASE(m_pShadowTex);
	
	//SAFE_RELEASE(m_pBackfaceCull_RS);
	//SAFE_RELEASE(m_pDepthBiasBackfaceCull_RS);
	//SAFE_RELEASE(m_pNoCull_RS);
	
	//SAFE_RELEASE(m_pDepthNoStencil_DS);
	//SAFE_RELEASE(m_pNoDepthNoStencil_DS);
	
	//SAFE_RELEASE(m_pNoBlend_BS);

	// debugging
	
	SAFE_RELEASE( m_pSamLinear );
	SAFE_RELEASE( m_pSamShadowPoint );
	SAFE_RELEASE( m_pSamShadowLinear );
	SAFE_RELEASE( m_pSamShadowLinearClamp );
	SAFE_RELEASE( m_pSamShadowAnisotropic );

	SAFE_RELEASE( m_pVarianceShadowArrayTEX );
	for (int index=0; index < NUM_CASCADES; ++index) 
	{
		SAFE_RELEASE( m_pVarianceShadowArraySliceRTV[index] );
		SAFE_RELEASE( m_pVarianceShadowArraySliceSRV[index] );
		SAFE_RELEASE( m_pVarianceShadowArraySliceUAV[index] );
	}
	SAFE_RELEASE( m_pVarianceShadowArraySRV );
	SAFE_RELEASE( m_pVarianceShadowArrayRTV);

	SAFE_RELEASE( m_pDepthBufferTextureArray );
	SAFE_RELEASE( m_pDepthBufferDSV );
	SAFE_RELEASE( m_pDepthBufferArrayDSV);
	SAFE_RELEASE( m_pDepthBufferSRV );

	SAFE_RELEASE( m_pShadowTempTEX );
	SAFE_RELEASE( m_pShadowTempRTV );
	SAFE_RELEASE( m_pShadowTempSRV );
	SAFE_RELEASE( m_pShadowTempUAV );

	SAFE_RELEASE(m_shadowCascadesProjCB);


	SAFE_RELEASE( m_cbLight );

	SAFE_RELEASE( m_prsShadow );

	for(int i = 0 ; i<3; ++i)
	{	
		SAFE_RELEASE(m_depthReductionResultStagingBUF[i]);
		SAFE_RELEASE(m_depthReductionResultBUF[i]);
		SAFE_RELEASE(m_depthReductionResultUAV[i]);
		SAFE_RELEASE(m_depthReductionResultReductionAtomicUAV[i]);
	}
	
	SAFE_RELEASE(m_depthScanBucketsBUF				);
	SAFE_RELEASE(m_depthScanBucketsResultsBUF		);
	SAFE_RELEASE(m_depthScanBucketsBlockResultsBUF	);
	
	SAFE_RELEASE(m_depthScanBucketsSRV				);
	SAFE_RELEASE(m_depthScanBucketsResultsSRV		);
	SAFE_RELEASE(m_depthScanBucketsBlockResultsSRV	);
	
	SAFE_RELEASE(m_depthScanBucketsUAV				);
	SAFE_RELEASE(m_depthScanBucketsResultsUAV		);
	SAFE_RELEASE(m_depthScanBucketsBlockResultsUAV	);

	SAFE_RELEASE(m_depthReductionCB);
}



//void ShadowMapping::ComputeCascades( CModelViewerCamera* renderCam, CModelViewerCamera* lightCam, Scene* scene )
void ShadowMapping::ComputeCascades(CBaseCamera* renderCam, CModelViewerCamera* lightCam, Scene* scene )
{
	XMVECTOR sceneAABBMin = XMLoadFloat3(&scene->GetSceneAABBMin());
	XMVECTOR sceneAABBMax = XMLoadFloat3(&scene->GetSceneAABBMax());
	sceneAABBMin = XMVectorSetW(sceneAABBMin, 1);
	sceneAABBMax = XMVectorSetW(sceneAABBMax, 1);
	
	const XMMATRIX& matViewCameraProjection = renderCam->GetProjMatrix();	
	const XMMATRIX& matViewCameraView = renderCam->GetViewMatrix();
	XMMATRIX matInverseViewCamera = XMMatrixInverse( NULL,  matViewCameraView );
	const XMMATRIX& matLightCameraView = lightCam->GetViewMatrix();


	// Convert from min max representation to center extents representation.
	// This will make it easier to pull the points out of the transformation.
	XMVECTOR vSceneCenter = (sceneAABBMin + sceneAABBMax) * g_vHalfVector;
	XMVECTOR vSceneExtents = (sceneAABBMax - sceneAABBMin) * g_vHalfVector ;
		
	XMVECTOR vSceneAABBPointsLightSpace[8];
	// This function simply converts the center and extents of an AABB into 8 points
	CreateAABBPoints( vSceneAABBPointsLightSpace, vSceneCenter, vSceneExtents );
	
	// Transform the scene AABB to Light space.
	for( int index =0; index < 8; ++index ) 
	{
#ifdef SHADOW_DEBUG
		XMStoreFloat3(&m_sceneAABBCorners[index],vSceneAABBPointsLightSpace[index]);
#endif
		vSceneAABBPointsLightSpace[index] = XMVector4Transform( vSceneAABBPointsLightSpace[index], matLightCameraView ); 		
	}

	const float minDistance = m_useDepthReduction ? m_depthMinMax.x : m_fCascadePartitionsMin;
	const float maxDistance = m_useDepthReduction ? m_depthMinMax.y : m_fCascadePartitionsMax;

	float cascadeSplits[4] = {0.0f, 0.0f, 0.0f, 1.0f};

	if(m_ePartitionMode == PartitionMode::MANUAL)
	{	
		cascadeSplits[0] = minDistance + m_fSplitDistance[0] * maxDistance;
		cascadeSplits[1] = minDistance + m_fSplitDistance[1] * maxDistance;
		cascadeSplits[2] = minDistance + m_fSplitDistance[2] * maxDistance;
	#if NUM_CASCADES == 4
		cascadeSplits[3] = minDistance + m_fSplitDistance[3] * maxDistance;
	#endif
	}
	else if(m_ePartitionMode == PartitionMode::LOGARTIHMIC ||  m_ePartitionMode == PartitionMode::PSSM)
	{
		float lambda = 1.0f;
		if(m_ePartitionMode == PartitionMode::PSSM)
		{
			lambda = m_pssmLambda;
		}

		float nearClip  = renderCam->GetNearClip();
		float farClip   = renderCam->GetFarClip();
		float clipRange = farClip - nearClip;

		float minZ = nearClip+minDistance*clipRange;
		float maxZ = nearClip+minDistance*clipRange;

		float range = maxZ - minZ;
		float ratio = maxZ / minZ;

		for(UINT i = 0; i < NUM_CASCADES; ++i)
		{
			float p = (i+1) / (float)NUM_CASCADES;
			float log = minZ*std::pow(ratio, p);
			float uniform = minZ + range * p;
			float d = lambda*(log - uniform) + uniform;
			cascadeSplits[i] = (d-nearClip)/clipRange;
		}
	}


	float fFrustumIntervalBegin, fFrustumIntervalEnd;
	XMVECTOR vLightCameraOrthographicMin;  // light space frustum AABB
	XMVECTOR vLightCameraOrthographicMax;
	float clipRange = renderCam->GetFarClip() - renderCam->GetNearClip();

	XMVECTOR vWorldUnitsPerTexel = g_vZero; 

	// We loop over the cascades to calculate the orthographic projection for each cascade.
	for( INT iCascadeIndex=0; iCascadeIndex < NUM_CASCADES; ++iCascadeIndex ) 
	{
		// Calculate the interval of the View Frustum that this cascade covers. We measure the interval 
		// the cascade covers as a Min and Max distance along the Z Axis.
		if( m_eSelectedCascadesFit == FIT_PROJECTION_TO::CASCADES ) 
		{
			// Because we want to fit the orthographic projection tightly around the Cascade, we set the minimum cascade 
			// value to the previous Frustum end Interval
			if( iCascadeIndex==0 ) fFrustumIntervalBegin = minDistance;//0.0f;
			else fFrustumIntervalBegin = cascadeSplits[ iCascadeIndex - 1 ];
		} 
		else 
		{
			// In the FIT_TO_SCENE technique the Cascades overlap each other.  In other words, interval 1 is covered by
			// cascades 1 to 8, interval 2 is covered by cascades 2 to 8 and so forth.
			fFrustumIntervalBegin = 0.0f;
		}

		// Scale the intervals between 0 and 1. They are now percentages that we can scale with.
		fFrustumIntervalEnd = cascadeSplits[ iCascadeIndex ];        
		fFrustumIntervalBegin = fFrustumIntervalBegin * clipRange;
		fFrustumIntervalEnd = fFrustumIntervalEnd * clipRange;


		XMVECTOR vFrustumPoints[8];		

		// This function takes the began and end intervals along with the projection matrix and returns the 8
		// points that represent the cascade Interval
		CreateFrustumPointsFromCascadeInterval( fFrustumIntervalBegin, fFrustumIntervalEnd, matViewCameraProjection, vFrustumPoints );

		vLightCameraOrthographicMin = g_vFLTMAX;
		vLightCameraOrthographicMax = g_vFLTMIN;

		XMVECTOR vTempTranslatedCornerPoint;
		// This next section of code calculates the min and max values for the orthographic projection.
		for( int icpIndex=0; icpIndex < 8; ++icpIndex ) 
		{
			// Transform the frustum from camera view space to world space.
			vFrustumPoints[icpIndex] = XMVector4Transform ( vFrustumPoints[icpIndex], matInverseViewCamera );
			
			// debug
			#ifdef SHADOW_DEBUG
			XMStoreFloat3(&m_viewFrustumCorners[iCascadeIndex][icpIndex], vFrustumPoints[icpIndex]);
			#endif

			// Transform the point from world space to Light Camera Space.
			vTempTranslatedCornerPoint = XMVector4Transform ( vFrustumPoints[icpIndex], matLightCameraView );

			// Find the closest point.
			vLightCameraOrthographicMin = XMVectorMin ( vTempTranslatedCornerPoint, vLightCameraOrthographicMin );
			vLightCameraOrthographicMax = XMVectorMax ( vTempTranslatedCornerPoint, vLightCameraOrthographicMax );
		}


		// This code removes the shimmering effect along the edges of shadows due to
		// the light changing to fit the camera.
		if( m_eSelectedCascadesFit == FIT_PROJECTION_TO::SCENE ) 
		{
			// Fit the ortho projection to the cascades far plane and a near plane of zero. 
			// Pad the projection to be the size of the diagonal of the Frustum partition. 
			// 
			// To do this, we pad the ortho transform so that it is always big enough to cover 
			// the entire camera view frustum.
			XMVECTOR vDiagonal = vFrustumPoints[0] - vFrustumPoints[6];
			vDiagonal = XMVector3Length( vDiagonal );

			// The bound is the length of the diagonal of the frustum interval.
			FLOAT fCascadeBound = XMVectorGetX( vDiagonal );

			// The offset calculated will pad the ortho projection so that it is always the same size 
			// and big enough to cover the entire cascade interval.
			XMVECTOR vBoarderOffset = ( vDiagonal - 
				( vLightCameraOrthographicMax - vLightCameraOrthographicMin ) ) 
				* g_vHalfVector;
			// Set the Z and W components to zero.
			vBoarderOffset *= g_vMultiplySetzwToZero;

			// Add the offsets to the projection.
			vLightCameraOrthographicMax += vBoarderOffset;
			vLightCameraOrthographicMin -= vBoarderOffset;

			// The world units per texel are used to snap the shadow the orthographic projection
			// to texel sized increments.  This keeps the edges of the shadows from shimmering.
			FLOAT fWorldUnitsPerTexel = fCascadeBound / (float)SHADOW_MAP_RES;
			vWorldUnitsPerTexel = XMVectorSet( fWorldUnitsPerTexel, fWorldUnitsPerTexel, 0.0f, 0.0f ); 
		} 
		else if( m_eSelectedCascadesFit == FIT_PROJECTION_TO::CASCADES ) 
		{

			// We calculate a looser bound based on the size of the PCF blur.  This ensures us that we're 
			// sampling within the correct map.
			float fScaleDuetoBlureAMT = ( (float)( BLUR_KERNEL_SIZE * 2 + 1 ) / (float)SHADOW_MAP_RES );
			XMVECTORF32 vScaleDuetoBlureAMT = { fScaleDuetoBlureAMT, fScaleDuetoBlureAMT, 0.0f, 0.0f };


			float fNormalizeByBufferSize = ( 1.0f / (float)SHADOW_MAP_RES );
			XMVECTOR vNormalizeByBufferSize = XMVectorSet( fNormalizeByBufferSize, fNormalizeByBufferSize, 0.0f, 0.0f );

			// We calculate the offsets as a percentage of the bound.
			XMVECTOR vBoarderOffset = vLightCameraOrthographicMax - vLightCameraOrthographicMin;
			vBoarderOffset *= g_vHalfVector;
			vBoarderOffset *= vScaleDuetoBlureAMT;
			vLightCameraOrthographicMax += vBoarderOffset;
			vLightCameraOrthographicMin -= vBoarderOffset;

			// The world units per texel are used to snap  the orthographic projection
			// to texel sized increments.  
			// Because we're fitting tightly to the cascades, the shimmering shadow edges will still be present when the 
			// camera rotates.  However, when zooming in or strafing the shadow edge will not shimmer.
			vWorldUnitsPerTexel = vLightCameraOrthographicMax - vLightCameraOrthographicMin;
			vWorldUnitsPerTexel *= vNormalizeByBufferSize;

		}


		if( m_bMoveLightTexelSize ) 
		{

			// We snap the camera to 1 pixel increments so that moving the camera does not cause the shadows to jitter.
			// This is a matter of integer dividing by the world space size of a texel
			vLightCameraOrthographicMin /= vWorldUnitsPerTexel;
			vLightCameraOrthographicMin = XMVectorFloor( vLightCameraOrthographicMin );
			vLightCameraOrthographicMin *= vWorldUnitsPerTexel;

			vLightCameraOrthographicMax /= vWorldUnitsPerTexel;
			vLightCameraOrthographicMax = XMVectorFloor( vLightCameraOrthographicMax );
			vLightCameraOrthographicMax *= vWorldUnitsPerTexel;

		}

		//These are the unconfigured near and far plane values.  They are purposly awful to show 
		// how important calculating accurate near and far planes is.
		FLOAT fNearPlane = 0.0f;
		FLOAT fFarPlane = 10000.0f;

		if( m_eSelectedNearFarFit == FIT_NEAR_FAR::AABB ) 
		{

			XMVECTOR vLightSpaceSceneAABBminValue = g_vFLTMAX;  // world space scene aabb 
			XMVECTOR vLightSpaceSceneAABBmaxValue = g_vFLTMIN;       
			// We calculate the min and max vectors of the scene in light space. The min and max "Z" values of the  
			// light space AABB can be used for the near and far plane. This is easier than intersecting the scene with the AABB
			// and in some cases provides similar results.
			for(int index=0; index< 8; ++index) 
			{
				vLightSpaceSceneAABBminValue = XMVectorMin( vSceneAABBPointsLightSpace[index], vLightSpaceSceneAABBminValue );
				vLightSpaceSceneAABBmaxValue = XMVectorMax( vSceneAABBPointsLightSpace[index], vLightSpaceSceneAABBmaxValue );
			}

			// The min and max z values are the near and far planes.
			fNearPlane = XMVectorGetZ( vLightSpaceSceneAABBminValue );
			fFarPlane  = XMVectorGetZ( vLightSpaceSceneAABBmaxValue );
		} 
		else if( m_eSelectedNearFarFit == FIT_NEAR_FAR::SCENE_AABB ) 
		{
			// By intersecting the light frustum with the scene AABB we can get a tighter bound on the near and far plane.
			ComputeNearAndFar( fNearPlane, fFarPlane, vLightCameraOrthographicMin,	vLightCameraOrthographicMax, vSceneAABBPointsLightSpace );
		} 
		else 
		{
			//exit(1);
		}

		// Crete the orthographic projection for this cascade.
		m_matShadowProj[ iCascadeIndex ] = XMMatrixOrthographicOffCenterLH(	XMVectorGetX( vLightCameraOrthographicMin ), 
																			XMVectorGetX( vLightCameraOrthographicMax ), 
																			XMVectorGetY( vLightCameraOrthographicMin ), 
																			XMVectorGetY( vLightCameraOrthographicMax ), 
																			fNearPlane, fFarPlane );

		XMMATRIX mShadowTexture = m_matShadowProj[iCascadeIndex] * g_matTextureScale * g_matTextureTranslation;
		XMFLOAT4X4 matShadowTexture;
		XMStoreFloat4x4(&matShadowTexture, mShadowTexture);

		m_cascadeOffsets[iCascadeIndex] = XMFLOAT3(matShadowTexture._41, matShadowTexture._42, matShadowTexture._43);
		m_cascadeScales[iCascadeIndex] = XMFLOAT3(matShadowTexture._11,	matShadowTexture._22, matShadowTexture._33);
		
		// debug
		#ifdef SHADOW_DEBUG
		{
			//std::cout << "cascade " << iCascadeIndex << std::endl;
			std::cout << "left: " << 	XMVectorGetX( vLightCameraOrthographicMin ) << ", right: " <<  XMVectorGetX( vLightCameraOrthographicMax ) 
				<< " bottom: " << XMVectorGetY( vLightCameraOrthographicMin ) << ", top: " << XMVectorGetY( vLightCameraOrthographicMax ) 
				<< " near: " << fNearPlane << " far: " << fFarPlane << " frustum intervalEnd: " << fFrustumIntervalEnd << std::endl;

			// debug
			XMMATRIX matInverseLightCameraView = XMMatrixInverse( NULL,  matLightCameraView );

			// transform corners to world space
			// near
			FLOAT l = XMVectorGetX( vLightCameraOrthographicMin );
			FLOAT r = XMVectorGetX( vLightCameraOrthographicMax );
			FLOAT b = XMVectorGetY( vLightCameraOrthographicMin );
			FLOAT t = XMVectorGetY( vLightCameraOrthographicMax );
						
			XMStoreFloat3(&m_cascadeFrustumCorners[iCascadeIndex][0], XMVector4Transform ( XMVectorSet(l, b, fNearPlane,1), matInverseLightCameraView )); // lbn
			XMStoreFloat3(&m_cascadeFrustumCorners[iCascadeIndex][1], XMVector4Transform ( XMVectorSet(r, b, fNearPlane,1), matInverseLightCameraView )); // rbn
			XMStoreFloat3(&m_cascadeFrustumCorners[iCascadeIndex][2], XMVector4Transform ( XMVectorSet(r, t, fNearPlane,1), matInverseLightCameraView )); // rtn
			XMStoreFloat3(&m_cascadeFrustumCorners[iCascadeIndex][3], XMVector4Transform ( XMVectorSet(l, t, fNearPlane,1), matInverseLightCameraView )); // ltn

			XMStoreFloat3(&m_cascadeFrustumCorners[iCascadeIndex][4], XMVector4Transform ( XMVectorSet(l, b, fFarPlane,1), matInverseLightCameraView ));  // lbf
			XMStoreFloat3(&m_cascadeFrustumCorners[iCascadeIndex][5], XMVector4Transform ( XMVectorSet(r, b, fFarPlane,1), matInverseLightCameraView ));  // rbf
			XMStoreFloat3(&m_cascadeFrustumCorners[iCascadeIndex][6], XMVector4Transform ( XMVectorSet(r, t, fFarPlane,1), matInverseLightCameraView ));  // rtf
			XMStoreFloat3(&m_cascadeFrustumCorners[iCascadeIndex][7], XMVector4Transform ( XMVectorSet(l, t, fFarPlane,1), matInverseLightCameraView ));  // ltf
		}
		#endif

		m_fCascadePartitionsFrustum[ iCascadeIndex ] = fFrustumIntervalEnd;
	}
	m_matShadowView = XMMatrixInverse(NULL, renderCam->GetViewMatrix()) * lightCam->GetViewMatrix();

	XMVECTOR lightEye    = XMLoadFloat3(lightCam->GetEyePt());
	XMVECTOR lightLookAt = XMLoadFloat3(lightCam->GetLookAtPt());
	XMVECTOR lightDir = XMVector3Normalize(lightEye-lightLookAt);	
	XMStoreFloat3(&m_lightDir, lightDir);

	if(m_useSinglePassShadow) // set per cascade projection matrix for geometry shader
	{
		UpdateShadowCascadesCB(DXUTGetD3D11DeviceContext());		
	}
}

HRESULT ShadowMapping::RenderShadowsForAllCascades( ID3D11DeviceContext1* pd3dImmediateContext, XMMATRIX lightViewMatrix, Scene* scene, RenderTri* triRenderer, RendererSubD* osdRenderer )
{
	HRESULT hr = S_OK;
	ID3D11RenderTargetView* nullView[] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL};
	
	{			

		triRenderer->SetGenShadows(true);
		osdRenderer->SetGenShadows(true);

		pd3dImmediateContext->RSSetState( m_prsShadow );
		
		g_app.SetViewMatrix(lightViewMatrix);
		
		//if(m_useSinglePassShadowGen)
		if(m_useSinglePassShadow)
		{
			PERF_EVENT_SCOPED(perfGen, L"Render Shadow Cascades");
			osdRenderer->SetGenShadowsFast(true);
			triRenderer->SetGenShadowsFast(true);

			pd3dImmediateContext->GSSetConstantBuffers( CB_LOC::CASCADE_PROJECTION, 1, &m_shadowCascadesProjCB );
			
			float ClearColor[4] = { 0.0, 0.0, 0.0, 0.0 };
			// clear rtv? pd3dDevice->ClearRenderTargetView( g_pEnvMapRTV, ClearColor );
			//pd3dImmediateContext->ClearRenderTargetView( m_pCascadedShadowMapVarianceRTVArrayAll, ClearColor );
			pd3dImmediateContext->ClearDepthStencilView( m_pDepthBufferArrayDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );
			pd3dImmediateContext->OMSetRenderTargets( 1, &m_pVarianceShadowArrayRTV, m_pDepthBufferArrayDSV );
			pd3dImmediateContext->RSSetViewports( 1, &m_RenderOneTileVP );
					
			scene->RenderSceneObjects(pd3dImmediateContext, triRenderer, osdRenderer);
			
			pd3dImmediateContext->OMSetRenderTargets(1, nullView, NULL );

			
			osdRenderer->SetGenShadowsFast(false);		
			triRenderer->SetGenShadowsFast(false);
		}
		else
		{		

			//pd3dDeviceContext->PSSetShaderResources( 12, 1, nullSRV );
			for ( int iCurrentCascade=0; iCurrentCascade < NUM_CASCADES; ++iCurrentCascade ) 
			{
#ifdef PROFILE
				std::wostringstream ss;				
				ss << L"Render Shadow Cascade" << iCurrentCascade;								
				PERF_EVENT_SCOPED(perfGen, ss.str().c_str());
#endif
				// CHECKME clear rtv needed?
				//float clearCol[4] = {0,0,0,0};
				//pd3dDeviceContext->ClearRenderTargetView(m_pCascadedShadowMapVarianceRTVArrayAll[iCurrentCascade],clearCol);


				pd3dImmediateContext->ClearDepthStencilView( m_pDepthBufferDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );
				pd3dImmediateContext->OMSetRenderTargets( 1, &m_pVarianceShadowArraySliceRTV[iCurrentCascade], m_pDepthBufferDSV );
				pd3dImmediateContext->RSSetViewports( 1, &m_RenderOneTileVP );

				g_app.SetProjectionMatrix(m_matShadowProj[iCurrentCascade]);
				scene->RenderSceneObjects(pd3dImmediateContext, triRenderer, osdRenderer);

				pd3dImmediateContext->OMSetRenderTargets(1, nullView, NULL );
			}
		}

		triRenderer->SetGenShadows(false);
		osdRenderer->SetGenShadows(false);
	}
	

	//g_app.GPUPerfTimerStart(pd3dImmediateContext);
	if (BLUR_KERNEL_SIZE > 1 ) 
	{		
		UINT blurCascadesBelow = NUM_CASCADES; // -1 // -2

		PERF_EVENT_SCOPED(perfBlur, L"Prefilter Shadow (blur)");
		if(	m_useBlurCS)
		{
			if(!m_useBlurSharedMemCS)
			{		
	
				UINT blockSize =(SHADOW_MAP_RES + WORK_GROUP_SIZE_BLUR-1)/WORK_GROUP_SIZE_BLUR;

				//INT iKernelShader = ( m_iShadowBlurSize / 2 - 1 ); //3 goes to 0, 5 goes to 1 
				for ( UINT iCurrentCascade=0; iCurrentCascade < blurCascadesBelow; ++iCurrentCascade ) 
				{ 

					// blur in x
					pd3dImmediateContext->CSSetShaderResources(12,1,&m_pVarianceShadowArraySliceSRV[iCurrentCascade]);
					pd3dImmediateContext->CSSetUnorderedAccessViews(1,1,&m_pShadowTempUAV,NULL);
					pd3dImmediateContext->CSSetShader(m_blurXCS->Get(), NULL, 0);

					pd3dImmediateContext->Dispatch(blockSize,blockSize,1);

					pd3dImmediateContext->CSSetShaderResources(12,1,g_ppSRVNULL);
					pd3dImmediateContext->CSSetUnorderedAccessViews(1,1,g_ppUAVNULL,NULL);

					// blur in y
					pd3dImmediateContext->CSSetShaderResources(12,1,&m_pShadowTempSRV);
					pd3dImmediateContext->CSSetUnorderedAccessViews(1,1,&m_pVarianceShadowArraySliceUAV[iCurrentCascade],NULL);
					pd3dImmediateContext->CSSetShader(m_blurYCS->Get(), NULL, 0);

					pd3dImmediateContext->Dispatch(blockSize,blockSize,1);

					pd3dImmediateContext->CSSetShaderResources(12,1,g_ppSRVNULL);
					pd3dImmediateContext->CSSetUnorderedAccessViews(1,1,g_ppUAVNULL,NULL);
				}
			}
			else
			{


				UINT blockSize =(SHADOW_MAP_RES + ROW_TILE_W-1)/ROW_TILE_W;
				pd3dImmediateContext->CSSetSamplers( 6, 1, &m_pSamShadowPoint );
				pd3dImmediateContext->CSSetSamplers( 7, 1, &m_pSamShadowLinearClamp );

				//INT iKernelShader = ( m_iShadowBlurSize / 2 - 1 ); //3 goes to 0, 5 goes to 1 
				for ( UINT iCurrentCascade=0; iCurrentCascade < blurCascadesBelow; ++iCurrentCascade ) 
				{ 

					// blur in x
					pd3dImmediateContext->CSSetShaderResources(12,1,&m_pVarianceShadowArraySliceSRV[iCurrentCascade]);
					pd3dImmediateContext->CSSetUnorderedAccessViews(1,1,&m_pShadowTempUAV,NULL);
					pd3dImmediateContext->CSSetShader(m_blurXSharedMemCS->Get(), NULL, 0);

					pd3dImmediateContext->Dispatch(blockSize,SHADOW_MAP_RES,1);

					pd3dImmediateContext->CSSetShaderResources(12,1,g_ppSRVNULL);
					pd3dImmediateContext->CSSetUnorderedAccessViews(1,1,g_ppUAVNULL,NULL);

					// blur in y
					pd3dImmediateContext->CSSetShaderResources(12,1,&m_pShadowTempSRV);
					pd3dImmediateContext->CSSetUnorderedAccessViews(1,1,&m_pVarianceShadowArraySliceUAV[iCurrentCascade],NULL);
					pd3dImmediateContext->CSSetShader(m_blurYSharedMemCS->Get(), NULL, 0);

					pd3dImmediateContext->Dispatch(blockSize,SHADOW_MAP_RES,1);

					pd3dImmediateContext->CSSetShaderResources(12,1,g_ppSRVNULL);
					pd3dImmediateContext->CSSetUnorderedAccessViews(1,1,g_ppUAVNULL,NULL);
				}
				ID3D11SamplerState* ppSSNULL[] = {NULL,NULL};
				pd3dImmediateContext->CSSetSamplers( 6, 2, ppSSNULL );

			}
		}
		else
		{	
			ID3D11DepthStencilView *dsNullview = NULL;
			pd3dImmediateContext->IASetInputLayout( NULL );
			pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			pd3dImmediateContext->VSSetShader( m_pvsQuadBlur->Get(), NULL, 0 );
			pd3dImmediateContext->PSSetSamplers( 6, 1, &m_pSamShadowPoint );
			ID3D11ShaderResourceView *srvNull = NULL;

			if (BLUR_KERNEL_SIZE > 1 ) 
			{
				//INT iKernelShader = ( m_iShadowBlurSize / 2 - 1 ); //3 goes to 0, 5 goes to 1 
				for ( UINT iCurrentCascade=0; iCurrentCascade < blurCascadesBelow; ++iCurrentCascade ) 
				{ 

					pd3dImmediateContext->OMSetRenderTargets(1, &m_pShadowTempRTV, dsNullview );
					pd3dImmediateContext->PSSetShaderResources( 12, 1, &m_pVarianceShadowArraySliceSRV[iCurrentCascade] );
					pd3dImmediateContext->PSSetShader( m_ppsQuadBlurX->Get(), NULL, 0 );
					pd3dImmediateContext->Draw(4, 0);

					pd3dImmediateContext->PSSetShaderResources( 12, 1, &srvNull );
					pd3dImmediateContext->OMSetRenderTargets(1, &m_pVarianceShadowArraySliceRTV[iCurrentCascade], dsNullview );
					pd3dImmediateContext->PSSetShaderResources( 12, 1, &m_pShadowTempSRV );
					pd3dImmediateContext->PSSetShader( m_ppsQuadBlurY->Get(), NULL, 0 );
					pd3dImmediateContext->Draw(4, 0);
					pd3dImmediateContext->PSSetShaderResources( 12, 1, &srvNull );
				}
			}

			pd3dImmediateContext->RSSetState( NULL );
			pd3dImmediateContext->OMSetRenderTargets(1, nullView, NULL );
		}
	}
	//g_app.GPUPerfTimerEnd(pd3dImmediateContext);
	//std::cout  << "compute cascades took: " << g_app.GPUPerfTimerElapsed(pd3dImmediateContext) << std::endl;

	return hr;
}

DirectX::XMMATRIX ShadowMapping::GetCascadeProjMatrix( int cascadeIndex ) const
{	
	cascadeIndex = XMMax(XMMin((int)NUM_CASCADES-1,cascadeIndex), 0);
	return m_matShadowProj[cascadeIndex];
}

HRESULT ShadowMapping::RunDepthPrepass( ID3D11DeviceContext1* pd3dImmediateContext, CModelViewerCamera* camera, Scene* scene, RenderTri* triRenderer, RendererSubD* osdRenderer )
{
	HRESULT hr = S_OK;

	if(!m_useDepthReduction)
		return hr;

	PERF_EVENT_SCOPED(perf, L"Depth Prepass");
	triRenderer->SetGenShadows(true);	
	osdRenderer->SetGenShadows(true);

	pd3dImmediateContext->RSSetState( m_prsShadow );
	

	g_app.SetViewMatrix(camera->GetViewMatrix());
	g_app.SetProjectionMatrix(camera->GetProjMatrix());

	ID3D11RenderTargetView* nullView[] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL};

	{		
		// CHECKME clear rtv needed?
		//float clearCol[4] = {1,0,0,0};
		//pd3dImmediateContext->ClearRenderTargetView(m_pShadowTempRTV, clearCol);
		pd3dImmediateContext->ClearDepthStencilView( m_pDepthBufferDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );
		//pd3dImmediateContext->OMSetRenderTargets( 1, &m_pShadowTempRTV, m_pDepthBufferDSV );
		pd3dImmediateContext->OMSetRenderTargets( 1, nullView, m_pDepthBufferDSV );
		pd3dImmediateContext->RSSetViewports( 1, &m_RenderOneTileVP );


		scene->RenderSceneObjects(pd3dImmediateContext, triRenderer, osdRenderer);

		pd3dImmediateContext->OMSetRenderTargets(1, nullView, NULL );
	}

	triRenderer->SetGenShadows(false);
	osdRenderer->SetGenShadows(false);

	pd3dImmediateContext->RSSetState( NULL );
	return hr;
}

HRESULT ShadowMapping::RunDepthReduction( ID3D11DeviceContext1* pd3dImmediateContext, CModelViewerCamera* camera, ID3D11DepthStencilView* dsv, ID3D11ShaderResourceView* depthSRV, UINT depthTexWidth, UINT depthTexHeight)
{
	HRESULT hr = S_OK;

	if(!m_useDepthReduction)	return hr;
	
	PERF_EVENT_SCOPED(perf, L"Shadow Map Depth Reduction");	
	{	
		// update near far clip cb
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		V_RETURN(pd3dImmediateContext->Map( m_depthReductionCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ));

		CB_DEPTH_REDUCTION* pCB = ( CB_DEPTH_REDUCTION* )MappedResource.pData;		
		pCB->nearPlane = camera->GetNearClip();
		pCB->farPlane  = camera->GetFarClip();
		pCB->numElements = depthTexWidth*depthTexHeight;
		pCB->padding = 0;
		pd3dImmediateContext->Unmap( m_depthReductionCB, 0 );

		pd3dImmediateContext->CSSetConstantBuffers(CB_LOC::DEPTH_REDUCTION, 1, &m_depthReductionCB);
	}


//#define WITH_TIMINGS
#ifdef WITH_TIMINGS
	UINT numRuns2 = 100;
	Timer t2;
	g_app.WaitForGPU();
	t2.Begin();
	for(int i = 0; i < numRuns2; ++i)
	{
#endif
	
//	if(1) 
	{
		// atomic variant with local reduction
		ID3D11RenderTargetView* ppNullRTV[] = {NULL};		
		ID3D11RenderTargetView* pMainRTV = NULL;
		ID3D11DepthStencilView* pMainDSV = NULL;
		
		pd3dImmediateContext->OMGetRenderTargets(1, &pMainRTV, &pMainDSV);		
		pd3dImmediateContext->OMSetRenderTargets(1, ppNullRTV, NULL);
		if(DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count > 1)
			pd3dImmediateContext->CSSetShader(m_depthReductionAtomicMSAACS->Get(), NULL, 0);
		else
			pd3dImmediateContext->CSSetShader(m_depthReductionAtomicCS->Get(), NULL, 0);
		pd3dImmediateContext->CSSetShaderResources(0, 1, &depthSRV);		
		
		FLOAT values[4] = {1,0,1,0};
		pd3dImmediateContext->ClearUnorderedAccessViewFloat(m_depthReductionResultUAV[m_depthReductionFrameCounter], values);
		pd3dImmediateContext->CSSetUnorderedAccessViews(1, 1, &m_depthReductionResultReductionAtomicUAV[m_depthReductionFrameCounter], NULL); 
		
		pd3dImmediateContext->Dispatch(UINT(((float)depthTexWidth+WORK_GROUP_SIZE_ATOMIC_REDUCTION_X-1)/WORK_GROUP_SIZE_ATOMIC_REDUCTION_X), UINT(((float)depthTexHeight+WORK_GROUP_SIZE_ATOMIC_REDUCTION_Y-1)/WORK_GROUP_SIZE_ATOMIC_REDUCTION_Y), 1); // CHECKME
		
		pd3dImmediateContext->CSSetShaderResources(0, 1, g_ppSRVNULL);	
		pd3dImmediateContext->CSSetUnorderedAccessViews(1, 1, g_ppUAVNULL,NULL);
		

		pd3dImmediateContext->OMSetRenderTargets(1, &pMainRTV, pMainDSV);
		SAFE_RELEASE(pMainRTV);
		SAFE_RELEASE(pMainDSV);
		
		pd3dImmediateContext->CopyResource(m_depthReductionResultStagingBUF[m_depthReductionFrameCounter], m_depthReductionResultBUF[m_depthReductionFrameCounter] );
				
		m_depthReductionFrameCounter = (m_depthReductionFrameCounter+1)%3;
	}
//	else
//	{
//		////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//		// PASS 1 - UP-SWEEP  scan buckets
//		pd3dImmediateContext->CSSetShader(m_depthScanBucketCS->Get(), NULL, 0);
//
//		//pd3dImmediateContext->CSSetShaderResources(0, 1, &m_pShadowTempSRV);						
//		pd3dImmediateContext->CSSetShaderResources(0, 1, &m_pDepthBufferSRV);	
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, &m_depthScanBucketsUAV, NULL); 
//
//#ifdef AUTO_MINMAX_DEPTH_ON_MAIN_DSV
//
//		UINT groupsPass1 = (depthTexWidth*depthTexHeight + m_scanBucketSize - 1) / m_scanBucketSize;
//#else
//		UINT groupsPass1 = (SHADOW_MAP_RES*SHADOW_MAP_RES + m_scanBucketSize - 1) / m_scanBucketSize;
//#endif
//
//		UINT dimX = SCAN_DISPATCH_THREADS_X;
//		UINT dimY = (groupsPass1 + dimX - 1) / dimX;
//		//std::cout << "dim xy: " << dimX << ", " << dimY << std::endl;
// 		pd3dImmediateContext->Dispatch(dimX, dimY, 1); // CHECKME
//
//		pd3dImmediateContext->CSSetShaderResources(0, 1, g_ppSRVNULL);	
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, g_ppUAVNULL,NULL);
//
//		////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//		// pass 2 UP-SWEEP scanBucketResults
//		pd3dImmediateContext->CSSetShader(m_depthScanBucketResultsCS->Get(), NULL, 0);
//
//		pd3dImmediateContext->CSSetShaderResources(1, 1, &m_depthScanBucketsSRV);
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, &m_depthScanBucketsResultsUAV, NULL);
//
//		UINT groupsPass2 = (groupsPass1 + m_scanBucketBlockSize - 1) / m_scanBucketBlockSize;
//		pd3dImmediateContext->Dispatch(groupsPass2, 1, 1);
//
//		pd3dImmediateContext->CSSetShaderResources(1, 1, g_ppSRVNULL);
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, g_ppUAVNULL, NULL);
//
//		////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//		// pass 3 UP-SWEEP
//		pd3dImmediateContext->CSSetShader(m_depthScanBucketBlockResultsCS->Get(), NULL, 0);
//
//		pd3dImmediateContext->CSSetShaderResources(1, 1, &m_depthScanBucketsResultsSRV);
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, &m_depthScanBucketsBlockResultsUAV, NULL);
//
//		pd3dImmediateContext->Dispatch(1,1,1);
//
//		pd3dImmediateContext->CSSetShaderResources(1, 1, g_ppSRVNULL);
//		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, g_ppUAVNULL, NULL);
//
//	
//		//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//		// pass 4 DOWN-SWEEP		
//		// not needed, directly fetch result
//		if(1)
//		{		
//			pd3dImmediateContext->CSSetShader(m_depthScanWriteResultCS->Get(), NULL, 0);
//			
//			pd3dImmediateContext->CSSetShaderResources(1, 1, &m_depthScanBucketsBlockResultsSRV);
//			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, &m_depthReductionResultUAV[m_depthReductionFrameCounter], NULL);
//			
//			pd3dImmediateContext->Dispatch(1, 1, 1);
//			
//			pd3dImmediateContext->CSSetShaderResources(0, 2, g_ppSRVNULL);
//			pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, g_ppUAVNULL, NULL);
//		}
//		
//		if(1)  // read back result
//		{				
//			pd3dImmediateContext->CopyResource(m_depthReductionResultStagingBUF[m_depthReductionFrameCounter], m_depthReductionResultBUF[m_depthReductionFrameCounter] );		
//		}	
//		m_depthReductionFrameCounter = (m_depthReductionFrameCounter+1)%3;
//		}
#ifdef WITH_TIMINGS
	}
	g_app.WaitForGPU();
	t2.End();	
	std::cout << "depth reduction after upsweep time: " << t2.MilliSecondsLastMeasurement() / numRuns2 << std::endl;
#endif

	{
		UINT latencyReadIdx = (m_depthReductionFrameCounter+1)%3;		
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		V_RETURN(pd3dImmediateContext->Map(m_depthReductionResultStagingBUF[latencyReadIdx], 0, D3D11_MAP_READ, 0, &MappedResource));   
		m_depthMinMax = ((XMFLOAT2*)MappedResource.pData)[0];
		pd3dImmediateContext->Unmap( m_depthReductionResultStagingBUF[latencyReadIdx], 0 );
				
		//std::cout << "depth min: " << m_depthMinMax.x << ", max: " << m_depthMinMax.y << std::endl;
	}

	return hr;
}

HRESULT ShadowMapping::UpdateShadowCB( ID3D11DeviceContext1* pContext, XMMATRIX matCameraView, XMMATRIX matCameraProj )
{
	HRESULT hr = S_OK;
	// set shadow mapping constant buffer

	XMMATRIX matWorldViewProjection =  matCameraView * matCameraProj;



	XMFLOAT4 lightDirES;	
	XMStoreFloat4(&lightDirES, XMVector3Normalize(XMVector4Transform(XMLoadFloat3(&m_lightDir), matCameraView)));
			
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V_RETURN( pContext->Map( m_cbLight, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_LIGHT* pCB = ( CB_LIGHT* )MappedResource.pData;
	
	pCB->lightDirWS = XMFLOAT4( m_lightDir.x,m_lightDir.y, m_lightDir.z, 0.0f );	
	pCB->lightDirES = lightDirES;
	pCB->worldToShadowMatrix = m_matShadowView;
	
	for(int index=0; index < NUM_CASCADES; ++index ) 
	{
		//XMMATRIX mShadowTexture = m_matShadowProj[index] * g_matTextureScale * g_matTextureTranslation;
		//XMFLOAT4X4 matShadowTexture;
		//XMStoreFloat4x4(&matShadowTexture, mShadowTexture);

		//pCB->cascadeOffset[index].x = matShadowTexture._41;
		//pCB->cascadeOffset[index].y = matShadowTexture._42;
		//pCB->cascadeOffset[index].z = matShadowTexture._43;
		//pCB->cascadeOffset[index].w = 0;
		//
		//pCB->cascadeScale[index].x = matShadowTexture._11;
		//pCB->cascadeScale[index].y = matShadowTexture._22;
		//pCB->cascadeScale[index].z = matShadowTexture._33;
		//pCB->cascadeScale[index].w = 1;

		pCB->cascadeOffset[index].x = m_cascadeOffsets[index].x;
		pCB->cascadeOffset[index].y = m_cascadeOffsets[index].y;
		pCB->cascadeOffset[index].z = m_cascadeOffsets[index].z;
		pCB->cascadeOffset[index].w = 0;

		pCB->cascadeScale[index].x = m_cascadeScales[index].x;
		pCB->cascadeScale[index].y = m_cascadeScales[index].y;
		pCB->cascadeScale[index].z = m_cascadeScales[index].z;
		pCB->cascadeScale[index].w = 1;

	}

	pCB->nCascadeLevels = NUM_CASCADES;
	pCB->showCascadeLevels = m_showCascades;

	// The border padding values keep the pixel shader from reading the borders during PCF filtering.
	pCB->maxBorderPadding = (float)( SHADOW_MAP_RES  - 1.0f ) / (float)SHADOW_MAP_RES;
	pCB->minBorderPadding = (float)( 1.0f ) / (float)SHADOW_MAP_RES;

	// These are the for loop begin end values. 
	// This is a floating point number that is used as the percentage to blur between maps.    
	pCB->cascadeBlendArea = m_fBlurBetweenCascadesAmount;

	float texelSize =  1.0f / (float)SHADOW_MAP_RES;
	pCB->texelSize = texelSize;
	pCB->nativeTexelSizeInX = texelSize/NUM_CASCADES;
	pCB->padding1 = 0.f;
	// Copy intervals for the depth interval selection method.
	memcpy( pCB->cascadeFrustumEyeSpaceDepths, m_fCascadePartitionsFrustum, NUM_CASCADES*4 );
	pCB->cascadeFrustumEyeSpaceDepths[0] = m_fCascadePartitionsFrustum[0];
	pCB->cascadeFrustumEyeSpaceDepths[1] = m_fCascadePartitionsFrustum[1];
	pCB->cascadeFrustumEyeSpaceDepths[2] = m_fCascadePartitionsFrustum[2];
	pCB->cascadeFrustumEyeSpaceDepths[3] = m_fCascadePartitionsFrustum[3];
//#if NUM_CASCADES<4
//	pCB->padding2= 0.f;
//#endif
	pContext->Unmap( m_cbLight, 0 );

	//std::cout << m_fCascadePartitionsFrustum[0] << std::endl;
	//std::cout << m_fCascadePartitionsFrustum[1] << std::endl;
	//std::cout << m_fCascadePartitionsFrustum[2] << std::endl;

	pContext->CSSetConstantBuffers( CB_LOC::LIGHT_PROPERTIES, 1, &m_cbLight );
	pContext->VSSetConstantBuffers( CB_LOC::LIGHT_PROPERTIES, 1, &m_cbLight );	
	pContext->DSSetConstantBuffers( CB_LOC::LIGHT_PROPERTIES, 1, &m_cbLight );
	pContext->HSSetConstantBuffers( CB_LOC::LIGHT_PROPERTIES, 1, &m_cbLight );
	pContext->GSSetConstantBuffers( CB_LOC::LIGHT_PROPERTIES, 1, &m_cbLight );
	pContext->PSSetConstantBuffers( CB_LOC::LIGHT_PROPERTIES, 1, &m_cbLight );

	return hr;
}

HRESULT ShadowMapping::UpdateShadowCascadesCB( ID3D11DeviceContext1* pd3dImmediateContext )
{
	HRESULT hr = S_OK;
	D3D11_MAPPED_SUBRESOURCE MappedResource;

	V_RETURN(pd3dImmediateContext->Map( m_shadowCascadesProjCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ));

	CB_CASCADES_PROJECTION* pCB = ( CB_CASCADES_PROJECTION* )MappedResource.pData;		
	for(int i = 0; i < NUM_CASCADES; ++i)
	{
		pCB->mMatProjCascade[i] = m_matShadowProj[i];
	}
	
	pd3dImmediateContext->Unmap( m_shadowCascadesProjCB, 0 );

	return hr;
}


void ShadowMapping::DebugRenderCascade(ID3D11DeviceContext1* pd3dImmediateContext, ID3D11RenderTargetView* outputRTV) const
{
	pd3dImmediateContext->OMSetRenderTargets(1, &outputRTV, NULL );

	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	pd3dImmediateContext->PSSetShaderResources( 11, 1, &m_pDepthBufferSRV );
	
	pd3dImmediateContext->VSSetShader( m_pvsQuadBlur->Get(), NULL, 0 );
	pd3dImmediateContext->PSSetShader( m_visShadowPS->Get(), NULL, 0 );

	pd3dImmediateContext->Draw(4, 0);
	
	pd3dImmediateContext->PSSetShaderResources( 11, 1, g_ppSRVNULL );

	pd3dImmediateContext->PSSetShaderResources( 12, 1, g_ppSRVNULL );
}

HRESULT ShadowMapping::RenderShadowMap( ID3D11DeviceContext1* pd3dImmediateContext, CBaseCamera* renderCam, CModelViewerCamera* lightCam, const XMFLOAT3& lightDir, Scene* scene, RenderTri* triRenderer, RendererSubD* osdRenderer )
{
	HRESULT hr = S_OK;

	ID3D11RenderTargetView* nullView[] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL};
 
	//const float minDistance = m_useDepthReduction ? XMMax(m_depthMinMax.x-0.1f, 0.f) : m_fCascadePartitionsMin;
	//const float maxDistance = m_useDepthReduction ? XMMin(m_depthMinMax.y+0.1f,1.f) : m_fCascadePartitionsMax;
	const float minDistance = m_useDepthReduction ? XMMax(m_depthMinMax.x, 0.f) : m_fCascadePartitionsMin;
	const float maxDistance = m_useDepthReduction ? XMMin(m_depthMinMax.y, 1.f) : m_fCascadePartitionsMax;

	float cascadeSplits[4] = {0.0f, 0.0f, 0.0f, 0.0f};

	if(m_ePartitionMode == PartitionMode::MANUAL)
	{	
		cascadeSplits[0] = minDistance + m_fSplitDistance[0] * maxDistance;
		cascadeSplits[1] = minDistance + m_fSplitDistance[1] * maxDistance;
		cascadeSplits[2] = minDistance + m_fSplitDistance[2] * maxDistance;
#if NUM_CASCADES == 4
		cascadeSplits[3] = minDistance + m_fSplitDistance[3] * maxDistance;
#endif
	}
	else if(m_ePartitionMode == PartitionMode::LOGARTIHMIC ||  m_ePartitionMode == PartitionMode::PSSM)
	{
		float lambda = 1.0f;
		if(m_ePartitionMode == PartitionMode::PSSM)
		{
			lambda = m_pssmLambda;
		}

		float nearClip  = renderCam->GetNearClip();
		float farClip   = renderCam->GetFarClip();
		float clipRange = farClip - nearClip;

		float minZ = nearClip+minDistance*clipRange;
		float maxZ = nearClip+maxDistance*clipRange;

		float range = maxZ - minZ;
		float ratio = maxZ / minZ;
		//std::cout << "ration = " << ratio << std::endl;
		for(UINT i = 0; i < NUM_CASCADES; ++i)
		{
			float p = float(i+1) / (float)NUM_CASCADES;
			//std::cout << "p_" <<i << " = " << p  << std::endl;
			float log = minZ*std::pow(ratio, p);
			float uniform = minZ + range * p;
			float d = lambda*(log - uniform) + uniform;
			cascadeSplits[i] = (d-nearClip)/clipRange;
			//std::cout << "cascadeSplit"  << i << " = " << cascadeSplits[i] << std::endl;
		}
	}


	// cascade splits computed


	// render cascades
	XMVECTOR vLightDir = XMVector3Normalize(-XMLoadFloat3(&lightDir)); //checkme -lightdir	
	XMStoreFloat3(&m_lightDir, vLightDir);

	XMMATRIX renderViewProjectionMatrix = renderCam->GetViewMatrix()*renderCam->GetProjMatrix();
	// compute global shadow matrix
	XMMATRIX globalShadowMatrix;
	{
		
		XMVECTOR frustumCorners[8] =
		{
			XMVectorSet(-1.0f,  1.0f, 0.0f, 1.0f),
			XMVectorSet( 1.0f,  1.0f, 0.0f, 1.0f),
			XMVectorSet( 1.0f, -1.0f, 0.0f, 1.0f),
			XMVectorSet(-1.0f, -1.0f, 0.0f, 1.0f),
			XMVectorSet(-1.0f,  1.0f, 1.0f, 1.0f),
			XMVectorSet( 1.0f,  1.0f, 1.0f, 1.0f),
			XMVectorSet( 1.0f, -1.0f, 1.0f, 1.0f),
			XMVectorSet(-1.0f, -1.0f, 1.0f, 1.0f),
		};
		
		XMMATRIX invViewProj = XMMatrixInverse(NULL, renderViewProjectionMatrix );
		XMVECTOR frustumCenter = XMVectorSet(0,0,0,0);
		for(unsigned int i = 0; i < 8; ++i)
		{			
			frustumCorners[i] = XMVector3TransformCoord(frustumCorners[i], invViewProj);//Float3::Transform(frustumCorners[i], invViewProj);
			frustumCenter += frustumCorners[i];
		}

		frustumCenter /= 8.0f;
		//XMVectorSetW(frustumCenter, 1);
		// Pick the up vector to use for the light camera
		

		XMFLOAT3 upDir = renderCam->GetWorldRight();
		
		// This needs to be constant for it to be stable
		if(m_stabilizeCascades)
			upDir = XMFLOAT3(0.0f, 0.0f, 1.0f); // HENRY CHECKME 0,0,1?
		
		XMVECTOR vUpDir = XMLoadFloat3(&upDir);
		

		// Create a temporary view matrix for the light
		XMVECTOR lightCameraPos = frustumCenter;
		XMVECTOR lookAt = frustumCenter - vLightDir;
		XMMATRIX lightView = XMMatrixLookAtLH(lightCameraPos, lookAt, vUpDir);


		// Get position of the shadow camera
		XMVECTOR shadowCameraPos = frustumCenter + vLightDir * -0.5;//-0.5f;

		//PrintXMVECTOR_F4("light cam shadowCameraPos ", shadowCameraPos );
		//PrintXMVECTOR_F4("light cam lookAt ", lookAt );

		// Come up with a new orthographic camera for the shadow caster

		XMMATRIX shadowProj = XMMatrixOrthographicOffCenterLH(-0.5, 0.5, -0.5, 0.5, 0.0, 1.0);
		XMMATRIX shadowView = XMMatrixLookAtLH(shadowCameraPos, frustumCenter, vUpDir);
		XMMATRIX shadowViewProj = shadowView * shadowProj;
		//globalShadowMatrix = shadowView * shadowProj * XMMatrixScaling(0.5f, -0.5f, 1.0f) * XMMatrixTranslation(0.5f, 0.5f, 0.0f);
		globalShadowMatrix = shadowViewProj *   XMMatrixScaling(0.5f, -0.5f, 1.0f) * XMMatrixTranslation(0.5f, 0.5f, 0.0f);
		m_matShadowView = XMMatrixInverse(NULL, renderCam->GetViewMatrix()) * globalShadowMatrix;//shadowView;
		//m_matShadowView = shadowView;		
	}


	// render meshes to cascades
	for(uint32_t cascadeIdx = 0; cascadeIdx < NUM_CASCADES; ++cascadeIdx)
	{
		// set viewport
		//set shadow map as render target
		//clear

		// Get the 8 points of the view frustum in world space
		XMVECTOR frustumCornersWS[8] =
		{
			XMVectorSet(-1.0f,  1.0f, 0.0f, 1.0f),
			XMVectorSet( 1.0f,  1.0f, 0.0f, 1.0f),
			XMVectorSet( 1.0f, -1.0f, 0.0f, 1.0f),
			XMVectorSet(-1.0f, -1.0f, 0.0f, 1.0f),
			XMVectorSet(-1.0f,  1.0f, 1.0f, 1.0f),
			XMVectorSet( 1.0f,  1.0f, 1.0f, 1.0f),
			XMVectorSet( 1.0f, -1.0f, 1.0f, 1.0f),
			XMVectorSet(-1.0f, -1.0f, 1.0f, 1.0f),
		};

		//float prevSplitDist = cascadeIdx == 0 ? minDistance : m_fSplitDistance[cascadeIdx - 1];		
		//float splitDist = m_fSplitDistance[cascadeIdx];
		float prevSplitDist = cascadeIdx == 0 ? minDistance : cascadeSplits[cascadeIdx - 1];
		float splitDist = cascadeSplits[cascadeIdx];
		
		XMMATRIX invViewProj = XMMatrixInverse(nullptr, renderCam->GetViewMatrix()*renderCam->GetProjMatrix());
		for(uint32_t i = 0; i < 8; ++i)
		{
			//frustumCornersWS[i] = Float3::Transform(frustumCornersWS[i], invViewProj);
			frustumCornersWS[i] = XMVector3TransformCoord(frustumCornersWS[i], invViewProj);			
		}

		// Get the corners of the current cascade slice of the view frustum
		for(uint32_t i = 0; i < 4; ++i)
		{
			XMVECTOR cornerRay = frustumCornersWS[i + 4] - frustumCornersWS[i];
			XMVECTOR nearCornerRay = cornerRay * prevSplitDist;
			XMVECTOR farCornerRay = cornerRay * splitDist;
			frustumCornersWS[i + 4] = frustumCornersWS[i] + farCornerRay;
			frustumCornersWS[i] = frustumCornersWS[i] + nearCornerRay;
		}

		// Calculate the centroid of the view frustum slice
		XMVECTOR frustumCenter = XMVectorSet(0,0,0,0);
		for(uint32_t i = 0; i < 8; ++i)
			frustumCenter = frustumCenter + frustumCornersWS[i];
		frustumCenter /=  8.0f;
		//XMVectorSetW(frustumCenter,1.0);

		// Pick the up vector to use for the light camera
		XMFLOAT3 upDir = renderCam->GetWorldRight();
		XMVECTOR vUpDir = XMLoadFloat3(&upDir);

		XMVECTOR minExtents;
		XMVECTOR maxExtents;
		if(m_stabilizeCascades)
		{
			// This needs to be constant for it to be stable
			vUpDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.f);

			// Calculate the radius of a bounding sphere surrounding the frustum corners
			float sphereRadius = 0.0f;
			for(uint32_t i = 0; i < 8; ++i)
			{
				
				float dist = XMVectorGetX(XMVector3Length(frustumCornersWS[i] - frustumCenter));
				sphereRadius = std::max(sphereRadius, dist);
			}

			//sphereRadius = std::ceil(sphereRadius * 16.0f) / 16.0f;
			//sphereRadius = 0.76*std::ceil(sphereRadius * 16.0f) / 16.0f; // CHECKME magic numer
			sphereRadius = std::ceil(sphereRadius * 16.0f) / 16.0f; // CHECKME magic numer


			maxExtents = XMVectorSet(sphereRadius, sphereRadius, sphereRadius, 0);
			minExtents = -maxExtents;
		}
		else
		{
			// Create a temporary view matrix for the light
			XMVECTOR lightCameraPos = frustumCenter;
			XMVECTOR lookAt = frustumCenter - vLightDir;
			XMMATRIX lightView = XMMatrixLookAtLH(lightCameraPos, lookAt, vUpDir);

			// Calculate an AABB around the frustum corners
			XMVECTOR mins = g_vFLTMAX;
			XMVECTOR maxes = g_vFLTMIN;
			for(uint32_t i = 0; i < 8; ++i)
			{
				XMVECTOR corner = XMVector3TransformCoord(frustumCornersWS[i], lightView);
				mins = XMVectorMin(mins, corner);
				maxes = XMVectorMax(maxes, corner);
			}

			minExtents = mins;
			maxExtents = maxes;

			// Adjust the min/max to accommodate the filtering size
			float scale = (SHADOW_MAP_RES + BLUR_KERNEL_SIZE) / static_cast<float>(SHADOW_MAP_RES);
			XMVECTOR vScale = XMVectorSet(scale,scale,1,1);	// scale x and y, leave z 
			minExtents *= vScale;
			maxExtents *= vScale;
		}

		XMVECTOR cascadeExtents = maxExtents - minExtents;

		// Get position of the shadow camera
		XMVECTOR shadowCameraPos = frustumCenter + vLightDir * -XMVectorGetZ(minExtents);

		// Come up with a new orthographic camera for the shadow caster
		//OrthographicCamera shadowCamera(minExtents.x, minExtents.y, maxExtents.x,
		//	maxExtents.y, 0.0f, cascadeExtents.z);
		//shadowCamera.SetLookAt(shadowCameraPos, frustumCenter, upDir);

		// Come up with a new orthographic camera for the shadow caster

		//XMMATRIX shadowCameraProj = XMMatrixOrthographicOffCenterLH(	XMVectorGetX(minExtents),
		//																XMVectorGetX(maxExtents),
		//																XMVectorGetY(minExtents),
		//																XMVectorGetY(maxExtents),
		//																0.0f,
		//																XMVectorGetZ(cascadeExtents)
		//																);

		XMMATRIX shadowCameraProj = XMMatrixOrthographicOffCenterLH(	XMVectorGetX(minExtents),
																		XMVectorGetX(maxExtents),
																		XMVectorGetY(minExtents),
																		XMVectorGetY(maxExtents),
																		0.0f,
																		XMVectorGetZ(cascadeExtents)
			);

		XMMATRIX shadowCameraView = XMMatrixLookAtLH(shadowCameraPos, frustumCenter, vUpDir);

		XMMATRIX shadowCameraViewProj = shadowCameraView * shadowCameraProj ;

		if(m_stabilizeCascades)
		{
			// Create the rounding matrix, by projecting the world-space origin and determining
			// the fractional offset in texel space
			//XMMATRIX shadowMatrix = shadowCamera.ViewProjectionMatrix().ToSIMD();
			XMVECTOR shadowOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
			shadowOrigin = XMVector4Transform(shadowOrigin, shadowCameraViewProj);
			shadowOrigin = XMVectorScale(shadowOrigin, SHADOW_MAP_RES / 2.0f);

			XMVECTOR roundedOrigin = XMVectorRound(shadowOrigin);
			XMVECTOR roundOffset = XMVectorSubtract(roundedOrigin, shadowOrigin);
			roundOffset = XMVectorScale(roundOffset, 2.0f / SHADOW_MAP_RES);
			roundOffset = XMVectorSetZ(roundOffset, 0.0f);
			roundOffset = XMVectorSetW(roundOffset, 0.0f);

			//XMMATRIX shadowProj = shadowCamera.ProjectionMatrix().ToSIMD();
			//shadowProj.r[3] = XMVectorAdd(shadowProj.r[3], roundOffset);
			//shadowCamera.SetProjection(shadowProj);

			shadowCameraProj.r[3] = XMVectorAdd(shadowCameraProj.r[3], roundOffset);
			shadowCameraViewProj = shadowCameraView * shadowCameraProj ;			
		}

		m_matShadowProj[cascadeIdx] = shadowCameraProj;

		// Draw the mesh with depth only, using the new shadow camera
		//RenderDepthCPU(context, shadowCamera, world, characterWorld, true);
				
		

		triRenderer->SetGenShadows(true);
		osdRenderer->SetGenShadows(true);

		pd3dImmediateContext->RSSetState( m_prsShadow );

		g_app.SetViewMatrix(shadowCameraView);
		{
#ifdef PROFILE
			std::wostringstream ss;				
			ss << L"Render Shadow Cascade" << cascadeIdx;								
			PERF_EVENT_SCOPED(perfGen, ss.str().c_str());
#endif
			// CHECKME clear rtv needed?
			//float clearCol[4] = {0,0,0,0};
			//pd3dDeviceContext->ClearRenderTargetView(m_pCascadedShadowMapVarianceRTVArrayAll[iCurrentCascade],clearCol);

			
			//float clearCol[4] = {0,0,0,0};
			//pd3dImmediateContext->ClearRenderTargetView(m_pVarianceShadowArraySliceRTV[cascadeIdx],clearCol);

			pd3dImmediateContext->RSSetViewports( 1, &m_RenderOneTileVP );
			pd3dImmediateContext->OMSetRenderTargets( 1, &m_pVarianceShadowArraySliceRTV[cascadeIdx], m_pDepthBufferDSV );
			//ID3D11RenderTargetView* nullRenderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };
			//pd3dImmediateContext->OMSetRenderTargets( D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, nullRenderTargets, m_pDepthBufferDSV );
			pd3dImmediateContext->ClearDepthStencilView( m_pDepthBufferDSV, D3D11_CLEAR_DEPTH, 1.f, 0 );
			
			//g_app.SetProjectionMatrix(m_matShadowProj[cascadeIdx]);
			g_app.SetProjectionMatrix(shadowCameraProj);
			
			scene->RenderSceneObjects(pd3dImmediateContext, triRenderer, osdRenderer);

			pd3dImmediateContext->OMSetRenderTargets(1, nullView, NULL );
		}

		triRenderer->SetGenShadows(false);
		osdRenderer->SetGenShadows(false);


		//// Apply the scale/offset matrix, which transforms from [-1,1]
		//// post-projection space to [0,1] UV space
		XMMATRIX texScaleBias;
		texScaleBias.r[0] = XMVectorSet(0.5f,  0.0f, 0.0f, 0.0f);
		texScaleBias.r[1] = XMVectorSet(0.0f, -0.5f, 0.0f, 0.0f);
		texScaleBias.r[2] = XMVectorSet(0.0f,  0.0f, 1.0f, 0.0f);
		texScaleBias.r[3] = XMVectorSet(0.5f,  0.5f, 0.0f, 1.0f);
		XMMATRIX shadowMatrix = shadowCameraViewProj * texScaleBias;
		//shadowMatrix = XMMatrixMultiply(shadowMatrix, texScaleBias);
		//shadowMatrix = shadowMatrix *  texScaleBias;
		//shadowMatrix = shadowMatrix * g_matTextureScale * g_matTextureTranslation;
		
		// Store the split distance in terms of view space depth
		const float clipDist = renderCam->GetFarClip() - renderCam->GetNearClip();
		//meshPSConstants.Data.CascadeSplits[cascadeIdx] = camera.NearClip() + splitDist * clipDist;
		//fFrustumIntervalEnd = cascadeSplits[ iCascadeIndex ];        
		//fFrustumIntervalBegin = fFrustumIntervalBegin * clipRange;
		//fFrustumIntervalEnd = fFrustumIntervalEnd * clipRange;
		//m_fCascadePartitionsFrustum[cascadeIdx] = renderCam->GetNearClip() + splitDist * clipDist;
		//m_fCascadePartitionsFrustum[cascadeIdx] = cascadeSplits[ cascadeIdx ] * clipDist;//renderCam->GetNearClip() + splitDist * clipDist;
		//m_fCascadePartitionsFrustum[cascadeIdx] = cascadeSplits[ cascadeIdx ] * renderCam->GetNearClip() + splitDist * clipDist;
		m_fCascadePartitionsFrustum[cascadeIdx] = renderCam->GetNearClip() + splitDist * clipDist;
		////m_fCascadePartitionsFrustum[ cascadeIdx ] = fFrustumIntervalEnd;

		// Calculate the position of the lower corner of the cascade partition, in the UV space
		// of the first cascade partition
		XMMATRIX invCascadeMat = XMMatrixInverse(nullptr, shadowMatrix);
		XMVECTOR cascadeCorner = XMVector3TransformCoord(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), invCascadeMat);
		cascadeCorner = XMVector3TransformCoord(cascadeCorner, globalShadowMatrix);

		//// Do the same for the upper corner
		XMVECTOR otherCorner = XMVector3TransformCoord(XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f), invCascadeMat);
		otherCorner = XMVector3TransformCoord(otherCorner, globalShadowMatrix);

		//// Calculate the scale and offset
		XMVECTOR cascadeScale = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f) / (otherCorner - cascadeCorner);

		//meshPSConstants.Data.CascadeOffsets[cascadeIdx] = Float4(-cascadeCorner, 0.0f);
		//meshPSConstants.Data.CascadeScales[cascadeIdx] = Float4(cascadeScale, 1.0f);


		XMStoreFloat3(&m_cascadeOffsets[cascadeIdx],-cascadeCorner);
		XMStoreFloat3(&m_cascadeScales[cascadeIdx],cascadeScale);
		//m_cascadeOffsets[cascadeIdx] = fCascadeOffset;
		//m_cascadeScales[cascadeIdx] = fCascadeScale;

		//if(AppSettings::UseVSM())
		//	ConvertToVSM(context, cascadeIdx, meshPSConstants.Data.CascadeScales[cascadeIdx].To3D(),
		//	meshPSConstants.Data.CascadeScales[0].To3D());
	}

	if (BLUR_KERNEL_SIZE > 1 ) 
	{		
		UINT blurCascadesBelow = NUM_CASCADES; // -1 // -2

		PERF_EVENT_SCOPED(perfBlur, L"Prefilter Shadow (blur)");
		if(	m_useBlurCS)
		{
			if(!m_useBlurSharedMemCS)
			{		

				UINT blockSize =(SHADOW_MAP_RES + WORK_GROUP_SIZE_BLUR-1)/WORK_GROUP_SIZE_BLUR;

				//INT iKernelShader = ( m_iShadowBlurSize / 2 - 1 ); //3 goes to 0, 5 goes to 1 
				for ( UINT iCurrentCascade=0; iCurrentCascade < blurCascadesBelow; ++iCurrentCascade ) 
				{ 

					// blur in x
					pd3dImmediateContext->CSSetShaderResources(12,1,&m_pVarianceShadowArraySliceSRV[iCurrentCascade]);
					pd3dImmediateContext->CSSetUnorderedAccessViews(1,1,&m_pShadowTempUAV,NULL);
					pd3dImmediateContext->CSSetShader(m_blurXCS->Get(), NULL, 0);

					pd3dImmediateContext->Dispatch(blockSize,blockSize,1);

					pd3dImmediateContext->CSSetShaderResources(12,1,g_ppSRVNULL);
					pd3dImmediateContext->CSSetUnorderedAccessViews(1,1,g_ppUAVNULL,NULL);

					// blur in y
					pd3dImmediateContext->CSSetShaderResources(12,1,&m_pShadowTempSRV);
					pd3dImmediateContext->CSSetUnorderedAccessViews(1,1,&m_pVarianceShadowArraySliceUAV[iCurrentCascade],NULL);
					pd3dImmediateContext->CSSetShader(m_blurYCS->Get(), NULL, 0);

					pd3dImmediateContext->Dispatch(blockSize,blockSize,1);

					pd3dImmediateContext->CSSetShaderResources(12,1,g_ppSRVNULL);
					pd3dImmediateContext->CSSetUnorderedAccessViews(1,1,g_ppUAVNULL,NULL);
				}
			}
			else
			{


				UINT blockSize =(SHADOW_MAP_RES + ROW_TILE_W-1)/ROW_TILE_W;
				pd3dImmediateContext->CSSetSamplers( 6, 1, &m_pSamShadowPoint );
				pd3dImmediateContext->CSSetSamplers( 7, 1, &m_pSamShadowLinearClamp );

				//INT iKernelShader = ( m_iShadowBlurSize / 2 - 1 ); //3 goes to 0, 5 goes to 1 
				for ( UINT iCurrentCascade=0; iCurrentCascade < blurCascadesBelow; ++iCurrentCascade ) 
				{ 

					// blur in x
					pd3dImmediateContext->CSSetShaderResources(12,1,&m_pVarianceShadowArraySliceSRV[iCurrentCascade]);
					pd3dImmediateContext->CSSetUnorderedAccessViews(1,1,&m_pShadowTempUAV,NULL);
					pd3dImmediateContext->CSSetShader(m_blurXSharedMemCS->Get(), NULL, 0);

					pd3dImmediateContext->Dispatch(blockSize,SHADOW_MAP_RES,1);

					pd3dImmediateContext->CSSetShaderResources(12,1,g_ppSRVNULL);
					pd3dImmediateContext->CSSetUnorderedAccessViews(1,1,g_ppUAVNULL,NULL);

					// blur in y
					pd3dImmediateContext->CSSetShaderResources(12,1,&m_pShadowTempSRV);
					pd3dImmediateContext->CSSetUnorderedAccessViews(1,1,&m_pVarianceShadowArraySliceUAV[iCurrentCascade],NULL);
					pd3dImmediateContext->CSSetShader(m_blurYSharedMemCS->Get(), NULL, 0);

					pd3dImmediateContext->Dispatch(blockSize,SHADOW_MAP_RES,1);

					pd3dImmediateContext->CSSetShaderResources(12,1,g_ppSRVNULL);
					pd3dImmediateContext->CSSetUnorderedAccessViews(1,1,g_ppUAVNULL,NULL);
				}
				ID3D11SamplerState* ppSSNULL[] = {NULL,NULL};
				pd3dImmediateContext->CSSetSamplers( 6, 2, ppSSNULL );

			}
		}
		else
		{	
			ID3D11DepthStencilView *dsNullview = NULL;
			pd3dImmediateContext->IASetInputLayout( NULL );
			pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			pd3dImmediateContext->VSSetShader( m_pvsQuadBlur->Get(), NULL, 0 );
			pd3dImmediateContext->PSSetSamplers( 6, 1, &m_pSamShadowPoint );
			ID3D11ShaderResourceView *srvNull = NULL;

			if (BLUR_KERNEL_SIZE > 1 ) 
			{
				//INT iKernelShader = ( m_iShadowBlurSize / 2 - 1 ); //3 goes to 0, 5 goes to 1 
				for ( UINT iCurrentCascade=0; iCurrentCascade < blurCascadesBelow; ++iCurrentCascade ) 
				{ 

					pd3dImmediateContext->OMSetRenderTargets(1, &m_pShadowTempRTV, dsNullview );
					pd3dImmediateContext->PSSetShaderResources( 12, 1, &m_pVarianceShadowArraySliceSRV[iCurrentCascade] );
					pd3dImmediateContext->PSSetShader( m_ppsQuadBlurX->Get(), NULL, 0 );
					pd3dImmediateContext->Draw(4, 0);

					pd3dImmediateContext->PSSetShaderResources( 12, 1, &srvNull );
					pd3dImmediateContext->OMSetRenderTargets(1, &m_pVarianceShadowArraySliceRTV[iCurrentCascade], dsNullview );
					pd3dImmediateContext->PSSetShaderResources( 12, 1, &m_pShadowTempSRV );
					pd3dImmediateContext->PSSetShader( m_ppsQuadBlurY->Get(), NULL, 0 );
					pd3dImmediateContext->Draw(4, 0);
					pd3dImmediateContext->PSSetShaderResources( 12, 1, &srvNull );
				}
			}

			pd3dImmediateContext->RSSetState( NULL );
			pd3dImmediateContext->OMSetRenderTargets(1, nullView, NULL );
		}
	}
	return hr;
}

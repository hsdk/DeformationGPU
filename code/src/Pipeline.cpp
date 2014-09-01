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

#include "Pipeline.h"
#include "App.h"
#include "dynamics/Physics.h"

#include "scene/ModelInstance.h"
#include "scene/DXSubDModel.h"

#include "rendering/RendererTri.h"
#include "rendering/RendererSubD.h"

#include "Deformation.h"
#include "IntersectPatches.h"
#include "MemoryManager.h"
#include "TileOverlapUpdater.h"
#include "Voxelization.h"

DeformationPipeline g_deformationPipeline;

#define VOXELIZE_COLLIDER_OBB

using namespace DirectX;

void DeformationPipeline::DetectDeformableCollisionPairs(Physics* physicsEngine, bool useCollisionsFromPhysics)
{
	m_deformablePenetratorPairs.clear();

	if (useCollisionsFromPhysics)
	{
		// Get collission pairs
		int btNumPairs = physicsEngine->GetBroadPhase()->getOverlappingPairCache()->getNumOverlappingPairs();
		//int btNumPairs = physicsEngine->GetDynamicsWorld()->getPairCache()->getNumOverlappingPairs();

		if (btNumPairs > 0)
		{
			//g_rendererAdaptive.SetWithCulling(g_app.g_useCulling);			

			btBroadphasePair* btPairArray = physicsEngine->GetBroadPhase()->getOverlappingPairCache()->getOverlappingPairArrayPtr();			// Get the broadphase pair array
			//btBroadphasePair* btPairArray = physicsEngine->GetDynamicsWorld()->getPairCache()->getOverlappingPairArrayPtr();
			UINT numDeformableCollisions = 0;
			for (int i = 0; i < btNumPairs; ++i)
			{

				btBroadphasePair* btPair = &btPairArray[i];
				//std::cout << "pairs: " << btNumPairs << "groups: " << g_scene->GetModelGroups().size()  << std::endl;

				// Retrieve the userpointers - pointing to our mesh objects
				ModelGroup* m0 = (ModelGroup*)((btCollisionObject*)(btPair->m_pProxy0->m_clientObject))->getUserPointer();
				ModelGroup* m1 = (ModelGroup*)((btCollisionObject*)(btPair->m_pProxy1->m_clientObject))->getUserPointer();

				// Sanity check
				if (m0 != NULL && m1 != NULL)
				{

					// Only allow cases m0->IsDeformable() XOR m1->IsDeformable() == true
					if (m0->HasDeformables() && m1->HasDeformables())
					{
						//std::cerr << "m0 and m1 contain deformable, aborting";
						//exit(1);
					}

					if (!(m0->HasDeformables() || m1->HasDeformables()))
						continue;
					//std::cout << "collission: " << m0->GetName() << " with " << m1->GetName() << std::endl;

					if (physicsEngine->GetDynamicsWorld()->getDispatcher()->needsCollision((btCollisionObject*)(btPair->m_pProxy0->m_clientObject), (btCollisionObject*)(btPair->m_pProxy1->m_clientObject)))
					{
						//if(m0 && m1)
						//	std::cout << m0->GetName() << " needs collision with " << m1->GetName() << std::endl;
					}
					else
					{
						//std::cout << m0->GetName() << ", " << m1->GetName() << std::endl;
						if (!(m0->IsSpecial() || m1->IsSpecial()))
							continue;
					}

					bool caseA = m0->HasDeformables() && !m1->HasDeformables();
					bool caseB = m1->HasDeformables() && !m0->HasDeformables();
					bool caseC = m0->HasDeformables() && m1->HasDeformables();
					//std::cout << "m0: " << m0->GetName() << std::endl;
					//std::cout << "m1: " << m1->GetName() << std::endl;

					if (caseC)
					{
						//CHECKME DOES NOT WORK CURRENTLY with both deformables because opensubdiv meshes are not rendered 100% watertight near extraordinary vertices (gregory patches)
						//ApplyDeformation(pd3dImmediateContext, m0, m1); // m0 deformable, m1 penetrator
						std::cerr << "two deformables currently disabled" << std::endl;
						exit(1);
						//UpdateOverlap();
						//ApplyDeformation(pd3dImmediateContext, m1, m0);   // m1 deformable, m0 penetrator
					}
					else //(caseA || caseB) 
					{
						if (caseB)
						{
							// caseA == caseB if we swap m0 and m1  for caseB
							std::swap(m0, m1);
						}
						// m0 is the deformable object
						// m1 is the penetrator - can be an OpenSubDMesh or a DXModel - m1 has to be voxelized

						// todo: support both groups contain deformables
						for (auto* deformable : m0->modelInstances)
						{
							if (!(deformable->IsDeformable() && deformable->IsSubD()))	continue;


							// loop over penetrating meshes
							for (auto* penetrator : m1->modelInstances)
							{

								if (penetrator->IsDeformable()) continue;
								if (penetrator->IsCollider() == false) continue;
								numDeformableCollisions++;

#ifdef VOXELIZE_COLLIDER_OBB								
								DXObjectOrientedBoundingBox isctOBB = penetrator->GetOBBWorld();
#else
								DXObjectOrientedBoundingBox isctOBB = penetrator->GetOBBWorld().intersect(deformable->GetOBBWorld());
#endif
								if (!isctOBB.IsValid()) continue;

								//D3D11ObjectOrientedBoundingBox colliderObbScaled = penetrator->GetOBBWorld();

								if (g_app.g_withVoxelOBBRotate)
								{
									float r0 = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)-0.5f) * 2.0f * (float)M_PI; // -M_PI .. +M_PI
									float r1 = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)-0.5f) * 2.0f * (float)M_PI; // -M_PI .. +M_PI
									float r2 = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)-0.5f) * 2.0f * (float)M_PI; // -M_PI .. +M_PI
									float f = 32.0f;// g_VoxelJitterRotateDivider;

									r0 /= f; r1 /= f; r2 /= f;

									XMMATRIX RV = (XMMatrixRotationRollPitchYaw(r0, r1, r2));
									XMFLOAT4X4 R;
									XMStoreFloat4x4(&R, RV);
									DXObjectOrientedBoundingBox isctOBB2;
									//isctOBB.GetTransformedRotateAxesOnly(Matrix4x4<float>((float*)&R.m[0]), isctOBB2);
									isctOBB.GetTransformedRotateAxesOnly(RV, isctOBB2);
									//isctOBB2.scale(1.05f); // TODO: fix scale

									XMFLOAT3 corners[16];
									isctOBB.GetCornerPoints(&corners[0]);
									isctOBB2.GetCornerPoints(&corners[8]);
									DXObjectOrientedBoundingBox isctOBB3(&corners[0], 16, isctOBB2);

									isctOBB = isctOBB3;
								}

								if (g_app.g_withVoxelJittering)
								{
									//std::cout << "jitter range: " << 1.f/penetrator->GetVoxelGridDefinition().m_VoxelGridSize.z << std::endl;
									float randScale = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) / (0.5f + penetrator->GetVoxelGridDefinition().m_VoxelGridSize.z);//0.01;
									//std::cout << randScale << std::endl;
									isctOBB = isctOBB.scale(1.01f/*g_obbScaler*/ + randScale);
								}
								else
								{
									isctOBB = isctOBB.scale(1.01f/*g_obbScaler*/);
								}

								if (m_deformablePenetratorPairs.find(deformable) == m_deformablePenetratorPairs.end())
								{
									std::unordered_map < ModelInstance*, DXObjectOrientedBoundingBox > penetratorEntry;
									penetratorEntry[penetrator] = isctOBB;
									m_deformablePenetratorPairs[deformable] = penetratorEntry;
								}
								else
								{
									m_deformablePenetratorPairs[deformable][penetrator] = isctOBB;
								}




							}// end loop penetrators
						}// end deformable loop
					} // end 					
				}
			}
			//g_rendererAdaptive.SetWithCulling(false);
			//std::cout << "num deformable collisions: " << numDeformableCollisions << std::endl;
		}
	}
}



void DeformationPipeline::CheckAndApplyDeformation(ID3D11Device1* pd3dDevice, ID3D11DeviceContext1* pd3dImmediateContext)
{
	int numVoxelizedLastRun = 0;

	// batch process deformation
	const uint32_t maxBatchSize = XMMin(6, DEFORMATION_BATCH_SIZE);

	for (auto& deformationPair : m_deformablePenetratorPairs)
	{
		ModelInstance* deformable = deformationPair.first;

		// build batches for each deformable
		std::vector<std::unordered_map<ModelInstance*, DXObjectOrientedBoundingBox>> deformationBatches(1);
		uint32_t currBatch = 0;

		for (auto& penetratorMap : deformationPair.second)
		{
			ModelInstance* penetrator = penetratorMap.first;
			DXObjectOrientedBoundingBox isctOBB = penetratorMap.second;

			// add new batch if if current batch is filled
			if (deformationBatches[currBatch].size() >= maxBatchSize)
			{
				deformationBatches.push_back(std::unordered_map<ModelInstance*, DXObjectOrientedBoundingBox>());
				currBatch++;
			}

			deformationBatches[currBatch][penetrator] = isctOBB;
		}


		// process deformation batches		
		if (g_app.g_useCulling)
		{
			for (auto penetratorMap : deformationBatches)
			{
				if (deformable->IsSubD())
				{
					//g_intersectGPU.IntersectOBB(pd3dImmediateContext, deformable, &isctOBB);
					g_intersectGPU.IntersectOBBBatch(pd3dImmediateContext, deformable, penetratorMap);
					if (!g_app.g_memDebugDoPrealloc)
						g_memoryManager.ManageDisplacementTiles(pd3dImmediateContext, deformable); // alloc memory, we do only alloc so  use faster atomic variant instead of scan variant

					//if (isSubDPenetrator)
					//{
					//	//g_intersect.IntersectOBB(pd3dImmediateContext, penetrator, &isctOBB); 
					//}
				}

				// process deformation
				uint32_t batchIdx = 0;
				for (auto penetratorDef : penetratorMap)
				{
					auto penetrator = penetratorDef.first;
					const auto &isctOBB = penetratorDef.second;

					bool isSubDPenetrator = penetrator->IsSubD();
					g_voxelization.StartVoxelizeSolid(pd3dImmediateContext, penetrator, isctOBB, false);

					g_app.UpdateCameraCBVoxel(pd3dImmediateContext, penetrator->GetVoxelGridDefinition());
					g_app.SetCameraConstantBuffersAllStages(pd3dImmediateContext);

					if (isSubDPenetrator)
					{
						PERF_EVENT_SCOPED(perf, L"VOXLIZE_SUBD");
						//std::cout << penetrator->GetVoxelGridDefinition().m_VoxelGridSize.x << ", " <<penetrator->GetVoxelGridDefinition().m_VoxelGridSize.y << ", " << penetrator->GetVoxelGridDefinition().m_VoxelGridSize.z << std::endl;
						g_rendererSubD.Voxelize(pd3dImmediateContext, penetrator, g_app.g_useCulling);
					}
					else
					{
						PERF_EVENT_SCOPED(perf, L"VOXLIZE_TRI");
						g_renderTriMeshes.Voxelize(pd3dImmediateContext, penetrator);
					}

					g_voxelization.EndVoxelizeSolid(pd3dImmediateContext);

					if (g_app.g_bRunSimulation)
					{
						g_deformation.VoxelDeformOSD(pd3dImmediateContext, deformable, penetrator, &g_intersectGPU, batchIdx);
					}

					batchIdx++;
					numVoxelizedLastRun++;
				}
			}

		}
	}
}

HRESULT DeformationPipeline::Create(ID3D11Device1* pd3dDevice)
{
	HRESULT hr = S_OK;
	return hr;
}

void DeformationPipeline::Destroy()
{

}



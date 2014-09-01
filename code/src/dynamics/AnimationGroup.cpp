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
#include "AnimationGroup.h"
//#include "BulletCollision/CollisionDispatch/btGhostObject.h"

using namespace SkinningAnimationClasses;

AnimationGroup::AnimationGroup()
	:ModelGroup()
{

	m_skinningMgr = new SkinningMeshAnimationManager();
	currAnimation = 0;
	queuedAnimation = 0;	
}

AnimationGroup::~AnimationGroup()
{
	
	SAFE_DELETE(m_skinningMgr);
}

//void AnimationGroup::SetPhysicsObject( btPairCachingGhostObject* physicsObject)
//{
//	m_MYphysicsObject = physicsObject;
//}
//
//void AnimationGroup::UpdateModelMatrix()
//{
//////	//exit(1);
//	if(m_MYphysicsObject)
//	{
//		//btTransform trafo;
//		//btMotionState* ms=_physicsObject->getMotionState();		
//		//ms->getWorldTransform(trafo);
//
//		//// set models rendering trafo
//		//// variant 1: dx quaternion, translation
//		//btVector3 o = trafo.getOrigin();
//		//btQuaternion q = trafo.getRotation();
//		//_modelMatrix = _physicsScaleMatrix * XMMatrixRotationQuaternion(XMVectorSet(q.x(), q.y(), q.z(), q.w())) * XMMatrixTranslation(o.x(), o.y(), o.z()) ;
//		btTransform& trafo = m_MYphysicsObject->getWorldTransform();
//		btVector3 o = trafo.getOrigin();
//		btQuaternion q = trafo.getRotation(); //DirectX::XMMatrixRotationZ(float(M_PI)*0.5f) 
//		_modelMatrix = XMMatrixTranslation(0.0f, 0.0f, 2.0f)   * XMMatrixRotationZ(-float(M_PI)*0.5f) * XMMatrixRotationQuaternion(DirectX::XMVectorSet(q.x(), q.y(), q.z(), -q.w())) * XMMatrixTranslation(o.x(), o.y(), o.z()) ;
//	}
//}


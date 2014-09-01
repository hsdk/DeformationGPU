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
#include "Physics.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"

#include <iostream>

void PhyNearCallback(btBroadphasePair& collisionPair, btCollisionDispatcher& dispatcher, const btDispatcherInfo& dispatchInfo)
{
	btManifoldArray manifoldArray;
	collisionPair.m_algorithm->getAllContactManifolds(manifoldArray);
	
	dispatcher.defaultNearCallback(collisionPair, dispatcher, dispatchInfo);
}

void customNearCallback(btBroadphasePair& collisionPair, btCollisionDispatcher& dispatcher, const btDispatcherInfo& dispatchInfo)
{
	//btCollisionObject* colObj0 = (btCollisionObject*)collisionPair.m_pProxy0->m_clientObject;
	//btCollisionObject* colObj1 = (btCollisionObject*)collisionPair.m_pProxy1->m_clientObject;

	//if (dispatcher.needsCollision(colObj0,colObj1))
	//{
	//	std::cout  << " needs collision " << std::endl;
	//	////dispatcher will keep algorithms persistent in the collision pair
	//	//if (!collisionPair.m_algorithm)
	//	//{
	//	//	collisionPair.m_algorithm = dispatcher.findAlgorithm(colObj0,colObj1);
	//	//}

	//	//if (collisionPair.m_algorithm)
	//	//{
	//	//	btManifoldResult contactPointResult(colObj0,colObj1);

	//	//	if (dispatchInfo.m_dispatchFunc == 		btDispatcherInfo::DISPATCH_DISCRETE)
	//	//	{
	//	//		//discrete collision detection query
	//	//		collisionPair.m_algorithm->processCollision(colObj0,colObj1,dispatchInfo,&contactPointResult);
	//	//	} else
	//	//	{
	//	//		//continuous collision detection query, time of impact (toi)
	//	//		float toi = collisionPair.m_algorithm->calculateTimeOfImpact(colObj0,colObj1,dispatchInfo,&contactPointResult);
	//	//		if (dispatchInfo.m_timeOfImpact > toi)
	//	//			dispatchInfo.m_timeOfImpact = toi;

	//	//	}
	//	//}
	//}
	dispatcher.defaultNearCallback(collisionPair, dispatcher, dispatchInfo);

}

Physics::Physics()
{

}

Physics::~Physics()
{
	delete _dynamicsWorld;
	delete _broadphase;
	delete _collisionConfig;
	delete _dispatcher;
	delete _solver;	
	
}

HRESULT Physics::init()
{
	HRESULT hr = S_OK;

	// axis sweep broad phase
	//btVector3 worldMin(-1000,-1000,-1000);
	//btVector3 worldMax(1000,1000,1000);
	//_broadphase = new btAxisSweep3(worldMin,worldMax);
		
	_broadphase			= new btDbvtBroadphase();								
	_collisionConfig	= new btDefaultCollisionConfiguration();
	_dispatcher			= new btCollisionDispatcher(_collisionConfig);	
	//_dispatcher->setNearCallback(customNearCallback);

	_solver				= new btSequentialImpulseConstraintSolver();			
	_dynamicsWorld		= new btDiscreteDynamicsWorld(_dispatcher, _broadphase, _solver, _collisionConfig);	
	
	_dynamicsWorld->getPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());
	
	return hr;
}

void Physics::stepSimulation( float fElapsedTime, int maxSubsteps )
{
	_dynamicsWorld->stepSimulation(fElapsedTime, maxSubsteps);
}

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

// bullet physics wrapper
#pragma once

#include <btBulletCollisionCommon.h>
#include <btBulletDynamicsCommon.h>

// fwd delcs
class btBroadphaseInterface;			
class btCollisionConfiguration;
class btCollisionDispatcher;			
class btSequentialImpulseConstraintSolver;
class btDiscreteDynamicsWorld;

class Physics
{
public:
	Physics();
	~Physics();

	HRESULT init();
	void stepSimulation( float fElapsedTime, int maxSubsteps);

	__forceinline btDiscreteDynamicsWorld* GetDynamicsWorld() const {		return _dynamicsWorld;	}
	__forceinline btDiscreteDynamicsWorld* GetDynamicsWorld()  {		return _dynamicsWorld;	}

	__forceinline btBroadphaseInterface* GetBroadPhase() const { return _broadphase;}
private:
	btBroadphaseInterface*				 _broadphase;		// shape overlap detection
	btCollisionConfiguration*			 _collisionConfig;	// 
	btCollisionDispatcher*				 _dispatcher;		
	btSequentialImpulseConstraintSolver* _solver;			// physics solver
	btDiscreteDynamicsWorld*			 _dynamicsWorld;	// world container
};
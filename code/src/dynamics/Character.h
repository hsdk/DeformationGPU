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
#include "BulletDynamics/Vehicle/btRaycastVehicle.h"
#include "BulletDynamics/Character/btKinematicCharacterController.h"
#include "BulletDynamics/Character/btCharacterControllerInterface.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"

#include "Physics.h"
#include "scene/Scene.h"
#include "scene/DXModel.h"
#include "dynamics/AnimationGroup.h"
/*
class CharacterController : public btCharacterControllerInterface {
public:
	CharacterController() {
		m_rayLambda[0] = 1.0;
		m_rayLambda[1] = 1.0;
		m_halfHeight = 1.0;
		m_turnAngle = 0.0;
		m_maxLinearVelocity = 10.0;
		m_walkVelocity = 8.0; // meters/sec
		m_turnVelocity = 1.0; // radians/sec
		m_shape = NULL;
		m_rigidBody = NULL;
	}

	~CharacterController() {
	}

	void Create(const std::string& filename) {

	}

	void Setup(btScalar height = 2.0, btScalar width = 0.25, btScalar stepHeight = 0.25) {
		btVector3 spherePositions[2];
		btScalar sphereRadii[2];
		
		sphereRadii[0] = width;
		sphereRadii[1] = width;
		spherePositions[0] = btVector3(0.0, (height/btScalar(2.0) - width), 0.0);
		spherePositions[1] = btVector3(0.0, (-height/btScalar(2.0) + width), 0.0);

		m_halfHeight = height/btScalar(2.0);

		m_shape = new btMultiSphereShape(&spherePositions[0], &sphereRadii[0], 2);

		btTransform startTransform;
		startTransform.setIdentity();
		startTransform.setOrigin(btVector3(0.0, 2.0, 0.0));
		btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
		btRigidBody::btRigidBodyConstructionInfo cInfo(1.0, myMotionState, m_shape);
		m_rigidBody = new btRigidBody(cInfo);
		// kinematic vs. static doesn't work
		//m_rigidBody->setCollisionFlags( m_rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		m_rigidBody->setSleepingThresholds(0.0, 0.0);
		m_rigidBody->setAngularFactor(0.0);
	}

	void Destroy() {
		if (m_shape) {
			delete m_shape;
			m_shape = 0;
		}

		if (m_rigidBody) {
			delete m_rigidBody;
			m_rigidBody = 0;
		}
	}

	virtual void Reset() {

	}

	virtual void Warp(const btVector3& origin) {

	}

	virtual void RegisterPairCacheAndDispatcher(btOverlappingPairCache* pairCache, btCollisionDispatcher* dispatcher) {

	}

	btCollisionObject* getCollisionObject() {
		return m_rigidBody;
	}

	void preStep (const btCollisionWorld* collisionWorld) {
		btTransform xform;
		m_rigidBody->getMotionState()->getWorldTransform (xform);
		btVector3 down = -xform.getBasis()[1];
		btVector3 forward = xform.getBasis()[2];
		down.normalize ();
		forward.normalize();

		m_raySource[0] = xform.getOrigin();
		m_raySource[1] = xform.getOrigin();

		m_rayTarget[0] = m_raySource[0] + down * m_halfHeight * btScalar(1.1);
		m_rayTarget[1] = m_raySource[1] + forward * m_halfHeight * btScalar(1.1);

		class ClosestNotMe : public btCollisionWorld::ClosestRayResultCallback {
		public:
			ClosestNotMe (btRigidBody* me) : btCollisionWorld::ClosestRayResultCallback(btVector3(0.0, 0.0, 0.0), btVector3(0.0, 0.0, 0.0)) { m_me = me; }

			virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult,bool normalInWorldSpace) {
				if (rayResult.m_collisionObject == m_me)
					return 1.0;
				return ClosestRayResultCallback::addSingleResult (rayResult, normalInWorldSpace
			);
		}
		protected:
			btRigidBody* m_me;
		};

		ClosestNotMe rayCallback(m_rigidBody);

		for (int i = 0; i < 2; i++) {
			rayCallback.m_closestHitFraction = 1.0;
			collisionWorld->rayTest (m_raySource[i], m_rayTarget[i], rayCallback);
			if (rayCallback.hasHit())
			{
				m_rayLambda[i] = rayCallback.m_closestHitFraction;
			} else {
				m_rayLambda[i] = 1.0;
			}
		}
	}

	void playerStep (const btCollisionWorld* collisionWorld,btScalar dt,
					 int forward,
					 int backward,
					 int left,
					 int right,
					 int jump) {
		btTransform xform;
		m_rigidBody->getMotionState()->getWorldTransform (xform);

		// Handle turning
		if (left)
			m_turnAngle -= dt * m_turnVelocity;
		if (right)
			m_turnAngle += dt * m_turnVelocity;

		xform.setRotation (btQuaternion(btVector3(0.0, 1.0, 0.0), m_turnAngle));

		btVector3 linearVelocity = m_rigidBody->getLinearVelocity();
		btScalar speed = m_rigidBody->getLinearVelocity().length();

		btVector3 forwardDir = xform.getBasis()[2];
		forwardDir.normalize();
		btVector3 walkDirection = btVector3(0.0, 0.0, 0.0);
		btScalar walkSpeed = m_walkVelocity * dt;

		if (forward)
			walkDirection += forwardDir;
		if (backward)
			walkDirection -= forwardDir;


	
		if (!forward && !backward && onGround())
		{
			// Dampen when on the ground and not being moved by the player
			linearVelocity *= btScalar(0.2);
			m_rigidBody->setLinearVelocity(linearVelocity);
		} else {
			if (speed < m_maxLinearVelocity)
			{
				btVector3 velocity = linearVelocity + walkDirection * walkSpeed;
				m_rigidBody->setLinearVelocity(velocity);
			}
		}

		m_rigidBody->getMotionState()->setWorldTransform(xform);
		m_rigidBody->setCenterOfMassTransform(xform);
	}

	bool canJump() const {
		return onGround();
	}

	void jump() {
		if (!canJump())
			return;

		btTransform xform;
		m_rigidBody->getMotionState()->getWorldTransform(xform);
		btVector3 up = xform.getBasis()[1];
		up.normalize ();
		btScalar magnitude = (btScalar(1.0)/m_rigidBody->getInvMass()) * btScalar(8.0);
		m_rigidBody->applyCentralImpulse(up * magnitude);	
	}

	bool onGround () const {
		return m_rayLambda[0] < btScalar(1.0);
	}

private:
	btScalar m_halfHeight;
	btCollisionShape* m_shape;
	btRigidBody* m_rigidBody;

	btVector3 m_raySource[2];
	btVector3 m_rayTarget[2];
	btScalar m_rayLambda[2];
	btVector3 m_rayNormal[2];

	btScalar m_turnAngle;

	btScalar m_maxLinearVelocity;
	btScalar m_walkVelocity;
	btScalar m_turnVelocity;
};
*/
struct CharacterEvent {
	int frame;
	int forward;
	int backward;
	int left;
	int right;
	int jump;
};

btScalar characterHeight = 5.75f;
btScalar characterWidth = 0.75f;

class Character {
public:
	btKinematicCharacterController* m_character;
	btPairCachingGhostObject*		m_ghostObject;
	//btRigidBody*		m_ghostObject;
	float							m_cameraHeight;
	float							m_minCameraDistance;
	float							m_maxCameraDistance;
	btVector3 m_cameraPosition;

	Physics* m_physics;
	btVector3*	m_vertices;
	btAlignedObjectArray<btCollisionShape*> m_collisionShapes;
	btBroadphaseInterface*	m_overlappingPairCache;
	btCollisionDispatcher*	m_dispatcher;
	btConstraintSolver*	m_constraintSolver;
	btDynamicsWorld* m_dynamicsWorld;
	btDefaultCollisionConfiguration* m_collisionConfiguration;
	btTriangleIndexVertexArray*	m_indexVertexArrays;

	bool m_playRecord;
	bool m_dumpRecord;

	std::vector<CharacterEvent> m_record;
	int m_replayFrame;
	UINT m_currentReplayIndex;
	int m_frame;

	Character() {
		m_physics = NULL;
		m_character = NULL;
		m_cameraPosition = btVector3(30,30,30);
		m_forward = 0;
		m_backward = 0;
		m_left = 0;
		m_right = 0;
		m_jump = 0;

		m_frame = -1;
		m_replayFrame = -1;
		m_currentReplayIndex = 0;

		m_playRecord = false;
		m_dumpRecord = false;
	}

	virtual ~Character() {
		if (m_dumpRecord)
			StoreRecord();
	}
	
	void ResetControls() {
		m_forward = 0;
		m_backward = 0;
		m_left = 0;
		m_right = 0;
		m_jump = 0;
	}
	
	void Forward() {
		m_forward = 1;
	}
	void Backward() {
		m_backward = 1;
	}
	void Left() {
		m_left = 1;
	}
	void Right() {
		m_right = 1;
	}

	void Jump() {
		m_jump = 1;
	}

	void CaptureEvent() {
		if (m_jump > 0 || m_forward > 0 || m_backward > 0 || m_right > 0 || m_left > 0) {
			CharacterEvent event;
			m_frame++;
			event.frame = m_frame;
			event.backward = m_backward;
			event.forward = m_forward;
			event.jump = m_jump;
			event.left = m_left;
			event.right = m_right;
			m_record.push_back(event);
		}
	
	}

	void Replay(float fElapsedTime) {
		ResetControls();

		int numEvents = (int)m_record.size();
		m_replayFrame++;
		int i =	m_currentReplayIndex;
		bool found = false;
		for (int i = m_currentReplayIndex; i < numEvents; ++i) {
			CharacterEvent& event = m_record[i];
			if (event.frame > m_replayFrame) {
				// Nothing to replay!
				return;
			}
			if (event.frame == m_replayFrame) {
				found = true;
				m_currentReplayIndex = i;
				break;
			}

		}

		if (found) {
			CharacterEvent& event = m_record[m_currentReplayIndex];
			if (event.right)
				Right();
			if (event.left)
				Left();
			if (event.forward)
				Forward();
			if (event.backward)	
				Backward();
			if(event.jump)
				Jump();
		}
		move(fElapsedTime);
	}

	void Create(Physics* physics, Scene* scene, AnimationGroup* animModelGroup, bool dumpRecord = false, bool playRecord = false) { //, const XMFLOAT3& initialPosition) {
		m_dumpRecord = dumpRecord;
		m_playRecord = playRecord;

		if (m_playRecord) {
			LoadRecord();
		}

		
		m_physics = physics;
		m_dynamicsWorld = m_physics->GetDynamicsWorld();

		//btCollisionShape* groundShape = new btBoxShape(btVector3(50,3,50));
		//m_collisionShapes.push_back(groundShape);
		//m_collisionConfiguration = new btDefaultCollisionConfiguration();
		//m_dispatcher = new btCollisionDispatcher(m_collisionConfiguration);
		//btVector3 worldMin(-1000,-1000,-1000);
		//btVector3 worldMax(1000,1000,1000);
		//btAxisSweep3* sweepBP = new btAxisSweep3(worldMin,worldMax); 
		//m_overlappingPairCache = sweepBP;
		//
		//m_constraintSolver = //new btSequentialImpulseConstraintSolver();
		//m_dynamicsWorld = new btDiscreteDynamicsWorld(m_dispatcher,m_overlappingPairCache,m_constraintSolver,m_collisionConfiguration);
		m_dynamicsWorld->getDispatchInfo().m_allowedCcdPenetration = 0.00001f;
		m_dynamicsWorld->getPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());
		btTransform startTransform;
		startTransform.setIdentity();
		//startTransform.setOrigin(btVector3(0.0, 4.0, 0.0));
		//startTransform.setOrigin(btVector3(10.210098f,-1.6433364f,16.453260f));
	
		
		m_ghostObject = new btPairCachingGhostObject();
		
		//m_ghostObject->setCollisionFlags(btCollisionObject::CF_KINEMATIC_OBJECT);
		//sweepBP->getOverlappingPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());

		btConvexShape* capsule = new btCapsuleShapeZ(characterWidth,characterHeight);

		//btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
		//btVector3 localInertia(0,0,0);
		//btRigidBody::btRigidBodyConstructionInfo cInfo(50,myMotionState,capsule,localInertia);
		//m_ghostObject= new btRigidBody(50, myMotionState, capsule, localInertia);


		m_ghostObject->setCollisionShape(capsule);
		m_ghostObject->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);
		m_ghostObject->setWorldTransform(startTransform);
		


		btScalar stepHeight = btScalar(0.35);
		m_character = new btKinematicCharacterController(m_ghostObject, capsule, stepHeight);
		m_character->setUpAxis(2);
		m_character->setUseGhostSweepTest(true);

		///only collide with static for now (no interaction with dynamic objects)
		m_dynamicsWorld->addCollisionObject(m_ghostObject, btBroadphaseProxy::CharacterFilter, btBroadphaseProxy::StaticFilter| btBroadphaseProxy::DefaultFilter);
		m_dynamicsWorld->addAction(m_character);
		
		m_ghostObject->setUserPointer(animModelGroup);
		//animModelGroup->SetPhysicsObject(m_ghostObject);
		animModelGroup->SetGhostObject(m_ghostObject);
		//m_dynamicsWorld->setGravity(btVector3(0,0,0));
		m_character->setGravity(10.0f);
		clientResetScene();	
	}

	const DirectX::XMMATRIX& getTrafo() {		
		btTransform& trafo = m_ghostObject->getWorldTransform();
		btVector3 o = trafo.getOrigin();
		btQuaternion q = trafo.getRotation(); //DirectX::XMMatrixRotationZ(float(M_PI)*0.5f) 
		return /*DirectX::XMMatrixTranslation(0.0f, 0.0f, 2.0f)  **/ DirectX::XMMatrixRotationZ(-float(M_PI)*0.5f) * DirectX::XMMatrixRotationQuaternion(DirectX::XMVectorSet(q.x(), q.y(), q.z(), -q.w())) * DirectX::XMMatrixTranslation(o.x(), o.y(), o.z()) ;
	}

	const btTransform& getCurrentCharTrafo() const {
		return m_ghostObject->getWorldTransform();
	}

	void setCurrentCharTrafo(const btTransform& t) {
		m_ghostObject->setWorldTransform(t);
	}

	const btTransform& getTransform() {		
		return m_ghostObject->getWorldTransform();
	}

	//void debugDrawContacts();
	void move(float dt = 1.0) {
		if (m_dynamicsWorld)
		{			
			//if (m_character->onGround())
			//	fprintf(stderr, "onground\n");
			//else
			//	fprintf(stderr, "not onground\n");

			//during idle mode, just run 1 simulation step maximum
//			int maxSimSubSteps = m_idle ? 1 : 2;
//			if (m_idle)
//				dt = 1.0/420.f;

			///set walkDirection for our character
			btTransform xform;
			xform = m_ghostObject->getWorldTransform();

			btVector3 forwardDir = xform.getBasis()[0];
		//	printf("forwardDir=%f,%f,%f\n",forwardDir[0],forwardDir[1],forwardDir[2]);
			btVector3 upDir = xform.getBasis()[2];
			btVector3 strafeDir = xform.getBasis()[1];
			forwardDir.normalize ();
			upDir.normalize ();
			strafeDir.normalize ();

			btVector3 walkDirection = btVector3(0.0, 0.0, 0.0);
			btScalar walkVelocity = btScalar(1.1f) * 15.0f; // 4 km/h -> 1.1 m/s
			btScalar walkSpeed = walkVelocity * dt;

			//rotate view
			if (m_left)
			{
				btMatrix3x3 orn = m_ghostObject->getWorldTransform().getBasis();
				orn *= btMatrix3x3(btQuaternion(btVector3(0,0,1),-dt * 2.0f));
				m_ghostObject->getWorldTransform ().setBasis(orn);
			}

			if (m_right)
			{
				btMatrix3x3 orn = m_ghostObject->getWorldTransform().getBasis();
				orn *= btMatrix3x3(btQuaternion(btVector3(0,0,1),dt * 2.0f));
				m_ghostObject->getWorldTransform().setBasis(orn);
			}

			if (m_forward)
				walkDirection += forwardDir;

			if (m_backward)
				walkDirection -= forwardDir;	
			
			m_character->setWalkDirection(walkDirection*walkSpeed);
				

			if (m_jump) {
				m_character->setJumpSpeed(10.0f);

				if (m_character->canJump())
					m_character->jump();

			}
		}
	}

	virtual void clientResetScene() {
		m_dynamicsWorld->getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(m_ghostObject->getBroadphaseHandle(), m_dynamicsWorld->getDispatcher());
		m_character->reset (m_dynamicsWorld);
		///WTF
		m_character->warp(btVector3(10.210001f,-2.0306311f,16.576973f));	
	}


	void StoreRecord() {
		std::ofstream outfile("character.dump", std::ios::out | std::ios::binary);
		size_t N = m_record.size();
		if (N == 0) {
			outfile.close();
			return;
		}
		outfile.write(reinterpret_cast<char*>(&N), sizeof(size_t));
		outfile.write(reinterpret_cast<char*>(&m_record.front()), sizeof(CharacterEvent) * N);
		outfile.close();
	}
	
	void LoadRecord() {
		std::ifstream infile("character.dump", std::ios::in | std::ios::binary);
		size_t N = 0;
		infile.read(reinterpret_cast<char*>(&N), sizeof(size_t));
		if (N <= 0) {
			infile.close();
			exit(1);
		}
		m_record.clear();
		m_record.resize(N);
		infile.read(reinterpret_cast<char*>(&m_record.front()), sizeof(CharacterEvent) * N);
		infile.close();
	}

protected:
	int m_forward;
	int m_backward;
	int m_left;
	int m_right;
	int m_jump;
};

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

#include "dynamics/Car.h"

#include "scene/ModelLoader.h"
#include "scene/ModelInstance.h"
#include "scene/DXModel.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"

using namespace DirectX;

#define FORCE_ZAXIS_UP
#ifdef FORCE_ZAXIS_UP
		int rightIndex = 0; 
		int upIndex = 2; 
		int forwardIndex = 1;
		btVector3 wheelDirectionCS0(0,0,-1);
		btVector3 wheelAxleCS(1,0,0);
#else
		int rightIndex = 0;
		int upIndex = 1;
		int forwardIndex = 2;
		btVector3 wheelDirectionCS0(0,-1,0);
		btVector3 wheelAxleCS(-1,0,0);
#endif

#define CUBE_HALF_EXTENTS 2.0f

static btRigidBody*	localCreateRigidBody(float mass, const btTransform& startTransform,btCollisionShape* shape, Physics* physics) {
	btAssert((!shape || shape->getShapeType() != INVALID_SHAPE_PROXYTYPE));

	//rigidbody is dynamic if and only if mass is non zero, otherwise static
	bool isDynamic = (mass != 0.f);

	btVector3 localInertia(0,0,0);
	if (isDynamic)
		shape->calculateLocalInertia(mass,localInertia);

	//using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects

#define USE_MOTIONSTATE 1
#ifdef USE_MOTIONSTATE
	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);

	btRigidBody::btRigidBodyConstructionInfo cInfo(mass,myMotionState,shape,localInertia);

	btRigidBody* body = new btRigidBody(cInfo);
	body->setContactProcessingThreshold(BT_LARGE_FLOAT);

#else
	btRigidBody* body = new btRigidBody(mass,0,shape,localInertia);	
	body->setWorldTransform(startTransform);
#endif//

	physics->GetDynamicsWorld()->addRigidBody(body);

	return body;
}

Car::Car() {
	m_playRecord = false;
	m_dumpRecord = false;

	m_currentCarEvent.frame = -1;
	m_currentCarEvent.accelerateBackward = false;
	m_currentCarEvent.accelerateForward = false;
	m_currentCarEvent.breaking = false;
	m_currentCarEvent.left = false;
	m_currentCarEvent.right = false;

	m_EngineForce = 0.f;
	m_BreakingForce = 0.f;
	m_maxEngineForce = 7000;//3500.f;//this should be engine/velocity dependent
	m_maxBreakingForce = 100.f;
	m_VehicleSteering = 0.f;
	m_steeringIncrement = 3.0f;
	m_steeringDecrement = 1.0f;
	m_steeringClamp = 0.6f;
	m_wheelRadius = 0.85f;   // einsinktiefe in terrain, kleiner == tiefer
	m_wheelWidth = 0.9f;//0.5;//0.6f;
	m_wheelFriction = 2.5;//1.9f;//BT_LARGE_FLOAT;  0 = rutschig, groesser = mehr grip und mehr force 
	m_suspensionStiffness = 20;//15.0f;
	m_suspensionDamping =  15.5f;//0.1f;
	m_suspensionCompression = 4.4f; // 4.4
	m_rollInfluence = 0.0f;//0.1f;//0.1f;//1.0f;
	m_suspensionRestLength = .55f;//.35f;  // hoehe der raeder

	m_vehicleRayCaster = NULL;
	m_vehicle = NULL;
	m_wheelShape = NULL;
	m_carChassis = NULL;
	m_replayFrame = -1;
	m_currentReplayIndex = 0;

	m_initialPosition = btVector3(0,0,0);
}

Car::~Car() {
	SAFE_DELETE(m_vehicleRayCaster);
	SAFE_DELETE(m_vehicle);
	SAFE_DELETE(m_wheelShape);
	//SAFE_DELETE(m_carChassis);
	if (m_dumpRecord)
		StoreRecord();
}

void Car::Reset(Physics* physics) {
	// Set the car somewhere into the scene - FIX ME trafo parameter
	if (m_dumpRecord) {
		m_emptyEvent = false;
		m_currentCarEvent.reset = true;
	}
	btTransform bt;
	bt.setIdentity();
	bt.setOrigin(m_initialPosition);
	m_carChassisBody->setWorldTransform(bt);

	m_VehicleSteering = 0.f;
	//m_carChassisBody->setCenterOfMassTransform(btTransform::getIdentity());
	//m_carChassisBody->setMassProps(800, btVector3(0,0,0));
	m_carChassisBody->setLinearVelocity(btVector3(0,0,0));
	m_carChassisBody->setAngularVelocity(btVector3(0,0,0));
	physics->GetDynamicsWorld()->getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(
		m_carChassisBody->getBroadphaseHandle(),
		physics->GetDynamicsWorld()->getDispatcher()
	);

	if (m_vehicle) {
		m_vehicle->resetSuspension();
		for (int i=0;i<m_vehicle->getNumWheels();i++) {
			//synchronize the wheels with the (interpolated) chassis worldtransform
			m_vehicle->updateWheelTransform(i, true);
		}
	}
}

void Car::Create(Physics* physics, Scene* scene, std::string chassisFile, std::string wheelFile, bool dumpRecord, bool playRecord, const XMFLOAT3& initialPosition) {
	m_dumpRecord = dumpRecord;
	m_playRecord = playRecord;

	if (m_playRecord) {
		LoadRecord();
	}

	m_initialPosition = btVector3(initialPosition.x, initialPosition.y, initialPosition.z);
	m_wheelShape = new btCylinderShapeX(btVector3(m_wheelWidth, m_wheelRadius, m_wheelRadius));

	m_carChassis = new ModelGroup();
	ModelLoader::Load(chassisFile, scene, NULL, m_carChassis);

#ifdef FORCE_ZAXIS_UP
//   indexRightAxis = 0; 
//   indexUpAxis = 2; 
//   indexForwardAxis = 1; 
	btCollisionShape* chassisShape = new btBoxShape(btVector3(1.f,2.f, 0.5f));
	btCompoundShape* compound = new btCompoundShape();
	btTransform localTrans;
	localTrans.setIdentity();
	//localTrans effectively shifts the center of mass with respect to the chassis
	localTrans.setOrigin(btVector3(0,0,0));
#else
	btCollisionShape* chassisShape = new btBoxShape(btVector3(1.f,0.5f,2.f));
	m_collisionShapes.push_back(chassisShape);

	btCompoundShape* compound = new btCompoundShape();
	m_collisionShapes.push_back(compound);
	btTransform localTrans;
	localTrans.setIdentity();
	//localTrans effectively shifts the center of mass with respect to the chassis
	localTrans.setOrigin(btVector3(0,0.85,0));
#endif
	
	//compound->addChildShape(localTrans,chassisShape);
	btTransform tr;
	//tr.setIdentity();
	//tr.setOrigin(btVector3(0,0,1000));
	//m_carChassis->GetPhysicsObject()->setCenterOfMassTransform(tr);

	compound->addChildShape(localTrans, m_carChassis->GetPhysicsObject()->getCollisionShape());

	tr.setIdentity();
	tr.setOrigin(btVector3(0,0,0));

	m_carChassisBody = localCreateRigidBody(600, tr, compound, physics);//chassisShape);


	m_carWheels[0] = new ModelGroup();
	m_carWheels[1] = new ModelGroup();
	m_carWheels[2] = new ModelGroup();
	m_carWheels[3] = new ModelGroup();
	

	
	ModelLoader::Load(wheelFile,   scene, NULL, m_carWheels[0]);
	ModelLoader::Load(wheelFile,   scene, NULL, m_carWheels[1]);
	ModelLoader::Load(wheelFile,   scene, NULL, m_carWheels[2]);
	ModelLoader::Load(wheelFile,   scene, NULL, m_carWheels[3]);

	scene->AddGroup(m_carChassis);
	scene->AddGroup(m_carWheels[0]);
	scene->AddGroup(m_carWheels[1]);
	scene->AddGroup(m_carWheels[2]);
	scene->AddGroup(m_carWheels[3]);


	// The bad news: the wheels don't need to be physics objects, disable that crap
	m_carChassis->GetPhysicsObject()->setActivationState(DISABLE_SIMULATION);
	m_carWheels[0]->GetPhysicsObject()->setActivationState(DISABLE_SIMULATION);
	m_carWheels[1]->GetPhysicsObject()->setActivationState(DISABLE_SIMULATION);
	m_carWheels[2]->GetPhysicsObject()->setActivationState(DISABLE_SIMULATION);
	m_carWheels[3]->GetPhysicsObject()->setActivationState(DISABLE_SIMULATION);

	//m_carWheels[0]->GetPhysicsObject()->setActivationState(DISABLE_DEACTIVATION);
	//m_carWheels[1]->GetPhysicsObject()->setActivationState(DISABLE_DEACTIVATION);
	//m_carWheels[2]->GetPhysicsObject()->setActivationState(DISABLE_DEACTIVATION);
	//m_carWheels[3]->GetPhysicsObject()->setActivationState(DISABLE_DEACTIVATION);


	//m_carWheels[0]->GetPhysicsObject()->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
	//m_carWheels[1]->GetPhysicsObject()->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
	//m_carWheels[2]->GetPhysicsObject()->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
	//m_carWheels[3]->GetPhysicsObject()->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);

	// Since we get the transform from the magic car raycaster thing (hopefully)
	// this little ugly hack kills of the update from the physics matrix, which is not generated anyways.
	m_carChassis->ResetPhysicsObject();
	m_carWheels[0]->ResetPhysicsObject();
	m_carWheels[1]->ResetPhysicsObject();
	m_carWheels[2]->ResetPhysicsObject();
	m_carWheels[3]->ResetPhysicsObject();

	m_carWheels[0]->_special = true;
	m_carWheels[1]->_special = true;
	m_carWheels[2]->_special = true;
	m_carWheels[3]->_special = true;

	m_vehicleRayCaster = new btDefaultVehicleRaycaster(physics->GetDynamicsWorld());
	
	m_tuning.m_maxSuspensionForce = 40000.0f;
	m_vehicle = new btRaycastVehicle(m_tuning, m_carChassisBody ,m_vehicleRayCaster);

	m_carChassisBody->setActivationState(DISABLE_DEACTIVATION);
	
	physics->GetDynamicsWorld()->addVehicle(m_vehicle);

	//m_vehicle->rayCast.m_axle
	float connectionHeight = -m_suspensionRestLength - 0.1f;

	bool isFrontWheel = true;
	//choose coordinate system
	m_vehicle->setCoordinateSystem(rightIndex,upIndex,forwardIndex);

#ifdef FORCE_ZAXIS_UP
		btVector3 connectionPointCS0(CUBE_HALF_EXTENTS-(0.3f*m_wheelWidth),2.0f*CUBE_HALF_EXTENTS-m_wheelRadius, connectionHeight);
#else
		btVector3 connectionPointCS0(CUBE_HALF_EXTENTS-(0.3f*m_wheelWidth),connectionHeight,2.0f*CUBE_HALF_EXTENTS-m_wheelRadius);
#endif
		m_vehicle->addWheel(connectionPointCS0,wheelDirectionCS0,wheelAxleCS, m_suspensionRestLength,m_wheelRadius,m_tuning,isFrontWheel);

#ifdef FORCE_ZAXIS_UP
		connectionPointCS0 = btVector3(-CUBE_HALF_EXTENTS+(0.3f*m_wheelWidth),2*CUBE_HALF_EXTENTS-m_wheelRadius, connectionHeight);
#else
		connectionPointCS0 = btVector3(-CUBE_HALF_EXTENTS+(0.3f*m_wheelWidth),connectionHeight,2.0f*CUBE_HALF_EXTENTS-m_wheelRadius);
#endif
		m_vehicle->addWheel(connectionPointCS0,wheelDirectionCS0,wheelAxleCS, m_suspensionRestLength,m_wheelRadius,m_tuning,isFrontWheel);



		isFrontWheel = false;
#ifdef FORCE_ZAXIS_UP
		connectionPointCS0 = btVector3(-CUBE_HALF_EXTENTS+(0.3f*m_wheelWidth),-2*CUBE_HALF_EXTENTS+m_wheelRadius, connectionHeight);
#else
		connectionPointCS0 = btVector3(-CUBE_HALF_EXTENTS+(0.3f*m_wheelWidth),connectionHeight,-2.0f*CUBE_HALF_EXTENTS+m_wheelRadius);
#endif //FORCE_ZAXIS_UP
		m_vehicle->addWheel(connectionPointCS0,wheelDirectionCS0,wheelAxleCS, m_suspensionRestLength,m_wheelRadius,m_tuning,isFrontWheel);
#ifdef FORCE_ZAXIS_UP
		connectionPointCS0 = btVector3(CUBE_HALF_EXTENTS-(0.3f*m_wheelWidth),-2.0f*CUBE_HALF_EXTENTS+m_wheelRadius, connectionHeight);
#else
		connectionPointCS0 = btVector3(CUBE_HALF_EXTENTS-(0.3f*m_wheelWidth),connectionHeight,-2.0f*CUBE_HALF_EXTENTS+m_wheelRadius);
#endif
		m_vehicle->addWheel(connectionPointCS0,wheelDirectionCS0,wheelAxleCS, m_suspensionRestLength,m_wheelRadius,m_tuning,isFrontWheel);

		
	for (int i=0;i<m_vehicle->getNumWheels();i++) {
		btWheelInfo& wheel = m_vehicle->getWheelInfo(i);
		wheel.m_suspensionStiffness = m_suspensionStiffness;
		wheel.m_wheelsDampingRelaxation = m_suspensionDamping;
		wheel.m_wheelsDampingCompression = m_suspensionCompression;
		wheel.m_frictionSlip = m_wheelFriction;
		wheel.m_rollInfluence = m_rollInfluence;
	}

	Reset(physics);
	Update(physics);

}

 void Car::Update(Physics* physics, float dt) {
	if (m_VehicleSteering < 0) {
		m_VehicleSteering += m_steeringDecrement * dt;
	} else {
		m_VehicleSteering -= m_steeringDecrement * dt;
	}

	btVector3 worldBoundsMin,worldBoundsMax;
	physics->GetDynamicsWorld()->getBroadphase()->getBroadphaseAabb(worldBoundsMin,worldBoundsMax);

	for (int i = 0; i < m_vehicle->getNumWheels(); i++) {
		//synchronize the wheels with the (interpolated) chassis worldtransform
		m_vehicle->updateWheelTransform(i, true);
		
		// set models rendering trafo
		// variant 1: dx quaternion, translation
		btTransform& trafo = m_vehicle->getWheelInfo(i).m_worldTransform;
		btVector3 o = trafo.getOrigin();
		btQuaternion q = trafo.getRotation();
		//m_carWheels[i]->GetPhysicsObject()->setWorldTransform(trafo);
		if (i == 0 || i == 3)
			m_carWheels[i]->_modelMatrix = m_carWheels[i]->_physicsScaleMatrix * XMMatrixRotationZ(-float(M_PI)*0.5f)  * XMMatrixRotationQuaternion(DirectX::XMVectorSet(q.x(), q.y(), q.z(), q.w())) * DirectX::XMMatrixTranslation(o.x(), o.y(), o.z()) ;
		else
			m_carWheels[i]->_modelMatrix = m_carWheels[i]->_physicsScaleMatrix * XMMatrixRotationZ(float(M_PI)*0.5f)  * XMMatrixRotationQuaternion(DirectX::XMVectorSet(q.x(), q.y(), q.z(), q.w())) * DirectX::XMMatrixTranslation(o.x(), o.y(), o.z()) ;
	}

	btTransform trafo;
	btMotionState* ms= m_carChassisBody->getMotionState();		
	ms->getWorldTransform(trafo);

	// set models rendering trafo
	// variant 1: dx quaternion, translation
	btVector3 o = trafo.getOrigin();
	btQuaternion q = trafo.getRotation();
	m_carChassis->_modelMatrix = /*m_carChassis->_physicsScaleMatrix */ XMMatrixTranslation(0.0f, 0.0f, 0.0f) * XMMatrixRotationQuaternion(XMVectorSet(q.x(), q.y(), q.z(), q.w())) * XMMatrixTranslation(o.x(), o.y(), o.z()) ;

	// Apply wheelsettings
	int wheelIndex = 2;
	//m_vehicle->applyEngineForce(m_EngineForce, wheelIndex);
	m_vehicle->setBrake(m_BreakingForce, wheelIndex);
	m_vehicle->setSteeringValue(-m_VehicleSteering*0.25f, wheelIndex);
	wheelIndex = 3;
	//m_vehicle->applyEngineForce(m_EngineForce, wheelIndex);
	m_vehicle->setBrake(m_BreakingForce, wheelIndex);
	m_vehicle->setSteeringValue(-m_VehicleSteering*0.25f, wheelIndex);
	
	// Front Wheels
	wheelIndex = 0;
	m_vehicle->setSteeringValue(m_VehicleSteering, wheelIndex);
	m_vehicle->applyEngineForce(m_EngineForce, wheelIndex);
	m_vehicle->setBrake(m_BreakingForce, wheelIndex);
	wheelIndex = 1;
	m_vehicle->setSteeringValue(m_VehicleSteering, wheelIndex);
	m_vehicle->applyEngineForce(m_EngineForce, wheelIndex);
	m_vehicle->setBrake(m_BreakingForce, wheelIndex);
}

void Car::BeginEvent(float d) {
	if (!m_dumpRecord) return;
	m_currentCarEvent.frame++;
	m_currentCarEvent.accelerateBackward = false;
	m_currentCarEvent.accelerateForward = false;
	m_currentCarEvent.breaking = false;
	m_currentCarEvent.left = false;
	m_currentCarEvent.right = false;
	m_currentCarEvent.reset = false;
	m_emptyEvent = true;
}

void Car::StartAccelerate(float d) {
	if (m_dumpRecord) {
		m_emptyEvent = false;
		if (d > 0.0f)
			m_currentCarEvent.accelerateForward = true;
		else
			m_currentCarEvent.accelerateBackward = true;
	}
	m_EngineForce = m_maxEngineForce * d;
};

void Car::StopAccelerate() {
	m_EngineForce = 0.0f;
};

void Car::StartBreak() {
	if (m_dumpRecord) {
		m_emptyEvent = false;
		m_currentCarEvent.breaking = true;
	}
	m_BreakingForce = m_maxBreakingForce;
};

void Car::StopBreak() {
	//fprintf(stderr, "stop break\n");
	m_BreakingForce = 0.0f;
};

void Car::SteerLeft(float dt) {
	if (m_dumpRecord) {
		m_emptyEvent = false;
		m_currentCarEvent.left = true;
	}
	m_VehicleSteering += m_steeringIncrement * dt;
	if (m_VehicleSteering > m_steeringClamp)
		m_VehicleSteering = m_steeringClamp;
};

void Car::SteerRight(float dt) {
	if (m_dumpRecord) {
		m_emptyEvent = false;
		m_currentCarEvent.right = true;
	}
	m_VehicleSteering -= m_steeringIncrement * dt;
	if (m_VehicleSteering < -m_steeringClamp)
		m_VehicleSteering = -m_steeringClamp;
};

void Car::EndEvent() {
	if (!m_dumpRecord) return;
	if (m_emptyEvent == false) {
		m_record.push_back(m_currentCarEvent);
	}
}

void Car::Replay(float fElapsedTime, Physics* physics) {
	StopAccelerate();
	StopBreak();

	int numEvents = (int)m_record.size();
	m_replayFrame++;
	int i =	m_currentReplayIndex;
	bool found = false;
	for (int i = m_currentReplayIndex; i < numEvents; ++i) {
		CarEvent& event = m_record[i];
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
		CarEvent& event = m_record[m_currentReplayIndex];
		if (event.right)
			SteerRight(fElapsedTime);
		if (event.left)
			SteerLeft(fElapsedTime);				
		if (event.accelerateForward)
			StartAccelerate();
		if (event.accelerateBackward)	
			StartAccelerate(-0.5f);
		if (event.breaking)
			StartBreak();
		if (event.reset)
			Reset(physics);

	}
}

void Car::StoreRecord() {
	std::ofstream outfile("car.dump", std::ios::out | std::ios::binary);
	size_t N = m_record.size();
	if (N == 0) {
		outfile.close();
		return;
	}
	outfile.write(reinterpret_cast<char*>(&N), sizeof(size_t));
	outfile.write(reinterpret_cast<char*>(&m_record.front()), sizeof(CarEvent) * N);
	outfile.close();
}

void Car::LoadRecord() {
	std::ifstream infile("car.dump", std::ios::in | std::ios::binary);
	size_t N = 0;
	infile.read(reinterpret_cast<char*>(&N), sizeof(size_t));
	if (N <= 0) {
		infile.close();
		exit(1);
	}
	m_record.clear();
	m_record.resize(N);
	infile.read(reinterpret_cast<char*>(&m_record.front()), sizeof(CarEvent) * N);
	infile.close();
}

bool Car::IsReplaying() {
	return m_playRecord;
}

XMFLOAT3 Car::getPosition() {
	btTransform trafo;
	btMotionState* ms= m_carChassisBody->getMotionState();		
	ms->getWorldTransform(trafo);

	// set models rendering trafo
	// variant 1: dx quaternion, translation
	btVector3 o = trafo.getOrigin();
	return XMFLOAT3(o.x(), o.y(), o.z());
}

XMFLOAT3 Car::getDirection() {
	btTransform trafo;
	btMotionState* ms= m_carChassisBody->getMotionState();	
	ms->getWorldTransform(trafo);
	btQuaternion r = trafo.getRotation();
	btVector3 v = quatRotate(r, btVector3(0,1,0));
	// set models rendering trafo
	// variant 1: dx quaternion, translation
	//btVector3 o = trafo.getOrigin();
	return XMFLOAT3(v.x(), v.y(), v.z());
}


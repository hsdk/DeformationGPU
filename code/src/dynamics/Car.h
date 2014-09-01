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
#include "Physics.h"
#include "scene/Scene.h"

class btRigidBody;
class btVehicleTuning;
struct btVehicleRaycaster;
class btCollisionShape;

struct CarEvent {
	UINT frame;
	bool accelerateForward;	
	bool accelerateBackward;
	bool breaking;
	bool left;
	bool right;
	bool reset;
};

class Car {
public:
	Car();

	~Car();

	void Create(Physics* physics, Scene* scene, std::string chassisFile, std::string wheelFile, bool dumpRecord = false, bool playRecord = false, const DirectX::XMFLOAT3& initialPosition = DirectX::XMFLOAT3(0.f,0.0f,30.0f));
	void Update(Physics* physics, float dt = 0.0f);

	void Reset(Physics* physics);
	
	void BeginEvent(float d);

	void StartAccelerate(float d = 1.0f); // forward key down
	void StopAccelerate(); // forward key up

	void StartBreak(); // backward key down
	void StopBreak(); // backward key up

	void SteerLeft(float dt = 1.0f); // left key down
	void SteerRight(float dt = 1.0f); // right key down#

	void EndEvent();
	void Replay(float fElapsedTime, Physics* physics);

	bool IsReplaying();

	DirectX::XMFLOAT3 getPosition();
	DirectX::XMFLOAT3 getDirection();
	
	void StoreRecord();
	void LoadRecord();

	ModelGroup* GetCarWheels(int idx) { return m_carWheels[idx];}
private:
	btRaycastVehicle::btVehicleTuning	m_tuning;
	btVehicleRaycaster*	m_vehicleRayCaster;
	btRaycastVehicle*	m_vehicle;
	btCollisionShape*	m_wheelShape;

	btRigidBody* m_carChassisBody;
	ModelGroup* m_carChassis;
	ModelGroup* m_carWheels[4];
	
	float m_EngineForce;
	float m_BreakingForce;
	float m_maxEngineForce;
	float m_maxBreakingForce;
	float m_VehicleSteering;
	float m_steeringIncrement;
	float m_steeringDecrement;
	float m_steeringClamp;
	float m_wheelRadius;
	float m_wheelWidth;
	float m_wheelFriction;
	float m_suspensionStiffness;
	float m_suspensionDamping;
	float m_suspensionCompression;
	float m_rollInfluence;
	btVector3 m_initialPosition;

	float m_suspensionRestLength;

	bool m_playRecord;
	bool m_dumpRecord;

	CarEvent m_currentCarEvent;
	bool m_emptyEvent;
	std::vector<CarEvent> m_record;
	UINT m_replayFrame;
	UINT m_currentReplayIndex;
};
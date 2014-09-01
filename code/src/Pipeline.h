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
#include <unordered_map>

class ModelInstance;
class DXObjectOrientedBoundingBox;
class Physics;


typedef std::unordered_map<ModelInstance*, std::unordered_map<ModelInstance*, DXObjectOrientedBoundingBox>> DeformableCollisionPair;

class DeformationPipeline
{
public:
	DeformationPipeline(){};
	~DeformationPipeline(){};
	HRESULT Create(ID3D11Device1* pd3dDevice);
	void Destroy();

	const DeformableCollisionPair& GetCollisionPairs() { return m_deformablePenetratorPairs; };
	void DetectDeformableCollisionPairs(Physics* physicsEngine, bool useCollisionsFromPhysics);
	void CheckAndApplyDeformation(ID3D11Device1* pd3dDevice, ID3D11DeviceContext1* pd3dImmediateContext);

protected:

	DeformableCollisionPair m_deformablePenetratorPairs;
};

extern DeformationPipeline g_deformationPipeline;
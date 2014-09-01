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

#include "scene/ModelInstance.h"
#include "SkinningAnimation.h"

// class to manage animated models
class AnimationGroup : public ModelGroup
{
public:
	AnimationGroup();
	virtual ~AnimationGroup();

	SkinningAnimationClasses::SkinningMeshAnimationManager* GetAnimationManager() { return m_skinningMgr; }
	const SkinningAnimationClasses::SkinningMeshAnimationManager* GetAnimationManager() const { return m_skinningMgr; }
	int GetCurrentAnimation() const {return currAnimation;}

protected:	
	SkinningAnimationClasses::SkinningMeshAnimationManager* m_skinningMgr;
	std::vector<std::string> m_animNames;
	int currAnimation;
	int queuedAnimation;	
};
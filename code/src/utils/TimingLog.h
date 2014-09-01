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

#include<DXUT.h>
#include <SDX/StringConversion.h>
#include <iostream>
 
class TimingLog
{
public:
	TimingLog(void) {
		//number of test runs for timings
		m_uNumRuns = 100;
		resetTimings();
	}

	~TimingLog(void) {

	}

	void resetTimings() {
		m_dDrawTotal = 0.0;
		m_uDrawCount = 0;
		m_dDrawSubDTotal = 0.0;
		m_uDrawSubDCount = 0;

		m_dVoxelizationTotal = 0.0;
		m_uVoxelizationCount = 0;
		m_dCullingTotal = 0.0;
		m_uCullingCount = 0;
		m_dRayCastTotal = 0.0;
		m_uRayCastCount = 0;
		m_dCompactVisibilityAllTotal = 0.0;
		m_uCompactVisibilityAllOverlapCount = 0;
		m_dOverlapTotal = 0.0;
		m_uOverlapCount = 0;
		m_dGPUSubdivTotal = 0.0;
		m_uGPUSubdivCount = 0;

		m_dShadowTotal = 0.0;
		m_uShadowCount = 0;

	}

	void printTimings() {
		std::cout << std::endl;
		std::cout << "Draw Tri\t\t" << m_dDrawTotal/(double)m_uDrawCount << " ms " << std::endl;
		std::cout << "Draw SubD\t\t" << m_dDrawSubDTotal/(double)m_uDrawSubDCount << " ms " << std::endl;
		std::cout << "Voxelization\t" << m_dVoxelizationTotal/(double)m_uVoxelizationCount << " ms " << std::endl;
		std::cout << "Culling\t\t" << m_dCullingTotal/(double)m_uCullingCount << " ms " << std::endl;
		std::cout << "RayCast\t\t" << m_dRayCastTotal/(double)m_uRayCastCount << " ms " << std::endl;
		std::cout << "Compact Vis\t\t" << m_dCompactVisibilityAllTotal/(double)m_uCompactVisibilityAllOverlapCount << " ms " << std::endl;
		std::cout << "Overlap\t\t" << m_dOverlapTotal/(double)m_uOverlapCount << " ms " << std::endl;
		std::cout << "GPU Subdiv\t\t" << m_dGPUSubdivTotal/(double)m_uGPUSubdivCount << " ms " << std::endl;
		//std::cout << "Shadow Cascades\t\t" << m_dShadowTotal/(double)m_uShadowCount << " ms " << std::endl;
	}

	double	m_dDrawTotal;
	UINT	m_uDrawCount;
	double  m_dDrawSubDTotal;
	UINT    m_uDrawSubDCount;
	double	m_dVoxelizationTotal;
	UINT	m_uVoxelizationCount;
	double	m_dCullingTotal;
	UINT	m_uCullingCount;
	double	m_dRayCastTotal;
	UINT	m_uRayCastCount;
	double	m_dOverlapTotal;
	UINT	m_uOverlapCount;
	double	m_dGPUSubdivTotal;
	UINT	m_uGPUSubdivCount;
	
	double	m_dCompactVisibilityAllTotal;
	UINT	m_uCompactVisibilityAllOverlapCount;

	double	m_dShadowTotal;
	UINT	m_uShadowCount;

	UINT	m_uNumRuns;
};


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

//! returns the time in seconds
double GetTime();

//! returns the time in milliseconds
double GetTimeMS();

//namespace hg {

long long int getFrequency();
long long int getCounter();

class Timer {
public:
	Timer();
	~Timer();

	void ResetAccumulation();

	void Begin(); // Start timer
	void End(); // Stop timer
	void EndBegin(); // Stop accumulate and "restart" timer

	// Total runtime
	double SecondsFromCreation();
	double MilliSecondsFromCreation();

	// Results from the last measurement
	double SecondsLastMeasurement();
	double MilliSecondsLastMeasurement();

	// Accumulated results
	double SecondsTotal();
	double SecondsAvg();
	double MilliSecondsTotal();
	double MilliSecondsAvg();

private:
	bool m_running;
	long long int m_begin;
	long long int m_end;

	long long int m_currentDelta;
	long long int m_accumulatedTime;
	int m_nSessions;

	long long int m_creationTime;
	long long int m_frequency;

};

//}

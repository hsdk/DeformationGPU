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
#include <Windows.h>
#include "Timer.h"

long long int GetFrequency() {
	static long long int frequency = 0;
	if (!frequency)
		QueryPerformanceFrequency((LARGE_INTEGER*)&frequency);
	return frequency;
}

long long int GetCounter() {
	long long int i;
	QueryPerformanceCounter((LARGE_INTEGER*)&i);
	return i;
}

Timer::Timer() : m_running(false), m_begin(0), m_end(0), m_currentDelta(0), m_accumulatedTime(0), m_nSessions(0) {
	m_creationTime = GetCounter();
}

Timer::~Timer() {

}

void Timer::ResetAccumulation() {
	m_begin =
	m_end =
	m_accumulatedTime = 0;
	m_nSessions = 0;
}

void Timer::Begin() {
	m_running = true;
	m_begin = GetCounter();
}

void Timer::End() {
	m_end = GetCounter();
	m_nSessions++;
	m_currentDelta = m_end - m_begin;
	m_accumulatedTime += m_currentDelta;
	m_running = false;
}

void Timer::EndBegin() {
	End();
	m_begin = m_end;
	m_running = true;
}

double Timer::SecondsFromCreation() {
	return (double)(GetCounter() - m_creationTime)/(double)GetFrequency();
}

double Timer::MilliSecondsFromCreation() {
	return 1000.0 * SecondsFromCreation();
}

double Timer::SecondsLastMeasurement() {
	return (double)m_currentDelta/(double)GetFrequency();
}

double Timer::MilliSecondsLastMeasurement() {
	return 1000.0 * SecondsLastMeasurement();
}

double Timer::SecondsTotal() {
	return (double)m_accumulatedTime / (double)GetFrequency();
}

double Timer::SecondsAvg() {
	if (m_nSessions)
		return SecondsTotal()/(double)m_nSessions;	
	return -1.0;
}


double Timer::MilliSecondsTotal() {
	return 1000.0 * SecondsTotal();
}

double Timer::MilliSecondsAvg() {
	return 1000.0 * SecondsAvg();
}




//}

double GetTime()
{
	unsigned __int64 pf;
	QueryPerformanceFrequency( (LARGE_INTEGER *)&pf );
	double freq_ = 1.0 / (double)pf;

	unsigned __int64 val;
	QueryPerformanceCounter( (LARGE_INTEGER *)&val );
	return (val) * freq_;
}

double GetTimeMS() {
	return GetTime()*1000.0;
}
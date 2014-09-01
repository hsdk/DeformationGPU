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
#include <DXUT.h>

#ifndef FORCE_DIRECTX_11_0
#if defined(PROFILE)
namespace PerfEvents{
class DXPerfEventScoped
{
public:

	DXPerfEventScoped(LPCWSTR markerName)
	{
		m_perf = NULL;
#ifndef FORCE_DIRECTX_11_0
		HRESULT hr = DXUTGetD3D11DeviceContext()->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), (LPVOID* )&m_perf);
		//if(SUCCEEDED(hr) && m_perf->GetStatus())		// only returns true in vs profiler, not nsight
		if(SUCCEEDED(hr))
		{
			m_perf->BeginEvent(markerName);		
		}
#endif
	}

	~DXPerfEventScoped()
	{
#ifndef FORCE_DIRECTX_11_0
		//if(m_perf->GetStatus())
		{
			m_perf->EndEvent();		
		}
#endif
		SAFE_RELEASE(m_perf);
	}
private:
	ID3DUserDefinedAnnotation* m_perf;

};

class DXPerfEvent
{
public:

	DXPerfEvent(LPCWSTR markerName)
	{
		m_perf = NULL;
#ifndef FORCE_DIRECTX_11_0
		HRESULT hr = DXUTGetD3D11DeviceContext()->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), (LPVOID* )&m_perf);
		//		if(DXUTGetUserDefinedAnnotation()->GetStatus())
		if(SUCCEEDED(hr))
		{
			m_perf->BeginEvent(markerName);			
		}
#endif
	}

	~DXPerfEvent()
	{
		SAFE_RELEASE(m_perf);
	}

	void End()
	{
#ifndef FORCE_DIRECTX_11_0
		//		if(DXUTGetUserDefinedAnnotation()->GetStatus()) // only returns true in vs profiler, not nsight
		{
			m_perf->EndEvent();	
		}
#endif
	}
private:
	ID3DUserDefinedAnnotation* m_perf;
};

}
// PROFILE is defined, so these macros call the D3DPERF functions
#define PERF_EVENT_SCOPED( objectName, nameStr )   PerfEvents::DXPerfEventScoped objectName( nameStr )
#define PERF_EVENT( objectName, nameStr )   PerfEvents::DXPerfEvent objectName( nameStr )

#else
// PROFILE is not defined, so these macros do nothing
#define PERF_EVENT_SCOPED(  objectName, nameStr )   (__noop)
#define PERF_EVENT(  objectName, nameStr )   (__noop)
#endif
#else

#define PERF_EVENT_SCOPED(  objectName, nameStr )   (__noop)
#define PERF_EVENT(  objectName, nameStr )   (__noop)

#endif


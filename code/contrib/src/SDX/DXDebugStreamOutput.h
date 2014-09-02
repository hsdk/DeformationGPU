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

#include <SDX/DXShaderManager.h>

struct streamStruct {
	DirectX::XMFLOAT4A position;
	DirectX::XMFLOAT3A normal;
};

class DXDebugStreamOutput
{
public:
	DXDebugStreamOutput(void);
	~DXDebugStreamOutput(void);
	
	HRESULT Create(ID3D11Device1* pd3dDevice);
	void Destroy();

	void BindBuffer(ID3D11DeviceContext1* pd3dDeviceContext, UINT targetBuffer);
	void UnbindBuffer(ID3D11DeviceContext1* pd3dDeviceContext);

	void BufferDataToCPU(ID3D11DeviceContext1* pd3dDeviceContext, UINT targetBuffer);

	bool CompareTwoBuffers(UINT buffer0, UINT buffer1);


	void AutomaticBind(ID3D11DeviceContext1* pd3dDeviceContext);
	void AutomaticUnbind(ID3D11DeviceContext1* pd3dDeviceContext);
	void AutomaticCompare();

	void ResetBufferCounter();
	void SetEnable(bool e);
	bool IsEnabled();
private:
	bool g_IsEnabled;
	UINT g_CurrBufferCounter;

	bool CheckBitwise(streamStruct* data0, streamStruct* data1, UINT len);
	static bool nearlyEqual(streamStruct &a, streamStruct &b, float eps);
	ID3D11Buffer**	g_OutputBuffer;
	ID3D11Buffer**	g_OutputBufferStaging;
	ID3D11RenderTargetView **g_OutputBufferRTV;
	Shader<ID3D11GeometryShader> * g_pGeometryShader;

	UINT g_NumElementsGPU;

	UINT g_NumBuffers;
	UINT g_NumCPUBufferElements;
	streamStruct **g_CPUBuffers;

};

extern DXDebugStreamOutput g_Dso;

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

#include <DXUT.h>
#include "DXDebugStreamOutput.h"
#include "DXShaderManager.h"

#include <iostream>
#include <fstream>

extern void WaitForGPU();

DXDebugStreamOutput g_Dso;
using namespace DirectX;

DXDebugStreamOutput::DXDebugStreamOutput(void)
{
	g_pGeometryShader = NULL;
	g_OutputBuffer = NULL;
	g_OutputBufferStaging = NULL;
	g_CPUBuffers = NULL;
	g_IsEnabled = false;
}

DXDebugStreamOutput::~DXDebugStreamOutput(void)
{
}

HRESULT DXDebugStreamOutput::Create( ID3D11Device1* pd3dDevice )
{
	HRESULT hr = S_OK;
	

	g_NumElementsGPU = 1000;

	g_NumBuffers = 10;
	g_NumCPUBufferElements = 1000;
	g_CPUBuffers = new streamStruct*[g_NumBuffers];
	g_OutputBuffer = new ID3D11Buffer*[g_NumBuffers];
	g_OutputBufferStaging = new ID3D11Buffer*[g_NumBuffers];
	g_OutputBufferRTV = new ID3D11RenderTargetView*[g_NumBuffers];

	D3D11_SO_DECLARATION_ENTRY soDecArray[2];

	D3D11_SO_DECLARATION_ENTRY soDecPos;
	ZeroMemory(&soDecPos, sizeof(D3D11_SO_DECLARATION_ENTRY));
	soDecPos.Stream = 0;
	soDecPos.SemanticName = "SV_POSITION";
	soDecPos.ComponentCount = 4;
	soDecPos.StartComponent = 0;


	D3D11_SO_DECLARATION_ENTRY soDecNormal;
	ZeroMemory(&soDecNormal, sizeof(D3D11_SO_DECLARATION_ENTRY));
	soDecNormal.Stream = 0;
	soDecNormal.SemanticName = "NORMAL";
	soDecNormal.ComponentCount = 3;
	soDecNormal.StartComponent = 0;

	soDecArray[0] = soDecPos;
	soDecArray[1] = soDecNormal;
		
	ID3DBlob* pBlob = NULL;
	//V_RETURN( CompileShaderFromFile( L"DebugStreamOutput.hlsl", "GSMain",  "gs_5_0", &pBlobGS, 0, D3DCOMPILE_IEEE_STRICTNESS ) );
	//V_RETURN( pd3dDevice->CreateGeometryShaderWithStreamOutput( pBlobGS->GetBufferPointer(), pBlobGS->GetBufferSize(),soDecArray, 2, 0, 0, 0, 0, &g_pGeometryShader) );
	g_pGeometryShader = g_shaderManager.AddGeometryShaderSO(L"shader/DebugStreamOutput.hlsl", "GSMain",  "gs_5_0", &pBlob, 0, D3DCOMPILE_IEEE_STRICTNESS, soDecArray, 2, 0, 0, 0, 0);
	
	SAFE_RELEASE(pBlob);

	for (UINT i = 0; i < g_NumBuffers; i++) 
	{

		// creating the vertex buffer and it's views
		D3D11_BUFFER_DESC bufferDesc;
		ZeroMemory( &bufferDesc, sizeof(D3D11_BUFFER_DESC) );
		bufferDesc.Usage				= D3D11_USAGE_DEFAULT;
		bufferDesc.ByteWidth			= g_NumElementsGPU * sizeof(streamStruct);
		bufferDesc.BindFlags			= D3D11_BIND_STREAM_OUTPUT | D3D11_BIND_RENDER_TARGET;
		bufferDesc.StructureByteStride	= sizeof(streamStruct);

		V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, NULL, &g_OutputBuffer[i] ));

		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, NULL, &g_OutputBufferStaging[i] ));


		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
		ZeroMemory(&rtvDesc, sizeof(rtvDesc));
		rtvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_BUFFER;
		rtvDesc.Buffer.ElementOffset  = 0;
		rtvDesc.Buffer.ElementWidth = sizeof(streamStruct);
		V_RETURN( pd3dDevice->CreateRenderTargetView(g_OutputBuffer[i], &rtvDesc, &g_OutputBufferRTV[i]))


		g_CPUBuffers[i] = new streamStruct[g_NumCPUBufferElements];
	}

	ResetBufferCounter();

	return hr;
}

void DXDebugStreamOutput::Destroy()
{
	//SAFE_RELEASE( g_pGeometryShader );
	for (UINT i = 0; i < g_NumBuffers; i++) {
		if (g_OutputBuffer)			SAFE_RELEASE( g_OutputBuffer[i] );
		if (g_OutputBufferStaging)	SAFE_RELEASE( g_OutputBufferStaging[i] );
		if (g_OutputBufferRTV)		SAFE_RELEASE( g_OutputBufferRTV[i]);
		if (g_CPUBuffers)			SAFE_DELETE(g_CPUBuffers[i]);
	}
	SAFE_DELETE_ARRAY(g_CPUBuffers);
	SAFE_DELETE_ARRAY(g_OutputBuffer);
	SAFE_DELETE_ARRAY(g_OutputBufferStaging);
	SAFE_DELETE_ARRAY(g_OutputBufferRTV);
}


void DXDebugStreamOutput::BindBuffer( ID3D11DeviceContext1* pd3dDeviceContext, UINT targetBuffer )
{
	assert(targetBuffer < g_NumBuffers);
	UINT offset[1] = {0};
	float clear[7] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
	pd3dDeviceContext->ClearRenderTargetView(g_OutputBufferRTV[targetBuffer], clear);
	pd3dDeviceContext->SOSetTargets(1, &g_OutputBuffer[targetBuffer], offset);
	pd3dDeviceContext->GSSetShader(g_pGeometryShader->Get(), 0, 0);

}

void DXDebugStreamOutput::UnbindBuffer( ID3D11DeviceContext1* pd3dDeviceContext )
{
	ID3D11Buffer *nullBuffer[1] = {NULL};
	UINT offset[1] = {0};
	pd3dDeviceContext->SOSetTargets(1, nullBuffer, offset);
	pd3dDeviceContext->GSSetShader(NULL, 0, 0);
}


void DXDebugStreamOutput::BufferDataToCPU( ID3D11DeviceContext1* pd3dDeviceContext, UINT targetBuffer )
{
	assert(targetBuffer < g_NumBuffers);
	
	D3D11_QUERY_DESC queryDesc;
	queryDesc.Query = D3D11_QUERY_EVENT;
	queryDesc.MiscFlags = 0;
	ID3D11Query* dxquery = NULL;

	HRESULT hr = S_OK;
	V(DXUTGetD3D11Device()->CreateQuery(&queryDesc, &dxquery));
		
	pd3dDeviceContext->End(dxquery); while (pd3dDeviceContext->GetData(dxquery, NULL, 0, 0) == S_FALSE) { /*Wait*/ }
	
	
	UINT num = g_NumCPUBufferElements;
	
	float* cpuMemory = (float*)(g_CPUBuffers[targetBuffer]);
	ZeroMemory(cpuMemory, sizeof(float)*7*num);

	pd3dDeviceContext->CopyResource(g_OutputBufferStaging[targetBuffer], g_OutputBuffer[targetBuffer]);
	pd3dDeviceContext->End(dxquery); while (pd3dDeviceContext->GetData(dxquery, NULL, 0, 0) == S_FALSE) { /*Wait*/ }

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	V(pd3dDeviceContext->Map(g_OutputBufferStaging[targetBuffer], D3D11CalcSubresource(0,0,0), D3D11_MAP_READ, 0, &mappedResource));	
	memcpy((void*)cpuMemory, (void*)mappedResource.pData, num*sizeof(streamStruct));
	pd3dDeviceContext->Unmap( g_OutputBufferStaging[targetBuffer], 0 );
	
	pd3dDeviceContext->End(dxquery); while (pd3dDeviceContext->GetData(dxquery, NULL, 0, 0) == S_FALSE) { /*Wait*/ }

	SAFE_RELEASE(dxquery);

	//CheckBitwise((streamStruct*)cpuMemory, (streamStruct*)cpuMemory, num);
	//
	//std::fstream of;
	//of.open("streamOutput.txt", ios_base::out);
	//of.setf(ios::fixed,ios::floatfield);
	//of.precision(20);
	//for (UINT i = 0; i < num; i++) {
	//	if (i%3 == 0)	of << "\n";
	//	if (i%6 == 0)	of << "Patch " << i/6 << "\n";
	//	float4 position = ((streamStruct*)cpuMemory)[i].position;
	//	float3 normal = ((streamStruct*)cpuMemory)[i].normal;
	//	of << position.x << " " << position.y << " " << position.z << " " << position.w << "\n";
	//	//of << normal.x << " " << normal.y << " " << normal.z << "\n";
	//}
	//of.close();

}



bool DXDebugStreamOutput::CheckBitwise( streamStruct* data0, streamStruct* data1, UINT len )
{
	float eps = 0.00001f;
	bool isIdentical = true;
	for (UINT i = 0; i < len; i++) {
		for (UINT k = i; k < len; k++) {
			if (nearlyEqual(data0[i], data1[k], eps) && !nearlyEqual(data0[i], data1[k], 0.0f)) {
 				isIdentical = false;
				//problem
				
				std::fstream of;
				of.open("streamOutput.txt", std::ios_base::out);
				of.setf(std::ios::fixed, std::ios::floatfield);
				of.precision(30);



				XMFLOAT4A position = data0[i].position;
				XMFLOAT3A normal = data0[i].normal;
				of << position.x << " " << position.y << " " << position.z << " " << position.w << "\n";
				of << normal.x << " " << normal.y << " " << normal.z << "\n\n";
				position = data1[k].position;
				normal = data1[k].normal;
				of << position.x << " " << position.y << " " << position.z << " " << position.w << "\n";
				of << normal.x << " " << normal.y << " " << normal.z << "\n";

				of.close();

				//assert(false);
			} 
		}
	}
	return isIdentical;
}

bool DXDebugStreamOutput::nearlyEqual( streamStruct &a, streamStruct &b, float eps )
{
	if (abs(a.position.x - b.position.x) > eps)	return false;
	if (abs(a.position.y - b.position.y) > eps)	return false;
	if (abs(a.position.z - b.position.z) > eps)	return false;
	if (abs(a.position.w - b.position.w) > eps)	return false;

	if (eps == 0.0f) {
		if (abs(a.normal.x - b.normal.x) > eps)	return false;
		if (abs(a.normal.y - b.normal.y) > eps)	return false;
		if (abs(a.normal.z - b.normal.z) > eps)	return false;
	}

	
	return true;

}

bool DXDebugStreamOutput::CompareTwoBuffers( UINT buffer0, UINT buffer1 )
{
	assert(buffer0 < g_NumBuffers && buffer1 < g_NumBuffers);
	return CheckBitwise(g_CPUBuffers[buffer0], g_CPUBuffers[buffer1], g_NumCPUBufferElements);
}

void DXDebugStreamOutput::ResetBufferCounter()
{
	g_CurrBufferCounter = 0;
}

void DXDebugStreamOutput::SetEnable( bool e )
{
	g_IsEnabled = e;
}



void DXDebugStreamOutput::AutomaticBind( ID3D11DeviceContext1* pd3dDeviceContext )
{
	if (!g_IsEnabled)	return;
	BindBuffer(pd3dDeviceContext, g_CurrBufferCounter);

}

void DXDebugStreamOutput::AutomaticUnbind( ID3D11DeviceContext1* pd3dDeviceContext )
{
	if (!g_IsEnabled)	return;
	UnbindBuffer(pd3dDeviceContext);
	BufferDataToCPU(pd3dDeviceContext, g_CurrBufferCounter);
	g_CurrBufferCounter++;
}

void DXDebugStreamOutput::AutomaticCompare()
{
	if (!g_IsEnabled)	return;

	for (UINT i = 0; i < g_CurrBufferCounter; i++) {
		for (UINT k = i; k < g_CurrBufferCounter; k++) {
			CompareTwoBuffers(i,k);
		}
	}
	ResetBufferCounter();
}

bool DXDebugStreamOutput::IsEnabled()
{
	return g_IsEnabled;
}



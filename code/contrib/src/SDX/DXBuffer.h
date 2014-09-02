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
#include <d3d11.h>

__forceinline void* CreateAndCopyToDebugBuf(ID3D11DeviceContext1* pd3dImmediateContext, ID3D11Buffer* pBuffer)
{
	ID3D11Buffer* debugbuf = NULL;

	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	pBuffer->GetDesc(&desc);
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.MiscFlags = 0;

	DXUTGetD3D11Device()->CreateBuffer(&desc, NULL, &debugbuf);

	pd3dImmediateContext->CopyResource(debugbuf, pBuffer);


	UINT *cpuMemory = new UINT[desc.ByteWidth / sizeof(UINT)];
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	pd3dImmediateContext->Map(debugbuf, D3D11CalcSubresource(0, 0, 0), D3D11_MAP_READ, 0, &mappedResource);
	memcpy((void*)cpuMemory, (void*)mappedResource.pData, desc.ByteWidth);
	pd3dImmediateContext->Unmap(debugbuf, 0);

	SAFE_RELEASE(debugbuf);

	return cpuMemory;
}

#ifdef DIRECTX_11_1_ENABLED
HRESULT WINAPI DXCreateBuffer(ID3D11Device1* pd3dDevice, UINT bindFlags, UINT byteWidth, UINT cpuAccessFlag, D3D11_USAGE usage, ID3D11Buffer* &pBuffer, void* pData = NULL, UINT misc_flags = 0, UINT structuredByteStride = 0);
#else
HRESULT WINAPI DXCreateBuffer(ID3D11Device* pd3dDevice, UINT bindFlags, UINT byteWidth, UINT cpuAccessFlag, D3D11_USAGE usage, ID3D11Buffer* &pBuffer, void* pData = NULL, UINT misc_flags = 0, UINT structuredByteStride = 0);
#endif

namespace DirectX{
	class DXBufferSRVUAV
	{
	public:
		DXBufferSRVUAV() :BUF(nullptr), SRV(nullptr), UAV(nullptr){}
		~DXBufferSRVUAV(){ Destroy(); }

		void Destroy()
		{
			if (BUF) BUF->Release(); BUF = nullptr;
			if (SRV) SRV->Release(); SRV = nullptr;
			if (UAV) UAV->Release(); UAV = nullptr;			
		}

		ID3D11Buffer* BUF;
		ID3D11ShaderResourceView* SRV;
		ID3D11UnorderedAccessView* UAV;
	};

	class DXBufferSRV
	{
	public:
		DXBufferSRV() :BUF(nullptr), SRV(nullptr){}
		~DXBufferSRV(){ Destroy(); }

		void Destroy()
		{
			if (BUF) BUF->Release(); BUF = nullptr;
			if (SRV) SRV->Release(); SRV = nullptr;			
		}

		ID3D11Buffer* BUF;
		ID3D11ShaderResourceView* SRV;		
	};

	class DXBufferUAV
	{
	public:
		DXBufferUAV() :BUF(nullptr), UAV(nullptr){}
		~DXBufferUAV(){ Destroy(); }

		void Destroy()
		{
			if (BUF) BUF->Release(); BUF = nullptr;			
			if (UAV) UAV->Release(); UAV = nullptr;
		}

		ID3D11Buffer* BUF;		
		ID3D11UnorderedAccessView* UAV;
	};
}
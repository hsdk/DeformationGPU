#include "DXBuffer.h"

#ifdef DIRECTX_11_1_ENABLED
HRESULT WINAPI DXCreateBuffer(ID3D11Device1* pd3dDevice, UINT bindFlags, UINT byteWidth, UINT cpuAccessFlag, D3D11_USAGE usage, ID3D11Buffer* &pBuffer, void* pData, UINT misc_flags, UINT stride)
#else
HRESULT WINAPI DXCreateBuffer(ID3D11Device* pd3dDevice, UINT bindFlags, UINT byteWidth, UINT cpuAccessFlag, D3D11_USAGE usage, ID3D11Buffer* &pBuffer, void* pData, UINT misc_flags, UINT stride)
#endif
{
	HRESULT hr = S_OK;

	D3D11_BUFFER_DESC desc = { 0 };
	desc.BindFlags = bindFlags;		// Type of resource
	desc.ByteWidth = byteWidth;			// size in bytes
	desc.CPUAccessFlags = cpuAccessFlag;    // D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ
	desc.Usage = usage;			// D3D11_USAGE_DEFAULT, D3D11_USAGE_IMMUTABLE, D3D11_USAGE_DYNAMIC, D3D11_USAGE_STAGING		
	desc.MiscFlags = misc_flags;
	desc.StructureByteStride = stride;


	if (pData)
	{
		D3D11_SUBRESOURCE_DATA vbData = { 0 };
		vbData.pSysMem = pData;
		V_RETURN(pd3dDevice->CreateBuffer(&desc, &vbData, &pBuffer));
	}
	else
		V_RETURN(pd3dDevice->CreateBuffer(&desc, NULL, &pBuffer));

	assert(SUCCEEDED(hr));
	return S_OK;
}
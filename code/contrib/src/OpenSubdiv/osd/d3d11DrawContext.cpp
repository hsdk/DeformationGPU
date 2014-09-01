//
//   Copyright 2013 Pixar
//
//   Licensed under the Apache License, Version 2.0 (the "Apache License")
//   with the following modification; you may not use this file except in
//   compliance with the Apache License and the following modification to it:
//   Section 6. Trademarks. is deleted and replaced with:
//
//   6. Trademarks. This License does not grant permission to use the trade
//      names, trademarks, service marks, or product names of the Licensor
//      and its affiliates, except as required to comply with Section 4(c) of
//      the License and to reproduce the content of the NOTICE file.
//
//   You may obtain a copy of the Apache License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the Apache License with the above modification is
//   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
//   KIND, either express or implied. See the Apache License for the specific
//   language governing permissions and limitations under the Apache License.
//

#include "../far/dispatcher.h"
#include "../osd/d3d11DrawContext.h"
#include <D3D11.h>
#include <DXBuffer.h>

#include <string.h>

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

OsdD3D11DrawContext::OsdD3D11DrawContext() :
    patchIndexBuffer(NULL),
	patchIndexBufferBUF(NULL),		// H
	patchIndexBufferSRV(NULL),		// H
    ptexCoordinateBuffer(NULL),
    ptexCoordinateBufferSRV(NULL),
    fvarDataBuffer(NULL),
    fvarDataBufferSRV(NULL),
    vertexBufferSRV(NULL),
    vertexValenceBuffer(NULL),
    vertexValenceBufferSRV(NULL),
    quadOffsetBuffer(NULL),
    quadOffsetBufferSRV(NULL)
{
}

OsdD3D11DrawContext::~OsdD3D11DrawContext()
{
    if (patchIndexBuffer) 		 patchIndexBuffer->Release();	
	if (patchIndexBufferBUF)	 patchIndexBufferBUF->Release(); // H
	if (patchIndexBufferSRV)	 patchIndexBufferSRV->Release(); // H

    if (ptexCoordinateBuffer) 	 ptexCoordinateBuffer->Release();
    if (ptexCoordinateBufferSRV) ptexCoordinateBufferSRV->Release();
    
	if (fvarDataBuffer)			 fvarDataBuffer->Release();
    if (fvarDataBufferSRV)		 fvarDataBufferSRV->Release();
    
	if (vertexBufferSRV)		 vertexBufferSRV->Release();
    
	if (vertexValenceBuffer)	 vertexValenceBuffer->Release();
    if (vertexValenceBufferSRV)	 vertexValenceBufferSRV->Release();
    
	if (quadOffsetBuffer)		 quadOffsetBuffer->Release();
    if (quadOffsetBufferSRV)	 quadOffsetBufferSRV->Release();
};

OsdD3D11DrawContext *
OsdD3D11DrawContext::Create(FarPatchTables const *patchTables,
                            ID3D11DeviceContext1 *pd3d11DeviceContext,
                            int numVertexElements,
                            bool requireFVarData)
{
    OsdD3D11DrawContext * result = new OsdD3D11DrawContext();
    if (result->create(patchTables, pd3d11DeviceContext, numVertexElements, requireFVarData))
        return result;

    delete result;
    return NULL;
}

bool
OsdD3D11DrawContext::create(FarPatchTables const *patchTables,
                            ID3D11DeviceContext1 *pd3d11DeviceContext,
                            int numVertexElements,
                            bool requireFVarData)
{
    // adaptive patches
    _isAdaptive = true;

    //ID3D11Device *pd3d11Device = NULL;
    //pd3d11DeviceContext->GetDevice(&pd3d11Device);
	ID3D11Device1 *pd3d11Device = DXUTGetD3D11Device();
    assert(pd3d11Device);

    ConvertPatchArrays(patchTables->GetPatchArrayVector(), patchArrays, patchTables->GetMaxValence(), numVertexElements);

    FarPatchTables::PTable const & ptables = patchTables->GetPatchTable();
    FarPatchTables::PatchParamTable const & ptexCoordTables = patchTables->GetPatchParamTable();
    int totalPatchIndices = (int)ptables.size();
    int totalPatches = (int)ptexCoordTables.size();

    // Allocate and fill index buffer.
    D3D11_BUFFER_DESC bd;
    //bd.ByteWidth = totalPatchIndices * sizeof(int);
    //bd.Usage = D3D11_USAGE_DYNAMIC;
    //bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    //bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    //bd.MiscFlags = 0;
    //bd.StructureByteStride = sizeof(int);
    //HRESULT hr = pd3d11Device->CreateBuffer(&bd, NULL, &patchIndexBuffer);
    //if (FAILED(hr)) {
    //    return false;
    //}
	//
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    //hr = pd3d11DeviceContext->Map(patchIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    //if (FAILED(hr)) {
    //    return false;
    //}
    //unsigned int * indexBuffer = (unsigned int *) mappedResource.pData;
    //memcpy(indexBuffer, &ptables[0], totalPatchIndices * sizeof(unsigned int));
	//
    //pd3d11DeviceContext->Unmap(patchIndexBuffer, 0);
	HRESULT hr;
	hr=DXUTCreateBuffer(pd3d11Device, D3D11_BIND_INDEX_BUFFER, totalPatchIndices*sizeof(int), 0, D3D11_USAGE_IMMUTABLE, patchIndexBuffer, (void*)&ptables[0], 0, sizeof(int));
	if (FAILED(hr)) 
		return false;	
	
	{
		// patch index buffer SRV for culling, deformation,... SRV bindable, do not use bind index buffer and bind srvs as it is much slower in combination
		hr=DXUTCreateBuffer(pd3d11Device, D3D11_BIND_SHADER_RESOURCE, totalPatchIndices*sizeof(int), 0, D3D11_USAGE_IMMUTABLE, patchIndexBufferBUF, (void*)&ptables[0], 0, sizeof(int));
		if (FAILED(hr)) {
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
		ZeroMemory(&srvd, sizeof(srvd));
		srvd.Format = DXGI_FORMAT_R32_UINT;
		srvd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvd.Buffer.FirstElement = 0;
		srvd.Buffer.NumElements = totalPatchIndices;
		hr = pd3d11Device->CreateShaderResourceView(patchIndexBufferBUF, &srvd, &patchIndexBufferSRV);
		if (FAILED(hr)) {
			return false;
		}
	}

    // create patch param buffer
    bd.ByteWidth = totalPatches * sizeof(FarPatchParam);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = 0;
    bd.StructureByteStride = sizeof(unsigned int);
    hr = pd3d11Device->CreateBuffer(&bd, NULL, &ptexCoordinateBuffer);
    if (FAILED(hr)) {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
    ZeroMemory(&srvd, sizeof(srvd));
    srvd.Format = DXGI_FORMAT_R32G32_UINT;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvd.Buffer.FirstElement = 0;
    srvd.Buffer.NumElements = totalPatches;
    hr = pd3d11Device->CreateShaderResourceView(ptexCoordinateBuffer, &srvd, &ptexCoordinateBufferSRV);
    if (FAILED(hr)) {
        return false;
    }

    hr = pd3d11DeviceContext->Map(ptexCoordinateBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) {
        return false;
    }
    unsigned int * ptexBuffer = (unsigned int *) mappedResource.pData;
    memcpy(ptexBuffer, &ptexCoordTables[0], totalPatches * sizeof(FarPatchParam));
    pd3d11DeviceContext->Unmap(ptexCoordinateBuffer, 0);

    // create vertex valence buffer and vertex texture
    FarPatchTables::VertexValenceTable const &
        valenceTable = patchTables->GetVertexValenceTable();

    if (not valenceTable.empty()) {
        D3D11_BUFFER_DESC bd;
        bd.ByteWidth = UINT(valenceTable.size() * sizeof(unsigned int));
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        bd.CPUAccessFlags = 0;
        bd.MiscFlags = 0;
        bd.StructureByteStride = sizeof(unsigned int);
        D3D11_SUBRESOURCE_DATA initData;
        initData.pSysMem = &valenceTable[0];
        HRESULT hr = pd3d11Device->CreateBuffer(&bd, &initData, &vertexValenceBuffer);
        if (FAILED(hr)) {
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
        ZeroMemory(&srvd, sizeof(srvd));
        srvd.Format = DXGI_FORMAT_R32_SINT;
        srvd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvd.Buffer.FirstElement = 0;
        srvd.Buffer.NumElements = UINT(valenceTable.size());
        hr = pd3d11Device->CreateShaderResourceView(vertexValenceBuffer, &srvd, &vertexValenceBufferSRV);
        if (FAILED(hr)) {
            return false;
        }
    }

    FarPatchTables::QuadOffsetTable const &
        quadOffsetTable = patchTables->GetQuadOffsetTable();

    if (not quadOffsetTable.empty()) {
        D3D11_BUFFER_DESC bd;
        bd.ByteWidth = UINT(quadOffsetTable.size() * sizeof(unsigned int));
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        bd.CPUAccessFlags = 0;
        bd.MiscFlags = 0;
        bd.StructureByteStride = sizeof(unsigned int);
        D3D11_SUBRESOURCE_DATA initData;
        initData.pSysMem = &quadOffsetTable[0];
        HRESULT hr = pd3d11Device->CreateBuffer(&bd, &initData, &quadOffsetBuffer);
        if (FAILED(hr)) {
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
        ZeroMemory(&srvd, sizeof(srvd));
        srvd.Format = DXGI_FORMAT_R32_SINT;
        srvd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvd.Buffer.FirstElement = 0;
        srvd.Buffer.NumElements = UINT(quadOffsetTable.size());
        hr = pd3d11Device->CreateShaderResourceView(quadOffsetBuffer, &srvd, &quadOffsetBufferSRV);
        if (FAILED(hr)) {
            return false;
        }
    }

	
	// create fvar data buffer if requested
	//FarPatchTables::FVarDataTable const &fvarTables = patchTables->GetFVarDataTable();
	auto const &fvarTables = patchTables->GetFVarData();
	//if(requireFVarData && !fvarTables.empty())
	if(requireFVarData && !fvarTables.GetAllData().empty())
	{
		//DXUTCreateBuffer(pd3d11Device, D3D11_BIND_SHADER_RESOURCE, fvarTables.size() * sizeof(float),0, D3D11_USAGE_IMMUTABLE,fvarDataBuffer,(void*)&fvarTables[0],0,sizeof(float));
		DXUTCreateBuffer(pd3d11Device, D3D11_BIND_SHADER_RESOURCE, fvarTables.GetAllData().size() * sizeof(float),0, D3D11_USAGE_IMMUTABLE,fvarDataBuffer,(void*)&fvarTables.GetAllData()[0],0,sizeof(float));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
		ZeroMemory(&srvd, sizeof(srvd));
		srvd.Format = DXGI_FORMAT_R32_FLOAT;
		srvd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvd.Buffer.FirstElement = 0;
		//srvd.Buffer.NumElements = UINT(fvarTables.size());
		srvd.Buffer.NumElements = UINT(fvarTables.GetAllData().size());
		hr = pd3d11Device->CreateShaderResourceView(fvarDataBuffer, &srvd, &fvarDataBufferSRV);
		if (FAILED(hr)) {
			return false;
		}
	}
    return true;
}


void
OsdD3D11DrawContext::updateVertexTexture(ID3D11Buffer *vbo,
                                         ID3D11DeviceContext1 *pd3d11DeviceContext,
                                         int numVertices,
                                         int numVertexElements)
{
    //ID3D11Device *pd3d11Device = NULL;
    //pd3d11DeviceContext->GetDevice(&pd3d11Device);
	ID3D11Device1 *pd3d11Device = DXUTGetD3D11Device();
    assert(pd3d11Device);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
    ZeroMemory(&srvd, sizeof(srvd));
    srvd.Format = DXGI_FORMAT_R32_FLOAT;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvd.Buffer.FirstElement = 0;
    srvd.Buffer.NumElements = numVertices * numVertexElements;
    HRESULT hr = pd3d11Device->CreateShaderResourceView(vbo, &srvd, &vertexBufferSRV);
    if (FAILED(hr)) {
        return;
    }
}

} // end namespace OPENSUBDIV_VERSION
} // end namespace OpenSubdiv

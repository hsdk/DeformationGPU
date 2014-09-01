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

#include "../osd/debug.h"
#include "../osd/error.h"
#include "../osd/d3d11KernelBundle.h"

//#include <App.h>
#define INITGUID        // for IID_ID3D11ShaderReflection
#include <D3D11.h>
#include <D3D11shader.h>
#include <D3Dcompiler.h>

#include <cassert>
#include <cstring>
#include <sstream>
#include <string>


namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

static const char *shaderSource = "";
//#include "../osd/hlslComputeKernel.gen.h"
//;

//#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

OsdD3D11ComputeKernelBundle::OsdD3D11ComputeKernelBundle(
        ID3D11DeviceContext1 * deviceContext) :
    _deviceContext(deviceContext),
    _computeShader(0),
    _classLinkage(0),
    _kernelCB(0),
    _kernelComputeFace(0),
	_kernelComputeQuadFace(0),
	_kernelComputeTriQuadFace(0),
    _kernelComputeEdge(0),
    _kernelComputeBilinearEdge(0),
    _kernelComputeVertex(0),
    _kernelComputeVertexA(0),
    _kernelComputeCatmarkVertexB(0),
    _kernelComputeLoopVertexB(0),
    _kernelEditAdd(0) {

    // XXX: too rough!
    _workGroupSize = 64;
}

OsdD3D11ComputeKernelBundle::~OsdD3D11ComputeKernelBundle() {
    SAFE_RELEASE(_kernelCB);
}

bool
OsdD3D11ComputeKernelBundle::Compile(	OsdVertexBufferDescriptor const &vertexDesc,
										OsdVertexBufferDescriptor const &varyingDesc) {

	_numVertexElements = vertexDesc.length;
	_vertexStride = vertexDesc.stride;
	_numVaryingElements = varyingDesc.length;
	_varyingStride = varyingDesc.stride;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	std::ostringstream ss;
	ss << _numVertexElements;
	std::string numVertexElementsStr(ss.str());
	ss.str("");
	ss << _numVaryingElements;
	std::string numVaryingElementsStr(ss.str());
	ss.str("");
	ss << _vertexStride;
	std::string vertexStrideStr(ss.str());
	ss.str("");
	ss << _varyingStride;
	std::string varyingStrideStr(ss.str());
	ss.str("");
	ss << _workGroupSize;
	std::string workGroupSizeStr(ss.str());

	D3D_SHADER_MACRO shaderDefines[] = {
		"NUM_VERTEX_ELEMENTS", numVertexElementsStr.c_str(),
		"VERTEX_STRIDE", vertexStrideStr.c_str(),
		"NUM_VARYING_ELEMENTS", numVaryingElementsStr.c_str(),
		"VARYING_STRIDE", varyingStrideStr.c_str(),
		"WORK_GROUP_SIZE", workGroupSizeStr.c_str(),
		0, 0
	};

    ID3DBlob* pComputeShaderBuffer = NULL;

	_kernelComputeFace				=  g_shaderManager.AddComputeShader( L"shader/OSDComputeKernel.hlsl", "CatmarkComputeFace",		"cs_5_0", &pComputeShaderBuffer, shaderDefines);
	_kernelComputeQuadFace			=  g_shaderManager.AddComputeShader( L"shader/OSDComputeKernel.hlsl", "CatmarkComputeQuadFace", "cs_5_0", &pComputeShaderBuffer, shaderDefines);
	_kernelComputeTriQuadFace		 = g_shaderManager.AddComputeShader( L"shader/OSDComputeKernel.hlsl", "CatmarkComputeTriQuadFace", "cs_5_0", &pComputeShaderBuffer, shaderDefines);
	_kernelComputeEdge				=  g_shaderManager.AddComputeShader( L"shader/OSDComputeKernel.hlsl", "CatmarkComputeEdge",		"cs_5_0", &pComputeShaderBuffer, shaderDefines);
	_kernelComputeBilinearEdge		=  g_shaderManager.AddComputeShader( L"shader/OSDComputeKernel.hlsl", "BilinearComputeEdge",	"cs_5_0", &pComputeShaderBuffer, shaderDefines);
	_kernelComputeVertex			=  g_shaderManager.AddComputeShader( L"shader/OSDComputeKernel.hlsl", "BilinearComputeVertex",	"cs_5_0", &pComputeShaderBuffer, shaderDefines);
	_kernelComputeVertexA			=  g_shaderManager.AddComputeShader( L"shader/OSDComputeKernel.hlsl", "CatmarkComputeVertexA",	"cs_5_0", &pComputeShaderBuffer, shaderDefines);
	_kernelComputeCatmarkVertexB	=  g_shaderManager.AddComputeShader( L"shader/OSDComputeKernel.hlsl", "CatmarkComputeVertexB",	"cs_5_0", &pComputeShaderBuffer, shaderDefines);
	_kernelComputeLoopVertexB		=  g_shaderManager.AddComputeShader( L"shader/OSDComputeKernel.hlsl", "LoopComputeVertexB",		"cs_5_0", &pComputeShaderBuffer, shaderDefines);
	_kernelEditAdd					=  g_shaderManager.AddComputeShader( L"shader/OSDComputeKernel.hlsl", "EditAdd",				"cs_5_0", &pComputeShaderBuffer, shaderDefines);

	SAFE_RELEASE(pComputeShaderBuffer);
    return true;
}

// must match constant buffer declaration in hlslComputeKernel.hlsl
__declspec(align(16))
struct OsdD3D11ComputeKernelBundle::KernelCB {
    int vertexOffset;   // vertex index offset for the batch
    int tableOffset;    // offset of subdivision table
    int indexStart;     // start index relative to tableOffset
    int indexEnd;       // end index relative to tableOffset
	int vertexBaseOffset;  // base vbo offset of the vertex buffer
	int varyingBaseOffset; // base vbo offset of the varying buffer
    BOOL vertexPass;    // 4-byte bool

// vertex edit kernel
    int editPrimVarOffset;
    int editPrimVarWidth;
};

void
OsdD3D11ComputeKernelBundle::dispatchCompute(
        ID3D11ComputeShader * kernel, KernelCB const & args) {

    int count = args.indexEnd - args.indexStart;
    if (count <= 0) return;

    if (! _kernelCB) {
		ID3D11Device1 *device = DXUTGetD3D11Device();
        //ID3D11Device *device = NULL;
        //_deviceContext->GetDevice(&device);
        assert(device);

        D3D11_BUFFER_DESC cbDesc;
        ZeroMemory(&cbDesc, sizeof(cbDesc));
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbDesc.MiscFlags = 0;
        cbDesc.ByteWidth = sizeof(KernelCB);
        device->CreateBuffer(&cbDesc, NULL, &_kernelCB);
    }
    assert(_kernelCB);

    D3D11_MAPPED_SUBRESOURCE MappedResource;
    _deviceContext->Map(_kernelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
    CopyMemory(MappedResource.pData, &args, sizeof(KernelCB));
    _deviceContext->Unmap(_kernelCB, 0);
    //_deviceContext->CSSetConstantBuffers(CB_LOC::RUNTABLES, 1, &_kernelCB); // b6
	_deviceContext->CSSetConstantBuffers(6, 1, &_kernelCB); // b6
    _deviceContext->CSSetShader(kernel, NULL, 0);
    _deviceContext->Dispatch(count/_workGroupSize + 1, 1, 1);
}

void
OsdD3D11ComputeKernelBundle::ApplyBilinearFaceVerticesKernel(
int vertexOffset, int tableOffset, int start, int end,
int vertexBaseOffset, int varyingBaseOffset) {

	KernelCB args;
	ZeroMemory(&args, sizeof(args));
	args.vertexOffset = vertexOffset;
	args.tableOffset = tableOffset;
	args.indexStart = start;
	args.indexEnd = end;
	args.vertexBaseOffset = vertexBaseOffset;
	args.varyingBaseOffset = varyingBaseOffset;
	dispatchCompute(_kernelComputeFace->Get(), args);
}

void
OsdD3D11ComputeKernelBundle::ApplyBilinearEdgeVerticesKernel(
int vertexOffset, int tableOffset, int start, int end,
int vertexBaseOffset, int varyingBaseOffset) {

	KernelCB args;
	ZeroMemory(&args, sizeof(args));
	args.vertexOffset = vertexOffset;
	args.tableOffset = tableOffset;
	args.indexStart = start;
	args.indexEnd = end;
	args.vertexBaseOffset = vertexBaseOffset;
	args.varyingBaseOffset = varyingBaseOffset;
	dispatchCompute(_kernelComputeBilinearEdge->Get(), args);
}

void
OsdD3D11ComputeKernelBundle::ApplyBilinearVertexVerticesKernel(
int vertexOffset, int tableOffset, int start, int end,
int vertexBaseOffset, int varyingBaseOffset) {

	KernelCB args;
	ZeroMemory(&args, sizeof(args));
	args.vertexOffset = vertexOffset;
	args.tableOffset = tableOffset;
	args.indexStart = start;
	args.indexEnd = end;
	args.vertexBaseOffset = vertexBaseOffset;
	args.varyingBaseOffset = varyingBaseOffset;
	dispatchCompute(_kernelComputeVertex->Get(), args);
}


void
OsdD3D11ComputeKernelBundle::ApplyCatmarkFaceVerticesKernel(
int vertexOffset, int tableOffset, int start, int end,
int vertexBaseOffset, int varyingBaseOffset) {

	KernelCB args;
	ZeroMemory(&args, sizeof(args));
	args.vertexOffset = vertexOffset;
	args.tableOffset = tableOffset;
	args.indexStart = start;
	args.indexEnd = end;
	args.vertexBaseOffset = vertexBaseOffset;
	args.varyingBaseOffset = varyingBaseOffset;
	dispatchCompute(_kernelComputeFace->Get(), args);
}

void
OsdD3D11ComputeKernelBundle::ApplyCatmarkQuadFaceVerticesKernel(
int vertexOffset, int tableOffset, int start, int end,
int vertexBaseOffset, int varyingBaseOffset) {

	KernelCB args;
	ZeroMemory(&args, sizeof(args));
	args.vertexOffset = vertexOffset;
	args.tableOffset = tableOffset;
	args.indexStart = start;
	args.indexEnd = end;
	args.vertexBaseOffset = vertexBaseOffset;
	args.varyingBaseOffset = varyingBaseOffset;
	dispatchCompute(_kernelComputeQuadFace->Get(), args);
}

void
OsdD3D11ComputeKernelBundle::ApplyCatmarkTriQuadFaceVerticesKernel(
int vertexOffset, int tableOffset, int start, int end,
int vertexBaseOffset, int varyingBaseOffset) {

	KernelCB args;
	ZeroMemory(&args, sizeof(args));
	args.vertexOffset = vertexOffset;
	args.tableOffset = tableOffset;
	args.indexStart = start;
	args.indexEnd = end;
	args.vertexBaseOffset = vertexBaseOffset;
	args.varyingBaseOffset = varyingBaseOffset;
	dispatchCompute(_kernelComputeTriQuadFace->Get(), args);
}

void
OsdD3D11ComputeKernelBundle::ApplyCatmarkEdgeVerticesKernel(
int vertexOffset, int tableOffset, int start, int end,
int vertexBaseOffset, int varyingBaseOffset) {

	KernelCB args;
	ZeroMemory(&args, sizeof(args));
	args.vertexOffset = vertexOffset;
	args.tableOffset = tableOffset;
	args.indexStart = start;
	args.indexEnd = end;
	args.vertexBaseOffset = vertexBaseOffset;
	args.varyingBaseOffset = varyingBaseOffset;
	dispatchCompute(_kernelComputeEdge->Get(), args);
}

void
OsdD3D11ComputeKernelBundle::ApplyCatmarkVertexVerticesKernelB(
int vertexOffset, int tableOffset, int start, int end,
int vertexBaseOffset, int varyingBaseOffset) {

	KernelCB args;
	ZeroMemory(&args, sizeof(args));
	args.vertexOffset = vertexOffset;
	args.tableOffset = tableOffset;
	args.indexStart = start;
	args.indexEnd = end;
	args.vertexBaseOffset = vertexBaseOffset;
	args.varyingBaseOffset = varyingBaseOffset;
	dispatchCompute(_kernelComputeCatmarkVertexB->Get(), args);
}

void
OsdD3D11ComputeKernelBundle::ApplyCatmarkVertexVerticesKernelA(
int vertexOffset, int tableOffset, int start, int end, bool pass,
int vertexBaseOffset, int varyingBaseOffset) {

	KernelCB args;
	ZeroMemory(&args, sizeof(args));
	args.vertexOffset = vertexOffset;
	args.tableOffset = tableOffset;
	args.indexStart = start;
	args.indexEnd = end;
	args.vertexPass = pass ? 1 : 0;
	args.vertexBaseOffset = vertexBaseOffset;
	args.varyingBaseOffset = varyingBaseOffset;
	dispatchCompute(_kernelComputeVertexA->Get(), args);
}

void
OsdD3D11ComputeKernelBundle::ApplyLoopEdgeVerticesKernel(
int vertexOffset, int tableOffset, int start, int end,
int vertexBaseOffset, int varyingBaseOffset) {

	KernelCB args;
	ZeroMemory(&args, sizeof(args));
	args.vertexOffset = vertexOffset;
	args.tableOffset = tableOffset;
	args.indexStart = start;
	args.indexEnd = end;
	args.vertexBaseOffset = vertexBaseOffset;
	args.varyingBaseOffset = varyingBaseOffset;
	dispatchCompute(_kernelComputeEdge->Get(), args);
}

void
OsdD3D11ComputeKernelBundle::ApplyLoopVertexVerticesKernelB(
int vertexOffset, int tableOffset, int start, int end,
int vertexBaseOffset, int varyingBaseOffset) {

	KernelCB args;
	ZeroMemory(&args, sizeof(args));
	args.vertexOffset = vertexOffset;
	args.tableOffset = tableOffset;
	args.indexStart = start;
	args.indexEnd = end;
	args.vertexBaseOffset = vertexBaseOffset;
	args.varyingBaseOffset = varyingBaseOffset;
	dispatchCompute(_kernelComputeLoopVertexB->Get(), args);
}

void
OsdD3D11ComputeKernelBundle::ApplyLoopVertexVerticesKernelA(
int vertexOffset, int tableOffset, int start, int end, bool pass,
int vertexBaseOffset, int varyingBaseOffset) {

	KernelCB args;
	ZeroMemory(&args, sizeof(args));
	args.vertexOffset = vertexOffset;
	args.tableOffset = tableOffset;
	args.indexStart = start;
	args.indexEnd = end;
	args.vertexPass = pass ? 1 : 0;
	args.vertexBaseOffset = vertexBaseOffset;
	args.varyingBaseOffset = varyingBaseOffset;
	dispatchCompute(_kernelComputeVertexA->Get(), args);
}

void
OsdD3D11ComputeKernelBundle::ApplyEditAdd(
int primvarOffset, int primvarWidth,
int vertexOffset, int tableOffset, int start, int end,
int vertexBaseOffset, int varyingBaseOffset) {

	KernelCB args;
	ZeroMemory(&args, sizeof(args));
	args.vertexOffset = vertexOffset;
	args.tableOffset = tableOffset;
	args.indexStart = start;
	args.indexEnd = end;
	args.editPrimVarOffset = primvarOffset;
	args.editPrimVarWidth = primvarWidth;
	args.vertexBaseOffset = vertexBaseOffset;
	args.varyingBaseOffset = varyingBaseOffset;
	dispatchCompute(_kernelEditAdd->Get(), args);
}

}  // end namespace OPENSUBDIV_VERSION
}  // end namespace OpenSubdiv

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
#include "../osd/d3d11DrawRegistry.h"
#include "../osd/error.h"

#include <D3D11.h>
#include <D3Dcompiler.h>

#include <sstream>

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

OsdD3D11DrawConfig::~OsdD3D11DrawConfig()
{
    // cleanup is done by shadermanager 
    //if (vertexShader) vertexShader->Release();
    //if (hullShader) hullShader->Release();
    //if (domainShader) domainShader->Release();
    //if (geometryShader) geometryShader->Release();
    //if (pixelShader) pixelShader->Release();
	delete srcConfig;
}

//static const char *commonShaderSource =
//#include "hlslPatchCommon.gen.h"
//;
//static const char *ptexShaderSource =
//#include "hlslPtexCommon.gen.h"
//;
//static const char *bsplineShaderSource =
//#include "hlslPatchBSpline.gen.h"
//;
//static const char *gregoryShaderSource =
//#include "hlslPatchGregory.gen.h"
//;
//static const char *transitionShaderSource =
//#include "hlslPatchTransition.gen.h"
//;

OsdD3D11DrawRegistryBase::~OsdD3D11DrawRegistryBase() {}

OsdD3D11DrawSourceConfig *
OsdD3D11DrawRegistryBase::_CreateDrawSourceConfig(
    OsdDrawContext::PatchDescriptor const & desc, ID3D11Device1 * pd3dDevice)
{
    OsdD3D11DrawSourceConfig * sconfig = _NewDrawSourceConfig();

    //sconfig->commonShader.source = commonShaderSource;

    //if (IsPtexEnabled()) {
    //    sconfig->commonShader.source += ptexShaderSource;
    //}


    //{
    //    std::ostringstream ss;
    //    ss << (int)desc.GetMaxValence();
    //    sconfig->commonShader.AddDefine("OSD_MAX_VALENCE", ss.str());
    //    ss.str("");
    //    ss << (int)desc.GetNumElements();
    //    sconfig->commonShader.AddDefine("OSD_NUM_ELEMENTS", ss.str());
    //}

    if (desc.GetPattern() == FarPatchTables::NON_TRANSITION) {
	//if (true){
        switch (desc.GetType()) {
        case FarPatchTables::REGULAR:
            sconfig->vertexShader.sourceFile = L"shader/OSDPatchBSpline.hlsl";
            sconfig->vertexShader.target = "vs_5_0";
            sconfig->vertexShader.entry = "vs_main_patches";
			sconfig->vertexConfig.type = FarPatchTables::REGULAR;				// H: hash val for shader loader

            sconfig->hullShader.sourceFile = L"shader/OSDPatchBSpline.hlsl";
            sconfig->hullShader.target = "hs_5_0";
            sconfig->hullShader.entry = "hs_main_patches";
			sconfig->hullConfig.type = FarPatchTables::REGULAR;					// H: hash val for shader loader

            sconfig->domainShader.sourceFile = L"shader/OSDPatchBSpline.hlsl";
            sconfig->domainShader.target = "ds_5_0";
            sconfig->domainShader.entry = "ds_main_patches";
			sconfig->domainConfig.type = FarPatchTables::REGULAR;				// H: hash val for shader loader
            break;
        case FarPatchTables::BOUNDARY:
            sconfig->vertexShader.sourceFile = L"shader/OSDPatchBSpline.hlsl";
            sconfig->vertexShader.target = "vs_5_0";
            sconfig->vertexShader.entry = "vs_main_patches";
			sconfig->vertexConfig.type = FarPatchTables::BOUNDARY;				// H: hash val for shader loader

            sconfig->hullShader.sourceFile = L"shader/OSDPatchBSpline.hlsl";
            sconfig->hullShader.target = "hs_5_0";
            sconfig->hullShader.entry = "hs_main_patches";
            sconfig->hullShader.AddDefine("OSD_PATCH_BOUNDARY");
			sconfig->hullConfig.type = FarPatchTables::BOUNDARY;				// H: hash val for shader loader

            sconfig->domainShader.sourceFile = L"shader/OSDPatchBSpline.hlsl";
            sconfig->domainShader.target = "ds_5_0";
            sconfig->domainShader.entry = "ds_main_patches";
			sconfig->domainConfig.type = FarPatchTables::BOUNDARY;				// H: hash val for shader loader
            break;
        case FarPatchTables::CORNER:
            sconfig->vertexShader.sourceFile = L"shader/OSDPatchBSpline.hlsl";
            sconfig->vertexShader.target = "vs_5_0";
            sconfig->vertexShader.entry = "vs_main_patches";
			sconfig->vertexConfig.type = FarPatchTables::CORNER;				// H: hash val for shader loader

            sconfig->hullShader.sourceFile = L"shader/OSDPatchBSpline.hlsl";
            sconfig->hullShader.target = "hs_5_0";
            sconfig->hullShader.entry = "hs_main_patches";
            sconfig->hullShader.AddDefine("OSD_PATCH_CORNER");
			sconfig->hullConfig.type = FarPatchTables::CORNER;					// H: hash val for shader loader

            sconfig->domainShader.sourceFile = L"shader/OSDPatchBSpline.hlsl";
            sconfig->domainShader.target = "ds_5_0";
            sconfig->domainShader.entry = "ds_main_patches";
			sconfig->domainConfig.type = FarPatchTables::CORNER;				// H: hash val for shader loader
            break;
        case FarPatchTables::GREGORY:
            sconfig->vertexShader.sourceFile = L"shader/OSDPatchGregory.hlsl";
            sconfig->vertexShader.target = "vs_5_0";
            sconfig->vertexShader.entry = "vs_main_patches";
			sconfig->vertexShader.AddDefine("WITH_GREGORY");					// H: for loading correct include
			sconfig->vertexConfig.type = FarPatchTables::GREGORY;				// H: hash val for shader loader

            sconfig->hullShader.sourceFile = L"shader/OSDPatchGregory.hlsl";
            sconfig->hullShader.target = "hs_5_0";
            sconfig->hullShader.entry = "hs_main_patches";
			sconfig->hullShader.AddDefine("WITH_GREGORY");						// H: for loading correct include
			sconfig->hullConfig.type = FarPatchTables::GREGORY;					// H: hash val for shader loader

            sconfig->domainShader.sourceFile = L"shader/OSDPatchGregory.hlsl";
            sconfig->domainShader.target = "ds_5_0";
            sconfig->domainShader.entry = "ds_main_patches";
			sconfig->domainShader.AddDefine("WITH_GREGORY");					// H: for loading correct include
			sconfig->domainConfig.type = FarPatchTables::GREGORY;				// H: hash val for shader loader
            break;
        case FarPatchTables::GREGORY_BOUNDARY:
            sconfig->vertexShader.sourceFile = L"shader/OSDPatchGregory.hlsl";
            sconfig->vertexShader.target = "vs_5_0";
            sconfig->vertexShader.entry = "vs_main_patches";
            sconfig->vertexShader.AddDefine("OSD_PATCH_GREGORY_BOUNDARY");
			sconfig->vertexShader.AddDefine("WITH_GREGORY");					// H: for loading correct include
			sconfig->vertexConfig.type = FarPatchTables::GREGORY_BOUNDARY;		// H: hash val for shader loader

            sconfig->hullShader.sourceFile = L"shader/OSDPatchGregory.hlsl";
            sconfig->hullShader.target = "hs_5_0";
            sconfig->hullShader.entry = "hs_main_patches";
            sconfig->hullShader.AddDefine("OSD_PATCH_GREGORY_BOUNDARY");
			sconfig->hullShader.AddDefine("WITH_GREGORY");						// H: for loading correct include
			sconfig->hullConfig.type = FarPatchTables::GREGORY_BOUNDARY;		// H: hash val for shader loader

            sconfig->domainShader.sourceFile = L"shader/OSDPatchGregory.hlsl";
            sconfig->domainShader.target = "ds_5_0";
            sconfig->domainShader.entry = "ds_main_patches";
            sconfig->domainShader.AddDefine("OSD_PATCH_GREGORY_BOUNDARY");
			sconfig->domainShader.AddDefine("WITH_GREGORY");					// H: for loading correct include
			sconfig->domainConfig.type = FarPatchTables::GREGORY_BOUNDARY;		// H: hash val for shader loader
            break;
        default: // POINTS, LINES, QUADS, TRIANGLES
            // do nothing
            break;
        }
    } else { // pattern != NON_TRANSITION - > TRANSITION PATCHES
        sconfig->vertexShader.sourceFile = L"shader/OSDPatchBSpline.hlsl";
        sconfig->vertexShader.target = "vs_5_0";
        sconfig->vertexShader.entry = "vs_main_patches";

        sconfig->hullShader.sourceFile = L"shader/OSDPatchBSpline.hlsl";            
        sconfig->hullShader.target = "hs_5_0";
		sconfig->hullShader.entry = "hs_main_patches";
        sconfig->hullShader.AddDefine("OSD_PATCH_TRANSITION");

        sconfig->domainShader.sourceFile = L"shader/OSDPatchBSpline.hlsl";            
        sconfig->domainShader.target = "ds_5_0";
        sconfig->domainShader.entry = "ds_main_patches";
        sconfig->domainShader.AddDefine("OSD_PATCH_TRANSITION");

        int pattern = desc.GetPattern() - 1;
        int rotation = desc.GetRotation();
        int subpatch = desc.GetSubPatch();

        std::ostringstream ss;
        ss << "OSD_TRANSITION_PATTERN" << pattern << subpatch;
        sconfig->hullShader.AddDefine(ss.str());
        sconfig->domainShader.AddDefine(ss.str());

        ss.str("");
        ss << rotation;
        sconfig->hullShader.AddDefine("OSD_TRANSITION_ROTATE", ss.str());
        sconfig->domainShader.AddDefine("OSD_TRANSITION_ROTATE", ss.str());
		sconfig->hullConfig.pattern = pattern;						// H: hash val for shader loader
		sconfig->hullConfig.subpatch = subpatch;					// H: hash val for shader loader
		sconfig->hullConfig.rotation = rotation;					// H: hash val for shader loader
		
		sconfig->domainConfig.pattern = pattern;					// H: hash val for shader loader
		sconfig->domainConfig.subpatch = subpatch;					// H: hash val for shader loader
		sconfig->domainConfig.rotation = rotation;					// H: hash val for shader loader
		
        if (desc.GetType() == FarPatchTables::BOUNDARY) {
            sconfig->hullShader.AddDefine("OSD_PATCH_BOUNDARY");
        } else if (desc.GetType() == FarPatchTables::CORNER) {
            sconfig->hullShader.AddDefine("OSD_PATCH_CORNER");
        }
    }

	// HENRY add common defines to all shaders
	//sconfig->commonShader.sourceFile = commonShaderSource;
	{
		std::ostringstream ss;
		ss << (int)desc.GetMaxValence();
		sconfig->commonShader.AddDefine	 ("OSD_MAX_VALENCE", ss.str());
		sconfig->vertexShader.AddDefine	 ("OSD_MAX_VALENCE", ss.str());
		sconfig->hullShader.AddDefine	 ("OSD_MAX_VALENCE", ss.str());		
		sconfig->domainShader.AddDefine	 ("OSD_MAX_VALENCE", ss.str());
		sconfig->geometryShader.AddDefine("OSD_MAX_VALENCE", ss.str());
		sconfig->pixelShader.AddDefine	 ("OSD_MAX_VALENCE", ss.str());	// not realy needed for pixel shader
		
		ss.str("");
		ss << (int)desc.GetNumElements();
		sconfig->commonShader.AddDefine	 ("OSD_NUM_ELEMENTS", ss.str());
		sconfig->vertexShader.AddDefine	 ("OSD_NUM_ELEMENTS", ss.str());
		sconfig->hullShader.AddDefine	 ("OSD_NUM_ELEMENTS", ss.str());
		sconfig->domainShader.AddDefine	 ("OSD_NUM_ELEMENTS", ss.str());
		sconfig->geometryShader.AddDefine("OSD_NUM_ELEMENTS", ss.str());
		sconfig->pixelShader .AddDefine	 ("OSD_NUM_ELEMENTS", ss.str());	// not realy needed for pixel shader

		// store values for precompiled shader naming
		sconfig->commonConfig.max_valence = (int)desc.GetMaxValence();
		sconfig->commonConfig.num_vertex_elements = (int) desc.GetNumElements();
	}
    return sconfig;
}

static ID3DBlob *
_CompileShader(
        OsdDrawShaderSource const & common,
        OsdDrawShaderSource const & source)
{
	std::cerr << "d3d11DrawRegistry::_CompileShader should never be called, aborting "<< std::endl;
	exit(1);
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* pBlob = NULL;
    //ID3DBlob* pBlobError = NULL;

    //std::vector<D3D_SHADER_MACRO> shaderDefines;
    //for (int i=0; i<(int)common.defines.size(); ++i) {
    //    const D3D_SHADER_MACRO def = {
    //        common.defines[i].first.c_str(),
    //        common.defines[i].second.c_str(),
    //    };
    //    shaderDefines.push_back(def);
    //}
    //for (int i=0; i<(int)source.defines.size(); ++i) {
    //    const D3D_SHADER_MACRO def = {
    //        source.defines[i].first.c_str(),
    //        source.defines[i].second.c_str(),
    //    };
    //    shaderDefines.push_back(def);
    //}
    //const D3D_SHADER_MACRO def = { 0, 0 };
    //shaderDefines.push_back(def);

    //std::string shaderSource = common.source + source.source;

    //HRESULT hr = D3DCompile(shaderSource.c_str(), shaderSource.size(),
    //                        NULL, &shaderDefines[0], NULL,
    //                        source.entry.c_str(), source.target.c_str(),
    //                        dwShaderFlags, 0, &pBlob, &pBlobError);
    //if (FAILED(hr)) {
    //    if ( pBlobError != NULL ) {
    //        OsdError(OSD_D3D11_COMPILE_ERROR,
    //                 "Error compiling HLSL shader: %s\n",
    //                 (CHAR*)pBlobError->GetBufferPointer());
    //        pBlobError->Release();
    //        return NULL;
    //    }
    //}

    return pBlob;
}

//#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

std::vector<D3D_SHADER_MACRO> createDXDefines(const OsdDrawShaderSource::DefineVector& defvec)
{
	std::vector<D3D_SHADER_MACRO> outDefs;
	for (int i=0; i<(int)defvec.size(); ++i) {
		const D3D_SHADER_MACRO def = {
			defvec[i].first.c_str(),
			defvec[i].second.c_str(),
		};
		outDefs.push_back(def);
	}

	const D3D_SHADER_MACRO zerodef = { 0, 0 };
	outDefs.push_back(zerodef);

	return outDefs;
}

void createDXDefines(const std::vector< const OsdDrawShaderSource::DefineVector* >& defvec, std::vector<D3D_SHADER_MACRO>& outDefs)
{	

	for (int i=0; i<(int)defvec.size(); ++i) {
		for(const auto& defI : *defvec[i])
		{
			const D3D_SHADER_MACRO def = {
				defI.first.c_str(),
				defI.second.c_str(),
			};
			outDefs.push_back(def);		
		}
	}

	const D3D_SHADER_MACRO zerodef = { 0, 0 };
	outDefs.push_back(zerodef);

}

OsdD3D11DrawConfig*
OsdD3D11DrawRegistryBase::_CreateDrawConfig(
        DescType const & desc,
        SourceConfigType const * sconfig,
        ID3D11Device1 * pd3dDevice,
        ID3D11InputLayout ** ppInputLayout,
        D3D11_INPUT_ELEMENT_DESC const * pInputElementDescs,
        int numInputElements)
{
    assert(sconfig);

	bool debug_shader_build = false;
	
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
	//dwShaderFlags |= D3DCOMPILE_IEEE_STRICTNESS;

	#ifdef _DEBUG
	dwShaderFlags |= D3DCOMPILE_DEBUG;
	debug_shader_build = true;
	#else
	//dwShaderFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
	#endif

    ID3DBlob * pBlob = NULL;
	
    Shader<ID3D11VertexShader> *vertexShader = NULL;	
    if (! sconfig->vertexShader.sourceFile.empty()) 
	{
		std::vector<int> config_hashes;
		config_hashes.push_back(sconfig->value);
		config_hashes.push_back(sconfig->commonConfig.max_valence);
		config_hashes.push_back(sconfig->commonConfig.num_vertex_elements);
		config_hashes.push_back(sconfig->commonConfig.common_render_hash);
		config_hashes.push_back(sconfig->vertexConfig.value);
		if(debug_shader_build)	config_hashes.push_back(1337);

		std::vector<const OsdDrawShaderSource::DefineVector *> fullDefs;
		fullDefs.push_back(&sconfig->commonShader.defines);
		fullDefs.push_back(&sconfig->vertexShader.defines);
		
		std::vector<D3D_SHADER_MACRO> defs;
		createDXDefines(fullDefs,defs);


		vertexShader = g_shaderManager.AddVertexShader(sconfig->vertexShader.sourceFile.c_str(), sconfig->vertexShader.entry.c_str(), sconfig->vertexShader.target.c_str(),
														&pBlob, &defs[0], dwShaderFlags, config_hashes);
		fullDefs.clear();
        assert(vertexShader->Get());

        if (ppInputLayout && !*ppInputLayout) {
            pd3dDevice->CreateInputLayout(pInputElementDescs, numInputElements,
                                          pBlob->GetBufferPointer(),
                                          pBlob->GetBufferSize(), ppInputLayout);
            assert(ppInputLayout);			
        }


        SAFE_RELEASE(pBlob);
    }

    Shader<ID3D11HullShader> *hullShader = NULL;
    if (! sconfig->hullShader.sourceFile.empty()) 
	{
		std::vector<int> config_hashes;
		config_hashes.push_back(sconfig->value);
		config_hashes.push_back(sconfig->commonConfig.max_valence);
		config_hashes.push_back(sconfig->commonConfig.num_vertex_elements);
		config_hashes.push_back(sconfig->commonConfig.common_render_hash);
		config_hashes.push_back(sconfig->hullConfig.value);
		if(debug_shader_build)	config_hashes.push_back(1337);

		std::vector<const OsdDrawShaderSource::DefineVector *> fullDefs;
		fullDefs.push_back(&sconfig->commonShader.defines);
		fullDefs.push_back(&sconfig->hullShader.defines);

		std::vector<D3D_SHADER_MACRO> defs;
		createDXDefines(fullDefs,defs);

		hullShader = g_shaderManager.AddHullShader(sconfig->hullShader.sourceFile.c_str(), sconfig->hullShader.entry.c_str(), sconfig->hullShader.target.c_str(),
									 &pBlob, &defs[0], dwShaderFlags, config_hashes);

        assert(hullShader->Get());
        SAFE_RELEASE(pBlob);

    }

    Shader<ID3D11DomainShader> *domainShader = NULL;
    if (! sconfig->domainShader.sourceFile.empty()) 
	{
		std::vector<int> config_hashes;
		config_hashes.push_back(sconfig->value);
		config_hashes.push_back(sconfig->commonConfig.max_valence);
		config_hashes.push_back(sconfig->commonConfig.num_vertex_elements);
		config_hashes.push_back(sconfig->commonConfig.common_render_hash);
		config_hashes.push_back(sconfig->domainConfig.value);
		if(debug_shader_build)	config_hashes.push_back(1337);

		std::vector<const OsdDrawShaderSource::DefineVector *> fullDefs;
		fullDefs.push_back(&sconfig->commonShader.defines);
		fullDefs.push_back(&sconfig->domainShader.defines);

		std::vector<D3D_SHADER_MACRO> defs;
		createDXDefines(fullDefs, defs);

		domainShader = g_shaderManager.AddDomainShader(sconfig->domainShader.sourceFile.c_str(), sconfig->domainShader.entry.c_str(), sconfig->domainShader.target.c_str(),
													   &pBlob, &defs[0], dwShaderFlags, config_hashes);

		assert(domainShader->Get());
        SAFE_RELEASE(pBlob);

    }

    Shader<ID3D11GeometryShader> *geometryShader = NULL;
	if (! sconfig->geometryShader.sourceFile.empty()) 
	{
		std::vector<int> config_hashes;
		config_hashes.push_back(sconfig->value);
		config_hashes.push_back(sconfig->commonConfig.max_valence);
		config_hashes.push_back(sconfig->commonConfig.num_vertex_elements);
		config_hashes.push_back(sconfig->commonConfig.common_render_hash);		
		if(debug_shader_build)	config_hashes.push_back(1337);
	
		//std::cerr << " todo d3dDrawregistry.cpp create geometry shader" << std::endl;
		//exit(1);
		config_hashes.push_back(sconfig->geometryConfig.value);
		
		// TODO tri/quad type to hash
		//sconfig->geometryConfig.value;
	
	
		std::vector<const OsdDrawShaderSource::DefineVector *> fullDefs;
		fullDefs.push_back(&sconfig->commonShader.defines);
		fullDefs.push_back(&sconfig->geometryShader.defines);
		std::vector<D3D_SHADER_MACRO> defs;
		createDXDefines(fullDefs,defs);
	
	
		geometryShader = g_shaderManager.AddGeometryShader(sconfig->geometryShader.sourceFile.c_str(), sconfig->geometryShader.entry.c_str(), sconfig->geometryShader.target.c_str(),
						&pBlob, &defs[0], dwShaderFlags, config_hashes);
	
		assert(geometryShader->Get());
		SAFE_RELEASE(pBlob);
	}

    Shader<ID3D11PixelShader> *pixelShader = NULL;
    if (! sconfig->pixelShader.sourceFile.empty()) 
	{
		std::vector<int> config_hashes;
		config_hashes.push_back(sconfig->value);
		config_hashes.push_back(sconfig->commonConfig.max_valence);
		config_hashes.push_back(sconfig->commonConfig.num_vertex_elements);
		config_hashes.push_back(sconfig->commonConfig.common_render_hash);
		config_hashes.push_back(sconfig->pixelConfig.value);
		if(debug_shader_build)	config_hashes.push_back(1337);


		std::vector<const OsdDrawShaderSource::DefineVector *> fullDefs;
		fullDefs.push_back(&sconfig->commonShader.defines);
		fullDefs.push_back(&sconfig->pixelShader.defines);
		std::vector<D3D_SHADER_MACRO> defs;
		createDXDefines(fullDefs,defs);

		pixelShader = g_shaderManager.AddPixelShader(sconfig->pixelShader.sourceFile.c_str(), sconfig->pixelShader.entry.c_str(), sconfig->pixelShader.target.c_str(),
													 &pBlob, &defs[0], dwShaderFlags, config_hashes);

		assert(pixelShader);
        SAFE_RELEASE(pBlob);
    }

    OsdD3D11DrawConfig * config = _NewDrawConfig();
	config->srcConfig		= const_cast<OsdD3D11DrawSourceConfig*>(sconfig);	// H
    config->vertexShader	= vertexShader;
    config->hullShader		= hullShader;
    config->domainShader	= domainShader;
    config->geometryShader	= geometryShader;
    config->pixelShader		= pixelShader;

    return config;
}

OsdD3D11DrawSourceConfig::~OsdD3D11DrawSourceConfig()
{
	for(auto e: commonShader.defines)
	{
		e.first.clear();
		e.second.clear();
	}

	for(auto e: vertexShader.defines)
	{
		e.first.clear();
		e.second.clear();
	}

	for(auto e: hullShader.defines)
	{
		e.first.clear();
		e.second.clear();
	}

	for(auto e: domainShader.defines)
	{
		e.first.clear();
		e.second.clear();
	}

	for(auto e: geometryShader.defines)
	{
		e.first.clear();
		e.second.clear();
	}

	for(auto e: pixelShader.defines)
	{
		e.first.clear();
		e.second.clear();
	}

	commonShader.defines.clear();
	vertexShader.defines.clear();
	hullShader.defines.clear();
	domainShader.defines.clear();
	geometryShader.defines.clear();
	pixelShader.defines.clear();
}

} // end namespace OPENSUBDIV_VERSION
} // end namespace OpenSubdiv

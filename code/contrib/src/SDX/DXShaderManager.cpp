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

#include <fstream>
#include <string>
#include <atlconv.h>
#include <atlbase.h> 
#include <atlstr.h>
#include <sstream>
#include "DXShaderManager.h"
#include "StringConversion.h"


ShaderManager g_shaderManager;

using namespace Microsoft::WRL;

template <> HRESULT CreateShader<ID3D11VertexShader>(ID3D11VertexShader** shader, ID3DBlob* pBlob)
{	
	HRESULT hr;
	ID3D11VertexShader* vs = NULL;
	V( DXUTGetD3D11Device()->CreateVertexShader( pBlob->GetBufferPointer(),pBlob->GetBufferSize(), NULL, &vs ) );
	
	if(FAILED(hr))	{
		SAFE_RELEASE(vs);		
	}
	else	
	{		
		SAFE_RELEASE(*shader);
		*shader = vs;
	}

	return hr;
};

template <> HRESULT CreateShader<ID3D11DomainShader>(ID3D11DomainShader** shader, ID3DBlob* pBlob)
{	
	HRESULT hr;
	ID3D11DomainShader* ds = NULL;
	V( DXUTGetD3D11Device()->CreateDomainShader( pBlob->GetBufferPointer(),pBlob->GetBufferSize(), NULL, &ds ) );
		
	if(FAILED(hr))
	{
		SAFE_RELEASE(ds);
	}
	else
	{		
		SAFE_RELEASE(*shader);
		*shader = ds;
	}

	return hr;
};

template <> HRESULT CreateShader<ID3D11HullShader>(ID3D11HullShader** shader, ID3DBlob* pBlob)
{	
	HRESULT hr;
	ID3D11HullShader* vs = NULL;
	V( DXUTGetD3D11Device()->CreateHullShader( pBlob->GetBufferPointer(),pBlob->GetBufferSize(), NULL, &vs ) );
		

	if(FAILED(hr))
	{
		SAFE_RELEASE(vs);
	}
	else
	{		
		SAFE_RELEASE(*shader);
		*shader = vs;
	}

	return hr;
};

template <> HRESULT CreateShader<ID3D11GeometryShader>(ID3D11GeometryShader** shader, ID3DBlob* pBlob)
{
	HRESULT hr;
	ID3D11GeometryShader* vs = NULL;
		
	V( DXUTGetD3D11Device()->CreateGeometryShader( pBlob->GetBufferPointer(),pBlob->GetBufferSize(), NULL, &vs ) );
		
	if(FAILED(hr))
	{
		SAFE_RELEASE(vs);
	}
	else
	{		
		SAFE_RELEASE(*shader);
		*shader = vs;
	}

	return hr;
};

template <> HRESULT CreateShader<ID3D11PixelShader>(ID3D11PixelShader** shader, ID3DBlob* pBlob)
{
	HRESULT hr;
	ID3D11PixelShader* ps = NULL;
	V( DXUTGetD3D11Device()->CreatePixelShader( pBlob->GetBufferPointer(),pBlob->GetBufferSize(), NULL, &ps ) );

	if(FAILED(hr))
	{
		SAFE_RELEASE(ps);
	}
	else
	{		
		SAFE_RELEASE(*shader);
		*shader = ps;
	}

	return hr;
};

template <> HRESULT CreateShader<ID3D11ComputeShader>(ID3D11ComputeShader** shader, ID3DBlob* pBlob)
{
	HRESULT hr;
	ID3D11ComputeShader* vs = NULL;
	V( DXUTGetD3D11Device()->CreateComputeShader( pBlob->GetBufferPointer(),pBlob->GetBufferSize(), NULL, &vs ) );
	
	if(FAILED(hr))
	{
		SAFE_RELEASE(vs);
	}
	else
	{	
		SAFE_RELEASE(*shader);
		*shader = vs;
	}

	return hr;
};

template <>
HRESULT CreateShaderStreamOutput<ID3D11VertexShader>(ID3D11VertexShader** shader, ID3DBlob* pBlob, D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT numSOEntries, UINT* pSOStrides, UINT numSOStrides, UINT rasterizerStream )
{
	return S_FALSE;
};


template <>
HRESULT CreateShaderStreamOutput<ID3D11HullShader>(ID3D11HullShader** shader, ID3DBlob* pBlob, D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT numSOEntries, UINT* pSOStrides, UINT numSOStrides, UINT rasterizerStream )
{
	return S_FALSE;
};


template <>
HRESULT CreateShaderStreamOutput<ID3D11DomainShader>(ID3D11DomainShader** shader, ID3DBlob* pBlob, D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT numSOEntries, UINT* pSOStrides, UINT numSOStrides, UINT rasterizerStream )
{
	return S_FALSE;
};


template <>
HRESULT CreateShaderStreamOutput<ID3D11GeometryShader>(ID3D11GeometryShader** shader, ID3DBlob* pBlob, D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT numSOEntries, UINT* pSOStrides, UINT numSOStrides, UINT rasterizerStream )
{
	HRESULT hr;
	ID3D11GeometryShader* vs = NULL;
		
	V( DXUTGetD3D11Device()->CreateGeometryShaderWithStreamOutput( pBlob->GetBufferPointer(), pBlob->GetBufferSize(),pSODeclaration, numSOEntries, pSOStrides, numSOStrides, rasterizerStream, NULL, &vs) );

	if(FAILED(hr))
	{
		SAFE_RELEASE(vs);
	}
	else
	{		
		SAFE_RELEASE(*shader);
		*shader = vs;
	}

	return hr;
	
};


template <>
HRESULT CreateShaderStreamOutput<ID3D11PixelShader>(ID3D11PixelShader** shader, ID3DBlob* pBlob, D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT numSOEntries, UINT* pSOStrides, UINT numSOStrides, UINT rasterizerStream )
{
	return S_FALSE;
};


template <>
HRESULT CreateShaderStreamOutput<ID3D11ComputeShader>(ID3D11ComputeShader** shader, ID3DBlob* pBlob, D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT numSOEntries, UINT* pSOStrides, UINT numSOStrides, UINT rasterizerStream )
{
	return S_FALSE;
};

#include <d3dcompiler.h>
template <typename T>
HRESULT Shader<T>::CompileShader( ID3DBlob** ppBlobOut )
{
	HRESULT hr = S_OK;

	bool srcChanged = false;
	const auto& globIncludes = g_shaderManager.GetIncludes();
	

	if(m_compiledInvalid)
	{
		srcChanged = true;
	}
	else
	{			
		WatchedFile* shaderSrcFileWatch = globIncludes.find(m_fileNameSrcString)->second;		
		srcChanged = CompareFileTime(&shaderSrcFileWatch->getLastWrite().ftLastWriteTime, &m_lastChangeCompiled.ftLastWriteTime) > 0;
	}

	if(srcChanged == false)
	{
		// check include

		if(m_firstLoad)		// first run
		{				
			ID3DBlob* srcBlob;
			D3DReadFileToBlob(m_fileNameSrc,&srcBlob);

			ID3DBlob* codeBlob;
			ID3DBlob* errBlob;

			// preparse
			CShaderInclude includeObj(m_fileNameSrc);

			D3DPreprocess(srcBlob->GetBufferPointer(),srcBlob->GetBufferSize(),NULL, m_pDefines, &includeObj, &codeBlob, &errBlob);
			const std::set<std::string>& includes = includeObj.GetIncludes();
			for(auto inclFileName : includes)
			{
				m_includes.insert(inclFileName);
				WatchedFile* currInclude = g_shaderManager.AddIncludeFile(inclFileName);										
			}

			SAFE_RELEASE(srcBlob);
			SAFE_RELEASE(codeBlob);
			SAFE_RELEASE(errBlob);	
			
		}

		// check if includes are newer 
		for(const auto it : m_includes)
		{					
			WatchedFile* incFile = globIncludes.find(it)->second;			
			if(CompareFileTime(&incFile->getLastWrite().ftLastWriteTime, &m_lastChangeCompiled.ftLastWriteTime ) > 0)
			{					
				srcChanged =  true;	
				std::cout << "src changed" << std::endl;
				break;
			}
		}			
	}

	if(m_firstLoad == false && srcChanged == false)
		return S_OK;

#ifndef _DEBUG
	// for nsight shader debugging: compile shaders on first load, then only modified
	if(!g_shaderManager.UsePrecompiledShaders())
	{
		if(m_firstLoad)	srcChanged = true;
	}
#endif

	if(srcChanged)
	{		
		std::cout << "." ;
		// make sure sub directories exist
		CreateDirectory(ExtractPath(m_fileNameCompiled).c_str(),  NULL);
		CShaderInclude includeObj(m_fileNameSrc);

		DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
    
		if(!g_shaderManager.UsePrecompiledShaders())
        {
           // dwShaderFlags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;
        }

#if defined( DEBUG ) || defined( _DEBUG )
		// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
		// Setting this flag improves the shader debugging experience, but still allows 
		// the shaders to be optimized and to run exactly the way they will run in 
		// the release configuration of this program.
		dwShaderFlags |= D3DCOMPILE_DEBUG;        
		//dwShaderFlags |= D3DCOMPILE_IEEE_STRICTNESS;
#endif
		//dwShaderFlags |= D3DCOMPILE_DEBUG;
		dwShaderFlags |= m_compilerFlags;


		ID3DBlob* pErrorBlob= NULL;		
		std::wcout << L"Compile: " << m_fileNameSrc << " Entrypoint: " << m_entryPoint << std::endl;
		hr = D3DCompileFromFile(m_fileNameSrc, m_pDefines, &includeObj, m_entryPoint, m_shaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
		if (pErrorBlob != NULL && pErrorBlob->GetBufferSize() > 0 && pErrorBlob->GetBufferPointer() != NULL) {
			OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
			std::cerr << (char*)pErrorBlob->GetBufferPointer() << std::endl;
		}
		SAFE_RELEASE( pErrorBlob );

		if( FAILED(hr) )
		{

			OutputDebugStringW(m_fileNameCompiled.c_str());
			OutputDebugStringW(L"\n");
			OutputDebugStringW(L"-------------------------------------------------------------------------------\n");
			std::wcout << L"filename " << m_fileNameCompiled << std::endl;
			std::wcout << L"-------------------------------------------------------------------------------" << std::endl;
			std::wcout << L" shader macros: " << std::endl;
			if(m_pDefines)
			{				
				for(int numDefs = 0; m_pDefines[numDefs].Name != NULL; ++numDefs)
				{
					//std::cout << numDefs << ": " <<  m_pDefines[numDefs].Name << " " <<  m_pDefines[numDefs].Definition << std::endl;				
					std::cout << m_pDefines[numDefs].Name << "=" <<  m_pDefines[numDefs].Definition << std::endl;				
				}
			}
			return hr;
		}
		//SAFE_RELEASE( pErrorBlob );

		// regenerate include file list
		//m_includes.clear();
		const std::set<std::string>& includes = includeObj.GetIncludes();

		for(auto inclFileName : includes)
		{
			m_includes.insert(inclFileName);
			WatchedFile* currInclude = g_shaderManager.AddIncludeFile(inclFileName);										
		}

		// write compiled shader to disk			
		//std::wcout << m_fileNameCompiled << std::endl;
		V(D3DWriteBlobToFile(*ppBlobOut,m_fileNameCompiled.c_str(), true));
		if(hr!= S_OK)
		{
			std::wcout << L"filename " << m_fileNameCompiled << "to long, aborting" << std::endl;
			V_RETURN(hr);
			exit(1);
		}

		std::ofstream compiledFile(m_fileNameCompiled.c_str(), std::ios::out | std::ios::binary);
		compiledFile.write((char*)(*ppBlobOut)->GetBufferPointer(), (*ppBlobOut)->GetBufferSize());
		compiledFile.close();

		// unref old shader if one is bound
		SAFE_RELEASE(m_shader);

		if(m_SONumEntries>0)
		{
			V(CreateShaderStreamOutput<T>(&m_shader, *ppBlobOut, m_SODeclarationArray, m_SONumEntries, m_SOStridesArray, m_SONumStrides, m_rasterizerStream));
		}
		else
			V(CreateShader<T>(&m_shader, *ppBlobOut));

		HANDLE hFindCompiled = FindFirstFile(m_fileNameCompiled.c_str(), &m_lastChangeCompiled);
		if(hFindCompiled != INVALID_HANDLE_VALUE)
			m_compiledInvalid = false;
		
	}
	else // load pre-compiled shader			
	{		
		std::cout << "." ;
		std::ifstream compiledFile(m_fileNameCompiled.c_str(), std::ios::in | std::ios::binary);
		assert(compiledFile.is_open());
		unsigned int size_data = m_lastChangeCompiled.nFileSizeLow;
		
		V(D3DReadFileToBlob(m_fileNameCompiled.c_str(),ppBlobOut));			
		if(hr!= S_OK)
		{
			std::wcout << L"filename " << m_fileNameCompiled << " path to long, aborting" << std::endl;
			V_RETURN(hr);
			exit(1);
		}
		//V(D3DCreateBlob(size_data,ppBlobOut));				
		//compiledFile.read((char*)(*ppBlobOut)->GetBufferPointer(), size_data);
		//compiledFile.close();

		// unref old shader if one is bound
		SAFE_RELEASE(m_shader);	

		if(m_SONumEntries>0)	
		{
			V(CreateShaderStreamOutput<T>(&m_shader, *ppBlobOut, m_SODeclarationArray, m_SONumEntries, m_SOStridesArray, m_SONumStrides, m_rasterizerStream));
		}
		else
		{
				V(CreateShader<T>(&m_shader, *ppBlobOut));
		}
		m_compiledInvalid = false;
	}
	m_firstLoad = false;

	return hr;			

}

ShaderManager::ShaderManager()
{
	m_compiledShaderDirectory = COMPILED_SHADER_DIR;
	CreateDirectory(m_compiledShaderDirectory.c_str(),  NULL);
	m_precompiledShadersEnabled = true;
}

ShaderManager::~ShaderManager()
{
	Destroy();
}
		  
Shader<ID3D11VertexShader>* ShaderManager::AddVertexShader( const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines /*= NULL*/, DWORD compilerFlags, const std::vector<int>& configHashes, ID3D11ClassLinkage* pClassLinkage /*= NULL */ )
{
	std::wstring compiledFileName = CreateCompiledFileName( szFileName, szEntryPoint, szShaderModel, pDefines, compilerFlags, configHashes);
	
	// check if shader is cached
	auto it = m_vertexShaders.find(compiledFileName);
	if(it != m_vertexShaders.end())
	{		
		return it->second;
	}
	
	// otherwise load the shader
	Shader<ID3D11VertexShader>* shader = new Shader<ID3D11VertexShader>(szFileName, szEntryPoint, szShaderModel, VERTEX_SHADER, ppBlobOut, compiledFileName, pDefines, compilerFlags, pClassLinkage);
		
	m_vertexShaders[compiledFileName] = shader;
	
	return shader;

}

Shader<ID3D11HullShader>* ShaderManager::AddHullShader( const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines /*= NULL*/, DWORD compilerFlags, const std::vector<int>& configHashes, ID3D11ClassLinkage* pClassLinkage /*= NULL */ )
{
	std::wstring compiledFileName = CreateCompiledFileName( szFileName, szEntryPoint, szShaderModel, pDefines, compilerFlags, configHashes);

	// check if shader is cached
	auto it = m_hullShaders.find(compiledFileName);
	if(it != m_hullShaders.end())
		return it->second;

	// otherwise load the shader
	Shader<ID3D11HullShader>* shader = new Shader<ID3D11HullShader>(szFileName, szEntryPoint, szShaderModel, HULL_SHADER, ppBlobOut, compiledFileName, pDefines, compilerFlags, pClassLinkage);
	m_hullShaders[compiledFileName] = shader;	
	return shader;
}


Shader<ID3D11DomainShader>* ShaderManager::AddDomainShader( const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines /*= NULL*/, DWORD compilerFlags, const std::vector<int>& configHashes, ID3D11ClassLinkage* pClassLinkage /*= NULL */ )
{
	std::wstring compiledFileName = CreateCompiledFileName( szFileName, szEntryPoint, szShaderModel, pDefines, compilerFlags, configHashes);

	// check if shader is cached
	auto it = m_domainShaders.find(compiledFileName);
	if(it != m_domainShaders.end())
		return it->second;

	// otherwise load the shader
	Shader<ID3D11DomainShader>* shader = new Shader<ID3D11DomainShader>(szFileName, szEntryPoint, szShaderModel, DOMAIN_SHADER, ppBlobOut, compiledFileName,  pDefines, compilerFlags, pClassLinkage);	
	if(shader->Get() != NULL)
	{
		m_domainShaders[compiledFileName] = shader;		
		return shader;
	}
	else return NULL;
}

Shader<ID3D11GeometryShader>* ShaderManager::AddGeometryShader( const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines /*= NULL*/, DWORD compilerFlags, const std::vector<int>& configHashes, ID3D11ClassLinkage* pClassLinkage /*= NULL */ )
{
	std::wstring compiledFileName = CreateCompiledFileName( szFileName, szEntryPoint, szShaderModel, pDefines, compilerFlags, configHashes);

	// check if shader is cached
	auto it = m_geometryShaders.find(compiledFileName);
	if(it != m_geometryShaders.end())
		return it->second;

	// otherwise load the shader
	Shader<ID3D11GeometryShader>* shader = new Shader<ID3D11GeometryShader>(szFileName, szEntryPoint, szShaderModel, GEOMETRY_SHADER, ppBlobOut, compiledFileName,  pDefines, compilerFlags, pClassLinkage);	
	m_geometryShaders[compiledFileName] = shader;
	
	return shader;
}

Shader<ID3D11GeometryShader>* ShaderManager::AddGeometryShaderSO( const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines /*= NULL*/, DWORD compilerFlags /*= 0*/, D3D11_SO_DECLARATION_ENTRY *pSODeclaration /*= NULL*/, UINT numSOEntries /*= 0*/, UINT* pSOStrides /*= NULL*/, UINT numSOStrides /*= 0*/, UINT rasterizerStream /*= D3D11_SO_NO_RASTERIZED_STREAM*/, ID3D11ClassLinkage* pClassLinkage /*= NULL*/ )
{
	std::wstring compiledFileName = CreateCompiledFileName( szFileName, szEntryPoint, szShaderModel, pDefines, compilerFlags);

	// check if shader is cached
	auto it = m_geometryShaders.find(compiledFileName);
	if(it != m_geometryShaders.end())
		return it->second;

	// otherwise load the shader
	Shader<ID3D11GeometryShader>* shader = new Shader<ID3D11GeometryShader>(szFileName, szEntryPoint, szShaderModel, GEOMETRY_SHADER, ppBlobOut, compiledFileName,  pDefines, compilerFlags,
																			pSODeclaration, numSOEntries, pSOStrides, numSOStrides, rasterizerStream,
																			pClassLinkage);	
	m_geometryShaders[compiledFileName] = shader;
	
	return shader;
}

Shader<ID3D11PixelShader>* ShaderManager::AddPixelShader( const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines /*= NULL*/, DWORD compilerFlags, const std::vector<int>& configHashes, ID3D11ClassLinkage* pClassLinkage /*= NULL */ )
{
	std::wstring compiledFileName = CreateCompiledFileName( szFileName, szEntryPoint, szShaderModel, pDefines, compilerFlags, configHashes);

	// check if shader is cached
	auto it = m_pixelShaders.find(compiledFileName);
	if(it != m_pixelShaders.end())
		return it->second;

	// otherwise load the shader
	Shader<ID3D11PixelShader>* shader = new Shader<ID3D11PixelShader>(szFileName, szEntryPoint, szShaderModel, PIXEL_SHADER, ppBlobOut, compiledFileName,  pDefines, compilerFlags, pClassLinkage);	
	m_pixelShaders[compiledFileName] = shader;	
	return shader;
}

Shader<ID3D11ComputeShader>* ShaderManager::AddComputeShader( const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines /*= NULL*/, DWORD compilerFlags, const std::vector<int>& configHashes, ID3D11ClassLinkage* pClassLinkage /*= NULL */ )
{
	std::wstring compiledFileName = CreateCompiledFileName( szFileName, szEntryPoint, szShaderModel, pDefines, compilerFlags, configHashes);

	// check if shader is cached
	auto it = m_computeShaders.find(compiledFileName);
	if(it != m_computeShaders.end())
		return it->second;

	// otherwise load the shader
	Shader<ID3D11ComputeShader>* shader = new Shader<ID3D11ComputeShader>(szFileName, szEntryPoint, szShaderModel, COMPUTE_SHADER, ppBlobOut, compiledFileName, pDefines, compilerFlags, pClassLinkage);	
	m_computeShaders[compiledFileName] = shader;	
	return shader;
}

#include <thread>
HRESULT ShaderManager::CheckAndReload()
{
	HRESULT hr = S_OK;	
	
	for(auto& inc : m_includeFiles )
	{
		inc.second->CheckIsModified();
	}
	
	//for( const auto& shader : m_vertexShaders)	{		hr = shader.second->CheckAndReload();	if(FAILED(hr)) return hr;}
	//for( const auto& shader : m_hullShaders)	{		hr = shader.second->CheckAndReload();	if(FAILED(hr)) return hr;}
	//for( const auto& shader : m_domainShaders)	{		hr = shader.second->CheckAndReload();	if(FAILED(hr)) return hr;}
	//for( const auto& shader : m_geometryShaders){		hr = shader.second->CheckAndReload();	if(FAILED(hr)) return hr;}
	//for( const auto& shader : m_pixelShaders)	{		hr = shader.second->CheckAndReload();	if(FAILED(hr)) return hr;}
	//for( const auto& shader : m_computeShaders)	{		hr = shader.second->CheckAndReload();	if(FAILED(hr)) return hr;}
	
	std::vector<std::thread> threads;
	for (const auto& shader : m_vertexShaders)	{ threads.push_back(std::thread([](Shader<ID3D11VertexShader>* shader)		{ shader->CheckAndReload();	}, std::ref(shader.second))); };
	for (const auto& shader : m_hullShaders)	{ threads.push_back(std::thread([](Shader<ID3D11HullShader>* shader)		{ shader->CheckAndReload();	}, std::ref(shader.second))); };
	for (const auto& shader : m_domainShaders)	{ threads.push_back(std::thread([](Shader<ID3D11DomainShader>* shader)		{ shader->CheckAndReload();	}, std::ref(shader.second))); };
	for (const auto& shader : m_geometryShaders){ threads.push_back(std::thread([](Shader<ID3D11GeometryShader>* shader)	{ shader->CheckAndReload();	}, std::ref(shader.second))); };
	for (const auto& shader : m_pixelShaders)	{ threads.push_back(std::thread([](Shader<ID3D11PixelShader>* shader)		{ shader->CheckAndReload();	}, std::ref(shader.second))); };
	for (const auto& shader : m_computeShaders)	{ threads.push_back(std::thread([](Shader<ID3D11ComputeShader>* shader)		{ shader->CheckAndReload();	}, std::ref(shader.second))); };

	for (auto& thread : threads){
		thread.join();
	}
	
	return hr;
}

void ShaderManager::Destroy()
{	
	for(auto& inc : m_includeFiles)
	{
		SAFE_DELETE(inc.second);
	}
	m_includeFiles.clear();
	for(auto &shader : m_vertexShaders)		delete shader.second;
	for(auto &shader : m_domainShaders)		delete shader.second;
	for(auto &shader : m_hullShaders)		delete shader.second;
	for(auto &shader : m_geometryShaders)	delete shader.second;
	for(auto &shader : m_pixelShaders)		delete shader.second;
	for(auto &shader : m_computeShaders)	delete shader.second;

	m_vertexShaders.clear();
	m_hullShaders.clear();
	m_domainShaders.clear();
	m_geometryShaders.clear();
	m_pixelShaders.clear();
	m_computeShaders.clear();

}

//const char* gSystemDir = "..\\Shader";


std::wstring ShaderManager::CreateCompiledFileName( const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, D3D_SHADER_MACRO* pDefines /*= NULL*/, DWORD compilerFlags /*= 0*/, const std::vector<int>& config_hashes )
{
	std::wstring compiledFileName = m_compiledShaderDirectory;
	compiledFileName.append(szFileName);
	compiledFileName.append(L"-");
	compiledFileName.append( ATL::CA2W(szEntryPoint) );
	
	
	if(!config_hashes.empty())
	{		
		for(auto hash : config_hashes)
		{
			std::wstringstream ws;
			ws << (int)hash;			
			compiledFileName.append(L"_");			
			compiledFileName.append(ws.str().c_str());

		}
	}
	else 
	{
		if(pDefines)
		{
			compiledFileName.append(L"-");
			for(UINT i = 0; pDefines[i].Name != NULL; ++i)
			{
				std::wstring name = ATL::CA2W(pDefines[i].Name);
				if(name[0] == L'\"') name[0] = L'x';
				if(name[name.length() -1 ] == '\"') name[name.length()-1] = L'x';
				
				std::wstring def = ATL::CA2W(pDefines[i].Definition);
				if(def[0] == L'\"') def[0] = L'x';
				if(def[def.length() -1 ] == L'\"') def[def.length()-1] = 'x';			

				compiledFileName.append(name);
				compiledFileName.append(L"_");
				compiledFileName.append(def);
				
				if(pDefines[i+1].Name != NULL)
					compiledFileName.append(L"-");
			}
		}
	}

	if(compilerFlags)
	{
		compiledFileName.append(L"-");
		std::wstringstream ss;
		ss << compilerFlags;
		compiledFileName.append(ss.str());
	}

	compiledFileName.append(L".p");

	//std::wcout << compiledFileName.c_str()<< std::endl;
	return compiledFileName;	
}

WatchedFile* ShaderManager::AddIncludeFile( std::string includeFileName )
{
	const auto& it = m_includeFiles.find(includeFileName);
	if(it == m_includeFiles.end())
	{
		WatchedFile* inc = new WatchedFile(includeFileName);
		m_includeFiles[includeFileName] =  inc;
		return inc;
	}
	else
		return it->second;
}



CShaderInclude::CShaderInclude( const WCHAR* shaderDir, const WCHAR* systemDir )	 
{
	m_ShaderDir = ExtractPath(utf8_encode(std::wstring(shaderDir)));

	m_SystemDir = ExtractPath(utf8_encode(std::wstring(systemDir)));;	
}

HRESULT __stdcall CShaderInclude::Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes) {
	try {
		std::string finalPath;
		switch(IncludeType) {
		case D3D_INCLUDE_LOCAL:
			finalPath = m_ShaderDir + "\\" + pFileName;
			break;
		case D3D_INCLUDE_SYSTEM:
			finalPath = m_SystemDir + "\\" + pFileName;
			break;
		default:
			assert(0);
		}
		m_includeFiles.insert(finalPath);
		//std::cout << finalPath << std::endl;
		std::ifstream includeFile(finalPath.c_str(), std::ios::in|std::ios::binary|std::ios::ate);

		if (includeFile.is_open()) {
			long long fileSize = includeFile.tellg();
			char* buf = new char[fileSize];
			includeFile.seekg (0, std::ios::beg);
			includeFile.read (buf, fileSize);
			includeFile.close();
			*ppData = buf;
			*pBytes = (UINT)fileSize;
		} else {
			return E_FAIL;
		}

		
		return S_OK;
	}
	catch(std::exception) {
		return E_FAIL;
	}
}

HRESULT __stdcall CShaderInclude::Close(LPCVOID pData) {
	char* buf = (char*)pData;
	delete[] buf;
	return S_OK;
}

WatchedFile::WatchedFile( const std::string& includeFileName )
{
	m_fileName =  string2wstring(includeFileName);
	HANDLE hFind = FindFirstFile(m_fileName.c_str(), &m_lastWrite);
}

WatchedFile::~WatchedFile()
{

}

bool WatchedFile::CheckIsModified()
{	
	auto lastWrite = m_lastWrite.ftLastWriteTime;
	
	HANDLE hFind = FindFirstFile(m_fileName.c_str(), &m_lastWrite);
	bool result =  CompareFileTime(&m_lastWrite.ftLastWriteTime,&lastWrite) > 0;	
	FindClose(hFind);
	return result;
}

DXShaderSourceConfig::~DXShaderSourceConfig()
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

	for(auto e: commonShader.defines)
	{
		e.first.clear();
		e.second.clear();
	}

	commonShader.defines.clear();
	computeShader.defines.clear();
	vertexShader.defines.clear();
	hullShader.defines.clear();
	domainShader.defines.clear();
	geometryShader.defines.clear();
	pixelShader.defines.clear();
}

DXShaderSourceConfig * DrawRegistryBase::_CreateDrawSourceConfig( DescType const & desc, ID3D11Device1 * pd3dDevice ) const
{
	DXShaderSourceConfig * sconfig = _NewDrawSourceConfig();

	sconfig->vertexShader.target	= "vs_5_0";
	sconfig->hullShader.target		= "hs_5_0";
	sconfig->domainShader.target	= "ds_5_0";
	sconfig->geometryShader.target	= "gs_5_0";
	sconfig->pixelShader.target		= "ps_5_0";
	sconfig->computeShader.target	= "cs_5_0";

	sconfig->vertexShader.sourceFile.clear();
	sconfig->hullShader.sourceFile.clear();
	sconfig->domainShader.sourceFile.clear();
	sconfig->geometryShader.sourceFile.clear();
	sconfig->pixelShader.sourceFile.clear();
	sconfig->computeShader.sourceFile.clear();

	return sconfig;
}

void createDefineMacro(const std::vector< const ShaderDefineVector* >& defvec, std::vector<D3D_SHADER_MACRO>& outDefs)
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


DXShaderConfig* DrawRegistryBase::_CreateDrawConfig( DescType const & desc, SourceConfigType const * sconfig, ID3D11Device1 * pd3dDevice, ID3D11InputLayout ** ppInputLayout, D3D11_INPUT_ELEMENT_DESC const * pInputElementDescs, int numInputElements ) const
{
	assert(sconfig);

	bool debug_shader_build = false;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
	
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
		if(debug_shader_build)	config_hashes.push_back(1337);

		std::vector<const ShaderDefineVector *> fullDefs;
		fullDefs.push_back(&sconfig->commonShader.defines);
		fullDefs.push_back(&sconfig->vertexShader.defines);

		std::vector<D3D_SHADER_MACRO> defs;
		createDefineMacro(fullDefs,defs);
		vertexShader = g_shaderManager.AddVertexShader(sconfig->vertexShader.sourceFile.c_str(), sconfig->vertexShader.entry.c_str(), sconfig->vertexShader.target.c_str(),	&pBlob, &defs[0], dwShaderFlags, config_hashes);
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
		if(debug_shader_build)	config_hashes.push_back(1337);

		std::vector<const ShaderDefineVector *> fullDefs;
		fullDefs.push_back(&sconfig->commonShader.defines);
		fullDefs.push_back(&sconfig->hullShader.defines);

		std::vector<D3D_SHADER_MACRO> defs;
		createDefineMacro(fullDefs,defs);

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
		if(debug_shader_build)	config_hashes.push_back(1337);

		std::vector<const ShaderDefineVector *> fullDefs;
		fullDefs.push_back(&sconfig->commonShader.defines);
		fullDefs.push_back(&sconfig->domainShader.defines);

		std::vector<D3D_SHADER_MACRO> defs;
		createDefineMacro(fullDefs, defs);

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
		if(debug_shader_build)	config_hashes.push_back(1337);

		std::vector<const ShaderDefineVector *> fullDefs;
		fullDefs.push_back(&sconfig->commonShader.defines);
		fullDefs.push_back(&sconfig->geometryShader.defines);
		std::vector<D3D_SHADER_MACRO> defs;
		createDefineMacro(fullDefs,defs);


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
		if(debug_shader_build)	config_hashes.push_back(1337);


		std::vector<const ShaderDefineVector *> fullDefs;
		fullDefs.push_back(&sconfig->commonShader.defines);
		fullDefs.push_back(&sconfig->pixelShader.defines);
		std::vector<D3D_SHADER_MACRO> defs;
		createDefineMacro(fullDefs,defs);

		pixelShader = g_shaderManager.AddPixelShader(sconfig->pixelShader.sourceFile.c_str(), sconfig->pixelShader.entry.c_str(), sconfig->pixelShader.target.c_str(),
			&pBlob, &defs[0], dwShaderFlags, config_hashes);

		assert(pixelShader);
		SAFE_RELEASE(pBlob);
	}

	Shader<ID3D11ComputeShader> *computeShader = NULL;
	if (! sconfig->computeShader.sourceFile.empty()) 
	{
		std::vector<int> config_hashes;
		config_hashes.push_back(sconfig->value);
		if(debug_shader_build)	config_hashes.push_back(1337);


		std::vector<const ShaderDefineVector *> fullDefs;
		fullDefs.push_back(&sconfig->commonShader.defines);
		fullDefs.push_back(&sconfig->computeShader.defines);
		std::vector<D3D_SHADER_MACRO> defs;
		createDefineMacro(fullDefs,defs);

		computeShader = g_shaderManager.AddComputeShader(sconfig->computeShader.sourceFile.c_str(), sconfig->computeShader.entry.c_str(), sconfig->computeShader.target.c_str(),
			&pBlob, &defs[0], dwShaderFlags, config_hashes);

		assert(computeShader);
		SAFE_RELEASE(pBlob);
	}

	DXShaderConfig * config = _NewDrawConfig();
	config->srcConfig = const_cast<DXShaderSourceConfig*>(sconfig);
	config->vertexShader   = vertexShader;
	config->hullShader	   = hullShader;
	config->domainShader   = domainShader;
	config->geometryShader = geometryShader;
	config->pixelShader    = pixelShader;
	config->computeShader  = computeShader;

	return config;

}

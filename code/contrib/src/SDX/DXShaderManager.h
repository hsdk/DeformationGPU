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
#include <SDKMisc.h>
#include <wrl.h>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>


#define COMPILED_SHADER_DIR L"compiled/"

enum SHADERTYPE {
	VERTEX_SHADER	= 1,
	HULL_SHADER		= 2,
	DOMAIN_SHADER	= 4,
	GEOMETRY_SHADER = 8,
	PIXEL_SHADER	= 16,
	COMPUTE_SHADER  = 32,
};



static ULARGE_INTEGER GetFileChangeTime(WCHAR* name) {
	HANDLE hFile = CreateFile(
		name,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
		);

	ULARGE_INTEGER create, access, write;
	GetFileTime(
		hFile,
		LPFILETIME(&create),
		LPFILETIME(&access),
		LPFILETIME(&write)
		);

	CloseHandle(hFile);
	return write;
}

static bool GetAndSetFileChange(WCHAR* name, ULARGE_INTEGER& lastWrite) {
	ULARGE_INTEGER write = GetFileChangeTime(name);
	if ( write.QuadPart > lastWrite.QuadPart ) {
		lastWrite = write;
		return true;
	}
	lastWrite = write;
	return false;
}

static bool GetFileChange(WCHAR* name, ULARGE_INTEGER lastWrite) {
	return GetFileChangeTime(name).QuadPart > lastWrite.QuadPart;
}

// custom include handle
class CShaderInclude : public ID3DInclude {
public:
	CShaderInclude(const WCHAR* shaderDir, const WCHAR* systemDir = L"");

	HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes);
	HRESULT __stdcall Close(LPCVOID pData);
	const std::set<std::string>& GetIncludes() const {return m_includeFiles;}
private:
	std::string m_ShaderDir;
	std::string m_SystemDir;
	std::set<std::string> m_includeFiles;
};


//template <typename T> HRESULT createShader(T* tmpShader, ID3DBlob* pBlob, T** shader )
template <typename T> HRESULT CreateShader(T** shader, ID3DBlob* pBlob)
{
	return S_FALSE;
};

template <typename T> HRESULT CreateShaderStreamOutput(T** shader, ID3DBlob* pBlob, D3D11_SO_DECLARATION_ENTRY *pSODeclaration = NULL, UINT numSOEntries = 0, UINT* pSOStrides = NULL, UINT numSOStrides = 0, UINT rasterizerStream = D3D11_SO_NO_RASTERIZED_STREAM)
{
	return S_FALSE;
};
 

template <typename T>
class Shader
{
	public:
		// TODO checkme: copy macros and stream output array data as data pointed to may be deleted 
		Shader(const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, SHADERTYPE shaderType, ID3DBlob** ppBlobOut, std::wstring& compiledFileName, D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0, ID3D11ClassLinkage* pClassLinkage = NULL)
		{
			HRESULT hr;			
			V( DXUTFindDXSDKMediaFileCch( m_fileNameSrc, MAX_PATH, szFileName ) );
			if(FAILED(hr))
			{
				std::wcout << "cannot find file " << szFileName << std::endl;
				return;
			}			

			init(szEntryPoint, szShaderModel, shaderType,
				compiledFileName, pDefines, compilerFlags,
				NULL, 0, NULL, 0, D3D11_SO_NO_RASTERIZED_STREAM,
				pClassLinkage);
						
			//m_lastChange.QuadPart = 0;
			//GetAndSetFileChange(m_fileNameSrc, m_lastChange);	
			//m_fileNameSrc;
			
			m_fileNameSrcString = utf8_encode(szFileName);
			g_shaderManager.AddIncludeFile(m_fileNameSrcString);

			m_compiledInvalid = false;			
			HANDLE hFindCompiled = FindFirstFile(m_fileNameCompiled.c_str(), &m_lastChangeCompiled);
			if(hFindCompiled == INVALID_HANDLE_VALUE) m_compiledInvalid = true;


			CompileShader(ppBlobOut);
		}

		// with stream out support
		Shader(	const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, SHADERTYPE shaderType, ID3DBlob** ppBlobOut,
				std::wstring& compiledFileName, D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0,
				D3D11_SO_DECLARATION_ENTRY *pSODeclaration = NULL, UINT numSOEntries = 0, UINT* pSOStrides = NULL, UINT numSOStrides = 0, UINT rasterizerStream = D3D11_SO_NO_RASTERIZED_STREAM,
				ID3D11ClassLinkage* pClassLinkage = NULL)
		{
			HRESULT hr;			
			V( DXUTFindDXSDKMediaFileCch( m_fileNameSrc, MAX_PATH, szFileName ) );
			if(FAILED(hr))
			{
				std::wcout << "cannot find file " << szFileName << std::endl;
				return;
			}			

			//g_shaderManager->AddIncludeFile(szFileName);

			init(szEntryPoint, szShaderModel, shaderType,
				 compiledFileName, pDefines, compilerFlags,
				 pSODeclaration, numSOEntries, pSOStrides, numSOStrides, rasterizerStream,
				 pClassLinkage);

			
			//m_lastChange.QuadPart = 0;
			//GetAndSetFileChange(m_fileNameSrc, m_lastChange);	

			m_fileNameSrcString = utf8_encode(szFileName);
			g_shaderManager.AddIncludeFile(m_fileNameSrcString);

			m_compiledInvalid = false;			
			HANDLE hFindCompiled = FindFirstFile(m_fileNameCompiled.c_str(), &m_lastChangeCompiled);
			if(hFindCompiled == INVALID_HANDLE_VALUE) m_compiledInvalid = true;

			CompileShader(ppBlobOut);			
		}

		void init(	LPCSTR szEntryPoint, LPCSTR szShaderModel, SHADERTYPE shaderType,
					std::wstring& compiledFileName, D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0,
					D3D11_SO_DECLARATION_ENTRY *pSODeclaration = NULL, UINT numSOEntries = 0, UINT* pSOStrides = NULL, UINT numSOStrides = 0, UINT rasterizerStream = D3D11_SO_NO_RASTERIZED_STREAM,
					ID3D11ClassLinkage* pClassLinkage = NULL)
		{
			m_shader			= NULL;
			m_entryPoint		= szEntryPoint;
			m_shaderModel		= szShaderModel;
			m_type				= shaderType;
			m_fileNameCompiled	= compiledFileName;
			m_pDefines			= NULL;
			m_classLinkage		= pClassLinkage;
			m_compilerFlags		= compilerFlags;

			m_SODeclarationArray = pSODeclaration;
			m_SONumEntries		 = numSOEntries;
			m_SOStridesArray	 = pSOStrides;
			m_SONumStrides		 = numSOStrides;
			m_rasterizerStream	 = rasterizerStream;
			m_firstLoad			 = true;

			
			// copy macros to local for shader reloading
			if(pDefines)
			{			
				// count macros
				int numDefs = 0;
				for(numDefs = 0; pDefines[numDefs].Name != NULL; ++numDefs);
				
			
				m_pDefines = new D3D_SHADER_MACRO[numDefs+1];
				for(int i=0; i < numDefs; ++i)
				{					
					size_t sizeName = strlen(pDefines[i].Name)+1;
					size_t sizeDef  = strlen(pDefines[i].Definition)+1;

					m_pDefines[i].Name		 = new char[sizeName];
					m_pDefines[i].Definition = new char[sizeDef];
					
					memcpy((void*)&(m_pDefines[i].Name[0]), (void*)&(pDefines[i].Name[0]),  sizeName);					
					memcpy((void*)&(m_pDefines[i].Definition[0]), (void*)&(pDefines[i].Definition[0]), sizeDef);

				}

				m_pDefines[numDefs].Name = NULL;					
				m_pDefines[numDefs].Definition = NULL;

				//for(int i = 0; m_pDefines[i].Name != NULL; ++i)
				//{
				//	std::cout << i << ": " <<  m_pDefines[i].Name << " " <<  m_pDefines[i].Definition << std::endl;
				//}

			}

			// TODO copy SO Declaration Array and SOStride Array to local

		}

		Shader()
		{
			m_shader = NULL;
		}
		
		T* Get() const {	return m_shader; }

		~Shader()
		{
			Destroy();
		}

		void Destroy();


		HRESULT CheckAndReload();
		

	private:
		HRESULT CompileShader(ID3DBlob** ppBlobOut);

		
		
		SHADERTYPE					m_type;
		WCHAR						m_fileNameSrc[MAX_PATH];
		std::string					m_fileNameSrcString;

		std::wstring				m_fileNameCompiled;

		LPCSTR						m_entryPoint;
		LPCSTR						m_shaderModel;
		D3D_SHADER_MACRO*			m_pDefines;
		ID3D11ClassLinkage*			m_classLinkage;
		DWORD						m_compilerFlags;

		//bool						m_isChanged; 
		//ULARGE_INTEGER				m_lastChange;
		WIN32_FIND_DATA				m_lastChangeCompiled;

		// Stream Output Support		
		D3D11_SO_DECLARATION_ENTRY* m_SODeclarationArray;
		UINT						m_SONumEntries;
		UINT*						m_SOStridesArray;
		UINT						m_SONumStrides;
		UINT						m_rasterizerStream;
		std::unordered_set<std::string> m_includes;
		bool						m_firstLoad;

		bool						m_compiledInvalid;
		
		T*	m_shader;
};

template <typename T>
HRESULT Shader<T>::CheckAndReload()
{

	HRESULT hr;
	// return if not changed
	//if (!GetAndSetFileChange(m_fileNameSrc, m_lastChange)) 
	//	return S_OK;						

	// is changed, reload
	ID3DBlob* pBlob = NULL;
	hr = CompileShader(&pBlob);
	SAFE_RELEASE(pBlob);

	if(FAILED(hr))				
	{
		if(m_type == VERTEX_SHADER)
			std::cerr << "vertex shader failed" << std::endl;
		else if(m_type == DOMAIN_SHADER)
			std::cerr << "domain shader failed" << std::endl;
		else if(m_type == HULL_SHADER)
			std::cerr << "hull shader failed" << std::endl;
		else if(m_type == GEOMETRY_SHADER)
			std::cerr << "geometry shader failed" << std::endl;
		else if(m_type == PIXEL_SHADER)
			std::cerr << "pixel shader failed" << std::endl;
		else if(m_type == COMPUTE_SHADER)
			std::cerr << "compute shader failed" << std::endl;
	}
	return hr;			

}

template <typename T>
void Shader<T>::Destroy()
{
	SAFE_RELEASE(m_shader);
	if(m_pDefines)
	{
		for(int i = 0; m_pDefines[i].Name != NULL; ++i)
		{
			SAFE_DELETE_ARRAY(m_pDefines[i].Name);
			SAFE_DELETE_ARRAY(m_pDefines[i].Definition);
		}
	}
	SAFE_DELETE_ARRAY(m_pDefines);
}

struct EffectCommonConfig{			
	int common_render_hash;		
	EffectCommonConfig()
	{
		common_render_hash = 0;
	}
};

typedef std::pair< std::string, std::string > ShaderDefine;
typedef std::vector< ShaderDefine > ShaderDefineVector;


struct DXShaderSource {
	//typedef std::pair< std::string, std::string > Define;
	//typedef std::vector< Define > DefineVector;

	void AddDefine(std::string const & name, std::string const & value = "1") 
	{
		defines.push_back( ShaderDefine(name, value) );
	}

	std::wstring sourceFile;
	std::string version;
	std::string target;
	std::string entry;

	ShaderDefineVector defines;
};

struct DXShaderSourceConfig {

	DXShaderSourceConfig() : value(0)
	{	
	}

	DXShaderSource commonShader;
	DXShaderSource vertexShader;
	DXShaderSource hullShader;
	DXShaderSource domainShader;
	DXShaderSource geometryShader;
	DXShaderSource pixelShader;
	DXShaderSource computeShader;

	// config hashes for shader precompile names
	EffectCommonConfig commonConfig;
	virtual ~DXShaderSourceConfig();

	int value;

};

struct DXShaderConfig  {
	DXShaderConfig() :
		vertexShader(0), hullShader(0), domainShader(0),
		geometryShader(0), pixelShader(0), computeShader(0), srcConfig(0) 
	{
	}

	virtual ~DXShaderConfig() 
	{
		SAFE_DELETE(srcConfig);
	}


	Shader<ID3D11VertexShader>*		vertexShader;
	Shader<ID3D11HullShader>*		hullShader;
	Shader<ID3D11DomainShader>*		domainShader;
	Shader<ID3D11GeometryShader>*	geometryShader;
	Shader<ID3D11PixelShader>*		pixelShader;
	Shader<ID3D11ComputeShader>*	computeShader;
	DXShaderSourceConfig*				srcConfig;
};

class DrawRegistryBase {
public:
	typedef int DescType;
	typedef DXShaderConfig ConfigType;
	typedef DXShaderSourceConfig SourceConfigType;

	virtual ~DrawRegistryBase(){};

protected:
	virtual ConfigType * _NewDrawConfig() const { return new ConfigType(); }
	virtual ConfigType * _CreateDrawConfig(	DescType const & desc,
											SourceConfigType const * sconfig,
											ID3D11Device1 * pd3dDevice,
											ID3D11InputLayout ** ppInputLayout,
											D3D11_INPUT_ELEMENT_DESC const * pInputElementDescs,
											int numInputElements) const;

	virtual SourceConfigType * _NewDrawSourceConfig()  const { return new SourceConfigType(); }
	virtual SourceConfigType *_CreateDrawSourceConfig(DescType const & desc, ID3D11Device1 * pd3dDevice) const;
};

template <  class DESC_TYPE          = Effect,
			class CONFIG_TYPE		 = DXShaderConfig,
			class SOURCE_CONFIG_TYPE = DXShaderSourceConfig >
class TEffectDrawRegistry : public DrawRegistryBase {
public:
	typedef DESC_TYPE DescType;
	typedef CONFIG_TYPE ConfigType;
	typedef SOURCE_CONFIG_TYPE SourceConfigType;

	typedef std::map<DescType, ConfigType *> ConfigMap;

public:	
	virtual ~TEffectDrawRegistry() 
	{
		Reset();
	}

	void Reset() 
	{
		if(_configMap.size() > 0)
		{		
			for(auto i : _configMap)
			{
				if(i.second) delete i.second;
			}
			_configMap.clear();
		}		
	}

	// fetch shader config
	ConfigType * GetDrawConfig(DescType const & desc, ID3D11Device1 * pd3dDevice, ID3D11InputLayout ** ppInputLayout = NULL, D3D11_INPUT_ELEMENT_DESC const * pInputElementDescs = NULL,	int numInputElements = 0) 
	{
		typename ConfigMap::iterator it = _configMap.find(desc);
		if (it != _configMap.end()) 
		{
			return it->second;
		}
		else 
		{
			auto* srcConfig = _CreateDrawSourceConfig(desc, pd3dDevice);
			ConfigType * config = _CreateDrawConfig(desc, srcConfig, pd3dDevice, ppInputLayout, pInputElementDescs, numInputElements);
			_configMap[desc] = config;				
			return config;
		}
	}

protected:
	virtual ConfigType * _NewDrawConfig() const { return new ConfigType(); }
	virtual ConfigType * _CreateDrawConfig(	DescType const & desc,
											SourceConfigType const * sconfig,
											ID3D11Device1 * pd3dDevice,
											ID3D11InputLayout ** ppInputLayout,
											D3D11_INPUT_ELEMENT_DESC const * pInputElementDescs,
											int numInputElements) const { return NULL; }

	virtual SourceConfigType * _NewDrawSourceConfig() const { return new SourceConfigType(); }
	virtual SourceConfigType * _CreateDrawSourceConfig(DescType const & desc, ID3D11Device1 * pd3dDevice) const { return NULL; }

private:
	ConfigMap _configMap;
};

class WatchedFile
{
public:
	WatchedFile(const std::string& includeFileName);
	~WatchedFile();

	bool CheckIsModified();
	WIN32_FIND_DATA& getLastWrite() { return m_lastWrite;} 
private:
	WIN32_FIND_DATA		m_lastWrite;
	std::wstring		m_fileName;
};

class ShaderManager{
public:
	ShaderManager();
	~ShaderManager();

	Shader<ID3D11VertexShader>*		AddVertexShader(	const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0,const std::vector<int>& configHashes = std::vector<int>(), ID3D11ClassLinkage* pClassLinkage = NULL);
	Shader<ID3D11HullShader>*		AddHullShader(		const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0,const std::vector<int>& configHashes = std::vector<int>(), ID3D11ClassLinkage* pClassLinkage = NULL);
	Shader<ID3D11DomainShader>*		AddDomainShader(	const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0,const std::vector<int>& configHashes = std::vector<int>(), ID3D11ClassLinkage* pClassLinkage = NULL);
	Shader<ID3D11GeometryShader>*	AddGeometryShader(	const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0,const std::vector<int>& configHashes = std::vector<int>(), ID3D11ClassLinkage* pClassLinkage = NULL);
	Shader<ID3D11GeometryShader>*	AddGeometryShaderSO(const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0,
														D3D11_SO_DECLARATION_ENTRY *pSODeclaration = NULL, UINT numSOEntries = 0, UINT* pSOStrides = NULL, UINT numSOStrides = 0, UINT rasterizerStream = D3D11_SO_NO_RASTERIZED_STREAM,
														ID3D11ClassLinkage* pClassLinkage = NULL);
	Shader<ID3D11PixelShader>*		AddPixelShader(		const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0, const std::vector<int>& configHashes = std::vector<int>(), ID3D11ClassLinkage* pClassLinkage = NULL);
	Shader<ID3D11ComputeShader>*	AddComputeShader(	const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0, const std::vector<int>& configHashes = std::vector<int>(), ID3D11ClassLinkage* pClassLinkage = NULL);

	// with support for compiled names from config hashes
	//Shader<ID3D11VertexShader>*		AddVertexShader(	const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, const std::vector<int>& configHashes, D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0, ID3D11ClassLinkage* pClassLinkage = NULL);
	//Shader<ID3D11HullShader>*		AddHullShader(		const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, const std::vector<int>& configHashes = std::vector<int>(), D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0, ID3D11ClassLinkage* pClassLinkage = NULL);
	//Shader<ID3D11DomainShader>*		AddDomainShader(	const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, const std::vector<int>& configHashes = std::vector<int>(), D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0, ID3D11ClassLinkage* pClassLinkage = NULL);
	//Shader<ID3D11GeometryShader>*	AddGeometryShader(	const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, const std::vector<int>& configHashes = std::vector<int>(), D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0, ID3D11ClassLinkage* pClassLinkage = NULL);
//	Shader<ID3D11GeometryShader>*	AddGeometryShaderSO(const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, const std::vector<int>& configHashes = std::vector<int>(), D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0,
//		D3D11_SO_DECLARATION_ENTRY *pSODeclaration = NULL, UINT numSOEntries = 0, UINT* pSOStrides = NULL, UINT numSOStrides = 0, UINT rasterizerStream = D3D11_SO_NO_RASTERIZED_STREAM,
//		ID3D11ClassLinkage* pClassLinkage = NULL);
//	Shader<ID3D11PixelShader>*		AddPixelShader(		const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, const std::vector<int>& configHashes = std::vector<int>(),D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0, ID3D11ClassLinkage* pClassLinkage = NULL);
//	Shader<ID3D11ComputeShader>*	AddComputeShader(	const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, const std::vector<int>& configHashes = std::vector<int>(),D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0, ID3D11ClassLinkage* pClassLinkage = NULL);


	HRESULT CheckAndReload();
	void Destroy();
	WatchedFile* AddIncludeFile(std::string includeFileName);
	const std::unordered_map<std::string, WatchedFile*>& GetIncludes() const { return m_includeFiles;}
	bool UsePrecompiledShaders() const {return m_precompiledShadersEnabled;}
	void SetPrecompiledShaders(bool enabled) { m_precompiledShadersEnabled = enabled; }
private:
	void CheckIncludes();
	std::wstring CreateCompiledFileName(const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, D3D_SHADER_MACRO* pDefines = NULL, DWORD compilerFlags = 0,const std::vector<int>& config_hashes = std::vector<int>());
	std::unordered_map<std::wstring, Shader<ID3D11VertexShader>*	> m_vertexShaders;	
	std::unordered_map<std::wstring, Shader<ID3D11HullShader>*		> m_hullShaders;
	std::unordered_map<std::wstring, Shader<ID3D11DomainShader>*	> m_domainShaders;
	std::unordered_map<std::wstring, Shader<ID3D11GeometryShader>*	> m_geometryShaders;
	std::unordered_map<std::wstring, Shader<ID3D11PixelShader>*		> m_pixelShaders;
	std::unordered_map<std::wstring, Shader<ID3D11ComputeShader>*	> m_computeShaders;
	
	std::wstring m_compiledShaderDirectory;

	std::vector<WatchedFile>						m_includes;
	std::unordered_map<std::string, WatchedFile*>	m_includeFiles;
	std::unordered_map<std::string, bool>			m_changedIncludeFiles; 
	bool m_precompiledShadersEnabled; // for nsight debugging
};

extern ShaderManager g_shaderManager;
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
#include <iostream>
#include <DXUT.h>
#include <SDKMisc.h>
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>

#include "DXTextureCache.h"
#include "StringConversion.h"


DXTextureCache g_textureManager;

DXTextureCache::DXTextureCache(){}

DXTextureCache::~DXTextureCache()
{
	Destroy();
}

void DXTextureCache::Destroy()
{
	for(auto it : m_textureCache)	SAFE_RELEASE(it.second);
	m_textureCache.clear();
}



ID3D11ShaderResourceView* DXTextureCache::AddTexture( std::string fileName )
{
	auto it = m_textureCache.find(fileName);
	
	if(it != m_textureCache.end())
		return it->second;

	{
		std::cout << "load texture" << fileName << std::endl;
		std::wstring wstr = s2ws(fileName);
		HRESULT hr = S_OK;
		WCHAR str[MAX_PATH];
		ZeroMemory(&str, MAX_PATH*sizeof(WCHAR));
		V( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, wstr.c_str() ) );
		if(hr == S_OK)
		{
			//std::cout << "found texture " << fileName << std::endl;
			
			ID3D11ShaderResourceView* srv = NULL;
			if(GetFileExt(fileName) != "dds")
			{
				V(DirectX::CreateWICTextureFromFile(DXUTGetD3D11Device(), DXUTGetD3D11DeviceContext(),str,NULL, &srv,0));
			}
			else
			{
				V(DirectX::CreateDDSTextureFromFile(DXUTGetD3D11Device(),str,NULL, &srv,NULL));
			}
			if(hr != S_OK)
			{
				std::cout << "[Error] could not load texture" << fileName << std::endl;
				exit(1);
			}			
			
			m_textureCache[fileName] = srv;
			return srv;
		}
		else		
		{
			std::cout << "[Error] texture not found " << fileName << std::endl;
			exit(1);
		}
		
	}

}

ID3D11ShaderResourceView* DXTextureCache::GetByName( std::string fileName )
{
	return NULL;
}

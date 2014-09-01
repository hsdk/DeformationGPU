#pragma once

#include <DXUT.h>
#include <string>
#include <map>

class DXTextureCache{
public:
	DXTextureCache();
	~DXTextureCache();

	void Destroy();

	ID3D11ShaderResourceView* AddTexture(std::string fileName);	
	ID3D11ShaderResourceView* GetByName(std::string fileName);
private:
	std::map<std::string, ID3D11ShaderResourceView*>  m_textureCache;
};

extern DXTextureCache g_textureManager;
#pragma once

#include <string>
#include <unordered_map>
#include "DXMaterial.h"

struct DXMaterialCache
{
	DXMaterialCache(){};
	~DXMaterialCache();
	
	void		AddMaterial(DXMaterial* material);
	bool		IsCached(const std::string& materialName) const;	
	DXMaterial* GetCachedMaterial(const std::string& materialName) const;

private:
	std::unordered_map<std::string, DXMaterial*>  _materialMap;
};
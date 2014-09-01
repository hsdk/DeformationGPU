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

#include "stdafx.h"
#include "DXMaterialCache.h"

//Henry: has to be last header
#include "utils/DbgNew.h"

DXMaterialCache::~DXMaterialCache()
{	
	for(auto &it : _materialMap)	delete it.second;
	_materialMap.clear();
}

bool DXMaterialCache::IsCached( const std::string& materialName ) const
{	
	if(_materialMap.find(materialName) == _materialMap.end())
		return false;
	else
		return true;
}

DXMaterial* DXMaterialCache::GetCachedMaterial( const std::string& materialName ) const
{
	auto it =_materialMap.find(materialName);
	if( it == _materialMap.end())	
		return NULL;
	else 
		return it->second;
}

void DXMaterialCache::AddMaterial( DXMaterial* material )
{
	if(IsCached(material->_name))
	{
		std::cerr << "[ERROR]: material with same name is already in cache" << std::endl;
		return;
	}
	else	
	{
		//std::cout << "[INFO]: add new material " << material->_name << std::endl;
		_materialMap[material->_name] = material;					
	}
}

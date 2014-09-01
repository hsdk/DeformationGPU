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

#ifndef OSD_D3D11_DRAW_REGISTRY_H
#define OSD_D3D11_DRAW_REGISTRY_H

#include "../version.h"

#include "../far/patchTables.h"
#include "../osd/drawRegistry.h"
#include "../osd/vertex.h"

#include <SDX/DXShaderManager.h>
#include <map>

#include <DXUT.h>

struct ID3D11VertexShader;
struct ID3D11HullShader;
struct ID3D11DomainShader;
struct ID3D11GeometryShader;
struct ID3D11PixelShader;

struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
//struct ID3D11Device1;

struct ID3D11InputLayout;
struct D3D11_INPUT_ELEMENT_DESC;

namespace OpenSubdiv {
	namespace OPENSUBDIV_VERSION {


		struct OSDSCommonConfig{			
			unsigned int max_valence;
			unsigned int num_vertex_elements;
			int common_render_hash;		
			OSDSCommonConfig()
			{
				max_valence = 6;
				num_vertex_elements = 3;
				common_render_hash = 0;
			}
		};

		union OSDSPatchConfig{			
			struct {
				unsigned int type:4;
				unsigned int pattern:3;	 // 0, 1, 2, 3, 4
				unsigned int subpatch:2; // 0, 1, 2, 3
				unsigned int rotation:2;
			}; 
			int value;

			OSDSPatchConfig(){value = 0;}
		};
		struct OsdD3D11DrawSourceConfig {
			int value;
			OsdD3D11DrawSourceConfig()
			{
				value = 0;
			}
			OsdDrawShaderSource commonShader;
			OsdDrawShaderSource vertexShader;
			OsdDrawShaderSource hullShader;
			OsdDrawShaderSource domainShader;
			OsdDrawShaderSource geometryShader;
			OsdDrawShaderSource pixelShader;

			// config hashes for shader precompile names
			OSDSCommonConfig commonConfig;
			OSDSPatchConfig vertexConfig;
			OSDSPatchConfig domainConfig;
			OSDSPatchConfig hullConfig;
			OSDSPatchConfig geometryConfig;
			OSDSPatchConfig pixelConfig;
			virtual ~OsdD3D11DrawSourceConfig();
		};

		struct OsdD3D11DrawConfig : public OsdDrawConfig {
			OsdD3D11DrawConfig() :
				vertexShader(0), hullShader(0), domainShader(0),
				geometryShader(0), pixelShader(0) {}
			virtual ~OsdD3D11DrawConfig();

			Shader<ID3D11VertexShader>*		vertexShader;
			Shader<ID3D11HullShader>*		hullShader;
			Shader<ID3D11DomainShader>*		domainShader;
			Shader<ID3D11GeometryShader>*	geometryShader;
			Shader<ID3D11PixelShader>*		pixelShader;
			OsdD3D11DrawSourceConfig*		srcConfig;
		};

		//------------------------------------------------------------------------------




		//------------------------------------------------------------------------------

		class OsdD3D11DrawRegistryBase {

		public:
			typedef OsdDrawContext::PatchDescriptor DescType;
			typedef OsdD3D11DrawConfig ConfigType;
			typedef OsdD3D11DrawSourceConfig SourceConfigType;

			OsdD3D11DrawRegistryBase(bool enablePtex=false) : _enablePtex(enablePtex) { }

			virtual ~OsdD3D11DrawRegistryBase();

			bool IsPtexEnabled() const {
				return _enablePtex;
			}

			void SetPtexEnabled(bool b) {
				_enablePtex=b;
			}

		protected:
			virtual ConfigType * _NewDrawConfig() { return new ConfigType(); }
			virtual ConfigType *
				_CreateDrawConfig(DescType const & desc,
				SourceConfigType const * sconfig,
				ID3D11Device1 * pd3dDevice,
				ID3D11InputLayout ** ppInputLayout,
				D3D11_INPUT_ELEMENT_DESC const * pInputElementDescs,
				int numInputElements);

			virtual SourceConfigType * _NewDrawSourceConfig() { return new SourceConfigType(); }
			virtual SourceConfigType *
				_CreateDrawSourceConfig(DescType const & desc, ID3D11Device1 * pd3dDevice);

		private:
			bool _enablePtex;
		};

		//------------------------------------------------------------------------------

		template <class DESC_TYPE = OsdDrawContext::PatchDescriptor,
		class CONFIG_TYPE = OsdD3D11DrawConfig,
		class SOURCE_CONFIG_TYPE = OsdD3D11DrawSourceConfig>
		class OsdD3D11DrawRegistry : public OsdD3D11DrawRegistryBase {

		public:
			typedef OsdD3D11DrawRegistryBase BaseRegistry;

			typedef DESC_TYPE DescType;
			typedef CONFIG_TYPE ConfigType;
			typedef SOURCE_CONFIG_TYPE SourceConfigType;

			typedef std::map<DescType, ConfigType *> ConfigMap;

		public:
			virtual ~OsdD3D11DrawRegistry() {
				Reset();
			}

			void Reset() {
				for (typename ConfigMap::iterator
					i = _configMap.begin(); i != _configMap.end(); ++i) {
						delete i->second;
				}
				_configMap.clear();
			}

			// fetch shader config
			ConfigType *
				GetDrawConfig(DescType const & desc,
				ID3D11Device1 * pd3dDevice,
				ID3D11InputLayout ** ppInputLayout = NULL,
				D3D11_INPUT_ELEMENT_DESC const * pInputElementDescs = NULL,
				int numInputElements = 0) {
					typename ConfigMap::iterator it = _configMap.find(desc);
					if (it != _configMap.end()) {
						return it->second;
					} else {
						ConfigType * config =
							_CreateDrawConfig(desc,
							_CreateDrawSourceConfig(desc, pd3dDevice),
							pd3dDevice,
							ppInputLayout,
							pInputElementDescs, numInputElements);
						_configMap[desc] = config;
						return config;
					}
			}

		protected:
			virtual ConfigType * _NewDrawConfig() {
				return new ConfigType();
			}

			virtual ConfigType * _CreateDrawConfig(DescType const & desc,
				SourceConfigType const * sconfig,
				ID3D11Device1 * pd3dDevice,
				ID3D11InputLayout ** ppInputLayout,
				D3D11_INPUT_ELEMENT_DESC const * pInputElementDescs,
				int numInputElements) {
					return NULL;
			}

			virtual SourceConfigType * _NewDrawSourceConfig() {
				return new SourceConfigType();
			}

			virtual SourceConfigType * _CreateDrawSourceConfig(DescType const & desc,
				ID3D11Device1 * pd3dDevice) {
					return NULL;
			}

		private:
			ConfigMap _configMap;
		};

	} // end namespace OPENSUBDIV_VERSION
	using namespace OPENSUBDIV_VERSION;

} // end namespace OpenSubdiv

#endif /* OSD_D3D11_DRAW_REGISTRY_H */

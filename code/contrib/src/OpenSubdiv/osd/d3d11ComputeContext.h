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

#ifndef OSD_D3D11_COMPUTE_CONTEXT_H
#define OSD_D3D11_COMPUTE_CONTEXT_H

#include "../version.h"

#include "../far/subdivisionTables.h"
#include "../far/vertexEditTables.h"
#include "../osd/vertex.h"
#include "../osd/vertexDescriptor.h"
#include "../osd/nonCopyable.h"

#include <DXUT.h>

#include <vector>

struct ID3D11Buffer;
//struct ID3D11Device1;
//struct ID3D11DeviceContext1;
struct ID3D11ShaderResourceView;

namespace OpenSubdiv {
	namespace OPENSUBDIV_VERSION {

		class OsdD3D11ComputeKernelBundle;

		class OsdD3D11ComputeTable : OsdNonCopyable < OsdD3D11ComputeTable > {
		public:
			template<typename T>
			OsdD3D11ComputeTable(const std::vector<T> &table, ID3D11DeviceContext1 *deviceContext, DXGI_FORMAT format) {
				createBuffer((int)table.size() * sizeof(T), table.empty() ? NULL : &table[0], format, (int)table.size(), deviceContext);
			}

			virtual ~OsdD3D11ComputeTable();

			ID3D11Buffer * GetBuffer() const;
			ID3D11ShaderResourceView * GetSRV() const;

		private:
			void createBuffer(int size, const void *ptr, DXGI_FORMAT format, int numElements, ID3D11DeviceContext1 *deviceContext);

			ID3D11Buffer * _buffer;
			ID3D11ShaderResourceView * _srv;
		};

		class OsdD3D11ComputeHEditTable : OsdNonCopyable < OsdD3D11ComputeHEditTable > {
		public:
			OsdD3D11ComputeHEditTable(const FarVertexEditTables::VertexEditBatch &batch,
				ID3D11DeviceContext1 *deviceContext);

			virtual ~OsdD3D11ComputeHEditTable();

			const OsdD3D11ComputeTable * GetPrimvarIndices() const;

			const OsdD3D11ComputeTable * GetEditValues() const;

			int GetOperation() const;

			int GetPrimvarOffset() const;

			int GetPrimvarWidth() const;

		private:
			OsdD3D11ComputeTable *_primvarIndicesTable;
			OsdD3D11ComputeTable *_editValuesTable;

			int _operation;
			int _primvarOffset;
			int _primvarWidth;
		};

		///
		/// \brief D3D Refine Context
		///
		/// The D3D implementation of the Refine module contextual functionality. 
		///
		/// Contexts interface the serialized topological data pertaining to the 
		/// geometric primitives with the capabilities of the selected discrete 
		/// compute device.
		///
		class OsdD3D11ComputeContext : public OsdNonCopyable < OsdD3D11ComputeContext > {
		public:
			/// Creates an OsdD3D11ComputeContext instance
			///
			/// @param subdivisionTables  the FarSubdivisionTables used for this Context.
			///
			/// @param vertexEditTables   the FarVertexEditTables used for this Context.
			///
			/// @param deviceContext  D3D device
			///
			static OsdD3D11ComputeContext * Create(FarSubdivisionTables const *subdivisionTables,
				FarVertexEditTables const *vertexEditTables,
				ID3D11DeviceContext1 *deviceContext);

			/// Destructor
			virtual ~OsdD3D11ComputeContext();

			/// Returns one of the vertex refinement tables.
			///
			/// @param tableIndex the type of table
			///
			const OsdD3D11ComputeTable * GetTable(int tableIndex) const;

			/// Returns the number of hierarchical edit tables
			int GetNumEditTables() const;

			/// Returns a specific hierarchical edit table
			///
			/// @param tableIndex the index of the table
			///
			const OsdD3D11ComputeHEditTable * GetEditTable(int tableIndex) const;

			void BindShaderStorageBuffers(ID3D11DeviceContext1 *deviceContext) const;

			void UnbindShaderStorageBuffers(ID3D11DeviceContext1 *deviceContext) const;

			void BindEditShaderStorageBuffers(int editIndex, ID3D11DeviceContext1 *deviceContext) const;

			void UnbindEditShaderStorageBuffers(ID3D11DeviceContext1 *deviceContext) const;

		protected:
			explicit OsdD3D11ComputeContext(FarSubdivisionTables const *subdivisionTables,
				FarVertexEditTables const *vertexEditTables,
				ID3D11DeviceContext1 *deviceContext);

		private:
			std::vector<OsdD3D11ComputeTable*> _tables;
			std::vector<OsdD3D11ComputeHEditTable*> _editTables;
		};

	}  // end namespace OPENSUBDIV_VERSION
	using namespace OPENSUBDIV_VERSION;

}  // end namespace OpenSubdiv

#endif  // OSD_D3D11_COMPUTE_CONTEXT_H
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

#ifndef FAR_VERTEX_EDIT_TABLES_FACTORY_H
#define FAR_VERTEX_EDIT_TABLES_FACTORY_H

#include "../version.h"

#include "../hbr/vertexEdit.h"

#include "../far/vertexEditTables.h"
#include "../far/kernelBatch.h"

#include <cassert>
#include <vector>

namespace OpenSubdiv {
	namespace OPENSUBDIV_VERSION {

		/// \brief A specialized factory for FarVertexEditTables
		///
		/// This factory is private to Far and should not be used by client code.
		///
		template <class T, class U> class FarVertexEditTablesFactory {

		public:
			typedef std::vector<FarMesh<U> const *> FarMeshVector;

			/// \brief Splices vertex edit tables from multiple meshes and returns it.
			/// Client code is responsible for deallocation.
			static FarVertexEditTables *Splice(FarMeshVector const &meshes);

		protected:
			template <class X, class Y> friend class FarMeshFactory;

			/// \brief Compares the number of subfaces in an edit (for sorting purposes)
			static bool compareEdits(HbrVertexEdit<T> const *a, HbrVertexEdit<T> const *b);

			static void insertHEditBatch(FarKernelBatchVector *batches, int batchIndex, int batchLevel, int batchCount, int tableOffset);

			/// \brief Creates a FarVertexEditTables instance.
			static FarVertexEditTables * Create(FarMeshFactory<T, U> const * factory, FarMesh<U> * mesh, FarKernelBatchVector *batches, int maxlevel);
		};

		template <class T, class U> bool
			FarVertexEditTablesFactory<T, U>::compareEdits(HbrVertexEdit<T> const *a, HbrVertexEdit<T> const *b) {

			return a->GetNSubfaces() < b->GetNSubfaces();
		}

		template <class T, class U> void
			FarVertexEditTablesFactory<T, U>::insertHEditBatch(FarKernelBatchVector *batches, int batchIndex, int batchLevel, int batchCount, int tableOffset) {

			FarKernelBatchVector::iterator it = batches->begin();

			while (it != batches->end()) {
				if (it->GetLevel() > batchLevel + 1)
					break;
				++it;
			}

			batches->insert(it, FarKernelBatch(FarKernelBatch::HIERARCHICAL_EDIT, batchLevel + 1, batchIndex, 0, batchCount, tableOffset, 0));
		}

		template <class T, class U> FarVertexEditTables *
			FarVertexEditTablesFactory<T, U>::Create(FarMeshFactory<T, U> const * factory, FarMesh<U> * mesh, FarKernelBatchVector *batches, int maxlevel) {

			assert(factory and mesh);

			FarVertexEditTables* result = new FarVertexEditTables();

			std::vector<HbrHierarchicalEdit<T>*> const & hEdits = factory->_hbrMesh->GetHierarchicalEdits();

			std::vector<HbrVertexEdit<T> const *> vertexEdits;
			vertexEdits.reserve(hEdits.size());

			for (int i = 0; i < (int)hEdits.size(); ++i) {
				HbrVertexEdit<T> *vedit = dynamic_cast<HbrVertexEdit<T> *>(hEdits[i]);
				if (vedit) {
					int editlevel = vedit->GetNSubfaces();
					if (editlevel > maxlevel)
						continue;   // far table doesn't contain such level

					vertexEdits.push_back(vedit);
				}
			}

			// sort vertex edits by level
			std::sort(vertexEdits.begin(), vertexEdits.end(), compareEdits);

			// First pass : count batches based on operation and primvar being edited
			std::vector<int> batchIndices;
			std::vector<int> batchSizes;
			for (int i = 0; i < (int)vertexEdits.size(); ++i) {
				HbrVertexEdit<T> const *vedit = vertexEdits[i];

				// translate operation enum
				FarVertexEdit::Operation op = (vedit->GetOperation() == HbrHierarchicalEdit<T>::Set) ?
					FarVertexEdit::Set : FarVertexEdit::Add;

				// determine which batch this edit belongs to (create it if necessary)
				// XXXX manuelk - if the number of edits becomes large, we may need to switch this
				// to a map.
				int batchIndex = -1;
				for (int j = 0; j < (int)result->_batches.size(); ++j) {
					if (result->_batches[j]._primvarIndex == vedit->GetIndex() &&
						result->_batches[j]._primvarWidth == vedit->GetWidth() &&
						result->_batches[j]._op == op) {
						batchIndex = j;
						break;
					}
				}
				if (batchIndex == -1) {
					// create new batch
					batchIndex = (int)result->_batches.size();
					result->_batches.push_back(typename FarVertexEditTables::VertexEditBatch(vedit->GetIndex(), vedit->GetWidth(), op));
					batchSizes.push_back(0);
				}
				batchSizes[batchIndex]++;
				batchIndices.push_back(batchIndex);
			}

			// Second pass : populate the batches
			int numBatches = result->GetNumBatches();
			for (int i = 0; i < numBatches; ++i) {
				result->_batches[i]._vertIndices.resize(batchSizes[i]);
				result->_batches[i]._edits.resize(batchSizes[i] * result->_batches[i].GetPrimvarWidth());
			}

			// Resolve vertexedits path to absolute offset and put them into corresponding batch
			std::vector<int> currentLevels(numBatches);
			std::vector<int> currentCounts(numBatches);
			std::vector<int> currentOffsets(numBatches);
			for (int i = 0; i < (int)vertexEdits.size(); ++i){
				HbrVertexEdit<T> const *vedit = vertexEdits[i];

				HbrFace<T> * f = factory->_hbrMesh->GetFace(vedit->GetFaceID());

				int level = vedit->GetNSubfaces();
				for (int j = 0; j < level; ++j)
					f = f->GetChild(vedit->GetSubface(j));

				int vertexID = f->GetVertex(vedit->GetVertexID())->GetID();

				// Remap vertex ID
				vertexID = factory->_remapTable[vertexID];

				int batchIndex = batchIndices[i];
				int & batchLevel = currentLevels[batchIndex];
				int & batchCount = currentCounts[batchIndex];
				typename FarVertexEditTables::VertexEditBatch &batch = result->_batches[batchIndex];

				// if a new batch is needed
				if (batchLevel != level - 1) {
					// if the current batch isn't empty, emit a kernelBatch
					if (batchCount != currentOffsets[batchIndex]) {
						insertHEditBatch(batches, batchIndex, batchLevel, batchCount - currentOffsets[batchIndex], currentOffsets[batchIndex]);
					}
					// prepare a next batch
					batchLevel = level - 1;
					currentOffsets[batchIndex] = batchCount;
				}

				// Set absolute vertex index
				batch._vertIndices[batchCount] = vertexID;

				// Copy edit values : Subtract edits are optimized into Add edits (fewer batches)
				const float *edit = vedit->GetEdit();

				bool negate = (vedit->GetOperation() == HbrHierarchicalEdit<T>::Subtract);

				for (int j = 0; j < batch.GetPrimvarWidth(); ++j)
					batch._edits[batchCount * batch.GetPrimvarWidth() + j] = negate ? -edit[j] : edit[j];

				batchCount++;
			}

			for (int i = 0; i < numBatches; ++i) {
				int & batchLevel = currentLevels[i];
				int & batchCount = currentCounts[i];

				if (batchCount > 0) {
					insertHEditBatch(batches, i, batchLevel, batchCount - currentOffsets[i], currentOffsets[i]);
				}
			}

			return result;
		}

		// splicing functions
		template <class T, class U> FarVertexEditTables *
			FarVertexEditTablesFactory<T, U>::Splice(FarMeshVector const &meshes) {
			FarVertexEditTables * result = new FarVertexEditTables();

			// at this moment, don't merge vertex edit tables (separate batch)
			for (size_t i = 0; i < meshes.size(); ++i) {
				const FarVertexEditTables *vertexEditTables = meshes[i]->GetVertexEditTables();
				if (not vertexEditTables) continue;

				// copy each edit batch  XXX:inefficient copy
				result->_batches.insert(result->_batches.end(),
					vertexEditTables->_batches.begin(),
					vertexEditTables->_batches.end());
			}

			if (result->_batches.empty()) {
				delete result;
				return NULL;
			}
			return result;
		}



	} // end namespace OPENSUBDIV_VERSION
	using namespace OPENSUBDIV_VERSION;

} // end namespace OpenSubdiv

#endif /* FAR_VERTEX_EDIT_TABLES_FACTORY_H */

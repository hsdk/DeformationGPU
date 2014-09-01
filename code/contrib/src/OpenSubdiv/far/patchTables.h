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

#ifndef FAR_PATCH_TABLES_H
#define FAR_PATCH_TABLES_H

#include "../version.h"

#include "../far/patchParam.h"

#include <cstdlib>
#include <cassert>
#include <algorithm>
#include <vector>
#include <map>

namespace OpenSubdiv {
	namespace OPENSUBDIV_VERSION {

		/// \brief Container for patch vertex indices tables
		///
		/// FarPatchTables contain the lists of vertices for each patch of an adaptive
		/// mesh representation.
		///
		class FarPatchTables {

		public:
			typedef std::vector<unsigned int>  PTable;
			typedef std::vector<int>           VertexValenceTable;
			typedef std::vector<unsigned int>  QuadOffsetTable;
			typedef std::vector<FarPatchParam> PatchParamTable;

			enum Type {
				NON_PATCH = 0,     ///< undefined

				POINTS,            ///< points  (useful for cage drawing)
				LINES,             ///< lines   (useful for cage drawing)

				QUADS,             ///< bilinear quads-only patches
				TRIANGLES,         ///< bilinear triangles-only mesh

				LOOP,              ///< Loop patch  (unsupported)

				REGULAR,           ///< feature-adaptive bicubic patches
				BOUNDARY,
				CORNER,
				GREGORY,
				GREGORY_BOUNDARY
			};

			enum TransitionPattern {
				NON_TRANSITION = 0,
				PATTERN0,
				PATTERN1,
				PATTERN2,
				PATTERN3,
				PATTERN4
			};

			/// \brief Describes the type of a patch
			///
			/// Uniquely identifies all the types of patches in a mesh :
			///
			/// * Raw polygon meshes are identified as POLYGONS and can contain faces
			///   with arbitrary number of vertices
			///
			/// * Uniformly subdivided meshes contain bilinear patches of either QUADS
			///   or TRIANGLES
			///
			/// * Adaptively subdivided meshes contain bicubic patches of types REGULAR,
			///   BOUNDARY, CORNER, GREGORY, GREGORY_BOUNDARY. These bicubic patches are
			///   also further distinguished by a transition pattern as well as a rotational
			///   orientation.
			///
			/// An iterator class is provided as a convenience to enumerate over the set
			/// of valid feature adaptive patch descriptors.
			///
			class Descriptor {

			public:
				/// \brief Default constructor.
				Descriptor() :
					_type(NON_PATCH), _pattern(NON_TRANSITION), _rotation(0) {}

				/// \brief Constructor
				Descriptor(int type, int pattern, unsigned char rotation) :
					_type(type), _pattern(pattern), _rotation(rotation) { }

				/// \brief Copy Constructor
				Descriptor(Descriptor const & d) :
					_type(d.GetType()), _pattern(d.GetPattern()), _rotation(d.GetRotation()) { }

				/// \brief Returns the type of the patch
				Type GetType() const {
					return (Type)_type;
				}

				/// \brief Returns the transition pattern of the patch if any (5 types)
				TransitionPattern GetPattern() const {
					return (TransitionPattern)_pattern;
				}

				/// \brief Returns the rotation of the patch (4 rotations)
				unsigned char GetRotation() const {
					return (unsigned char)_rotation;
				}

				/// \brief Returns the number of control vertices expected for a patch of the
				/// type described
				static inline short GetNumControlVertices(Type t);

				/// \brief Returns the number of control vertices expected for a patch of the
				/// type described
				short GetNumControlVertices() const {
					return GetNumControlVertices(this->GetType());
				}

				/// Returns a vector of all the legal patch descriptors
				static inline std::vector<Descriptor> const & GetAllValidDescriptors();

				/// \brief Allows ordering of patches by type
				inline bool operator < (Descriptor const other) const;

				/// \brief True if the descriptors are identical
				inline bool operator == (Descriptor const other) const;

				/// \brief Descriptor Iterator
				/// Iterates through the patches in the following preset order
				///
				/// ANY order:
				///        POINTS
				///        LINES
				///        QUADS
				///        TRIANGLES
				///        LOOP
				///
				/// FEATURE_ADAPTIVE_CATMARK order:
				///
				///       NON_TRANSITION ( REGULAR
				///                         BOUNDARY
				///                         CORNER
				///                         GREGORY
				///                         GREGORY_BOUNDARY )
				///
				///        PATTERN0 ( REGULAR
				///                   BOUNDARY ROT0 ROT1 ROT2 ROT3
				///                   CORNER   ROT0 ROT1 ROT2 ROT3 )
				///
				///        PATTERN1 ( REGULAR
				///                   BOUNDARY ROT0 ROT1 ROT2 ROT3
				///                   CORNER   ROT0 ROT1 ROT2 ROT3 )
				///        ...
				///
				///        NON_TRANSITION NON_PATCH ROT0 (end)
				///
				class iterator;

				enum PrimType {
					ANY,
					FEATURE_ADAPTIVE_CATMARK
				};

				/// \brief Returns a patch type iterator
				/// @param type       if type=ANY then the iterater points to type POINTS
				///                   if type=FEATURE_ADAPTIVE_CATMARK then the iterator
				///                   points to type NON_TRANSITION REGULAR
				static iterator begin(PrimType type);

				/// \brief Returns an iterator to the end of the list of patch types (NON_PATCH)
				static iterator end();

			private:
				template <class T> friend class FarPatchTablesFactory;
				friend class iterator;

				unsigned int  _type : 4;
				unsigned int  _pattern : 3;
				unsigned int  _rotation : 2;
			};




			/// \brief Describes an array of patches of the same type
			class PatchArray {

			public:
				/// \brief Constructor.
				///
				/// @param desc             descriptor information for the patches in
				///                         the array
				///
				/// @param vertIndex        absolute index to the first control vertex
				///                         of the first patch in the PTable
				///
				/// @param patchIndex       absolute index of the first patch in the
				///                         array
				///
				/// @param npatches         number of patches in the array
				///
				/// @param quadOffsetIndex  absolute index of the first quad offset
				///                         entry
				///
				PatchArray(Descriptor desc, unsigned int vertIndex, unsigned int patchIndex, unsigned int npatches, unsigned int quadOffsetIndex) :
					_desc(desc), _range(vertIndex, patchIndex, npatches, quadOffsetIndex) { }

				/// Returns a patch descriptor defining the type of patches in the array
				Descriptor GetDescriptor() const {
					return _desc;
				}

				/// \brief Describes the range of patches in a PatchArray
				struct ArrayRange {

					/// \brief Constructor
					///
					/// @param ivertIndex        absolute index to the first control vertex
					///                          of the first patch in the PTable
					///
					/// @param ipatchIndex       absolute index of the first patch in the
					///                          array
					///
					/// @param inpatches         number of patches in the array
					///
					/// @param iquadOffsetIndex  absolute index of the first quad offset
					///                          entry
					///
					ArrayRange(unsigned int ivertIndex, unsigned int ipatchIndex, unsigned int inpatches, unsigned int iquadOffsetIndex) :
						vertIndex(ivertIndex), patchIndex(ipatchIndex), npatches(inpatches), quadOffsetIndex(iquadOffsetIndex) { }

					unsigned int vertIndex,       // absolute index to the first control vertex of the first patch in the PTable
						patchIndex,      // absolute index of the first patch in the array
						npatches,        // number of patches in the array
						quadOffsetIndex; // absolute index of the first quad offset entry
				};

				/// \brief Returns a array range struct
				ArrayRange const & GetArrayRange() const {
					return _range;
				}

				/// \brief Returns the index of the first control vertex of the first patch
				/// of this array in the global PTable
				unsigned int GetVertIndex() const {
					return _range.vertIndex;
				}

				/// \brief Returns the global index of the first patch in this array (Used to
				/// access param / fvar table data)
				unsigned int GetPatchIndex() const {
					return _range.patchIndex;
				}

				/// \brief Returns the number of patches in the array
				unsigned int GetNumPatches() const {
					return _range.npatches;
				}

				/// \brief Returns the index to the first entry in the QuadOffsetTable
				unsigned int GetQuadOffsetIndex() const {
					return _range.quadOffsetIndex;
				}

			private:
				template <class T> friend class FarPatchTablesFactory;

				Descriptor _desc;   // type of patches in the array

				ArrayRange _range;  // index locators in the array
			};

			typedef std::vector<PatchArray> PatchArrayVector;

			/// \brief Constructor
			///
			/// @param patchArrays      Vector of descriptors and ranges for arrays of patches
			///
			/// @param patches          Indices of the control vertices of the patches
			///
			/// @param vertexValences   Vertex valance table
			///
			/// @param quadOffsets      Quad offset table
			///
			/// @param patchParams      Local patch parameterization
			///
			/// @param fvarData         Face varying data table
			///
			/// @param fvarWidth        Primvar data width (number of floats)
			///
			/// @param maxValence       Highest vertex valence allowed in the mesh
			///
			FarPatchTables(PatchArrayVector const & patchArrays,
				PTable const & patches,
				VertexValenceTable const * vertexValences,
				QuadOffsetTable const * quadOffsets,
				PatchParamTable const * patchParams,
				std::vector<float> const * fvarData,
				int fvarWidth,
				int maxValence);

			/// \brief Get the table of patch control vertices
			PTable const & GetPatchTable() const { return _patches; }

			/// \brief Returns a pointer to the array of patches matching the descriptor
			PatchArray const * GetPatchArray(Descriptor desc) const {
				return const_cast<FarPatchTables *>(this)->findPatchArray(desc);
			}

			/// \brief Returns all arrays of patches
			PatchArrayVector const & GetPatchArrayVector() const {
				return _patchArrays;
			}

			/// \brief Returns a pointer to the vertex indices of uniformly subdivided faces
			///
			/// In uniform mode the FarPatchTablesFactory can be set to generate either a
			/// patch array containing the faces at the highest level of subdivision, or
			/// a range of arrays, corresponding to multiple successive levels of subdivision.
			///
			/// Note : level '0' is not the coarse mesh. Currently there is no path in the
			/// factories to convert the coarse mesh to FarPatchTables.
			///
			/// @param level  the level of subdivision of the faces (returns the highest
			///               level by default)
			///
			/// @return       a pointer to the first vertex index or NULL if the mesh
			///               is not uniformly subdivided or the level cannot be found.
			///
			unsigned int const * GetFaceVertices(int level = 0) const;

			/// \brief Returns the number of faces in a uniformly subdivided mesh at a given level
			///
			/// In uniform mode the FarPatchTablesFactory can be set to generate either a
			/// patch array containing the faces at the highest level of subdivision, or
			/// a range of arrays, corresponding to multiple successive levels of subdivision.
			///
			/// Note : level '0' is not the coarse mesh. Currently there is no path in the
			/// factories to convert the coarse mesh to FarPatchTables.
			///
			/// @param level  the level of subdivision of the faces (returns the highest
			///               level by default)
			///
			/// @return       the number of faces in the mesh given the subdivision level
			///               or -1 if the mesh is not uniform or the level is incorrect.
			///
			int GetNumFaces(int level = 0) const;

			/// \brief Returns a vertex valence table used by Gregory patches
			VertexValenceTable const & GetVertexValenceTable() const { return _vertexValenceTable; }

			/// \brief Returns a quad offsets table used by Gregory patches
			QuadOffsetTable const & GetQuadOffsetTable() const { return _quadOffsetTable; }

			/// \brief Returns a PatchParamTable for each type of patch
			PatchParamTable const & GetPatchParamTable() const { return _paramTable; }

			/// \brief Ringsize of Regular Patches in table.
			static short GetRegularPatchRingsize() { return 16; }

			/// \brief Ringsize of Boundary Patches in table.
			static short GetBoundaryPatchRingsize() { return 12; }

			/// \brief Ringsize of Boundary Patches in table.
			static short GetCornerPatchRingsize() { return 9; }

			/// \brief Ringsize of Gregory (and Gregory Boundary) Patches in table.
			static short GetGregoryPatchRingsize() { return 4; }

			/// \brief Returns the total number of patches stored in the tables
			int GetNumPatches() const;

			/// \brief Returns the total number of control vertex indices in the tables
			int GetNumControlVertices() const;

			/// \brief Returns max vertex valence
			int GetMaxValence() const { return _maxValence; }

			/// \brief True if the patches are of feature adaptive types
			bool IsFeatureAdaptive() const;

			/// \brief Returns the total number of vertices in the mesh across across all depths
			int GetNumPtexFaces() const { return _numPtexFaces; }


			/// \brief Container for static face-varying primvar data
			class FVarData {

			public:

				/// \brief Returns a vector of floats containing the face-varying data attached
				/// to each patch.
				///
				/// The data is stored as a run of FVarWidth floats per-vertex per-face
				/// e.g.: for UV data it has the structure of float[p][4][2] where
				/// p=primitiveID and FVarWidth=2:
				///      [ [ uv uv uv uv ] [ uv uv uv uv ] [ ... ] ]
				///            prim 0           prim 1
				///
				/// In uniform mode the FarPatchTablesFactory can be set to generate either a
				/// patch array containing the faces at the highest level of subdivision, or
				/// a range of arrays, corresponding to multiple successive levels of subdivision.
				///
				/// Note : level '0' is not the coarse mesh. Currently there is no path in the
				/// factories to convert the coarse mesh to FarPatchTables.
				///
				/// @param level  the level of subdivision of the faces (returns the highest
				///               level by default) **Uniform subdivision only**
				///
				float const * GetData(int level = 0) const;

				/// \brief Returns a vector of floats containing the face-varying data attached
				std::vector<float> const & GetAllData() const {
					return _data;
				}

				/// \brief Returns the width of the interleaved face-varying data
				int GetFVarWidth() const {
					return _fvarWidth;
				}

			private:

				template <class T> friend class FarPatchTablesFactory;
				friend class FarPatchTables;

				FVarData() : _fvarWidth(0) { }

				inline FVarData(std::vector<float> const * data, int fvarWidth);

				std::vector<float> _data;      // face-varying data stored per-face-per-vertex
				std::vector<int>   _offsets;   // a vector of offsets if multiple leves of
				// subdivision are stored in _data
				int                _fvarWidth; // width of the face-varying data
			};

			/// \brief Returns a container for face-varying data
			FVarData const & GetFVarData() const { return _fvarData; }

		private:

			template <class T> friend class FarPatchTablesFactory;

			// Returns the array of patches of type "desc", or NULL if there aren't any in the primitive
			inline PatchArray * findPatchArray(Descriptor desc);

			// Private constructor
			FarPatchTables(int maxvalence) : _maxValence(maxvalence) { }


			PatchArrayVector    _patchArrays;        // Vector of descriptors for arrays of patches

			PTable              _patches;            // Indices of the control vertices of the patches

			VertexValenceTable  _vertexValenceTable; // vertex valence table (for Gregory patches)

			QuadOffsetTable     _quadOffsetTable;    // quad offsets table (for Gregory patches)

			PatchParamTable     _paramTable;

			FVarData            _fvarData;           // face-varying data

			// highest vertex valence allowed in the mesh (used for Gregory
			// vertexValance & quadOffset tables)
			int _maxValence;

			// number of total ptex faces in quads or triangles(loop)
			int _numPtexFaces;

		};

		/// \brief Descriptor iterator class
		class FarPatchTables::Descriptor::iterator {
		public:

			/// Constructor
			iterator() : _pos(-1) {}

			/// Copy Constructor
			iterator(Descriptor desc);

			/// Iteration increment operator
			iterator & operator ++ ();

			/// True of the two descriptors are identical
			bool operator == (iterator const & other) const {
				return (_pos == other._pos);
			}

			/// True if the two descriptors are different
			bool operator != (iterator const & other) const {
				return not(*this == other);
			}

			/// Dereferencing operator
			Descriptor const * operator -> () const {
				return  getValue();
			}

			/// Dereferencing operator
			Descriptor const & operator * () const {
				return *getValue();
			}

		private:
			inline Descriptor const * getValue() const;

			int _pos;
		};

		// Iterator constructor
		inline FarPatchTables::Descriptor::iterator::iterator(Descriptor desc) {

			_pos = -1;
			std::vector<Descriptor> const & descs =
				Descriptor::GetAllValidDescriptors();

			for (int i = 0; i < (int)descs.size(); ++i) {
				if (descs[i] == desc) {
					_pos = i;
					break;
				}
			}
		}

		// Iteration increment operator
		inline FarPatchTables::Descriptor::iterator &
			FarPatchTables::Descriptor::iterator::operator ++ () {

			if (++_pos >= (int)Descriptor::GetAllValidDescriptors().size()) {
				_pos = -1;
			}
			return *this;
		}

		inline FarPatchTables::Descriptor const *
			FarPatchTables::Descriptor::iterator::getValue() const {

			static Descriptor _nonpatch;

			std::vector<Descriptor> const & descs =
				Descriptor::GetAllValidDescriptors();

			if (_pos >= 0 and _pos < (int)descs.size()) {
				return &descs[_pos];
			}

			return &_nonpatch;
		}

		inline std::vector<FarPatchTables::Descriptor> const &
			FarPatchTables::Descriptor::GetAllValidDescriptors() {

			static std::vector<Descriptor> _descriptors;

			if (_descriptors.empty()) {
				_descriptors.reserve(55);

				// non-patch primitives
				for (int i = POINTS; i <= LOOP; ++i) {
					_descriptors.push_back(Descriptor(i, NON_TRANSITION, 0));
				}

				// non-transition patches
				for (int i = REGULAR; i <= GREGORY_BOUNDARY; ++i) {
					_descriptors.push_back(Descriptor(i, NON_TRANSITION, 0));
				}

				// transition patches
				for (int i = PATTERN0; i <= PATTERN4; ++i) {

					_descriptors.push_back(Descriptor(REGULAR, i, 0));

					// 4 rotations for boundary & corner patches
					for (int j = 0; j < 4; ++j) {
						_descriptors.push_back(Descriptor(BOUNDARY, i, j));
					}

					for (int j = 0; j < 4; ++j) {
						_descriptors.push_back(Descriptor(CORNER, i, j));
					}
				}
			}

			return _descriptors;
		}

		// Returns an iterator to the first type of patch (REGULAR NON_TRANSITION ROT0)
		inline FarPatchTables::Descriptor::iterator
			FarPatchTables::Descriptor::begin(PrimType type) {
			switch (type) {
			case ANY:
				return iterator(Descriptor(POINTS, NON_TRANSITION, 0));
			case FEATURE_ADAPTIVE_CATMARK:
				return iterator(Descriptor(REGULAR, NON_TRANSITION, 0));
			default:
				return iterator(Descriptor());
			}
		}

		// Returns an iterator to the end of the list of patch types (NON_PATCH)
		inline FarPatchTables::Descriptor::iterator
			FarPatchTables::Descriptor::end() {
			return iterator(Descriptor());
		}

		// Constructor
		inline
			FarPatchTables::FarPatchTables(PatchArrayVector const & patchArrays,
			PTable const & patches,
			VertexValenceTable const * vertexValences,
			QuadOffsetTable const * quadOffsets,
			PatchParamTable const * patchParams,
			std::vector<float> const * fvarData,
			int fvarWidth,
			int maxValence) :
			_patchArrays(patchArrays),
			_patches(patches),
			_fvarData(fvarData, fvarWidth),
			_maxValence(maxValence),
			_numPtexFaces(0) {

			// copy other tables if exist
			if (vertexValences)
				_vertexValenceTable = *vertexValences;
			if (quadOffsets)
				_quadOffsetTable = *quadOffsets;
			if (patchParams)
				_paramTable = *patchParams;
		}

		inline bool
			FarPatchTables::IsFeatureAdaptive() const {

			// the vertex valence table is only used by Gregory patches, so the PatchTables
			// contain feature adaptive patches if this is not empty.
			if (not _vertexValenceTable.empty())
				return true;

			PatchArrayVector const & parrays = GetPatchArrayVector();

			// otherwise, we have to check each patch array
			for (int i = 0; i < (int)parrays.size(); ++i) {

				if (parrays[i].GetDescriptor().GetType() >= REGULAR and
					parrays[i].GetDescriptor().GetType() <= GREGORY_BOUNDARY)
					return true;

			}
			return false;
		}

		// Returns the number of control vertices expected for a patch of this type
		inline short
			FarPatchTables::Descriptor::GetNumControlVertices(FarPatchTables::Type type) {
			switch (type) {
			case REGULAR: return FarPatchTables::GetRegularPatchRingsize();
			case QUADS: return 4;
			case GREGORY:
			case GREGORY_BOUNDARY: return FarPatchTables::GetGregoryPatchRingsize();
			case BOUNDARY: return FarPatchTables::GetBoundaryPatchRingsize();
			case CORNER: return FarPatchTables::GetCornerPatchRingsize();
			case TRIANGLES: return 3;
			case LINES: return 2;
			case POINTS: return 1;
			default: return -1;
			}
		}

		// Returns a pointer to the vertex indices of uniformly subdivided faces
		inline unsigned int const *
			FarPatchTables::GetFaceVertices(int level) const {

			if (IsFeatureAdaptive())
				return NULL;

			PatchArrayVector const & parrays = GetPatchArrayVector();

			if (parrays.empty())
				return NULL;

			if (level < 1) {
				return &GetPatchTable()[parrays.rbegin()->GetVertIndex()];
			}
			else if ((level - 1) < (int)parrays.size()) {
				return &GetPatchTable()[parrays[level - 1].GetVertIndex()];
			}

			return NULL;
		}

		// Returns the number of faces in a uniformly subdivided mesh at a given level
		inline int
			FarPatchTables::GetNumFaces(int level) const {

			if (IsFeatureAdaptive())
				return -1;

			PatchArrayVector const & parrays = GetPatchArrayVector();

			if (parrays.empty())
				return -1;

			if (level < 1) {
				return parrays.rbegin()->GetNumPatches();
			}
			else if ((level - 1) < (int)parrays.size()) {
				return parrays[level - 1].GetNumPatches();
			}

			return -1;
		}

		// Allows ordering of patches by type
		inline bool
			FarPatchTables::Descriptor::operator < (Descriptor const other) const {
			return _pattern < other._pattern or((_pattern == other._pattern) and
				(_type < other._type or((_type == other._type) and
				(_rotation < other._rotation))));
		}

		// True if the descriptors are identical
		inline bool
			FarPatchTables::Descriptor::operator == (Descriptor const other) const {
			return     _pattern == other._pattern    and
				_type == other._type       and
				_rotation == other._rotation;
		}

		// Returns a pointer to the array of patches matching the descriptor
		inline FarPatchTables::PatchArray *
			FarPatchTables::findPatchArray(FarPatchTables::Descriptor desc) {

			for (int i = 0; i < (int)_patchArrays.size(); ++i) {
				if (_patchArrays[i].GetDescriptor() == desc)
					return &_patchArrays[i];
			}
			return 0;
		}

		// Returns the total number of patches stored in the tables
		inline int
			FarPatchTables::GetNumPatches() const {
			// there is one PatchParam record for each patch in the mesh
			return (int)GetPatchParamTable().size();
		}

		// Returns the total number of control vertex indices in the tables
		inline int
			FarPatchTables::GetNumControlVertices() const {

			int result = 0;
			for (int i = 0; i < (int)_patchArrays.size(); ++i) {
				result += _patchArrays[i].GetDescriptor().GetNumControlVertices() *
					_patchArrays[i].GetNumPatches();
			}

			return result;
		}

		// Constructor
		inline
			FarPatchTables::FVarData::FVarData(std::vector<float> const * data, int fvarWidth) :
			_fvarWidth(fvarWidth) {

			if (data) {
				_data = *data;
			}
		}

		// Returns a vector of float containing the face-varying data attached
		inline float const *
			FarPatchTables::FVarData::GetData(int level) const {

			if ((level - 1) < (int)_offsets.size()) {

				int offset = 0;

				if ((level > 0) and(not _offsets.empty())) {

					offset = _offsets[level - 1];
				}

				return &_data[offset];
			}

			return NULL;
		}


	} // end namespace OPENSUBDIV_VERSION
	using namespace OPENSUBDIV_VERSION;

} // end namespace OpenSubdiv

#endif /* FAR_PATCH_TABLES */

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

// based on assimp lib spatial sort
#include <vector>
#include <DXUT.h>

// ------------------------------------------------------------------------------------------------
/** A little helper class to quickly find all vertices in the epsilon environment of a given
 * position. Construct an instance with an array of positions. The class stores the given positions
 * by their indices and sorts them by their distance to an arbitrary chosen plane.
 * You can then query the instance for all vertices close to a given position in an average O(log n) 
 * time, with O(n) worst case complexity when all vertices lay on the plane. The plane is chosen
 * so that it avoids common planes in usual data sets. */
// ------------------------------------------------------------------------------------------------
class SpatialSort
{
public:

	SpatialSort();

	// ------------------------------------------------------------------------------------
	/** Constructs a spatially sorted representation from the given position array.
	 * Supply the positions in its layout in memory, the class will only refer to them
	 * by index.
	 * @param pPositions Pointer to the first position vector of the array.
	 * @param pNumPositions Number of vectors to expect in that array.
	 * @param pElementOffset Offset in bytes from the beginning of one vector in memory 
	 *   to the beginning of the next vector. */
	SpatialSort( const DirectX::XMFLOAT4A* pPositions, unsigned int pNumPositions, unsigned int pElementOffset);

	/** Destructor */
	~SpatialSort();

public:

	// ------------------------------------------------------------------------------------
	/** Sets the input data for the SpatialSort. This replaces existing data, if any.
	 *  The new data receives new indices in ascending order.
	 *
	 * @param pPositions Pointer to the first position vector of the array.
	 * @param pNumPositions Number of vectors to expect in that array.
	 * @param pElementOffset Offset in bytes from the beginning of one vector in memory 
	 *   to the beginning of the next vector. 
	 * @param pFinalize Specifies whether the SpatialSort's internal representation
	 *   is finalized after the new data has been added. Finalization is
	 *   required in order to use #FindPosition() or #GenerateMappingTable().
	 *   If you don't finalize yet, you can use #Append() to add data from
	 *   other sources.*/
	void Fill( const DirectX::XMFLOAT4A* pPositions, unsigned int pNumPositions, unsigned int pElementOffset,	bool pFinalize = true);


	// ------------------------------------------------------------------------------------
	/** Same as #Fill(), except the method appends to existing data in the #SpatialSort. */
	void Append( const DirectX::XMFLOAT4A* pPositions, unsigned int pNumPositions, unsigned int pElementOffset, bool pFinalize = true);


	// ------------------------------------------------------------------------------------
	/** Finalize the spatial hash data structure. This can be useful after
	 *  multiple calls to #Append() with the pFinalize parameter set to false.
	 *  This is finally required before one of #FindPositions() and #GenerateMappingTable()
	 *  can be called to query the spatial sort.*/
	void Finalize();

	// ------------------------------------------------------------------------------------
	/** Returns an iterator for all positions close to the given position.
	 * @param pPosition The position to look for vertices.
	 * @param pRadius Maximal distance from the position a vertex may have to be counted in.
	 * @param poResults The container to store the indices of the found positions. 
	 *   Will be emptied by the call so it may contain anything.
	 * @return An iterator to iterate over all vertices in the given area.*/
	void FindPositions( const DirectX::XMFLOAT4A& pPosition, float pRadius, std::vector<unsigned int>& poResults) const;

	// ------------------------------------------------------------------------------------
	/** Fills an array with indices of all positions indentical to the given position. In
	 *  opposite to FindPositions(), not an epsilon is used but a (very low) tolerance of
	 *  four floating-point units.
	 * @param pPosition The position to look for vertices.
	 * @param poResults The container to store the indices of the found positions. 
	 *   Will be emptied by the call so it may contain anything.*/
	void FindIdenticalPositions( const DirectX::XMFLOAT4A& pPosition, std::vector<unsigned int>& poResults) const;

	// ------------------------------------------------------------------------------------
	/** Compute a table that maps each vertex ID referring to a spatially close
	 *  enough position to the same output ID. Output IDs are assigned in ascending order
	 *  from 0...n.
	 * @param fill Will be filled with numPositions entries. 
	 * @param pRadius Maximal distance from the position a vertex may have to
	 *   be counted in.
	 *  @return Number of unique vertices (n).  */
	unsigned int GenerateMappingTable(std::vector<unsigned int>& fill, float pRadius) const;

protected:
	/** Normal of the sorting plane, normalized. The center is always at (0, 0, 0) */
	DirectX::XMVECTOR mPlaneNormal;

	/** An entry in a spatially sorted position array. Consists of a vertex index,
	 * its position and its precalculated distance from the reference plane */
	struct Entry
	{
		unsigned int mIndex; ///< The vertex referred by this entry
		DirectX::XMFLOAT4A mPosition; ///< Position
		float mDistance; ///< Distance of this vertex to the sorting plane

		Entry() { /** intentionally not initialized.*/ }
		Entry( unsigned int pIndex, const DirectX::XMFLOAT4A& pPosition, float pDistance) 
			: mIndex( pIndex), mPosition( pPosition), mDistance( pDistance)
		{ 	}

		bool operator < (const Entry& e) const { return mDistance < e.mDistance; }
	};

	// all positions, sorted by distance to the sorting plane
	std::vector<Entry> mPositions;
};



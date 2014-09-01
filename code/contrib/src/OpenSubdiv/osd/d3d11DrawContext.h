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

#ifndef OSD_D3D11L_DRAW_CONTEXT_H
#define OSD_D3D11L_DRAW_CONTEXT_H

#include "../version.h"

#include "../far/patchTables.h"
#include "../osd/drawContext.h"
#include "../osd/vertex.h"

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
//struct ID3D11DeviceContext1;

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

/// \brief D3D11 specialized DrawContext class
///
/// OsdD3D11DrawContext implements the OSD drawing interface with Microsoft(c)
/// DirectX D3D11 API.
/// Some functionality may be disabled depending on compile and run-time driver
/// support.
///
/// Contexts interface the serialized topological data pertaining to the 
/// geometric primitives with the capabilities of the selected discrete 
/// compute device.
///
class OsdD3D11DrawContext : public OsdDrawContext {
public:
    typedef ID3D11Buffer * VertexBufferBinding;

    virtual ~OsdD3D11DrawContext();

    /// \brief Create an OsdD3D11DrawContext from FarPatchTables
    ///
    /// @param patchTables          a valid set of FarPatchTables
    ///
    /// @param pd3d11DeviceContext  a device context
    ///
    /// @param numVertexElements    the number of vertex elements
    ///
    /// @param requireFVarData      set to true to enable face-varying data to be 
    ///                             carried over from the Far data structures.
    ///
    ///
    static OsdD3D11DrawContext *Create(FarPatchTables const *patchTables,
                                       ID3D11DeviceContext1 *pd3d11DeviceContext,
                                       int numVertexElements,
                                       bool requireFVarData=false);

    /// Set vbo as a vertex texture (for gregory patch drawing)
    ///
    /// @param vbo                  the vertex buffer object to update
    ///
    /// @param pd3d11DeviceContext  a device context
    ///
    template<class VERTEX_BUFFER>
    void UpdateVertexTexture(VERTEX_BUFFER *vbo, ID3D11DeviceContext1 *pd3d11DeviceContext) {
        if (vbo)
            updateVertexTexture(vbo->BindD3D11Buffer(pd3d11DeviceContext),
                                pd3d11DeviceContext,
                                vbo->GetNumVertices(),
                                vbo->GetNumElements());
    }

    ID3D11Buffer             *patchIndexBuffer;
	ID3D11Buffer 			 *patchIndexBufferBUF; // henry: for deformation, culling shaders
	ID3D11ShaderResourceView *patchIndexBufferSRV; // henry: for deformation, culling shaders

    ID3D11Buffer             *ptexCoordinateBuffer;
    ID3D11ShaderResourceView *ptexCoordinateBufferSRV;
    ID3D11Buffer             *fvarDataBuffer;
    ID3D11ShaderResourceView *fvarDataBufferSRV;

    ID3D11ShaderResourceView *vertexBufferSRV;
    ID3D11Buffer             *vertexValenceBuffer;
    ID3D11ShaderResourceView *vertexValenceBufferSRV;
    ID3D11Buffer             *quadOffsetBuffer;
    ID3D11ShaderResourceView *quadOffsetBufferSRV;

private:
    OsdD3D11DrawContext();

    // allocate buffers from patchTables
    bool create(FarPatchTables const *patchTables,
                ID3D11DeviceContext1 *pd3d11DeviceContext,
                int numVertexElements,
                bool requireFVarData);

    void updateVertexTexture(ID3D11Buffer *vbo,
                             ID3D11DeviceContext1 *pd3d11DeviceContext,
                             int numVertices,
                             int numVertexElements);

    int _numVertices;
};

} // end namespace OPENSUBDIV_VERSION
using namespace OPENSUBDIV_VERSION;

} // end namespace OpenSubdiv

#endif /* OSD_D3D11L_DRAW_CONTEXT_H */

//
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

#ifndef OSD_D3D11_PTEX_TEXTURE_H
#define OSD_D3D11_PTEX_TEXTURE_H

#include "../version.h"

#include "../osd/nonCopyable.h"

class PtexTexture;
struct ID3D11Buffer;
struct ID3D11Texture2D;
struct ID3D11DeviceContext1;

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

/// OsdD3D11PtexTexture : implements simple support for ptex textures
///
/// The current implementation declares _texels as a TEXTURE2D_ARRAY of
/// n pages of a resolution that matches that of the largest face in the PTex file.
///
/// Two TEXTURE_BUFFER constructs are used
/// as lookup tables :
/// * _pages stores the array index in which a given face is located
/// * _layout stores 4 float coordinates : top-left corner and width/height for each face
///
/// GLSL fragments use SV_PrimitiveID and SV_DomainLocation to access the _pages and _layout
/// indirection tables, which provide then texture coordinates for the texels stored in
/// the _texels texture array.
///
/// Hbr provides per-face support for a ptex face indexing scheme. OsdD3D11DrawContext
/// class provides ptex face index lookup table as a texture buffer object that
/// can be accessed by HLSL shaders.
///
class OsdD3D11PtexTexture : OsdNonCopyable<OsdD3D11PtexTexture> {
public:
    static OsdD3D11PtexTexture * Create(ID3D11DeviceContext1 *deviceContext,
                                     PtexTexture * reader,
                                     unsigned long int targetMemory = 0,
                                     int gutterWidth = 0,
                                     int pageMargin = 0);

    /// Returns the texture buffer containing the lookup table associate each ptex
    /// face index with its 3D texture page in the texels texture array.
    ID3D11Buffer *GetPagesTextureBuffer() const { return _pages; }

    /// Returns the texture buffer containing the layout of the ptex faces in the
    /// texels texture array.
    ID3D11Buffer *GetLayoutTextureBuffer() const { return _layout; }

    /// Returns the texels texture array.
    ID3D11Texture2D *GetTexelsTexture() const { return _texels; }

    ~OsdD3D11PtexTexture();

private:
    OsdD3D11PtexTexture();

    int _width,   // widht / height / depth of the 3D texel buffer
        _height,
        _depth;

    int _format;  // texel color format

    ID3D11Buffer *_pages,    // per-face page indices into the texel array
                 *_layout;   // per-face lookup table
                             // (vec4 : top-left corner & width / height)
    ID3D11Texture2D *_texels;   // texel data
};

}  // end namespace OPENSUBDIV_VERSION
using namespace OPENSUBDIV_VERSION;

}  // end namespace OpenSubdiv

#endif  // OSD_D3D11_PTEX_TEXTURE_H
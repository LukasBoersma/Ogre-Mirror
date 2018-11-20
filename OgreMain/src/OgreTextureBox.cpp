/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2017 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/

#include "OgreStableHeaders.h"
#include "OgreTextureBox.h"

namespace Ogre
{
    ColourValue TextureBox::getColourAt( size_t x, size_t y, size_t z,
                                         PixelFormatGpu pixelFormat ) const
    {
        OGRE_ASSERT_HIGH( (!isCompressed() &&
                           PixelFormatGpuUtils::getBytesPerPixel( pixelFormat ) == bytesPerPixel) ||
                          (isCompressed() && getCompressedPixelFormat() == pixelFormat) );

        ColourValue retVal;
        const void *data = atFromOffsettedOrigin( x, y, z );
        PixelFormatGpuUtils::unpackColour( &retVal, pixelFormat, data );
        return retVal;
    }
    //-------------------------------------------------------------------------
    void TextureBox::setColourAt( const ColourValue &cv, size_t x, size_t y, size_t z,
                                  PixelFormatGpu pixelFormat )
    {
        OGRE_ASSERT_HIGH( (!isCompressed() &&
                           PixelFormatGpuUtils::getBytesPerPixel( pixelFormat ) == bytesPerPixel) ||
                          (isCompressed() && getCompressedPixelFormat() == pixelFormat) );

        void *data = atFromOffsettedOrigin( x, y, z );
        PixelFormatGpuUtils::packColour( cv, pixelFormat, data );
    }
}

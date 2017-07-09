/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org

Copyright (c) 2000-2014 Torus Knot Software Ltd

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

#ifndef _OgreGL3PlusAsyncTextureTicket_H_
#define _OgreGL3PlusAsyncTextureTicket_H_

#include "OgreGL3PlusPrerequisites.h"
#include "OgreAsyncTextureTicket.h"
#include "OgreTextureBox.h"

namespace Ogre
{
    /** See AsyncTextureTicket
    */
    class _OgreGL3PlusExport GL3PlusAsyncTextureTicket : public AsyncTextureTicket
    {
    protected:
        GLuint      mVboName;
        /// In case GL_ARB_get_texture_sub_image / GL 4.5 is not available and
        /// user requested to only download a subregion of the texture.
        GLuint      mTmpVboName;
        TextureBox  mSubregion;

        uint32      mDownloadFrame;
        GLsync      mAccurateFence;
        GL3PlusVaoManager   *mVaoManager;
        bool                mSupportsGetTextureSubImage;

        GLuint createBuffer( uint32 width, uint32 height, uint32 depthOrSlices );

        virtual TextureBox mapImpl(void);
        virtual void unmapImpl(void);

        void waitForDownloadToFinish(void);

    public:
        GL3PlusAsyncTextureTicket( uint32 width, uint32 height, uint32 depthOrSlices,
                                   TextureTypes::TextureTypes textureType,
                                   PixelFormatGpu pixelFormatFamily,
                                   GL3PlusVaoManager *vaoManager,
                                   bool supportsGetTextureSubImage );
        virtual ~GL3PlusAsyncTextureTicket();

        virtual void download( TextureGpu *textureSrc, uint8 mipLevel,
                               bool accurateTracking, TextureBox *srcBox=0 );

        virtual bool queryIsTransferDone(void);
    };
}

#endif

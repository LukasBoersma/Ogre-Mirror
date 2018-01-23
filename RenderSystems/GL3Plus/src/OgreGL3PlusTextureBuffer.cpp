/*
  -----------------------------------------------------------------------------
  This source file is part of OGRE
  (Object-oriented Graphics Rendering Engine)
  For the latest info, see http://www.ogre3d.org/

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

#include "OgreGL3PlusRenderSystem.h"
#include "OgreRoot.h"

#include "OgreTextureManager.h"

#include "OgreGL3PlusHardwareBufferManager.h"
#include "OgreGL3PlusHardwarePixelBuffer.h"
#include "OgreGL3PlusTextureBuffer.h"
#include "OgreGL3PlusPixelFormat.h"
#include "OgreGL3PlusFBORenderTexture.h"
#include "OgreGL3PlusStateCacheManager.h"

#include "OgreGLSLMonolithicProgram.h"
#include "OgreGLSLProgramManager.h"
#include "OgreGLSLSeparableProgram.h"

#include "OgreGL3PlusTexture.h"

namespace Ogre {

    GL3PlusTextureBuffer::GL3PlusTextureBuffer(GL3PlusTexture* parent,
                                               GLint face, GLint level, uint32 width, uint32 height,
                                               uint32 depth)
        : GL3PlusHardwarePixelBuffer(width, height, depth, parent->getFormat(), (Usage)parent->getUsage()),
          mTarget(parent->getGL3PlusTextureTarget()), mTextureID(parent->getGLID()), mFace(face), mLevel(level), mSliceTRT(0)
    {
        // Get face identifier
        mFaceTarget = mTarget;
        if (mTarget == GL_TEXTURE_CUBE_MAP)
            mFaceTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;

        // Get format
        mGLInternalFormat = GL3PlusPixelUtil::getGLInternalFormat(mFormat, parent->isHardwareGammaEnabled());

        // Default
        mRowPitch = mWidth;
        mSlicePitch = mHeight*mWidth;
        mSizeInBytes = PixelUtil::getMemorySize(mWidth, mHeight, mDepth, mFormat);

        // Log a message
        //        std::stringstream str;
        //        str << "GL3PlusHardwarePixelBuffer constructed for texture: " << mTextureID
        //            << " bytes: " << mSizeInBytes
        //            << " face: " << mFace << " level: " << mLevel
        //            << " width: " << mWidth << " height: "<< mHeight << " depth: " << mDepth
        //            << " format: " << PixelUtil::getFormatName(mFormat)
        //            << "(internal 0x" << std::hex << value << ")";
        //        LogManager::getSingleton().logMessage(LML_NORMAL, str.str());

        // Set up a pixel box
        mBuffer = PixelBox(mWidth, mHeight, mDepth, mFormat);

        if (mWidth==0 || mHeight==0 || mDepth==0)
            // We are invalid, do not allocate a buffer
            return;

        // Is this a render target?
        if (mUsage & TU_RENDERTARGET)
        {
            // Create render target for each slice
            mSliceTRT.reserve(mDepth);
            for(uint32 zoffset=0; zoffset<mDepth; ++zoffset)
            {
                String name;
                name = "rtt/" + StringConverter::toString((size_t)this) + "/" + parent->getName();
                GLSurfaceDesc surface;
                surface.buffer = this;
                surface.zoffset = zoffset;
                RenderTexture* trt = GL3PlusRTTManager::getSingleton().createRenderTexture(
                    name, surface, parent->isHardwareGammaEnabled(), parent->getFSAA());
                mSliceTRT.push_back(trt);
                Root::getSingleton().getRenderSystem()->attachRenderTarget(*mSliceTRT[zoffset]);
            }
        }
    }


    GL3PlusTextureBuffer::~GL3PlusTextureBuffer()
    {
        if (mUsage & TU_RENDERTARGET)
        {
            // Delete all render targets that are not yet deleted via _clearSliceRTT because the rendertarget
            // was deleted by the user.
            for (SliceTRT::const_iterator it = mSliceTRT.begin(); it != mSliceTRT.end(); ++it)
            {
                Root::getSingleton().getRenderSystem()->destroyRenderTarget((*it)->getName());
            }
        }
    }


    void GL3PlusTextureBuffer::upload(const PixelBox &data, const Box &dest)
    {
        mRenderSystem->_getStateCacheManager()->bindGLTexture( mTarget, mTextureID );

        // PBOs have no advantage with this usage pattern
        // see: https://www.khronos.org/opengl/wiki/Pixel_Buffer_Object
#ifdef USE_PBO
        // Calculate size for all mip levels of the texture.
        size_t dataSize = data.getConsecutiveSize();
        GL3PlusHardwareBuffer buffer(GL_PIXEL_UNPACK_BUFFER, dataSize, mUsage);
        buffer.writeData(0, dataSize, data.data, false);

        PixelBox tmp(data.getWidth(), data.getHeight(), data.getHeight(), data.format);
        tmp.data = buffer.lockImpl(0, dataSize, HardwareBuffer::HBL_DISCARD);
        PixelUtil::bulkPixelConversion(data, tmp);
        buffer.unlockImpl(dataSize);

        // std::stringstream str;
        // str << "GL3PlusHardwarePixelBuffer::upload: " << mTextureID
        // << " pixel buffer: " << buffer.getGLBufferId()
        // << " bytes: " << mSizeInBytes
        // << " dest depth: " << dest.getDepth()
        // << " dest front: " << dest.front
        // << " datasize: " << dataSize
        // << " face: " << mFace << " level: " << mLevel
        // << " width: " << mWidth << " height: "<< mHeight << " depth: " << mDepth
        // << " format: " << PixelUtil::getFormatName(mFormat);
        // LogManager::getSingleton().logMessage(LML_NORMAL, str.str());

        void* pdata = NULL;
#else
        void* pdata = data.getTopLeftFrontPixelPtr();
#endif

        if (PixelUtil::isCompressed(data.format))
        {
            if (data.format != mFormat || !data.isConsecutive())
                OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                            "Compressed images must be consecutive and in the designated source format",
                            "GL3PlusTextureBuffer::upload");

            GLenum format = GL3PlusPixelUtil::getGLInternalFormat(mFormat);
            // Data must be consecutive and at beginning of buffer as
            // PixelStorei not allowed for compressed formats.
            switch(mTarget)
            {
            case GL_TEXTURE_1D:
                // Some systems (e.g. old Apple) don't like compressed
                // subimage calls so prefer non-sub versions.
                OGRE_CHECK_GL_ERROR(glCompressedTexSubImage1D(
                    GL_TEXTURE_1D, mLevel,
                    dest.left,
                    dest.getWidth(),
                    format, data.getConsecutiveSize(),
                    pdata));
                break;
            case GL_TEXTURE_2D:
            case GL_TEXTURE_CUBE_MAP:
            case GL_TEXTURE_RECTANGLE:
                OGRE_CHECK_GL_ERROR(glCompressedTexSubImage2D(
                    mFaceTarget, mLevel,
                    dest.left, dest.top,
                    dest.getWidth(), dest.getHeight(),
                    format, data.getConsecutiveSize(),
                    pdata));
                break;
            case GL_TEXTURE_3D:
            case GL_TEXTURE_2D_ARRAY:
                OGRE_CHECK_GL_ERROR(glCompressedTexSubImage3D(
                    mTarget, mLevel,
                    dest.left, dest.top, dest.front,
                    dest.getWidth(), dest.getHeight(), dest.getDepth(),
                    format, data.getConsecutiveSize(),
                    pdata));
                break;
            }

        }
        else
        {
#ifndef USE_PBO
            if (data.getWidth() != data.rowPitch)
                OGRE_CHECK_GL_ERROR(glPixelStorei(GL_UNPACK_ROW_LENGTH, data.rowPitch));
            if (data.getHeight() * data.getWidth() != data.slicePitch)
                OGRE_CHECK_GL_ERROR(glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, (data.slicePitch/data.getWidth())));
#endif
            if ((data.getWidth()*PixelUtil::getNumElemBytes(data.format)) & 3) {
                // Standard alignment of 4 is not right.
                OGRE_CHECK_GL_ERROR(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
            }

            GLenum type = GL3PlusPixelUtil::getGLOriginDataType(data.format);

            if (data.format == PF_DEPTH)
            {
                switch (GL3PlusPixelUtil::getGLInternalFormat(data.format))
                {
                    case GL_DEPTH_COMPONENT16:
                        type = GL_UNSIGNED_SHORT;
                        break;

                    default:
                    case GL_DEPTH_COMPONENT24:
                    case GL_DEPTH_COMPONENT32:
                        type = GL_UNSIGNED_INT;
                        break;
                }
            }

            switch(mTarget)
            {
            case GL_TEXTURE_1D:
                OGRE_CHECK_GL_ERROR(glTexSubImage1D(
                    GL_TEXTURE_1D, mLevel,
                    dest.left,
                    dest.getWidth(),
                    GL3PlusPixelUtil::getGLOriginFormat(data.format),
                    type,
                    pdata));
                break;
            case GL_TEXTURE_2D:
            case GL_TEXTURE_CUBE_MAP:
            case GL_TEXTURE_RECTANGLE:
                OGRE_CHECK_GL_ERROR(glTexSubImage2D(
                    mFaceTarget, mLevel,
                    dest.left, dest.top,
                    dest.getWidth(), dest.getHeight(),
                    GL3PlusPixelUtil::getGLOriginFormat(data.format),
                    type,
                    pdata));
                break;
            case GL_TEXTURE_3D:
            case GL_TEXTURE_2D_ARRAY:
                OGRE_CHECK_GL_ERROR(glTexSubImage3D(
                    mTarget, mLevel,
                    dest.left, dest.top, dest.front,
                    dest.getWidth(), dest.getHeight(), dest.getDepth(),
                    GL3PlusPixelUtil::getGLOriginFormat(data.format),
                    type,
                    pdata));
                break;
            }
        }

        // TU_AUTOMIPMAP is only enabled when there are no custom mips
        // so we do not have to care about overwriting
        if ((mUsage & TU_AUTOMIPMAP) && (mLevel == 0))
        {
            OGRE_CHECK_GL_ERROR(glGenerateMipmap(mTarget));
        }

        // Restore defaults.
        OGRE_CHECK_GL_ERROR(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
        OGRE_CHECK_GL_ERROR(glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0));
        OGRE_CHECK_GL_ERROR(glPixelStorei(GL_UNPACK_ALIGNMENT, 4));
    }


    void GL3PlusTextureBuffer::download(const PixelBox &data)
    {
        if (data.getWidth() != getWidth() ||
            data.getHeight() != getHeight() ||
            data.getDepth() != getDepth())
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "only download of entire buffer is supported by GL",
                        "GL3PlusTextureBuffer::download");

        // Download data to PBO
        GL3PlusHardwareBuffer buffer(GL_PIXEL_PACK_BUFFER, mSizeInBytes, mUsage);

        //        std::stringstream str;
        //        str << "GL3PlusHardwarePixelBuffer::download: " << mTextureID
        //        << " pixel buffer: " << mBufferId
        //        << " bytes: " << mSizeInBytes
        //        << " face: " << mFace << " level: " << mLevel
        //        << " width: " << mWidth << " height: "<< mHeight << " depth: " << mDepth
        //        << " format: " << PixelUtil::getFormatName(mFormat);
        //        LogManager::getSingleton().logMessage(LML_NORMAL, str.str());

        mRenderSystem->_getStateCacheManager()->bindGLTexture(mTarget, mTextureID);

        if (PixelUtil::isCompressed(data.format))
        {
            if (data.format != mFormat || !data.isConsecutive())
                OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                            "Compressed images must be consecutive, in the source format",
                            "GL3PlusTextureBuffer::download");
            // Data must be consecutive and at beginning of buffer as PixelStorei not allowed
            // for compressed formate
            OGRE_CHECK_GL_ERROR(glGetCompressedTexImage(mFaceTarget, mLevel, 0));
        }
        else
        {
            if (data.getWidth() != data.rowPitch)
                OGRE_CHECK_GL_ERROR(glPixelStorei(GL_PACK_ROW_LENGTH, data.rowPitch));
            if (data.getHeight()*data.getWidth() != data.slicePitch)
                OGRE_CHECK_GL_ERROR(glPixelStorei(GL_PACK_IMAGE_HEIGHT, (data.slicePitch/data.getWidth())));
            if ((data.getWidth()*PixelUtil::getNumElemBytes(data.format)) & 3) {
                // Standard alignment of 4 is not right
                OGRE_CHECK_GL_ERROR(glPixelStorei(GL_PACK_ALIGNMENT, 1));
            }
            // We can only get the entire texture
            OGRE_CHECK_GL_ERROR(glGetTexImage(mFaceTarget, mLevel,
                                              GL3PlusPixelUtil::getGLOriginFormat(data.format),
                                              GL3PlusPixelUtil::getGLOriginDataType(data.format),
                                              0));

            // Restore defaults
            OGRE_CHECK_GL_ERROR(glPixelStorei(GL_PACK_ROW_LENGTH, 0));
            OGRE_CHECK_GL_ERROR(glPixelStorei(GL_PACK_IMAGE_HEIGHT, 0));
            OGRE_CHECK_GL_ERROR(glPixelStorei(GL_PACK_ALIGNMENT, 4));
        }

        GLint offsetInBytes = 0;
        uint32 width = mWidth;
        uint32 height = mHeight;
        uint32 depth = mDepth;
        for(GLint i = 0; i < mLevel; i++)
        {
            offsetInBytes += PixelUtil::getMemorySize(width, height, depth, data.format);

            if (width > 1)
                width = width / 2;
            if (height > 1)
                height = height / 2;
            if (depth > 1)
                depth = depth / 2;
        }

        // Copy to destination buffer
        buffer.readData(offsetInBytes, mSizeInBytes, data.getTopLeftFrontPixelPtr());
    }


    void GL3PlusTextureBuffer::bindToFramebuffer(uint32 attachment, uint32 zoffset)
    {
        // Delegate the framebuffer binding to a more specific function
        // This call retains the original implementation using GL_FRAMEBUFFER (aka GL_DRAW_FRAMEBUFFER)
        _bindToFramebuffer(attachment, zoffset, GL_DRAW_FRAMEBUFFER);
    }


    void GL3PlusTextureBuffer::copyFromFramebuffer(uint32 zoffset)
    {
        mRenderSystem->_getStateCacheManager()->bindGLTexture(mTarget, mTextureID);
        switch(mTarget)
        {
        case GL_TEXTURE_1D:
            OGRE_CHECK_GL_ERROR(glCopyTexSubImage1D(mFaceTarget, mLevel, 0, 0, 0, mWidth));
            break;
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP:
        case GL_TEXTURE_RECTANGLE:
            OGRE_CHECK_GL_ERROR(glCopyTexSubImage2D(mFaceTarget, mLevel, 0, 0, 0, 0, mWidth, mHeight));
            break;
        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
            OGRE_CHECK_GL_ERROR(glCopyTexSubImage3D(mFaceTarget, mLevel, 0, 0, zoffset, 0, 0, mWidth, mHeight));
            break;
        }
    }


    void GL3PlusTextureBuffer::blit(const HardwarePixelBufferSharedPtr &src, const Box &srcBox, const Box &dstBox)
    {
        GL3PlusTextureBuffer *srct = static_cast<GL3PlusTextureBuffer *>(src.get());
        // Check for FBO support first
        // Destination texture must be 1D, 2D, 3D, or Cube
        // Source texture must be 1D, 2D or 3D

        // This does not seem to work for RTTs after the first update
        // I have no idea why! For the moment, disable
        if ((src->getUsage() & TU_RENDERTARGET) == 0 &&
            (srct->mTarget == GL_TEXTURE_1D || srct->mTarget == GL_TEXTURE_2D
             || srct->mTarget == GL_TEXTURE_RECTANGLE || srct->mTarget == GL_TEXTURE_3D)
            && mTarget != GL_TEXTURE_2D_ARRAY)
        {
            blitFromTexture(srct, srcBox, dstBox);
        }
        else
        {
            GL3PlusHardwarePixelBuffer::blit(src, srcBox, dstBox);
        }
    }


    // Very fast texture-to-texture blitter and hardware bi/trilinear scaling implementation using FBO
    // Destination texture must be 1D, 2D, 3D, or Cube
    // Source texture must be 1D, 2D or 3D
    // Supports compressed formats as both source and destination format, it will use the hardware DXT compressor
    // if available.
    // @author W.J. van der Laan
    void GL3PlusTextureBuffer::blitFromTexture(GL3PlusTextureBuffer *src, const Box &srcBox, const Box &dstBox)
    {
        //        std::cerr << "GL3PlusTextureBuffer::blitFromTexture " <<
        //        src->mTextureID << ":" << srcBox.left << "," << srcBox.top << "," << srcBox.right << "," << srcBox.bottom << " " <<
        //        mTextureID << ":" << dstBox.left << "," << dstBox.top << "," << dstBox.right << "," << dstBox.bottom << std::endl;
        // Store reference to FBO manager
        GL3PlusFBOManager *fboMan = static_cast<GL3PlusFBOManager *>(GL3PlusRTTManager::getSingletonPtr());

        GLenum filtering = GL_LINEAR;

        // Set filtering modes depending on the dimensions and source
        if (srcBox.getWidth()==dstBox.getWidth() &&
            srcBox.getHeight()==dstBox.getHeight() &&
            srcBox.getDepth()==dstBox.getDepth())
        {
            // Dimensions match -- use nearest filtering (fastest and pixel correct)
            filtering = GL_NEAREST;
        }

        // Store old binding so it can be restored later
        GLint oldfb;
        OGRE_CHECK_GL_ERROR(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldfb));

        // Set up temporary FBO
        GLuint tempFBO[2] = { 0, 0 };
        OGRE_CHECK_GL_ERROR(glGenFramebuffers(1, &tempFBO[0]));
        OGRE_CHECK_GL_ERROR(glGenFramebuffers(1, &tempFBO[1]));
        mRenderSystem->_getStateCacheManager()->bindGLFrameBuffer( GL_DRAW_FRAMEBUFFER, tempFBO[0] );
        mRenderSystem->_getStateCacheManager()->bindGLFrameBuffer( GL_READ_FRAMEBUFFER, tempFBO[1] );

        TexturePtr tempTex;
        if (!fboMan->checkFormat(mFormat))
        {
            // If target format not directly supported, create intermediate texture
            tempTex = TextureManager::getSingleton().createManual(
                "GLBlitFromTextureTMP", ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME, TEX_TYPE_2D,
                dstBox.getWidth(), dstBox.getHeight(), dstBox.getDepth(), 0,
                fboMan->getSupportedAlternative(mFormat));

            OGRE_CHECK_GL_ERROR(
                glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     static_pointer_cast<GL3PlusTexture>(tempTex)->getGLID(), 0));

            OGRE_CHECK_GL_ERROR(glCheckFramebufferStatus(GL_FRAMEBUFFER));

            // Set viewport to size of destination slice
            mRenderSystem->_getStateCacheManager()->setViewport(0, 0, dstBox.getWidth(), dstBox.getHeight());
        }
        else
        {
            // We are going to bind directly, so set viewport to size and position of destination slice
            mRenderSystem->_getStateCacheManager()->setViewport(dstBox.left, dstBox.top, dstBox.getWidth(), dstBox.getHeight());
        }

        // Process each destination slice
        for(uint32 slice = dstBox.front; slice < dstBox.back; ++slice)
        {
            if (!tempTex)
            {
                // Bind directly
                if (mFormat == PF_DEPTH)
                    bindToFramebuffer(GL_DEPTH_ATTACHMENT, slice);
                else
                    bindToFramebuffer(GL_COLOR_ATTACHMENT0, slice);
            }

            OGRE_CHECK_GL_ERROR(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));

            GLbitfield mask = GL_ZERO;

            // Bind the appropriate source texture to the read framebuffer
            if (mFormat == PF_DEPTH)
            {
                src->_bindToFramebuffer(GL_DEPTH_ATTACHMENT, slice, GL_READ_FRAMEBUFFER);

                OGRE_CHECK_GL_ERROR(glReadBuffer(GL_NONE));

                mask |= GL_DEPTH_BUFFER_BIT;

                // Depth framebuffer sources can only be blit with nearest filtering
                filtering = GL_NEAREST;
            }
            else
            {
                src->_bindToFramebuffer(GL_COLOR_ATTACHMENT0, slice, GL_READ_FRAMEBUFFER);

                OGRE_CHECK_GL_ERROR(glReadBuffer(GL_COLOR_ATTACHMENT0));

                mask |= GL_COLOR_BUFFER_BIT;
            }

            OGRE_CHECK_GL_ERROR(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER));

            assert(mask != GL_ZERO);

            // Perform blit from the source texture bound to read framebuffer to
            // this texture bound to draw framebuffer using the pixel coorinates.
            // Sampling ouside the source box is implicitly handled using GL_CLAMP_TO_EDGE.
            OGRE_CHECK_GL_ERROR(glBlitFramebuffer(srcBox.left, srcBox.top, srcBox.right, srcBox.bottom,
                                                  dstBox.left, dstBox.top, dstBox.right, dstBox.bottom,
                                                  mask, filtering));
        }

        // Finish up
        if (!tempTex)
        {
            // Generate mipmaps
            if (mUsage & TU_AUTOMIPMAP)
            {
                mRenderSystem->_getStateCacheManager()->bindGLTexture( mTarget, mTextureID );
                OGRE_CHECK_GL_ERROR(glGenerateMipmap(mTarget));
            }
        }

        // Reset source texture to sane state
        mRenderSystem->_getStateCacheManager()->bindGLTexture( src->mTarget, src->mTextureID );

        if (mFormat == PF_DEPTH)
        {
            OGRE_CHECK_GL_ERROR(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                                          GL_RENDERBUFFER, 0));
        }
        else
        {
            OGRE_CHECK_GL_ERROR(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                          GL_RENDERBUFFER, 0));
        }

        // Reset read buffer/framebuffer
        OGRE_CHECK_GL_ERROR(glReadBuffer(GL_NONE));
        mRenderSystem->_getStateCacheManager()->bindGLFrameBuffer( GL_READ_FRAMEBUFFER, 0 );

        // Restore old framebuffer
        mRenderSystem->_getStateCacheManager()->bindGLFrameBuffer( GL_DRAW_FRAMEBUFFER, oldfb);
        
        mRenderSystem->_getStateCacheManager()->deleteGLFrameBuffer(GL_FRAMEBUFFER, tempFBO[0]);
        mRenderSystem->_getStateCacheManager()->deleteGLFrameBuffer(GL_FRAMEBUFFER, tempFBO[1]);
        
        if(tempTex)
            TextureManager::getSingleton().remove(tempTex);
    }

    void GL3PlusTextureBuffer::_bindToFramebuffer(GLenum attachment, uint32 zoffset, GLenum which)
    {
        assert(zoffset < mDepth);
        assert(which == GL_READ_FRAMEBUFFER || which == GL_DRAW_FRAMEBUFFER || which == GL_FRAMEBUFFER);

        mRenderSystem->_getStateCacheManager()->bindGLTexture( mTarget, mTextureID );
        switch(mTarget)
        {
        case GL_TEXTURE_1D:
        case GL_TEXTURE_2D:
        case GL_TEXTURE_RECTANGLE:
            OGRE_CHECK_GL_ERROR(glFramebufferTexture(which, attachment,
                                                     mTextureID, mLevel));
            break;
        case GL_TEXTURE_CUBE_MAP:
            OGRE_CHECK_GL_ERROR(glFramebufferTexture2D(which, GL_COLOR_ATTACHMENT0,
                                                       mFaceTarget, mTextureID, mLevel));
            break;
        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
            OGRE_CHECK_GL_ERROR(glFramebufferTexture3D(which, attachment,
                                                       mFaceTarget, mTextureID, mLevel, zoffset));
            break;
        }
    }


    // blitFromMemory doing hardware bilinear scaling
    void GL3PlusTextureBuffer::blitFromMemory(const PixelBox &src, const Box &dstBox)
    {
        // Fall back to normal GLHardwarePixelBuffer::blitFromMemory in case
        // the source dimensions match the destination ones, in which case no scaling is needed
        if (src.getWidth() == dstBox.getWidth() && src.getHeight() == dstBox.getHeight() &&
            src.getDepth() == dstBox.getDepth())
        {
            GL3PlusHardwarePixelBuffer::blitFromMemory(src, dstBox);
            return;
        }
        if (!mBuffer.contains(dstBox))
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "Destination box out of range",
                        "GL3PlusTextureBuffer::blitFromMemory");

        TextureType type = (src.getDepth() != 1) ? TEX_TYPE_3D : TEX_TYPE_2D;

        // no mipmaps. blitFromTexture does not use them
        TexturePtr tex = TextureManager::getSingleton().createManual(
            "GLBlitFromMemoryTMP", ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME, type,
            src.getWidth(), src.getHeight(), src.getDepth(), 0, src.format);

        // Upload data to 0,0,0 in temporary texture
        Box tempTarget(0, 0, 0, src.getWidth(), src.getHeight(), src.getDepth());
        tex->getBuffer()->blitFromMemory(src, tempTarget);

        // Blit from texture
        blit(tex->getBuffer(), tempTarget, dstBox);

        // Delete temp texture
        TextureManager::getSingleton().remove(tex);
    }


    RenderTexture *GL3PlusTextureBuffer::getRenderTarget(size_t zoffset)
    {
        assert(mUsage & TU_RENDERTARGET);
        assert(zoffset < mDepth);
        return mSliceTRT[zoffset];
    }

}

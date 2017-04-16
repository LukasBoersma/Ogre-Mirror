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

#include "OgreGL3PlusTextureGpu.h"
#include "OgreGL3PlusMappings.h"

#include "OgreVector2.h"

#include "OgreException.h"

namespace Ogre
{
    GL3PlusTextureGpu::GL3PlusTextureGpu( GpuPageOutStrategy::GpuPageOutStrategy pageOutStrategy,
                                          VaoManager *vaoManager, IdString name, uint32 textureFlags ) :
        TextureGpu( pageOutStrategy, vaoManager, name, textureFlags ),
        mTextureName( 0 ),
        mGlTextureTarget( GL_NONE )
    {
    }
    //-----------------------------------------------------------------------------------
    GL3PlusTextureGpu::~GL3PlusTextureGpu()
    {
        destroyInternalResourcesImpl();
    }
    //-----------------------------------------------------------------------------------
    void GL3PlusTextureGpu::createInternalResourcesImpl(void)
    {
        OCGE( glGenTextures( 1u, &mTextureName ) );

        mGlTextureTarget = GL3PlusMappings::get( mTextureType );

        if( mMsaa > 1u )
        {
            assert( mTextureType == TextureTypes::Type2D || mTextureType == TextureTypes::Type2DArray );
            mGlTextureTarget = mTextureType == TextureTypes::Type2D ? GL_TEXTURE_2D_MULTISAMPLE :
                                                                      GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
        }

        OCGE( glBindTexture( mGlTextureTarget, mTextureName ) );
        OCGE( glTexParameteri( mGlTextureTarget, GL_TEXTURE_BASE_LEVEL, 0 ) );
        OCGE( glTexParameteri( mGlTextureTarget, GL_TEXTURE_MAX_LEVEL, 0 ) );
        OCGE( glTexParameteri( mGlTextureTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST ) );
        OCGE( glTexParameteri( mGlTextureTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST ) );
        OCGE( glTexParameteri( mGlTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE ) );
        OCGE( glTexParameteri( mGlTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE ) );
        OCGE( glTexParameteri( mGlTextureTarget, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE ) );
        OCGE( glTexParameteri( mGlTextureTarget, GL_TEXTURE_MAX_LEVEL, mNumMipmaps - 1u ) );

        GLenum format = GL3PlusMappings::get( mPixelFormat );

        if( mMsaa <= 1u )
        {
            switch( mTextureType )
            {
            case TextureTypes::Unknown:
                OGRE_EXCEPT( Exception::ERR_INVALID_STATE, "Ogre should never hit this path",
                             "GL3PlusTextureGpu::createInternalResourcesImpl" );
                break;
            case TextureTypes::Type1D:
                OCGE( glTexStorage1D( GL_TEXTURE_1D, GLsizei(mNumMipmaps), format, GLsizei(mWidth) ) );
                break;
            case TextureTypes::Type1DArray:
                OCGE( glTexStorage2D( GL_TEXTURE_1D_ARRAY, GLsizei(mNumMipmaps), format,
                                      GLsizei(mWidth), GLsizei(mDepthOrSlices) ) );
                break;
            case TextureTypes::Type2D:
                OCGE( glTexStorage2D( GL_TEXTURE_2D, GLsizei(mNumMipmaps), format,
                                      GLsizei(mWidth), GLsizei(mHeight) ) );
                break;
            case TextureTypes::Type2DArray:
                OCGE( glTexStorage3D( GL_TEXTURE_2D_ARRAY, GLsizei(mNumMipmaps), format,
                                      GLsizei(mWidth), GLsizei(mHeight), GLsizei(mDepthOrSlices) ) );
                break;
            case TextureTypes::TypeCube:
                OCGE( glTexStorage2D( GL_TEXTURE_CUBE_MAP, GLsizei(mNumMipmaps), format,
                                      GLsizei(mWidth), GLsizei(mHeight) ) );
                break;
            case TextureTypes::TypeCubeArray:
                OCGE( glTexStorage3D( GL_TEXTURE_CUBE_MAP_ARRAY, GLsizei(mNumMipmaps), format,
                                      GLsizei(mWidth), GLsizei(mHeight), GLsizei(mDepthOrSlices) ) );
                break;
            case TextureTypes::Type3D:
                OCGE( glTexStorage3D( GL_TEXTURE_3D, GLsizei(mNumMipmaps), format,
                                      GLsizei(mWidth), GLsizei(mHeight), GLsizei(mDepthOrSlices) ) );
                break;
            }
        }
        else
        {
            const GLboolean fixedsamplelocations = mMsaaPattern != MsaaPatterns::Undefined;

            if( mTextureType == TextureTypes::Type2D )
            {
                glTexImage2DMultisample( mGlTextureTarget, mMsaa, format,
                                         GLsizei(mWidth), GLsizei(mHeight), fixedsamplelocations );
            }
            else
            {
                glTexImage3DMultisample( mGlTextureTarget, mMsaa, format,
                                         GLsizei(mWidth), GLsizei(mHeight), GLsizei(mDepthOrSlices),
                                         fixedsamplelocations );
            }
        }

        //Allocate internal buffers for automipmaps before we load anything into them
        if( hasAutomaticBatching() )
            OCGE( glGenerateMipmap( mGlTextureTarget ) );
    }
    //-----------------------------------------------------------------------------------
    void GL3PlusTextureGpu::destroyInternalResourcesImpl(void)
    {
        if( mTextureName )
        {
            glDeleteTextures( 1, &mTextureName );
            mTextureName = 0;
        }
    }
    //-----------------------------------------------------------------------------------
    void GL3PlusTextureGpu::getSubsampleLocations( vector<Vector2>::type locations )
    {
        locations.reserve( mMsaa );
        if( mMsaa <= 1u )
        {
            locations.push_back( Vector2( 0.0f, 0.0f ) );
        }
        else
        {
            assert( mMsaaPattern != MsaaPatterns::Undefined );

            float vals[2];
            for( int i=0; i<mMsaa; ++i )
            {
                glGetMultisamplefv( GL_SAMPLE_POSITION, i, vals );
                locations.push_back( Vector2( vals[0], vals[1] ) * 2.0f - 1.0f );
            }
        }
    }
}

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

#include "OgreMetalRenderSystem.h"
#include "OgreMetalRenderWindow.h"
#include "OgreMetalTextureManager.h"
#include "Vao/OgreMetalVaoManager.h"

#include "OgreDefaultHardwareBufferManager.h"

#include "OgreMetalMappings.h"

#import <Metal/Metal.h>

namespace Ogre
{
    //-------------------------------------------------------------------------
    MetalRenderSystem::MetalRenderSystem() :
        RenderSystem(),
        mInitialized( false ),
        mHardwareBufferManager( 0 )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::shutdown(void)
    {
        OGRE_DELETE mHardwareBufferManager;
        mHardwareBufferManager = 0;

        OGRE_DELETE mTextureManager;
        mTextureManager = 0;

        {
            vector<RenderTarget*>::type::const_iterator itor = mRenderTargets.begin();
            vector<RenderTarget*>::type::const_iterator end  = mRenderTargets.end();

            while( itor != end )
            {
                OGRE_DELETE *itor;
                ++itor;
            }

            mRenderTargets.clear();
        }
    }
    //-------------------------------------------------------------------------
    const String& MetalRenderSystem::getName(void) const
    {
        static String strName("Metal Rendering Subsystem");
        return strName;
    }
    //-------------------------------------------------------------------------
    const String& MetalRenderSystem::getFriendlyName(void) const
    {
        static String strName("Metal_RS");
        return strName;
    }
    //-------------------------------------------------------------------------
    HardwareOcclusionQuery* MetalRenderSystem::createHardwareOcclusionQuery(void)
    {
        return 0; //TODO
    }
    //-------------------------------------------------------------------------
    RenderSystemCapabilities* MetalRenderSystem::createRenderSystemCapabilities(void) const
    {
        RenderSystemCapabilities* rsc = new RenderSystemCapabilities();
        rsc->setRenderSystemName(getName());

        rsc->setCapability(RSC_HWSTENCIL);
        rsc->setStencilBufferBitDepth(8);
        rsc->setNumTextureUnits(16);
        rsc->setCapability(RSC_ANISOTROPY);
        rsc->setCapability(RSC_AUTOMIPMAP);
        rsc->setCapability(RSC_BLENDING);
        rsc->setCapability(RSC_DOT3);
        rsc->setCapability(RSC_CUBEMAPPING);
        rsc->setCapability(RSC_TEXTURE_COMPRESSION);
        rsc->setCapability(RSC_TEXTURE_COMPRESSION_DXT);
        rsc->setCapability(RSC_VBO);
        rsc->setCapability(RSC_TWO_SIDED_STENCIL);
        rsc->setCapability(RSC_STENCIL_WRAP);
        rsc->setCapability(RSC_USER_CLIP_PLANES);
        rsc->setCapability(RSC_VERTEX_FORMAT_UBYTE4);
        rsc->setCapability(RSC_INFINITE_FAR_PLANE);
        rsc->setCapability(RSC_TEXTURE_3D);
        rsc->setCapability(RSC_NON_POWER_OF_2_TEXTURES);
        rsc->setNonPOW2TexturesLimited(false);
        rsc->setCapability(RSC_HWRENDER_TO_TEXTURE);
        rsc->setCapability(RSC_TEXTURE_FLOAT);
        rsc->setCapability(RSC_POINT_SPRITES);
        rsc->setCapability(RSC_POINT_EXTENDED_PARAMETERS);
        rsc->setMaxPointSize(256);

        rsc->setMaximumResolutions( 16384, 4096, 16384 );

        return rsc;
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::reinitialise(void)
    {
        this->shutdown();
        this->_initialise(true);
    }
    //-------------------------------------------------------------------------
    RenderWindow* MetalRenderSystem::_initialise( bool autoCreateWindow, const String& windowTitle )
    {
        RenderWindow *autoWindow = 0;
        if( autoCreateWindow )
            autoWindow = _createRenderWindow( windowTitle, 1, 1, false );
        RenderSystem::_initialise(autoCreateWindow, windowTitle);

        return autoWindow;
    }
    //-------------------------------------------------------------------------
    RenderWindow* MetalRenderSystem::_createRenderWindow( const String &name,
                                                         unsigned int width, unsigned int height,
                                                         bool fullScreen,
                                                         const NameValuePairList *miscParams )
    {
        RenderWindow *win = OGRE_NEW MetalRenderWindow();

        if( !mInitialized )
        {
            mRealCapabilities = createRenderSystemCapabilities();
            mCurrentCapabilities = mRealCapabilities;

            mHardwareBufferManager = new v1::DefaultHardwareBufferManager();
            mTextureManager = new MetalTextureManager();
            mVaoManager = OGRE_NEW MetalVaoManager();

            mInitialized = true;
        }

        return win;
    }
    //-------------------------------------------------------------------------
    MultiRenderTarget* MetalRenderSystem::createMultiRenderTarget(const String & name)
    {
        return 0;
    }
    //-------------------------------------------------------------------------
    String MetalRenderSystem::getErrorDescription(long errorNumber) const
    {
        return BLANKSTRING;
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_useLights(const LightList& lights, unsigned short limit)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setWorldMatrix(const Matrix4 &m)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setViewMatrix(const Matrix4 &m)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setProjectionMatrix(const Matrix4 &m)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setSurfaceParams( const ColourValue &ambient,
                            const ColourValue &diffuse, const ColourValue &specular,
                            const ColourValue &emissive, Real shininess,
                            TrackVertexColourType tracking )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setPointSpritesEnabled(bool enabled)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setPointParameters(Real size, bool attenuationEnabled,
        Real constant, Real linear, Real quadratic, Real minSize, Real maxSize)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::queueBindUAV( uint32 slot, TexturePtr texture,
                                         ResourceAccess::ResourceAccess access,
                                         int32 mipmapLevel, int32 textureArrayIndex,
                                         PixelFormat pixelFormat )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::queueBindUAV( uint32 slot, UavBufferPacked *buffer,
                                         ResourceAccess::ResourceAccess access,
                                         size_t offset, size_t sizeBytes )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::clearUAVs(void)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::flushUAVs(void)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_bindTextureUavCS( uint32 slot, Texture *texture,
                                              ResourceAccess::ResourceAccess access,
                                              int32 mipmapLevel, int32 textureArrayIndex,
                                              PixelFormat pixelFormat )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setTextureCS( uint32 slot, bool enabled, Texture *texPtr )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setHlmsSamplerblockCS( uint8 texUnit, const HlmsSamplerblock *samplerblock )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setTexture(size_t unit, bool enabled,  Texture *texPtr)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setTextureCoordSet(size_t unit, size_t index)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setTextureCoordCalculation( size_t unit, TexCoordCalcMethod m,
                                                        const Frustum* frustum )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setTextureBlendMode(size_t unit, const LayerBlendModeEx& bm)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setTextureMatrix(size_t unit, const Matrix4& xform)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setIndirectBuffer( IndirectBufferPacked *indirectBuffer )
    {
    }
    //-------------------------------------------------------------------------
    DepthBuffer* MetalRenderSystem::_createDepthBufferFor( RenderTarget *renderTarget,
                                                          bool exactMatchFormat )
    {
        return 0;
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_beginFrame(void)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_endFrame(void)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setViewport(Viewport *vp)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_hlmsPipelineStateObjectCreated( HlmsPso *newPso )
    {
//        MTLRenderPipelineDescriptor *psd = [[MTLRenderPipelineDescriptor alloc] init];
//        [psd setSampleCount: newPso->pass.multisampleCount];
////		[psd setVertexFunction:vsShader];
////		[psd setFragmentFunction:psShader];

//        //TODO: Ogre should track the number of RTTs instead of assuming
//        //OGRE_MAX_MULTIPLE_RENDER_TARGETS (will NOT work on Metal!)
//        for( int i=0; i<OGRE_MAX_MULTIPLE_RENDER_TARGETS; ++i )
//        {
//            HlmsBlendblock const *blendblock = newPso->blendblock;
//            psd.colorAttachments[i].pixelFormat = MetalMappings::getPixelFormat( newPso->pass.colourFormat[i] );

//            if( blendblock->mBlendOperation == SBO_ADD &&
//                blendblock->mSourceBlendFactor == SBF_ONE &&
//                blendblock->mDestBlendFactor == SBF_ZERO &&
//                (!blendblock->mSeparateBlend ||
//                 blendblock->mBlendOperation == blendblock->mBlendOperationAlpha &&
//                 blendblock->mSourceBlendFactor == blendblock->mSourceBlendFactorAlpha &&
//                 blendblock->mDestBlendFactor == blendblock->mDestBlendFactorAlpha) )
//            {
//                //Note: Can NOT use blendblock->mIsTransparent. What Ogre understand
//                //as transparent differs from what Metal understands.
//                psd.colorAttachments[i].blendingEnabled = NO;
//            }
//            else
//            {
//                psd.colorAttachments[i].blendingEnabled = YES;
//            }
//            psd.colorAttachments[i].rgbBlendOperation           = MetalMappings::get( blendblock->mBlendOperation );
//            psd.colorAttachments[i].alphaBlendOperation         = MetalMappings::get( blendblock->mBlendOperationAlpha );
//            psd.colorAttachments[i].sourceRGBBlendFactor        = MetalMappings::get( blendblock->mSourceBlendFactor );
//            psd.colorAttachments[i].destinationRGBBlendFactor   = MetalMappings::get( blendblock->mDestBlendFactor );
//            psd.colorAttachments[i].sourceAlphaBlendFactor      = MetalMappings::get( blendblock->mSourceBlendFactorAlpha );
//            psd.colorAttachments[i].destinationAlphaBlendFactor = MetalMappings::get( blendblock->mDestBlendFactorAlpha );

//            psd.colorAttachments[i].writeMask = MetalMappings::get( blendblock->mBlendChannelMask );
//        }

//        psd.depthAttachmentPixelFormat = MetalMappings::get( newPso->pass.depthFormat );

//        NSError* error = NULL;
//        id <MTLRenderPipelineState> pso =
//                [mDevice newRenderPipelineStateWithDescriptor:psd error:&error];

//        //TODO: Ogre should throw.
//        if( !pso ) {
//            NSLog(@"Failed to created pipeline state, error %@", error);
//        }

//        MTLDepthStencilDescriptor *depthStateDesc = [[MTLDepthStencilDescriptor alloc] init];
//        if( newPso->macroblock->mDepthCheck )
//        {
//            depthStateDesc.depthCompareFunction = MetalMappings::get( newPso->macroblock->mDepthFunc );
//            depthStateDesc.depthWriteEnabled    = newPso->macroblock->mDepthWrite;
//        }
//        else
//        {
//            depthStateDesc.depthCompareFunction = MTLCompareFunctionAlways;
//            depthStateDesc.depthWriteEnabled    = NO;
//        }
//        if( newPso->pass.stencilParams.enabled )
//        {
//            newPso->pass.stencilParams.readMask;
//            newPso->pass.stencilParams.writeMask;
//            depthStateDesc.frontFaceStencil =;
//            depthStateDesc.backFaceStencil =;
//        }
//        id <MTLDepthStencilState> depthStencilState =
//                [mDevice newDepthStencilStateWithDescriptor:depthStateDesc];
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_hlmsPipelineStateObjectDestroyed( HlmsPso *pso )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_hlmsSamplerblockCreated( HlmsSamplerblock *newPso )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_hlmsSamplerblockDestroyed( HlmsSamplerblock *pso )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setHlmsSamplerblock( uint8 texUnit, const HlmsSamplerblock *Samplerblock )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setPipelineStateObject( const HlmsPso *pso )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setComputePso( const HlmsComputePso *pso )
    {
    }
    //-------------------------------------------------------------------------
    VertexElementType MetalRenderSystem::getColourVertexElementType(void) const
    {
        return VET_COLOUR_ARGB;
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_dispatch( const HlmsComputePso &pso )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setVertexArrayObject( const VertexArrayObject *vao )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_render( const CbDrawCallIndexed *cmd )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_render( const CbDrawCallStrip *cmd )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_renderEmulated( const CbDrawCallIndexed *cmd )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_renderEmulated( const CbDrawCallStrip *cmd )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setRenderOperation( const v1::CbRenderOp *cmd )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_render( const v1::CbDrawCallIndexed *cmd )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_render( const v1::CbDrawCallStrip *cmd )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::bindGpuProgramParameters(GpuProgramType gptype,
        GpuProgramParametersSharedPtr params, uint16 variabilityMask)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::bindGpuProgramPassIterationParameters(GpuProgramType gptype)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::clearFrameBuffer( unsigned int buffers, const ColourValue& colour,
                                             Real depth, unsigned short stencil )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::discardFrameBuffer( unsigned int buffers )
    {
    }
    //-------------------------------------------------------------------------
    Real MetalRenderSystem::getHorizontalTexelOffset(void)
    {
        return 0.0f;
    }
    //-------------------------------------------------------------------------
    Real MetalRenderSystem::getVerticalTexelOffset(void)
    {
        return 0.0f;
    }
    //-------------------------------------------------------------------------
    Real MetalRenderSystem::getMinimumDepthInputValue(void)
    {
        return 0.0f;
    }
    //-------------------------------------------------------------------------
    Real MetalRenderSystem::getMaximumDepthInputValue(void)
    {
        return 1.0f;
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::_setRenderTarget(RenderTarget *target, bool colourWrite)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::preExtraThreadsStarted()
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::postExtraThreadsStarted()
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::registerThread()
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::unregisterThread()
    {
    }
    //-------------------------------------------------------------------------
    const PixelFormatToShaderType* MetalRenderSystem::getPixelFormatToShaderType(void) const
    {
        return &mPixelFormatToShaderType;
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::beginProfileEvent( const String &eventName )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::endProfileEvent( void )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::markProfileEvent( const String &event )
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::setClipPlanesImpl(const PlaneList& clipPlanes)
    {
    }
    //-------------------------------------------------------------------------
    void MetalRenderSystem::initialiseFromRenderSystemCapabilities(RenderSystemCapabilities* caps, RenderTarget* primary)
    {
    }
}

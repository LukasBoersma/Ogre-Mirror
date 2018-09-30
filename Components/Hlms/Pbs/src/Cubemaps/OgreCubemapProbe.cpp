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

#include "OgreStableHeaders.h"

#include "Cubemaps/OgreCubemapProbe.h"
#include "Cubemaps/OgreParallaxCorrectedCubemap.h"

#include "OgreTextureGpuManager.h"
#include "OgrePixelFormatGpuUtils.h"
#include "OgreLogManager.h"
#include "OgreLwString.h"
#include "OgreId.h"

#include "Compositor/OgreCompositorManager2.h"
#include "Compositor/OgreCompositorWorkspace.h"
#include "OgreSceneManager.h"

#include "OgreInternalCubemapProbe.h"

#include "Vao/OgreConstBufferPacked.h"

//Disable as OpenGL version of copyToTexture is super slow (makes a GPU->CPU->GPU roundtrip)
#define USE_RTT_DIRECTLY 1

namespace Ogre
{
    CubemapProbe::CubemapProbe( ParallaxCorrectedCubemap *creator ) :
        mProbeCameraPos( Vector3::ZERO ),
        mArea( Aabb::BOX_NULL ),
        mAreaInnerRegion( Vector3::ZERO ),
        mOrientation( Matrix3::IDENTITY ),
        mInvOrientation( Matrix3::IDENTITY ),
        mProbeShape( Aabb::BOX_NULL ),
        mTexture( 0 ),
        mCubemapArrayIdx( 0u ),
        mMsaa( 1u ),
        mClearWorkspace( 0 ),
        mWorkspace( 0 ),
        mCamera( 0 ),
        mCreator( creator ),
        mInternalProbe( 0 ),
        mConstBufferForManualProbes( 0 ),
        mNumDatablockUsers( 0 ),
        mStatic( true ),
        mEnabled( true ),
        mDirty( true ),
        mNumIterations( 8 ),
        mMask( 0xffffffff )
    {
    }
    //-----------------------------------------------------------------------------------
    CubemapProbe::~CubemapProbe()
    {
        destroyWorkspace();
        destroyTexture();

        assert( !mNumDatablockUsers &&
                "There's still datablocks using this probe! Pointers will become dangling!" );
        if( mConstBufferForManualProbes )
        {
            SceneManager *sceneManager = mCreator->getSceneManager();
            VaoManager *vaoManager = sceneManager->getDestinationRenderSystem()->getVaoManager();
            vaoManager->destroyConstBuffer( mConstBufferForManualProbes );
            mConstBufferForManualProbes = 0;
            mCreator->_removeManuallyActiveProbe( this );
        }
    }
    //-----------------------------------------------------------------------------------
    void CubemapProbe::destroyWorkspace(void)
    {
        if( mWorkspace )
        {
#if !USE_RTT_DIRECTLY
            if( mStatic )
            {
                TextureGpu *channel = mWorkspace->getExternalRenderTargets()[0];
                mCreator->releaseTmpRtt( channel.textures[0] );
            }
#endif

            CompositorManager2 *compositorManager = mWorkspace->getCompositorManager();
            compositorManager->removeWorkspace( mWorkspace );
            mWorkspace = 0;
        }

        if( mClearWorkspace )
        {
            CompositorManager2 *compositorManager = mClearWorkspace->getCompositorManager();
            compositorManager->removeWorkspace( mClearWorkspace );
            mClearWorkspace = 0;
        }

        if( mTexture && mTexture->getResidencyStatus() != GpuResidency::OnStorage )
            mTexture->_transitionTo( GpuResidency::OnStorage, (uint8*)0 );

        if( mCamera )
        {
            SceneManager *sceneManager = mCamera->getSceneManager();
            sceneManager->destroyCamera( mCamera );
            mCamera = 0;
        }
    }
    //-----------------------------------------------------------------------------------
    void CubemapProbe::destroyTexture(void)
    {
        assert( !mWorkspace );
        if( mTexture )
        {
            SceneManager *sceneManager = mCreator->getSceneManager();
            TextureGpuManager *textureManager =
                    sceneManager->getDestinationRenderSystem()->getTextureGpuManager();
            textureManager->destroyTexture( mTexture );
            mTexture = 0;
        }
    }
    //-----------------------------------------------------------------------------------
    void CubemapProbe::createInternalProbe(void)
    {
        destroyInternalProbe();
        if( !mCreator->getAutomaticMode() )
            return;

        SceneManager *sceneManager = mCreator->getSceneManager();
        const SceneMemoryMgrTypes sceneType = mStatic ? SCENE_STATIC : SCENE_DYNAMIC;
        mInternalProbe = sceneManager->_createCubemapProbe( sceneType );

        SceneNode *sceneNode =
                sceneManager->getRootSceneNode( sceneType )->createChildSceneNode( sceneType );
        sceneNode->attachObject( mInternalProbe );
    }
    //-----------------------------------------------------------------------------------
    void CubemapProbe::destroyInternalProbe(void)
    {
        if( !mInternalProbe )
            return;

        SceneNode *sceneNode = mInternalProbe->getParentSceneNode();
        sceneNode->getParentSceneNode()->removeAndDestroyChild( sceneNode );
        mCreator->getSceneManager()->_destroyCubemapProbe( mInternalProbe );
        mInternalProbe = 0;
    }
    //-----------------------------------------------------------------------------------
    void CubemapProbe::switchInternalProbeStaticValue(void)
    {
        if( mInternalProbe && mInternalProbe->isStatic() != mStatic )
        {
            SceneNode *sceneNode = mInternalProbe->getParentSceneNode();
            sceneNode->getParent()->removeChild( sceneNode );

            sceneNode->setStatic( mStatic );

            SceneManager *sceneManager = mCreator->getSceneManager();
            SceneNode *rootNode = sceneManager->getRootSceneNode( mStatic ? SCENE_STATIC :
                                                                            SCENE_DYNAMIC );
            rootNode->addChild( sceneNode );
        }
    }
    //-----------------------------------------------------------------------------------
    void CubemapProbe::syncInternalProbe(void)
    {
        if( !mInternalProbe )
            return;
        Quaternion qRot( mOrientation );
        SceneNode *sceneNode = mInternalProbe->getParentSceneNode();
        sceneNode->setPosition( mArea.mCenter );
        sceneNode->setScale( mArea.mHalfSize );
        sceneNode->setOrientation( qRot );

        mCreator->fillConstBufferData( *this, Matrix4::IDENTITY, Matrix3::IDENTITY,
                                       reinterpret_cast<float*>( mInternalProbe->mGpuData ) );
        mInternalProbe->mGpuData[3][3] = static_cast<float>( mCubemapArrayIdx );
    }
    //-----------------------------------------------------------------------------------
    void CubemapProbe::setTextureParams( uint32 width, uint32 height, bool useManual,
                                         PixelFormatGpu pf, bool isStatic, uint8 msaa )
    {
        float cameraNear = 0.5;
        float cameraFar = 1000;

        if( mCamera )
        {
            cameraNear = mCamera->getNearClipDistance();
            cameraFar = mCamera->getFarClipDistance();
        }

        const bool reinitWorkspace = isInitialized();
        destroyWorkspace();
        destroyTexture();

        char tmpBuffer[64];
        LwString texName( LwString::FromEmptyPointer( tmpBuffer, sizeof(tmpBuffer) ) );
        texName.a( "CubemapProbe_", Id::generateNewId<CubemapProbe>() );

#if !USE_RTT_DIRECTLY
        const uint32 flags = isStatic ? TU_STATIC_WRITE_ONLY : (TU_RENDERTARGET|TU_AUTOMIPMAP);
#else
    #if GENERATE_MIPMAPS_ON_BLEND
        uint32 flags = TextureFlags::RenderToTexture;
    #else
        const uint32 flags = TextureFlags::RenderToTexture|TextureFlags::AllowAutomipmaps;
    #endif
#endif

#if GENERATE_MIPMAPS_ON_BLEND
        uint numMips = 1u;
        if( useManual )
        {
            numMips = PixelFormatGpuUtils::getMaxMipmapCount( width, height );
            flags |= TextureFlags::AllowAutomipmaps;
        }
#else
        const uint numMips = PixelUtil::getMaxMipmapCount( width, height, 1 );
#endif

        mMsaa = msaa;
        msaa = isStatic ? 0 : msaa;

        SceneManager *sceneManager = mCreator->getSceneManager();
        TextureGpuManager *textureManager =
                sceneManager->getDestinationRenderSystem()->getTextureGpuManager();
        mTexture = textureManager->createTexture(texName.c_str(), GpuPageOutStrategy::Discard,
                                                flags, TextureTypes::TypeCube );
        mTexture->setResolution( width, height );
        mTexture->setPixelFormat( pf );
        mTexture->setNumMipmaps( numMips );
        mTexture->setMsaa( msaa );
        mStatic = isStatic;
        mDirty = true;

        if( reinitWorkspace )
            initWorkspace( cameraNear, cameraFar, mWorkspaceDefName );
    }
    //-----------------------------------------------------------------------------------
    void CubemapProbe::initWorkspace( float cameraNear, float cameraFar, IdString workspaceDefOverride )
    {
        assert( mTexture != 0 && "Call setTextureParams first!" );

        destroyWorkspace();

        CompositorWorkspaceDef const *workspaceDef = mCreator->getDefaultWorkspaceDef();
        CompositorManager2 *compositorManager = workspaceDef->getCompositorManager();

        if( workspaceDefOverride != IdString() )
            workspaceDef = compositorManager->getWorkspaceDefinition( workspaceDefOverride );

        mWorkspaceDefName = workspaceDef->getName();
        SceneManager *sceneManager = mCreator->getSceneManager();
        mCamera = sceneManager->createCamera( mTexture->getNameStr(), true, true );
        mCamera->setFOVy( Degree(90) );
        mCamera->setAspectRatio( 1 );
        mCamera->setFixedYawAxis(false);
        mCamera->setNearClipDistance( cameraNear );
        mCamera->setFarClipDistance( cameraFar );

        TextureGpu *rtt = mTexture;
        if( mStatic )
        {
#if !USE_RTT_DIRECTLY
            //Grab tmp texture
            rtt = mCreator->findTmpRtt( mTexture );
#endif
            //Set camera to skip light culling (efficiency)
            mCamera->setLightCullingVisibility( false, false );
        }
        else
        {
            mCamera->setLightCullingVisibility( true, true );
        }

        mTexture->_transitionTo( GpuResidency::Resident, (uint8*)0 );

        CompositorChannelVec channels( 1, rtt );
        mWorkspace = compositorManager->addWorkspace( sceneManager, channels, mCamera,
                                                      mWorkspaceDefName, false );

        if( !mStatic )
        {
            mClearWorkspace =
                    compositorManager->addWorkspace( sceneManager, channels,
                                                     mCamera,
                                                     "AutoGen_ParallaxCorrectedCubemapClear_Workspace",
                                                     false );
        }
    }
    //-----------------------------------------------------------------------------------
    bool CubemapProbe::isInitialized(void) const
    {
        return mWorkspace != 0;
    }
    //-----------------------------------------------------------------------------------
    void CubemapProbe::set( const Vector3 &cameraPos, const Aabb &area, const Vector3 &areaInnerRegion,
                            const Matrix3 &orientation, const Aabb &probeShape )
    {
        mProbeCameraPos     = cameraPos;
        mArea               = area;
        mAreaInnerRegion    = areaInnerRegion;
        mOrientation        = orientation;
        mInvOrientation     = mOrientation.Inverse();
        mProbeShape         = probeShape;
        mProbeShape.mHalfSize *= 1.005; //Add some padding.

        mAreaInnerRegion.makeCeil( Vector3::ZERO );
        mAreaInnerRegion.makeFloor( Vector3::UNIT_SCALE );

        Aabb areaLocalToShape = mArea;
        areaLocalToShape.mCenter -= mProbeShape.mCenter;
        areaLocalToShape.mCenter = mInvOrientation * areaLocalToShape.mCenter;
        areaLocalToShape.mCenter += mProbeShape.mCenter;

        if( !mProbeShape.contains( mArea ) )
        {
            LogManager::getSingleton().logMessage(
                        "WARNING: Area must be fully inside probe's shape otherwise "
                        "artifacts appear. Forcing area to be inside probe" );
            Vector3 vMin = mArea.getMinimum() * 0.98f;
            Vector3 vMax = mArea.getMaximum() * 0.98f;

            vMin.makeCeil( mProbeShape.getMinimum() );
            vMax.makeFloor( mProbeShape.getMaximum() );
            mArea.setExtents( vMin, vMax );
        }

        mDirty = true;
    }
    //-----------------------------------------------------------------------------------
    void CubemapProbe::setStatic( bool isStatic )
    {
        if( mStatic != isStatic && mTexture )
        {
            setTextureParams( mTexture->getWidth(), mTexture->getHeight(), mTexture->getNumMipmaps() > 0,
                              mTexture->getPixelFormat(), isStatic, mTexture->getMsaa() );
        }
        else
        {
            //We're not initialized yet, but still save the intention...
            mStatic = isStatic;
        }
    }
    //-----------------------------------------------------------------------------------
    Real CubemapProbe::getNDF( const Vector3 &posLS ) const
    {
        //Work in the upper left corner of the box. (Like Aabb::distance)
        Vector3 dist;
        dist.x = Math::Abs( posLS.x );
        dist.y = Math::Abs( posLS.y );
        dist.z = Math::Abs( posLS.z );

        const Vector3 innerRange = mArea.mHalfSize * mAreaInnerRegion;
        const Vector3 outerRange = mArea.mHalfSize;

        //1e-6f avoids division by zero.
        Vector3 ndf = (dist - innerRange) / (outerRange - innerRange + Real(1e-6f));

        return Ogre::max( Ogre::max( ndf.x, ndf.y ), ndf.z );
    }
    //-----------------------------------------------------------------------------------
    void CubemapProbe::_prepareForRendering(void)
    {
        mCamera->setPosition( mProbeCameraPos );
        mCamera->setOrientation( Quaternion( mOrientation ) );
        if( mStatic )
            mCamera->setLightCullingVisibility( true, true );
    }
    //-----------------------------------------------------------------------------------
    void CubemapProbe::_clearCubemap(void)
    {
        if( !mClearWorkspace )
        {
            CompositorWorkspaceDef const *workspaceDef = mCreator->getDefaultWorkspaceDef();
            CompositorManager2 *compositorManager = workspaceDef->getCompositorManager();

            SceneManager *sceneManager = mCreator->getSceneManager();
            CompositorChannelVec channels( mWorkspace->getExternalRenderTargets() );
            mClearWorkspace =
                    compositorManager->addWorkspace( sceneManager, channels,
                                                     mCamera,
                                                     "AutoGen_ParallaxCorrectedCubemapClear_Workspace",
                                                     false );
        }

        mClearWorkspace->_update();

        if( mStatic )
        {
            CompositorManager2 *compositorManager = mClearWorkspace->getCompositorManager();
            compositorManager->removeWorkspace( mClearWorkspace );
            mClearWorkspace = 0;
        }
    }
    //-----------------------------------------------------------------------------------
    void CubemapProbe::_updateRender(void)
    {
        assert( mDirty || !mStatic );
        mWorkspace->_update();

        if( mStatic )
        {
#if !USE_RTT_DIRECTLY
            //Copy from tmp RTT to real texture.
            TextureGpu *channel = mWorkspace->getExternalRenderTargets()[0];
            channel.textures[0]->copyToTexture( mTexture );
#endif

            mCamera->setLightCullingVisibility( false, false );
        }
    }
    //-----------------------------------------------------------------------------------
    void CubemapProbe::_addReference(void)
    {
        ++mNumDatablockUsers;

        if( !mConstBufferForManualProbes )
        {
            SceneManager *sceneManager = mCreator->getSceneManager();
            VaoManager *vaoManager = sceneManager->getDestinationRenderSystem()->getVaoManager();
            mConstBufferForManualProbes = vaoManager->createConstBuffer(
                        ParallaxCorrectedCubemap::getConstBufferSize(),
                        BT_DEFAULT, 0, false );
            mCreator->_addManuallyActiveProbe( this );
        }
    }
    //-----------------------------------------------------------------------------------
    void CubemapProbe::_removeReference(void)
    {
        --mNumDatablockUsers;
        if( !mNumDatablockUsers )
        {
            assert( mConstBufferForManualProbes );
            if( mConstBufferForManualProbes )
            {
                SceneManager *sceneManager = mCreator->getSceneManager();
                VaoManager *vaoManager = sceneManager->getDestinationRenderSystem()->getVaoManager();
                vaoManager->destroyConstBuffer( mConstBufferForManualProbes );
                mConstBufferForManualProbes = 0;
                mCreator->_removeManuallyActiveProbe( this );
            }
        }
    }
}

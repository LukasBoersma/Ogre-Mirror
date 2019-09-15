/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-present Torus Knot Software Ltd

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

#include "OgreRadialDensityMask.h"

#include "OgreMaterialManager.h"
#include "OgrePass.h"
#include "OgreRectangle2D2.h"
#include "OgreSceneManager.h"
#include "OgreTechnique.h"

#include "OgreHlmsCompute.h"
#include "OgreHlmsComputeJob.h"
#include "OgreHlmsManager.h"
#include "Vao/OgreConstBufferPacked.h"

namespace Ogre
{
    struct float4
    {
        float x, y, z, w;
        float4() {}
        float4( const Vector4 &val ) : x( val.x ), y( val.y ), z( val.z ), w( val.w ) {}
        float4( const Vector2 &valXY, const Vector2 &valZW ) :
            x( valXY.x ),
            y( valXY.y ),
            z( valZW.x ),
            w( valZW.y )
        {
        }
    };
    struct float2
    {
        float x, y;
        float2() {}
        float2( const Vector2 &val ) : x( val.x ), y( val.y ) {}
    };
    // struct uint4
    //{
    //    uint32 x, y, z, w;
    //    uint4() {}
    //    uint4( const Vector4 &val ) :
    //        x( static_cast<uint32>( val.x ) ),
    //        y( static_cast<uint32>( val.y ) ),
    //        z( static_cast<uint32>( val.z ) ),
    //        w( static_cast<uint32>( val.w ) )
    //    {
    //    }
    //    uint4( const Vector2 &valXY, const Vector2 &valZW ) :
    //        x( static_cast<uint32>( valXY.x ) ),
    //        y( static_cast<uint32>( valXY.y ) ),
    //        z( static_cast<uint32>( valZW.x ) ),
    //        w( static_cast<uint32>( valZW.y ) )
    //    {
    //    }
    //};

    struct RdmShaderParams
    {
        float4 rightEyeStart_radius;
        float4 leftEyeCenter_rightEyeCenter;
        float4 invBlockResolution_invResolution;
    };

    RadialDensityMask::RadialDensityMask( SceneManager *sceneManager, const float radius[3],
                                          HlmsManager *hlmsManager ) :
        mRectangle( 0 ),
        mLeftEyeCenter( Vector2::ZERO ),
        mRightEyeCenter( Vector2::ZERO ),
        mDirty( true ),
        mLastVpWidth( 0 ),
        mLastVpHeight( 0 ),
        mReconstructJob( 0 ),
        mJobParams( 0 )
    {
        memcpy( mRadius, radius, sizeof( mRadius ) );

        mRectangle =
            OGRE_NEW Rectangle2D( Id::generateNewId<MovableObject>(),
                                  &sceneManager->_getEntityMemoryManager( SCENE_STATIC ), sceneManager );

        mRectangle->setHollowRectRadius( mRadius[0] );
        mRectangle->setGeometry( mLeftEyeCenter, mRightEyeCenter );
        mRectangle->initialize(
            BT_IMMUTABLE, Rectangle2D::GeometryFlagStereo | Rectangle2D::GeometryFlagHollowFsRect );

        const String baseMatName = "Ogre/VR/RadialDensityMask";
        const String matName = baseMatName + StringConverter::toString( sceneManager->getId() );

        MaterialPtr material = MaterialManager::getSingleton().getByName( matName );
        if( !material )
        {
            material = MaterialManager::getSingleton().getByName( baseMatName );
            if( !material )
            {
                OGRE_EXCEPT( Exception::ERR_FILE_NOT_FOUND,
                             "To use the radial density mask for VR, bundle the resources included in "
                             "Samples/Media/Common",
                             "RadialDensityMask::RadialDensityMask" );
            }

            material->load();
            material = material->clone( baseMatName );
        }

        mRectangle->setMaterial( material );
        sceneManager->getRootSceneNode( SCENE_STATIC )->attachObject( mRectangle );

        Pass *pass = material->getTechnique( 0 )->getPass( 0 );
        mPsParams = pass->getFragmentProgramParameters();

        GpuProgramParametersSharedPtr vsParams = pass->getVertexProgramParameters();
        vsParams->setNamedConstant( "ogreBaseVertex", (float)mRectangle->getVaos( VpNormal )
                                                          .back()
                                                          ->getBaseVertexBuffer()
                                                          ->_getFinalBufferStart() );

        HlmsCompute *hlmsCompute = hlmsManager->getComputeHlms();
        mReconstructJob = hlmsCompute->findComputeJobNoThrow( "VR/RadialDensityMaskReconstruct" );
        if( !mReconstructJob )
        {
#if OGRE_NO_JSON
            OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS,
                         "To use RadialDensityMask, Ogre must be build with JSON support "
                         "and you must include the resources bundled at "
                         "Samples/Media/Compute/VR",
                         "RadialDensityMask::RadialDensityMask" );
#endif
            OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS,
                         "To use RadialDensityMask, you must include the resources bundled at "
                         "Samples/Media/Compute/VR\n"
                         "Could not find VCT/Voxelizer",
                         "RadialDensityMask::RadialDensityMask" );
        }

        VaoManager *vaoManager = sceneManager->getDestinationRenderSystem()->getVaoManager();
        mJobParams = vaoManager->createConstBuffer(
            alignToNextMultiple( sizeof( RdmShaderParams ), 16u ), BT_DEFAULT, 0, false );
        mReconstructJob->setConstBuffer( 0, mJobParams );
    }
    //-------------------------------------------------------------------------
    RadialDensityMask::~RadialDensityMask()
    {
        VaoManager *vaoManager =
            mRectangle->_getManager()->getDestinationRenderSystem()->getVaoManager();
        vaoManager->destroyConstBuffer( mJobParams );
        mJobParams = 0;

        MaterialPtr material = mRectangle->getMaterial();
        MaterialManager::getSingleton().remove( material );

        OGRE_DELETE mRectangle;
        mRectangle = 0;
    }
    //-------------------------------------------------------------------------
    void RadialDensityMask::setEyeCenter( Real *outEyeCenter, Vector2 inEyeCenterClipSpace,
                                          const Viewport &vp )
    {
        outEyeCenter[0] = ( inEyeCenterClipSpace.x * 0.5f + 0.5f ) * vp.getWidth() + vp.getLeft();
        outEyeCenter[1] = ( inEyeCenterClipSpace.y * 0.5f + 0.5f ) * vp.getHeight() + vp.getTop();
    }
    //-------------------------------------------------------------------------
    void RadialDensityMask::update( Viewport *viewports )
    {
        const int32 width = viewports[1].getActualLeft() + viewports[1].getActualWidth();
        const int32 height = viewports[1].getActualHeight();

        if( width != mLastVpWidth || height != mLastVpHeight )
        {
            mLastVpWidth = width;
            mLastVpHeight = height;
            mDirty = true;
        }

        if( !mDirty )
            return;

        Vector4 leftEyeCenter_rightEyeCenter;
        setEyeCenter( &leftEyeCenter_rightEyeCenter[0], mLeftEyeCenter, viewports[0] );
        setEyeCenter( &leftEyeCenter_rightEyeCenter[2], mRightEyeCenter, viewports[1] );

        Vector4 rightEyeStart_radius;
        rightEyeStart_radius.x = viewports[1].getActualLeft();
        for( size_t i = 0u; i < 3u; ++i )
            rightEyeStart_radius[i + 1u] = mRadius[i] * 0.5f;

        const Vector2 invBlockResolution( 8.0f / width, 8.0f / height );

        mPsParams->setNamedConstant( "leftEyeCenter_rightEyeCenter", leftEyeCenter_rightEyeCenter );
        mPsParams->setNamedConstant( "rightEyeStart_radius", rightEyeStart_radius );
        mPsParams->setNamedConstant( "invBlockResolution", invBlockResolution );

        RdmShaderParams shaderParams;
        shaderParams.rightEyeStart_radius = rightEyeStart_radius;
        shaderParams.leftEyeCenter_rightEyeCenter = leftEyeCenter_rightEyeCenter;
        shaderParams.invBlockResolution_invResolution =
            float4( invBlockResolution, Vector2( 1.0f / width, 1.0f / height ) );
        mJobParams->upload( &shaderParams, 0, sizeof( shaderParams ) );

        const uint32 threadGroupsX = ( (uint32)width + mReconstructJob->getThreadsPerGroupX() - 1u ) /
                                     mReconstructJob->getThreadsPerGroupX();
        const uint32 threadGroupsY = ( (uint32)height + mReconstructJob->getThreadsPerGroupY() - 1u ) /
                                     mReconstructJob->getThreadsPerGroupY();
        mReconstructJob->setNumThreadGroups( threadGroupsX, threadGroupsY, 1u );
    }
    //-------------------------------------------------------------------------
    void RadialDensityMask::setEyesCenter( const Vector2 &leftEyeCenter, const Vector2 &rightEyeCenter )
    {
        mLeftEyeCenter = leftEyeCenter;
        mRightEyeCenter = rightEyeCenter;
        mDirty = true;
    }
    //-------------------------------------------------------------------------
    void RadialDensityMask::setNewRadius( const float radius[3] )
    {
        const bool bNeedsNewRect = fabsf( mRadius[0] - radius[0] ) > 1e-6f;
        memcpy( mRadius, radius, sizeof( mRadius ) );

        if( bNeedsNewRect )
        {
            MaterialPtr material = mRectangle->getMaterial();
            SceneManager *sceneManager = mRectangle->_getManager();

            OGRE_DELETE mRectangle;
            mRectangle = OGRE_NEW Rectangle2D( Id::generateNewId<MovableObject>(),
                                               &sceneManager->_getEntityMemoryManager( SCENE_STATIC ),
                                               sceneManager );

            mRectangle->setHollowRectRadius( mRadius[0] );
            mRectangle->initialize(
                BT_IMMUTABLE, Rectangle2D::GeometryFlagStereo | Rectangle2D::GeometryFlagHollowFsRect );

            mRectangle->setMaterial( material );
            sceneManager->getRootSceneNode( SCENE_STATIC )->attachObject( mRectangle );

            Pass *pass = material->getTechnique( 0 )->getPass( 0 );
            GpuProgramParametersSharedPtr vsParams = pass->getVertexProgramParameters();
            vsParams->setNamedConstant( "ogreBaseVertex", (float)mRectangle->getVaos( VpNormal )
                                                              .back()
                                                              ->getBaseVertexBuffer()
                                                              ->_getFinalBufferStart() );
        }
        mDirty = true;
    }
}  // namespace Ogre

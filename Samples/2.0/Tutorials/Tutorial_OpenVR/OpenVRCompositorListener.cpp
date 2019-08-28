
#include "OpenVRCompositorListener.h"
#include "OgreTextureGpu.h"
#include "OgreRenderSystem.h"
#include "Compositor/Pass/OgreCompositorPass.h"
#include "Compositor/Pass/OgreCompositorPassDef.h"
#include "Compositor/Pass/PassScene/OgreCompositorPassScene.h"

namespace Demo
{
    OpenVRCompositorListener::OpenVRCompositorListener(
            vr::IVRSystem *hmd, vr::IVRCompositor *vrCompositor,
            Ogre::TextureGpu *vrTexture, Ogre::RenderSystem *renderSystem,
            Ogre::Camera *camera ) :
        mHMD( hmd ),
        mVrCompositor( vrCompositor ),
        mVrTexture( vrTexture ),
        mRenderSystem( renderSystem ),
        mValidPoseCount( 0 ),
        mCamera( camera ),
        mWaitingMode( VrWaitingMode::BeforeSceneGraph ),
        mFirstGlitchFreeMode( VrWaitingMode::NumVrWaitingModes ),
        mLastCamNear( 0 ),
        mLastCamFar( 0 ),
        mMustSyncAtEndOfFrame( false )
    {
        memset( mTrackedDevicePose, 0, sizeof( mTrackedDevicePose ) );
        memset( mDevicePose, 0, sizeof( mDevicePose ) );
        memset( &mVrData, 0, sizeof( mVrData ) );

        mCamera->setVrData( &mVrData );
        syncCameraProjection();
    }
    //-------------------------------------------------------------------------
    OpenVRCompositorListener::~OpenVRCompositorListener()
    {
        if( mCamera )
            mCamera->setVrData( 0 );
    }
    //-------------------------------------------------------------------------
    Ogre::Matrix4 OpenVRCompositorListener::convertSteamVRMatrixToMatrix4( vr::HmdMatrix34_t matPose )
    {
        Ogre::Matrix4 matrixObj(
                    matPose.m[0][0], matPose.m[0][1], matPose.m[0][2], matPose.m[0][3],
                    matPose.m[1][0], matPose.m[1][1], matPose.m[1][2], matPose.m[1][3],
                    matPose.m[2][0], matPose.m[2][1], matPose.m[2][2], matPose.m[2][3],
                               0.0f,            0.0f,            0.0f,            1.0f );
        return matrixObj;
    }
    //-------------------------------------------------------------------------
    Ogre::Matrix4 OpenVRCompositorListener::convertSteamVRMatrixToMatrix4( vr::HmdMatrix44_t matPose )
    {
        Ogre::Matrix4 matrixObj(
                    matPose.m[0][0], matPose.m[0][1], matPose.m[0][2], matPose.m[0][3],
                    matPose.m[1][0], matPose.m[1][1], matPose.m[1][2], matPose.m[1][3],
                    matPose.m[2][0], matPose.m[2][1], matPose.m[2][2], matPose.m[2][3],
                    matPose.m[3][0], matPose.m[3][1], matPose.m[3][2], matPose.m[3][3] );
        return matrixObj;
    }
    //-------------------------------------------------------------------------
    void OpenVRCompositorListener::updateHmdTrackingPose(void)
    {
        mVrCompositor->WaitGetPoses( mTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0 );

        mValidPoseCount = 0;
        for( size_t nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice )
        {
            if ( mTrackedDevicePose[nDevice].bPoseIsValid )
            {
                ++mValidPoseCount;
                mDevicePose[nDevice] = convertSteamVRMatrixToMatrix4(
                                           mTrackedDevicePose[nDevice].mDeviceToAbsoluteTracking );
            }
        }

        if( mTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid )
        {
            const bool canSync = canSyncCameraTransformImmediately();
            if( canSync )
                syncCamera();
            else
                mMustSyncAtEndOfFrame = true;
        }
    }
    //-------------------------------------------------------------------------
    void OpenVRCompositorListener::syncCamera(void)
    {
        OGRE_ASSERT_MEDIUM( mTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid );
        mCamera->setPosition( mDevicePose[vr::k_unTrackedDeviceIndex_Hmd].getTrans() );
        mCamera->setOrientation( mDevicePose[vr::k_unTrackedDeviceIndex_Hmd].extractQuaternion() );
        mMustSyncAtEndOfFrame = false;
    }
    //-------------------------------------------------------------------------
    void OpenVRCompositorListener::syncCameraProjection(void)
    {
        const Ogre::Real camNear = mCamera->getNearClipDistance();
        const Ogre::Real camFar  = mCamera->getFarClipDistance();

        if( mLastCamNear != camNear || mLastCamFar != camFar )
        {
            Ogre::Matrix4 eyeToHead[2] =
            {
                convertSteamVRMatrixToMatrix4( mHMD->GetEyeToHeadTransform( vr::Eye_Left ) ),
                convertSteamVRMatrixToMatrix4( mHMD->GetEyeToHeadTransform( vr::Eye_Right ) )
            };
            Ogre::Matrix4 projectionMatrix[2] =
            {
                convertSteamVRMatrixToMatrix4( mHMD->GetProjectionMatrix( vr::Eye_Left,
                                                                          camNear, camFar ) ),
                convertSteamVRMatrixToMatrix4( mHMD->GetProjectionMatrix( vr::Eye_Right,
                                                                          camNear, camFar ) )
            };

            mVrData.set( eyeToHead, projectionMatrix );
            mLastCamNear = camNear;
            mLastCamFar = camFar;
        }
    }
    //-------------------------------------------------------------------------
    bool OpenVRCompositorListener::frameStarted( const Ogre::FrameEvent& evt )
    {
        if( mWaitingMode == VrWaitingMode::BeforeSceneGraph )
            updateHmdTrackingPose();
        return true;
    }
    //-------------------------------------------------------------------------
    bool OpenVRCompositorListener::frameRenderingQueued( const Ogre::FrameEvent &evt )
    {
        vr::VRTextureBounds_t texBounds;
        texBounds.vMin = 0;
        texBounds.vMax = 1.0f;

        vr::Texture_t eyeTexture =
        {
            0,
            vr::TextureType_OpenGL,
            vr::ColorSpace_Gamma
        };
        mVrTexture->getCustomAttribute( Ogre::TextureGpu::msFinalTextureBuffer, &eyeTexture.handle );

        texBounds.uMin = 0;
        texBounds.uMax = 0.5f;
        mVrCompositor->Submit( vr::Eye_Left, &eyeTexture, &texBounds );
        texBounds.uMin = 0.5f;
        texBounds.uMax = 1.0f;
        mVrCompositor->Submit( vr::Eye_Right, &eyeTexture, &texBounds );

        mRenderSystem->flushCommands();

        return true;
    }
    //-------------------------------------------------------------------------
    bool OpenVRCompositorListener::frameEnded( const Ogre::FrameEvent& evt )
    {
        syncCameraProjection();
        if( mWaitingMode == VrWaitingMode::AfterSwap )
            updateHmdTrackingPose();
        if( mMustSyncAtEndOfFrame )
            syncCamera();
        return true;
    }
    //-------------------------------------------------------------------------
    void OpenVRCompositorListener::workspacePreUpdate( Ogre::CompositorWorkspace *workspace )
    {
        if( mWaitingMode == VrWaitingMode::AfterSceneGraph )
            updateHmdTrackingPose();
    }
    //-------------------------------------------------------------------------
    void OpenVRCompositorListener::passPreExecute( Ogre::CompositorPass *pass )
    {
        if( mWaitingMode == VrWaitingMode::BeforeShadowmaps &&
            pass->getDefinition()->getType() == Ogre::PASS_SCENE &&
            pass->getDefinition()->mIdentifier == 0x01234567 )
        {
            updateHmdTrackingPose();
        }
    }
    //-------------------------------------------------------------------------
    void OpenVRCompositorListener::passSceneAfterShadowMaps( Ogre::CompositorPassScene *pass )
    {
        if( mWaitingMode == VrWaitingMode::BeforeFrustumCulling &&
            pass->getDefinition()->mIdentifier == 0x01234567 )
        {
            updateHmdTrackingPose();
        }
    }
    //-------------------------------------------------------------------------
    void OpenVRCompositorListener::passSceneAfterFrustumCulling( Ogre::CompositorPassScene *pass )
    {
        if( mWaitingMode == VrWaitingMode::AfterFrustumCulling &&
            pass->getDefinition()->mIdentifier == 0x01234567 )
        {
            updateHmdTrackingPose();
        }
    }
    //-------------------------------------------------------------------------
    void OpenVRCompositorListener::setGlitchFree( VrWaitingMode::VrWaitingMode firstGlitchFreeMode )
    {
        mFirstGlitchFreeMode = firstGlitchFreeMode;
    }
    //-------------------------------------------------------------------------
    bool OpenVRCompositorListener::canSyncCameraTransformImmediately(void) const
    {
        return mWaitingMode <= VrWaitingMode::BeforeSceneGraph ||
               mWaitingMode <= mFirstGlitchFreeMode;
    }
}

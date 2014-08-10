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

#include "Vao/OgreVaoManager.h"
#include "Vao/OgreStagingBuffer.h"
#include "OgreTimer.h"
#include "OgreCommon.h"

namespace Ogre
{
    VaoManager::VaoManager() :
        mTimer( 0 ),
        mDefaultStagingBufferLifetime( 300000 ), //5 minutes
        mNextStagingBufferTimestampCheckpoint( ~0 ),
        mDynamicBufferMultiplier( 3 ),
        mDynamicBufferCurrentFrame( 0 )
    {
        mTimer = OGRE_NEW Timer();
    }
    //-----------------------------------------------------------------------------------
    VaoManager::~VaoManager()
    {
        for( size_t i=0; i<2; ++i )
        {
            StagingBufferVec::const_iterator itor = mStagingBuffers[i].begin();
            StagingBufferVec::const_iterator end  = mStagingBuffers[i].end();

            while( itor != end )
            {
                OGRE_DELETE *itor;
                ++itor;
            }
        }

        OGRE_DELETE mTimer;
        mTimer = 0;
    }
    //-----------------------------------------------------------------------------------
    uint32 VaoManager::calculateVertexSize( const VertexElement2Vec &vertexElements )
    {
        VertexElement2Vec::const_iterator itor = vertexElements.begin();
        VertexElement2Vec::const_iterator end  = vertexElements.end();

        uint32 bytesPerVertex = 0;

        while( itor != end )
        {
            bytesPerVertex += VertexElement::getTypeSize( itor->mType );
            ++itor;
        }

        return bytesPerVertex;
    }
    //-----------------------------------------------------------------------------------
    VertexBufferPacked* VaoManager::createVertexBuffer( const VertexElement2Vec &vertexElements,
                                                        size_t numVertices, BufferType bufferType,
                                                        void *initialData, bool keepAsShadow )
    {
        uint32 bytesPerVertex = calculateVertexSize( vertexElements );

        VertexBufferPacked *retVal = createVertexBufferImpl( numVertices, bytesPerVertex, bufferType,
                                                             initialData, keepAsShadow, vertexElements );

        mVertexBuffers.push_back( retVal );
        return retVal;
    }
    //-----------------------------------------------------------------------------------
    void VaoManager::destroyVertexBuffer( VertexBufferPacked *vertexBuffer )
    {
        if( vertexBuffer->getMultiSourcePool() )
        {
            OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS,
                         "Vertex Buffer belongs to a Multisource pool, not this VaoManager",
                         "VaoManager::destroyVertexBuffer" );
        }

        VertexBufferPackedVec::iterator itor = std::find( mVertexBuffers.begin(),
                                                          mVertexBuffers.end(), vertexBuffer );

        if( itor == mVertexBuffers.end() )
        {
            OGRE_EXCEPT( Exception::ERR_INVALID_STATE,
                         "Vertex Buffer has already been destroyed or "
                         "doesn't belong to this VaoManager.",
                         "VaoManager::destroyVertexBuffer" );
        }

        destroyVertexBufferImpl( vertexBuffer );

        OGRE_DELETE vertexBuffer;
        efficientVectorRemove( mVertexBuffers, itor );
    }
    //-----------------------------------------------------------------------------------
    IndexBufferPacked* VaoManager::createIndexBuffer( IndexBufferPacked::IndexType indexType,
                                                      size_t numIndices, BufferType bufferType,
                                                      void *initialData, bool keepAsShadow )
    {
        return createIndexBufferImpl( numIndices, indexType == IndexBufferPacked::IT_16BIT ? 2 : 4,
                                      bufferType, initialData, keepAsShadow );
    }
    //-----------------------------------------------------------------------------------
    void VaoManager::destroyIndexBuffer( IndexBufferPacked *indexBuffer )
    {
        destroyIndexBufferImpl( indexBuffer );
        OGRE_DELETE indexBuffer;
    }
    //-----------------------------------------------------------------------------------
    const VertexArrayObject *VaoManager::createVertexArrayObject(
                                                            const VertexBufferPackedVec &vertexBuffers,
                                                            IndexBufferPacked *indexBuffer,
                                                            RenderOperation::OperationType opType )
    {
        if( vertexBuffers.size() > 1 )
        {
            size_t multiSourceId                            = vertexBuffers[0]->getMultiSourceId();
            MultiSourceVertexBufferPool *multiSourcePool    = vertexBuffers[0]->getMultiSourcePool();

            VertexBufferPackedVec::const_iterator itor = vertexBuffers.begin();
            VertexBufferPackedVec::const_iterator end  = vertexBuffers.end();

            while( itor != end )
            {
                if( !(*itor)->getMultiSourcePool() )
                {
                    OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS,
                                 "Cannot use a non-multisource vertex buffer "
                                 "in a multisource declaration",
                                 "VaoManager::createVertexArrayObject" );
                }

                if( multiSourceId != (*itor)->getMultiSourceId() ||
                    multiSourcePool != (*itor)->getMultiSourcePool() )
                {
                    OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS,
                                 "Multisource vertex buffers can only be bound together with "
                                 "the other buffers returned by the same "
                                 "MultiSourceVertexBufferPool::createVertexBuffer call. "
                                 "Don't mix vertex buffers from different pools or calls.",
                                 "VaoManager::createVertexArrayObject" );
                }

                ++itor;
            }
        }

        return createVertexArrayObjectImpl( vertexBuffers, indexBuffer, opType );
    }
    //-----------------------------------------------------------------------------------
    StagingBuffer* VaoManager::getStagingBuffer( size_t minSizeBytes, bool forUpload )
    {
        StagingBuffer *candidates[NUM_STALL_TYPES];
        memset( candidates, 0, sizeof( candidates ) );

        StagingBufferVec::const_iterator itor = mStagingBuffers[forUpload].begin();
        StagingBufferVec::const_iterator end  = mStagingBuffers[forUpload].end();

        while( itor != end && minSizeBytes < (*itor)->getMaxSize() )
        {
            StagingStallType stallType = (*itor)->willStall( minSizeBytes );
            candidates[stallType] = *itor;

            //This is best case scenario, we can stop looking.
            if( stallType == STALL_NONE )
                break;

            ++itor;
        }

        StagingBuffer *retVal = candidates[STALL_FULL];

        for( size_t i=0; i<NUM_STALL_TYPES && !retVal; ++i )
            retVal = candidates[i];

        if( !retVal )
        {
            //No buffer is large enough. Get a new one.
            retVal = createStagingBuffer( minSizeBytes, forUpload );
        }
        else
        {
            retVal->addReferenceCount();
        }

        return retVal;
    }
    //-----------------------------------------------------------------------------------
    void VaoManager::_notifyStagingBufferEnteredZeroRef( StagingBuffer *stagingBuffer )
    {
        mZeroRefStagingBuffers[stagingBuffer->getUploadOnly()].push_back( stagingBuffer );
    }
    //-----------------------------------------------------------------------------------
    void VaoManager::_notifyStagingBufferLeftZeroRef( StagingBuffer *stagingBuffer )
    {
        StagingBufferVec &zeroRefStagingBuffers = mZeroRefStagingBuffers[stagingBuffer->getUploadOnly()];
        StagingBufferVec::iterator itor = std::find( zeroRefStagingBuffers.begin(),
                                                     zeroRefStagingBuffers.end(), stagingBuffer );

        assert( itor != zeroRefStagingBuffers.end() );
        efficientVectorRemove( zeroRefStagingBuffers, itor );
    }
}


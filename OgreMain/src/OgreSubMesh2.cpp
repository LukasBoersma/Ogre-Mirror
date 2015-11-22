﻿/*
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
#include "OgreSubMesh2.h"
#include "OgreSubMesh.h"

//#include "OgreMesh.h"
#include "OgreMesh2.h"
#include "OgreException.h"
#include "OgreHardwareBufferManager.h"
#include "OgreLogManager.h"

#include "Vao/OgreVaoManager.h"
#include "Vao/OgreAsyncTicket.h"

#include "OgreVertexShadowMapHelper.h"

namespace Ogre {
    //-----------------------------------------------------------------------
    SubMesh::SubMesh() :
        mParent( 0 ),
        mBoneAssignmentsOutOfDate( false )
    {
    }
    //-----------------------------------------------------------------------
    SubMesh::~SubMesh()
    {
        destroyShadowMappingVaos();
        destroyVaos( mVao[0], mParent->mVaoManager );
    }
    //-----------------------------------------------------------------------
    void SubMesh::addBoneAssignment(const VertexBoneAssignment& vertBoneAssign)
    {
        mBoneAssignments.push_back( vertBoneAssign );
        mBoneAssignmentsOutOfDate = true;
    }
    //-----------------------------------------------------------------------
    void SubMesh::clearBoneAssignments(void)
    {
        mBoneAssignments.clear();
        mBoneAssignmentsOutOfDate = true;
    }
    //-----------------------------------------------------------------------
    /*void SubMesh::_compileBoneAssignments(void)
    {
        uint8 maxBones = rationaliseBoneAssignments( vertexData->vertexCount );

        if (maxBones != 0)
        {
            mParent->compileBoneAssignments(mBoneAssignments, maxBones,
                blendIndexToBoneIndexMap, vertexData);
        }

        mBoneAssignmentsOutOfDate = false;
    }
    //---------------------------------------------------------------------
    uint8 SubMesh::rationaliseBoneAssignments( size_t vertexCount )
    {
        // Iterate through, finding the largest # bones per vertex
        uint8 maxBonesPerVertex         = 0;
        bool existsNonSkinnedVertices   = false;

        //Ensure bone assignments are sorted.
        std::sort( mBoneAssignments.begin(), mBoneAssignments.end() );

        VertexBoneAssignmentVec::const_iterator end = mBoneAssignments.end();

        for( size_t i=0; i < vertexCount; ++i )
        {
            uint8 bonesPerVertex = 0;
            VertexBoneAssignmentVec::iterator first = std::lower_bound( mBoneAssignments.begin(),
                                                                        mBoneAssignments.end(), i );
            VertexBoneAssignmentVec::iterator itor = first;

            while( itor != end && itor->vertexIndex == i )
                ++bonesPerVertex;

            maxBonesPerVertex = std::max( maxBonesPerVertex, bonesPerVertex );

            if( itor == end || itor->vertexIndex != i )
            {
                existsNonSkinnedVertices = true;
            }
            else
            {
                if( bonesPerVertex > OGRE_MAX_BLEND_WEIGHTS )
                {
                    //Trim the assignments that exceed limit allowed.
                    mBoneAssignments.erase( first + OGRE_MAX_BLEND_WEIGHTS, itor );
                    bonesPerVertex = OGRE_MAX_BLEND_WEIGHTS;
                }

                //Always normalize the weights.
                Real totalWeight = 0;

                itor = first;
                while( itor != end && (itor - first) < bonesPerVertex )
                    totalWeight += itor->weight;

                totalWeight = 1.0f / totalWeight;
                itor = first;
                while( itor != end && (itor - first) < bonesPerVertex )
                    itor->weight *= totalWeight;
            }
        }

        if( maxBonesPerVertex > OGRE_MAX_BLEND_WEIGHTS )
        {
            // Warn that we've reduced bone assignments
            LogManager::getSingleton().logMessage("WARNING: the mesh '" + mParent->mName + "' "
                "includes vertices with up to " + StringConverter::toString( maxBonesPerVertex ) +
                " bone assignments, which is more than the limit of " +
                StringConverter::toString(OGRE_MAX_BLEND_WEIGHTS) +
                "The lowest weighted assignments beyond this limit have been removed, so "
                "your animation may look slightly different. To eliminate this, reduce "
                "the number of bone assignments per vertex on your mesh to " +
                StringConverter::toString(OGRE_MAX_BLEND_WEIGHTS) + ".", LML_CRITICAL);

            // we've adjusted them down to the max
            maxBonesPerVertex = OGRE_MAX_BLEND_WEIGHTS;
        }

        if( existsNonSkinnedVertices )
        {
            // Warn that we've non-skinned vertices
            LogManager::getSingleton().logMessage("WARNING: the mesh '" + mParent->mName + "' "
                "includes vertices without bone assignments. Those vertices will "
                "transform to wrong position when skeletal animation enabled. "
                "To eliminate this, assign at least one bone assignment per vertex "
                "on your mesh.", LML_CRITICAL);
        }

        return maxBonesPerVertex;
    }
    //---------------------------------------------------------------------
    void SubMesh::buildIndexMap( IndexMap &boneIndexToBlendIndexMap, IndexMap &blendIndexToBoneIndexMap )
    {
        if( mBoneAssignments.empty() )
        {
            // Just in case
            boneIndexToBlendIndexMap.clear();
            blendIndexToBoneIndexMap.clear();
            return;
        }

        typedef set<unsigned short>::type BoneIndexSet;
        BoneIndexSet usedBoneIndices;

        // Collect actually used bones
        VertexBoneAssignmentVec itor = mBoneAssignments.begin();
        VertexBoneAssignmentVec end  = mBoneAssignments.end();
        while( itor != end )
        {
            usedBoneIndices.insert( itor->boneIndex );
            ++itor;
        }

        // Allocate space for index map
        blendIndexToBoneIndexMap.resize( usedBoneIndices.size() );
        boneIndexToBlendIndexMap.resize( *usedBoneIndices.rbegin() + 1 );

        // Make index map between bone index and blend index
        BoneIndexSet::const_iterator itBoneIndex = usedBoneIndices.begin();
        BoneIndexSet::const_iterator enBoneIndex = usedBoneIndices.end();
        unsigned short blendIndex = 0;

        while( itBoneIndex != enBoneIndex )
        {
            boneIndexToBlendIndexMap[*itBoneIndex] = blendIndex;
            blendIndexToBoneIndexMap[blendIndex] = *itBoneIndex;

            ++itBoneIndex;
            ++blendIndex;
        }
    }*/
    //---------------------------------------------------------------------
    SubMesh* SubMesh::clone( Mesh *parentMesh )
    {
        SubMesh* newSub;
        if( !parentMesh )
            parentMesh = mParent;

        newSub = parentMesh->createSubMesh();

        newSub->mBlendIndexToBoneIndexMap = mBlendIndexToBoneIndexMap;
        newSub->mMaterialName = mMaterialName;
        //newSub->mParent = parentMesh; //Not needed, already set
        assert( newSub->mParent == parentMesh );

        newSub->mBoneAssignments            = mBoneAssignments;
        newSub->mBoneAssignmentsOutOfDate   = mBoneAssignmentsOutOfDate;

        const uint8 numVaoPasses = mParent->hasIndependentShadowMappingVaos() + 1;
        for( uint8 i=0; i<numVaoPasses; ++i )
        {
            newSub->mVao[i].reserve( mVao[i].size() );

            SharedVertexBufferMap sharedBuffers;
            VertexArrayObjectArray::const_iterator itor = mVao[i].begin();
            VertexArrayObjectArray::const_iterator end  = mVao[i].end();

            while( itor != end )
            {
                VertexArrayObject *newVao = (*itor)->clone( parentMesh->mVaoManager, &sharedBuffers );
                newSub->mVao[i].push_back( newVao );
                ++itor;
            }
        }

        if( numVaoPasses == 1 )
            newSub->mVao[1] = newSub->mVao[0];

        return 0;
    }
    //---------------------------------------------------------------------
    void SubMesh::importFromV1( v1::SubMesh *subMesh, bool halfPos, bool halfTexCoords, bool qTangents )
    {
        mMaterialName = subMesh->getMaterialName();

        if( subMesh->parent->hasSkeleton() )
            subMesh->_compileBoneAssignments();

        const v1::SubMesh::VertexBoneAssignmentList& v1BoneAssignments = subMesh->getBoneAssignments();
        mBoneAssignments.reserve( v1BoneAssignments.size() );

        {
            v1::SubMesh::VertexBoneAssignmentList::const_iterator itor = v1BoneAssignments.begin();
            v1::SubMesh::VertexBoneAssignmentList::const_iterator end  = v1BoneAssignments.end();

            while( itor != end )
            {
                mBoneAssignments.push_back( VertexBoneAssignment( itor->second ) );
                ++itor;
            }
        }
        std::sort( mBoneAssignments.begin(), mBoneAssignments.end() );
        mBlendIndexToBoneIndexMap = subMesh->blendIndexToBoneIndexMap;
        mBoneAssignmentsOutOfDate = false;

        importBuffersFromV1( subMesh, halfPos, halfTexCoords, qTangents, 0 );

        assert( subMesh->parent->hasValidShadowMappingBuffers() );

        //Deal with shadow mapping optimized buffers
        if( subMesh->vertexData[0] != subMesh->vertexData[1] ||
            subMesh->indexData[0] != subMesh->indexData[1] )
        {
            //Use the special version already built for v1
            importBuffersFromV1( subMesh, halfPos, halfTexCoords, qTangents, 1 );
        }
        else
        {
            //No special version in the v1 format, let the autogeneration routine decide.
            this->_prepareForShadowMapping( false );
        }
    }
    //---------------------------------------------------------------------
    void SubMesh::importBuffersFromV1( v1::SubMesh *subMesh, bool halfPos, bool halfTexCoords,
                                       bool qTangents, size_t vaoPassIdx )
    {
        VertexElement2Vec vertexElements;
        char *data = _arrangeEfficient( subMesh, halfPos, halfTexCoords, qTangents, &vertexElements,
                                        vaoPassIdx );

        //Wrap the ptrs around these, because the VaoManager's call
        //can throw thus causing a leak if we don't free them.
        FreeOnDestructor dataPtrContainer( data );

        VaoManager *vaoManager = mParent->mVaoManager;
        VertexBufferPackedVec vertexBuffers;

        //Create the vertex buffer
        bool keepAsShadow = mParent->mVertexBufferShadowBuffer;
        VertexBufferPacked *vertexBuffer = vaoManager->createVertexBuffer( vertexElements,
                                                        subMesh->vertexData[vaoPassIdx]->vertexCount,
                                                        mParent->mVertexBufferDefaultType,
                                                        data, keepAsShadow );
        vertexBuffers.push_back( vertexBuffer );

        if( keepAsShadow ) //Don't free the pointer ourselves
            dataPtrContainer.ptr = 0;

        IndexBufferPacked *indexBuffer = importFromV1( subMesh->indexData[vaoPassIdx] );

        {
            VertexArrayObject *vao = vaoManager->createVertexArrayObject( vertexBuffers, indexBuffer,
                                                                          subMesh->operationType );
            mVao[vaoPassIdx].push_back( vao );
        }

        //Now deal with the automatic LODs
        v1::SubMesh::LODFaceList::const_iterator itor = subMesh->mLodFaceList[vaoPassIdx].begin();
        v1::SubMesh::LODFaceList::const_iterator end  = subMesh->mLodFaceList[vaoPassIdx].end();

        while( itor != end )
        {
            IndexBufferPacked *lodIndexBuffer = importFromV1( *itor );

            VertexArrayObject *vao = vaoManager->createVertexArrayObject( vertexBuffers, lodIndexBuffer,
                                                                          subMesh->operationType );

            mVao[vaoPassIdx].push_back( vao );
            ++itor;
        }
    }
    //---------------------------------------------------------------------
    IndexBufferPacked* SubMesh::importFromV1( v1::IndexData *indexData )
    {
        if( !indexData )
            return 0;

        //Create & copy the index buffer
        bool keepAsShadow = mParent->mIndexBufferShadowBuffer;
        VaoManager *vaoManager = mParent->mVaoManager;
        void *indexDataPtr = OGRE_MALLOC_SIMD( indexData->indexCount *
                                               indexData->indexBuffer->getIndexSize(),
                                               MEMCATEGORY_GEOMETRY );
        FreeOnDestructor indexDataPtrContainer( indexDataPtr );
        IndexBufferPacked::IndexType indexType = static_cast<IndexBufferPacked::IndexType>(
                                                        indexData->indexBuffer->getType() );

        memcpy( indexDataPtr, indexData->indexBuffer->lock( v1::HardwareBuffer::HBL_READ_ONLY ),
                indexData->indexBuffer->getIndexSize() * indexData->indexCount );
        indexData->indexBuffer->unlock();

        IndexBufferPacked *indexBuffer = vaoManager->createIndexBuffer( indexType, indexData->indexCount,
                                                                        mParent->mIndexBufferDefaultType,
                                                                        indexDataPtr, keepAsShadow );

        if( keepAsShadow ) //Don't free the pointer ourselves
            indexDataPtrContainer.ptr = 0;

        return indexBuffer;
    }
    //---------------------------------------------------------------------
    void SubMesh::arrangeEfficient( bool halfPos, bool halfTexCoords, bool qTangents )
    {
        uint8 numVaoPasses = mParent->hasIndependentShadowMappingVaos() + 1;

        for( uint8 vaoPassIdx=0; vaoPassIdx<numVaoPasses; ++vaoPassIdx )
        {
            VertexArrayObjectArray newVaos;
            newVaos.reserve( mVao[vaoPassIdx].size() );
            SharedVertexBufferMap sharedBuffers;
            VertexArrayObjectArray::const_iterator itor = mVao[vaoPassIdx].begin();
            VertexArrayObjectArray::const_iterator end  = mVao[vaoPassIdx].end();

            while( itor != end )
            {
                newVaos.push_back( arrangeEfficient( halfPos, halfTexCoords, qTangents, *itor,
                                                     sharedBuffers, mParent->mVaoManager ) );
                ++itor;
            }

            mVao[vaoPassIdx].swap( newVaos );
            //Now 'newVaos' contains the old ones. We need to destroy all of them at
            //the end because vertex buffers may be shared while we still iterate.
            destroyVaos( newVaos, mParent->mVaoManager, false );
        }

        //If we shared vaos, we need to share the new Vaos (and remove the dangling pointers)
        if( numVaoPasses == 1 )
            mVao[1] = mVao[0];
    }
    //---------------------------------------------------------------------
    VertexArrayObject* SubMesh::arrangeEfficient( bool halfPos, bool halfTexCoords, bool qTangents,
                                                  VertexArrayObject *vao,
                                                  SharedVertexBufferMap &sharedBuffers,
                                                  VaoManager *vaoManager )
    {
        const VertexBufferPackedVec &vertexBuffers = vao->getVertexBuffers();
        VertexBufferPacked *newVertexBuffer = 0;

        SharedVertexBufferMap::const_iterator itShared = sharedBuffers.find( vertexBuffers[0] );
        if( itShared != sharedBuffers.end() )
        {
            //Shared vertex buffer. We've already converted this one. Reuse.
            newVertexBuffer = itShared->second;
        }
        else
        {
            VertexElement2Vec vertexElements;
            SourceDataArray srcData;
            bool hasTangents = false;

            vector<AsyncTicketPtr>::type asyncTickets;
            FastArray<char const *> srcPtrs;

            for( size_t i=0; i<vertexBuffers.size(); ++i )
            {
                //Retrieve the data from each buffer
                AsyncTicketPtr asyncTicket = vertexBuffers[i]->readRequest(
                                                    0, vertexBuffers[i]->getNumElements() );
                asyncTickets.push_back( asyncTicket );
                srcPtrs.push_back( reinterpret_cast<const char*>( asyncTicket->map() ) );

                //Setup the VertexElement array and the srcData for the conversion.
                size_t accumOffset = 0, reorderedElements = 0;
                VertexElement2Vec::const_iterator itor = vertexBuffers[i]->getVertexElements().begin();
                VertexElement2Vec::const_iterator end  = vertexBuffers[i]->getVertexElements().end();

                while( itor != end )
                {
                    const VertexElement2 &origElement = *itor;

                    const SourceData sourceData( srcPtrs.back() + accumOffset,
                                                 vertexBuffers[i]->getBytesPerElement(),
                                                 *itor );

                    if( origElement.mSemantic == VES_TANGENT ||
                        origElement.mSemantic == VES_BINORMAL )
                    {
                        hasTangents = true;
                        //Put VES_TANGENT & VES_BINORMAL at the bottom of the array.
                        srcData.push_back( sourceData );
                        ++reorderedElements;
                    }
                    else
                    {
                        vertexElements.push_back( origElement );
                        srcData.insert( srcData.end() - reorderedElements, sourceData );
                    }

                    accumOffset += v1::VertexElement::getTypeSize( itor->mType );

                    //We can't convert to half if it wasn't in floating point
                    //Also avoid converting 1 Float ==> 2 Half.
                    if( v1::VertexElement::getBaseType( origElement.mType ) == VET_FLOAT1 &&
                        v1::VertexElement::getTypeCount( origElement.mType ) != 1 )
                    {
                        if( (origElement.mSemantic == VES_POSITION && halfPos) ||
                            (origElement.mSemantic == VES_TEXTURE_COORDINATES && halfTexCoords) )
                        {
                            VertexElementType type = v1::VertexElement::multiplyTypeCount(
                                        VET_HALF2, v1::VertexElement::getTypeCount( origElement.mType ) );

                            VertexElement2 &lastInserted = *(vertexElements.end() -
                                                             reorderedElements - 1);
                            lastInserted.mType = type;
                        }
                    }

                    ++itor;
                }

                //If the vertex format has tangents, prepare the normal to hold QTangents.
                if( hasTangents == true && qTangents )
                {
                    VertexElement2Vec::iterator it = std::find( vertexElements.begin(),
                                                                vertexElements.end(),
                                                                VertexElement2( VET_FLOAT3,
                                                                                VES_NORMAL ) );
                    if( it != vertexElements.end() )
                        it->mType = VET_SHORT4_SNORM;
                }
            }

            char *data = _arrangeEfficient( srcData, vertexElements, vertexBuffers[0]->getNumElements() );
            FreeOnDestructor dataPtrContainer( data );

            //Cleanup the mappings, free some memory.
            for( size_t i=0; i<asyncTickets.size(); ++i )
                asyncTickets[i]->unmap();
            asyncTickets.clear();

            //Create the new vertex buffer.
            const bool keepAsShadow = vertexBuffers[0]->getShadowCopy() != 0;
            newVertexBuffer = vaoManager->createVertexBuffer( vertexElements,
                                                              vertexBuffers[0]->getNumElements(),
                                                              vertexBuffers[0]->getBufferType(),
                                                              data, keepAsShadow );

            if( keepAsShadow ) //Don't free the pointer ourselves
                dataPtrContainer.ptr = 0;

            sharedBuffers[vertexBuffers[0]] = newVertexBuffer;
        }

        VertexBufferPackedVec newVertexBuffers;
        newVertexBuffers.push_back( newVertexBuffer );

        return vaoManager->createVertexArrayObject( newVertexBuffers, vao->getIndexBuffer(),
                                                    vao->getOperationType() );
    }
    //---------------------------------------------------------------------
    bool sortVertexElementsBySemantic2( const VertexElement2 &l, const VertexElement2 &r )
    {
        return l.mSemantic < r.mSemantic;
    }
    bool sortVertexElementsBySemantic( const v1::VertexElement &l, const v1::VertexElement &r )
    {
        if( l.getSemantic() == r.getSemantic() )
            return l.getIndex() < r.getIndex();

        return l.getSemantic() < r.getSemantic();
    }

    char* SubMesh::_arrangeEfficient( v1::SubMesh *subMesh, bool halfPos, bool halfTexCoords,
                                      bool qTangents, VertexElement2Vec *outVertexElements,
                                      size_t vaoPassIdx )
    {
        typedef FastArray<v1::VertexElement> VertexElementArray;

        VertexElement2Vec vertexElements;
        VertexElementArray srcElements;
        bool hasTangents = false;

        v1::VertexData *vertexData = subMesh->vertexData[vaoPassIdx];

        {
            //Get an AZDO-friendly vertex declaration out of the original declaration.
            const v1::VertexDeclaration::VertexElementList &origElements = vertexData->
                                                                vertexDeclaration->getElements();
            srcElements.reserve( origElements.size() );
            v1::VertexDeclaration::VertexElementList::const_iterator itor = origElements.begin();
            v1::VertexDeclaration::VertexElementList::const_iterator end  = origElements.end();

            while( itor != end )
            {
                const v1::VertexElement &origElement = *itor;

                if( origElement.getSemantic() == VES_TANGENT ||
                    origElement.getSemantic() == VES_BINORMAL )
                {
                    hasTangents = true;
                }
                else
                {
                    vertexElements.push_back( VertexElement2( origElement.getType(),
                                                              origElement.getSemantic() ) );
                }

                srcElements.push_back( *itor );

                //We can't convert to half if it wasn't in floating point
                //Also avoid converting 1 Float ==> 2 Half.
                if( origElement.getBaseType( origElement.getType() ) == VET_FLOAT1 &&
                    origElement.getTypeCount( origElement.getType() ) != 1 )
                {
                    if( (origElement.getSemantic() == VES_POSITION && halfPos) ||
                        (origElement.getSemantic() == VES_TEXTURE_COORDINATES && halfTexCoords) )
                    {
                        VertexElementType type = origElement.multiplyTypeCount(
                                                            VET_HALF2, origElement.getTypeCount(
                                                                            origElement.getType() ) );

                        VertexElement2 &lastInserted = vertexElements.back();
                        lastInserted.mType = type;
                    }
                }

                ++itor;
            }

            //If it has tangents, prepare the normal to hold QTangents.
            if( hasTangents == true && qTangents )
            {
                VertexElement2Vec::iterator it = std::find( vertexElements.begin(),
                                                            vertexElements.end(),
                                                            VertexElement2( VET_FLOAT3,
                                                                            VES_NORMAL ) );
                if( it != vertexElements.end() )
                    it->mType = VET_SHORT4_SNORM;
            }
        }

        std::sort( vertexElements.begin(), vertexElements.end(), sortVertexElementsBySemantic2 );
        std::sort( srcElements.begin(), srcElements.end(), sortVertexElementsBySemantic );

        {
            //Put VES_TANGENT & VES_BINORMAL at the bottom of the array.
            size_t reorderedElements = 0;
            VertexElementArray::iterator itor = srcElements.begin();
            VertexElementArray::iterator end  = srcElements.end();
            while( itor != end )
            {
                if( itor->getSemantic() == VES_TANGENT || itor->getSemantic() == VES_BINORMAL )
                {
                    v1::VertexElement element = *itor;
                    const size_t idx = (itor - srcElements.begin());
                    ++reorderedElements;

                    itor = srcElements.erase( itor );
                    srcElements.push_back( element );
                    itor = srcElements.begin() + idx;
                    end  = srcElements.end() - reorderedElements;
                }
                else
                {
                    ++itor;
                }
            }
        }

        //Prepare for the transfer between buffers.
        FastArray<char*> srcPtrs;
        FastArray<size_t> vertexBuffSizes;
        srcPtrs.reserve( vertexData->vertexBufferBinding->getBufferCount() );
        for( size_t i=0; i<vertexData->vertexBufferBinding->getBufferCount(); ++i )
        {
            const v1::HardwareVertexBufferSharedPtr &vBuffer = vertexData->vertexBufferBinding->
                                                                                getBuffer( i );
            srcPtrs.push_back( static_cast<char*>( vBuffer->lock( v1::HardwareBuffer::HBL_READ_ONLY ) ) );
            vertexBuffSizes.push_back( vBuffer->getVertexSize() );
        }

        SourceDataArray sourceData;
        sourceData.reserve( srcElements.size() );

        VertexElementArray::const_iterator itor = srcElements.begin();
        VertexElementArray::const_iterator end  = srcElements.end();

        while( itor != end )
        {
            const VertexElement2 element( itor->getType(), itor->getSemantic() );
            const SourceData srcData( srcPtrs[itor->getSource()] + itor->getOffset(),
                                      vertexBuffSizes[itor->getSource()],
                                      element );
            sourceData.push_back( srcData );
            ++itor;
        }

        //Perform actual transfer
        char *retVal = _arrangeEfficient( sourceData, vertexElements,
                                          static_cast<uint32>(vertexData->vertexCount) );

        //Cleanup
        for( size_t i=0; i<vertexData->vertexBufferBinding->getBufferCount(); ++i )
            vertexData->vertexBufferBinding->getBuffer( i )->unlock();

        if( outVertexElements )
            outVertexElements->swap( vertexElements );

        return retVal;
    }
    //---------------------------------------------------------------------
    char* SubMesh::_arrangeEfficient( SourceDataArray srcData,
                                      const VertexElement2Vec &vertexElements,
                                      uint32 vertexCount )
    {
        //Prepare for the transfer between buffers.
        size_t vertexSize = VaoManager::calculateVertexSize( vertexElements );
        char *data = static_cast<char*>( OGRE_MALLOC_SIMD( vertexSize * vertexCount,
                                                           MEMCATEGORY_GEOMETRY ) );
        char *dstData = data;

        SourceData *tangentSrc = 0;
        SourceData *binormalSrc = 0;

        {
            //Find the pointers for tangentSrc & binormalSrc (may both be null) since
            //we will be merging all three (normal, tangent & binormal) into just one
            //element.
            bool wantsQTangents = false;
            {
                VertexElement2Vec::const_iterator itor = vertexElements.begin();
                VertexElement2Vec::const_iterator end  = vertexElements.end();

                while( itor != end && !wantsQTangents )
                {
                    if( itor->mSemantic == VES_NORMAL && itor->mType == VET_SHORT4_SNORM )
                        wantsQTangents = true;

                    ++itor;
                }
            }

            if( wantsQTangents )
            {
                SourceDataArray::iterator itor = srcData.begin();
                SourceDataArray::iterator end  = srcData.end();

                while( itor != end )
                {
                    if( itor->element.mSemantic == VES_TANGENT )
                    {
                        tangentSrc = &(*itor);

                        assert( (itor - srcData.begin() >= srcData.size() - 2u) &&
                                (srcData.back().element.mSemantic == VES_TANGENT ||
                                 srcData.back().element.mSemantic == VES_BINORMAL ) &&
                                "Tangent element must be at the end of srcData array!" );
                    }
                    else if( itor->element.mSemantic == VES_BINORMAL )
                    {
                        binormalSrc = &(*itor);

                        assert( (itor - srcData.begin() >= srcData.size() - 2u) &&
                                (srcData.back().element.mSemantic == VES_TANGENT ||
                                 srcData.back().element.mSemantic == VES_BINORMAL ) &&
                                "Binormal element must be at the end of srcData array!" );
                    }

                    ++itor;
                }
            }
        }

        //Perform the transfer. Note that vertexElements & srcElements do not match.
        //As vertexElements is modified for smaller types and may include padding
        //for alignment reasons.
        for( size_t i=0; i<vertexCount; ++i )
        {
            size_t acumOffset = 0;
            VertexElement2Vec::const_iterator itor = vertexElements.begin();
            VertexElement2Vec::const_iterator end  = vertexElements.end();
            SourceDataArray::iterator itSrc = srcData.begin();

            while( itor != end )
            {
                const VertexElement2 &vElement = *itor;
                size_t writeSize = v1::VertexElement::getTypeSize( vElement.mType );

                assert( itor->mSemantic == itSrc->element.mSemantic );

                if( vElement.mSemantic == VES_NORMAL &&
                    vElement.mType == VET_SHORT4_SNORM && tangentSrc )
                {
                    //QTangents
                    const size_t readSize = v1::VertexElement::getTypeSize( itSrc->element.mType );
                    const size_t tangentSize = v1::VertexElement::getTypeSize( tangentSrc->element.mType );

                    //Convert TBN matrix (between 6 to 9 floats, 24-36 bytes)
                    //to a QTangent (4 shorts, 8 bytes)
                    assert( readSize == sizeof(float) * 3 );
                    assert( tangentSize <= sizeof(float) * 4 &&
                            tangentSize >= sizeof(float) * 3 );

                    float normal[3];
                    float tangent[4];
                    tangent[3] = 1.0f;
                    memcpy( normal, itSrc->data, readSize );
                    memcpy( tangent, tangentSrc->data, tangentSize );

                    tangentSrc->data += tangentSrc->bytesPerVertex;

                    Vector3 vNormal( normal[0], normal[1], normal[2] );
                    Vector3 vTangent( tangent[0], tangent[1], tangent[2] );

                    if( binormalSrc )
                    {
                        const size_t binormalSize = v1::VertexElement::getTypeSize( binormalSrc->
                                                                                    element.mType );

                        assert( binormalSize == sizeof(float) * 3 );
                        float binormal[3];
                        memcpy( binormal, binormalSrc->data, binormalSize );

                        Vector3 vBinormal( binormal[0], binormal[1], binormal[2] );

                        //It is reflected.
                        Vector3 naturalBinormal = vTangent.crossProduct( vNormal );
                        if( naturalBinormal.dotProduct( vBinormal ) <= 0 )
                            tangent[3] = -1.0f;

                        binormalSrc->data += binormalSrc->bytesPerVertex;
                    }

                    Matrix3 tbn;
                    tbn.SetColumn( 0, vNormal );
                    tbn.SetColumn( 1, vTangent );
                    tbn.SetColumn( 2, vNormal.crossProduct( vTangent ) );

                    //See Spherical Skinning with Dual-Quaternions and QTangents,
                    //Ivo Zoltan Frey, SIGRAPH 2011 Vancounver.
                    //http://www.crytek.com/download/izfrey_siggraph2011.ppt

                    Quaternion qTangent( tbn );
                    qTangent.normalise();

                    //Bias = 1 / [2^(bits-1) - 1]
                    const Real bias = 1.0f / 32767.0f;

                    //Make sure QTangent is always positive
                    if( qTangent.w < 0 )
                        qTangent = -qTangent;

                    //Because '-0' sign information is lost when using integers,
                    //we need to apply a "bias"; while making sure the Quatenion
                    //stays normalized.
                    // ** Also our shaders assume qTangent.w is never 0. **
                    if( qTangent.w < bias )
                    {
                        Real normFactor = Math::Sqrt( 1 - bias * bias );
                        qTangent.w = bias;
                        qTangent.x *= normFactor;
                        qTangent.y *= normFactor;
                        qTangent.z *= normFactor;
                    }

                    //Now negate if we require reflection
                    if( tangent[3] < 0 )
                        qTangent = -qTangent;

                    int16 *dstData16 = reinterpret_cast<int16*>(dstData + acumOffset);

                    dstData16[0] = Bitwise::floatToSnorm16( qTangent.x );
                    dstData16[1] = Bitwise::floatToSnorm16( qTangent.y );
                    dstData16[2] = Bitwise::floatToSnorm16( qTangent.z );
                    dstData16[3] = Bitwise::floatToSnorm16( qTangent.w );
                }
                else if( v1::VertexElement::getBaseType( vElement.mType ) == VET_HALF2 &&
                         v1::VertexElement::getBaseType( itSrc->element.mType ) == VET_FLOAT1 )
                {
                    size_t readSize = v1::VertexElement::getTypeSize( itSrc->element.mType );

                    //Convert float to half.
                    float fpData[4];
                    fpData[0] = fpData[1] = fpData[2] = 0.0f;
                    fpData[3] = 1.0f;
                    memcpy( fpData, itSrc->data, readSize );

                    uint16 *dstData16 = reinterpret_cast<uint16*>(dstData + acumOffset);

                    for( size_t j=0; j<v1::VertexElement::getTypeCount( vElement.mType ); ++j )
                        dstData16[j] = Bitwise::floatToHalf( fpData[j] );
                }
                else
                {
                    //Raw. Transfer as is.
                    memcpy( dstData + acumOffset, itSrc->data, writeSize ); //writeSize = readSize
                }

                acumOffset  += writeSize;
                itSrc->data += itSrc->bytesPerVertex;

                ++itSrc;
                ++itor;
            }

            dstData += vertexSize;
        }

        assert( dstData == data + vertexSize * vertexCount );

        return data;
    }
    //---------------------------------------------------------------------
    void SubMesh::dearrangeToInefficient(void)
    {
        const uint8 numVaoPasses = mParent->hasIndependentShadowMappingVaos() + 1;

        for( uint8 vaoPassIdx=0; vaoPassIdx<numVaoPasses; ++vaoPassIdx )
        {
            VertexArrayObjectArray newVaos;
            newVaos.reserve( mVao[vaoPassIdx].size() );
            SharedVertexBufferMap sharedBuffers;
            VertexArrayObjectArray::const_iterator itor = mVao[vaoPassIdx].begin();
            VertexArrayObjectArray::const_iterator end  = mVao[vaoPassIdx].end();

            while( itor != end )
            {
                newVaos.push_back( dearrangeEfficient( *itor, sharedBuffers, mParent->mVaoManager ) );
                ++itor;
            }

            mVao[vaoPassIdx].swap( newVaos );
            //Now 'newVaos' contains the old ones. We need to destroy all of them at
            //the end because vertex buffers may be shared while we still iterate.
            destroyVaos( newVaos, mParent->mVaoManager, false );
        }

        //If we shared vaos, we need to share the new Vaos (and remove the dangling pointers)
        if( numVaoPasses == 1 )
            mVao[1] = mVao[0];
    }
    //---------------------------------------------------------------------
    VertexArrayObject* SubMesh::dearrangeEfficient( const VertexArrayObject *vao,
                                                    SharedVertexBufferMap &sharedBuffers,
                                                    VaoManager *vaoManager )
    {
        const VertexBufferPackedVec &vertexBuffers = vao->getVertexBuffers();

        VertexElement2VecVec newVertexElements;
        newVertexElements.resize( vertexBuffers.size() );
        VertexBufferPackedVec newVertexBuffers;
        newVertexBuffers.reserve( vertexBuffers.size() );

        VertexElement2VecVec::iterator itNewElementVec = newVertexElements.begin();
        VertexBufferPackedVec::const_iterator itor = vertexBuffers.begin();
        VertexBufferPackedVec::const_iterator end  = vertexBuffers.end();

        while( itor != end )
        {
            AsyncTicketPtr asyncTicket = (*itor)->readRequest( 0, (*itor)->getNumElements() );

            SharedVertexBufferMap::const_iterator itShared = sharedBuffers.find( *itor );
            if( itShared != sharedBuffers.end() )
            {
                //Shared vertex buffer. We've already converted this one. Reuse.
                newVertexBuffers.push_back( itShared->second );
            }
            else
            {
                const void *srcData = asyncTicket->map();
                char *data = _dearrangeEfficient( reinterpret_cast<const char * RESTRICT_ALIAS>(srcData),
                                                  (*itor)->getNumElements(), (*itor)->getVertexElements(),
                                                  &(*itNewElementVec) );
                asyncTicket->unmap();

                FreeOnDestructor dataPtrContainer( data );
                const bool keepAsShadow = (*itor)->getShadowCopy() != 0;
                VertexBufferPacked *newVertexBuffer =
                        vaoManager->createVertexBuffer( *itNewElementVec,
                                                        (*itor)->getNumElements(),
                                                        (*itor)->getBufferType(),
                                                        data, keepAsShadow );

                if( keepAsShadow ) //Don't free the pointer ourselves
                    dataPtrContainer.ptr = 0;

                sharedBuffers[*itor] = newVertexBuffer;
                newVertexBuffers.push_back( newVertexBuffer );
            }

            ++itNewElementVec;
            ++itor;
        }

        return vaoManager->createVertexArrayObject( newVertexBuffers, vao->getIndexBuffer(),
                                                    vao->getOperationType() );
    }
    //---------------------------------------------------------------------
    char* SubMesh::_dearrangeEfficient( char const * RESTRICT_ALIAS srcData, uint32 numElements,
                                        const VertexElement2Vec &vertexElements,
                                        VertexElement2Vec *outVertexElements )
    {
        VertexElement2Vec newVertexElements;

        VertexElement2Vec::const_iterator itElements = vertexElements.begin();
        VertexElement2Vec::const_iterator enElements = vertexElements.end();
        while( itElements != enElements )
        {
            VertexElement2 element( *itElements );

            const VertexElementType baseType = v1::VertexElement::getBaseType( element.mType );
            if( baseType == VET_HALF2 )
            {
                //Convert from half to float
                element.mType = v1::VertexElement::multiplyTypeCount( VET_FLOAT1,
                                                                      v1::VertexElement::
                                                                      getTypeCount( element.mType ) );

                newVertexElements.push_back( element );
            }
            else if( element.mSemantic == VES_NORMAL && element.mType == VET_SHORT4_SNORM )
            {
                //Dealing with QTangents.
                newVertexElements.push_back( VertexElement2( VET_FLOAT3, VES_NORMAL ) );
                newVertexElements.push_back( VertexElement2( VET_FLOAT4, VES_TANGENT ) );
            }
            else
            {
                //Send through
                newVertexElements.push_back( element );
            }

            ++itElements;
        }

        const size_t newVertexSize = VaoManager::calculateVertexSize( newVertexElements );

        char *data = static_cast<char*>( OGRE_MALLOC_SIMD( numElements * newVertexSize, MEMCATEGORY_GEOMETRY ) );
        char *dstData = data;

        for( uint32 i=0; i<numElements; ++i )
        {
            itElements = vertexElements.begin();

            while( itElements != enElements )
            {
                const size_t readSize = v1::VertexElement::getTypeSize( itElements->mType );

                const VertexElementType baseType = v1::VertexElement::getBaseType( itElements->mType );
                if( baseType == VET_HALF2 )
                {
                    //Convert half to float.
                    uint16 hfData[4];
                    memcpy( hfData, srcData, readSize );

                    uint32 *dstData32 = reinterpret_cast<uint32*>(dstData);
                    const size_t typeCount = v1::VertexElement::getTypeCount( itElements->mType );

                    for( size_t j=0; j<typeCount; ++j )
                        dstData32[j] = Bitwise::halfToFloatI( hfData[j] );

                    dstData += typeCount * sizeof(uint32);
                }
                else if( itElements->mSemantic == VES_NORMAL && itElements->mType == VET_SHORT4_SNORM )
                {
                    //Dealing with QTangents.
                    Quaternion qTangent;
                    const int16 *srcData16 = reinterpret_cast<const int16*>( srcData );
                    qTangent.x = Bitwise::snorm16ToFloat( srcData16[0] );
                    qTangent.y = Bitwise::snorm16ToFloat( srcData16[1] );
                    qTangent.z = Bitwise::snorm16ToFloat( srcData16[2] );
                    qTangent.w = Bitwise::snorm16ToFloat( srcData16[3] );

                    float reflection = 1.0f;
                    if( qTangent.w < 0 )
                        reflection = -1.0f;

                    Vector3 vNormal = qTangent.xAxis();
                    Vector3 vTangent = qTangent.yAxis();

                    float *dstDataF32 = reinterpret_cast<float*>(dstData);
                    dstDataF32[0] = vNormal.x;
                    dstDataF32[1] = vNormal.y;
                    dstDataF32[2] = vNormal.z;
                    dstDataF32[3] = vTangent.x;
                    dstDataF32[4] = vTangent.y;
                    dstDataF32[5] = vTangent.z;
                    dstDataF32[6] = reflection;

                    dstData += 7 * sizeof(float);
                }
                else
                {
                    //Raw. Transfer as is.
                    memcpy( dstData, srcData, readSize );
                    dstData += readSize;
                }

                srcData += readSize;
                ++itElements;
            }
        }

        outVertexElements->swap( newVertexElements );

        return data;
    }
    //---------------------------------------------------------------------
    void SubMesh::destroyVaos( VertexArrayObjectArray &vaos, VaoManager *vaoManager,
                               bool destroyIndexBuffer )
    {
        typedef set<VertexBufferPacked*>::type VertexBufferPackedSet;
        VertexBufferPackedSet destroyedBuffers;

        VertexArrayObjectArray::const_iterator itor = vaos.begin();
        VertexArrayObjectArray::const_iterator end  = vaos.end();
        while( itor != end )
        {
            VertexArrayObject *vao = *itor;

            const VertexBufferPackedVec &vertexBuffers = vao->getVertexBuffers();
            VertexBufferPackedVec::const_iterator itBuffers = vertexBuffers.begin();
            VertexBufferPackedVec::const_iterator enBuffers = vertexBuffers.end();

            //If LOD share the same buffers, we'll try to destroy the buffers only once.
            while( itBuffers != enBuffers )
            {
                std::pair<VertexBufferPackedSet::iterator, bool> bufferNotSeenYet =
                                                        destroyedBuffers.insert( *itBuffers );
                if( bufferNotSeenYet.second )
                    vaoManager->destroyVertexBuffer( *itBuffers );

                ++itBuffers;
            }

            if( vao->getIndexBuffer() && destroyIndexBuffer )
                vaoManager->destroyIndexBuffer( vao->getIndexBuffer() );
            vaoManager->destroyVertexArrayObject( vao );

            ++itor;
        }

        vaos.clear();
    }
    //---------------------------------------------------------------------
    void SubMesh::destroyShadowMappingVaos(void)
    {
        if( mVao[0].empty() || mVao[1].empty() || mVao[0][0] == mVao[1][0] )
            mVao[1].clear(); //Using the same Vaos for both shadow mapping and regular rendering

        destroyVaos( mVao[1], mParent->mVaoManager );

        mVao[1].reserve( mVao[0].size() );
    }
    //---------------------------------------------------------------------
    void SubMesh::_prepareForShadowMapping( bool forceSameBuffers )
    {
        destroyShadowMappingVaos();

        if( !forceSameBuffers && Mesh::msOptimizeForShadowMapping )
            VertexShadowMapHelper::optimizeForShadowMapping( mParent->mVaoManager, mVao[0], mVao[1] );
        else
            VertexShadowMapHelper::useSameVaos( mParent->mVaoManager, mVao[0], mVao[1] );
    }
}
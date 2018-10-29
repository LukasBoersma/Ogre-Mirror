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

#include "OgreHlmsDiskCache.h"
#include "OgreHlmsManager.h"
#include "OgreLogManager.h"
#include "OgreStringConverter.h"
#include "OgreProfiler.h"

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
    #include "iOS/macUtils.h"
#endif

namespace Ogre
{
    HlmsDiskCache::HlmsDiskCache() :
        mTemplatesOutOfDate( false ),
        mHlmsManager( 0 )
    {
    }
    //-----------------------------------------------------------------------------------
    HlmsDiskCache::~HlmsDiskCache()
    {
        clearCache();
    }
    //-----------------------------------------------------------------------------------
    void HlmsDiskCache::clearCache(void)
    {
        mTemplatesOutOfDate = false;
        mCache.sourceCode.clear();
        mCache.pso.clear();
    }
    //-----------------------------------------------------------------------------------
    //-----------------------------------------------------------------------------------
    //-----------------------------------------------------------------------------------
    HlmsDiskCache::SourceCode::SourceCode() :
        mergedCache( HlmsPropertyVec(), 0 )
    {
    }
    //-----------------------------------------------------------------------------------
    HlmsDiskCache::SourceCode::SourceCode( const Hlms::ShaderCodeCache &shaderCodeCache ) :
        mergedCache( shaderCodeCache.mergedCache )
    {
        for( size_t i=0; i<NumShaderTypes; ++i )
        {
            if( shaderCodeCache.shaders[i] )
                this->sourceFile[i] = shaderCodeCache.shaders[i]->getSource();
        }
    }
    //-----------------------------------------------------------------------------------
    HlmsDiskCache::Pso::Pso() :
        renderableCache( HlmsPropertyVec(), 0 )
    {
    }
    //-----------------------------------------------------------------------------------
    HlmsDiskCache::Pso::Pso( const Hlms::RenderableCache &srcRenderableCache,
                             const Hlms::PassCache &srcPassCache, const HlmsCache *srcPsoCache ) :
        renderableCache( srcRenderableCache ),
        passProperties( srcPassCache.properties ),
        pso( srcPsoCache->pso ),
        macroblock( *srcPsoCache->pso.macroblock ),
        blendblock( *srcPsoCache->pso.blendblock )
    {
    }
    //-----------------------------------------------------------------------------------
    //-----------------------------------------------------------------------------------
    //-----------------------------------------------------------------------------------
    void HlmsDiskCache::copyFrom( Hlms *hlms )
    {
        clearCache();

        mCache.type = hlms->getType();

        {
            //Copy shaders
            mCache.sourceCode.reserve( hlms->mShaderCodeCache.size() );
            Hlms::ShaderCodeCacheVec::const_iterator itor = hlms->mShaderCodeCache.begin();
            Hlms::ShaderCodeCacheVec::const_iterator end  = hlms->mShaderCodeCache.end();

            while( itor != end )
            {
                SourceCode sourceCode( *itor );
                mCache.sourceCode.push_back( sourceCode );
                ++itor;
            }
        }

        {
            //Copy PSOs
            mCache.pso.reserve( hlms->mShaderCache.size() );
            HlmsCacheVec::const_iterator itor = hlms->mShaderCache.begin();
            HlmsCacheVec::const_iterator end  = hlms->mShaderCache.end();

            while( itor != end )
            {
                const uint32 finalHash = (*itor)->hash;

                const uint32 renderableIdx  = (finalHash >> HlmsBits::RenderableShift) &
                                              (uint32)HlmsBits::RendarebleHlmsTypeMask;
                const uint32 passIdx        = (finalHash >> HlmsBits::PassShift) &
                                              (uint32)HlmsBits::PassMask;
//                const uint32 inputLayout    = (finalHash >> HlmsBits::InputLayoutShift) &
//                                              (uint32)HlmsBits::InputLayoutMask;
                Pso pso( hlms->mRenderableCache[renderableIdx], hlms->mPassCache[passIdx], *itor );
                mCache.pso.push_back( pso );
                ++itor;
            }
        }
    }
    //-----------------------------------------------------------------------------------
    void HlmsDiskCache::applyTo( Hlms *hlms )
    {
        if( mCache.type != hlms->getType() )
        {
            LogManager::getSingleton().logMessage(
                        "WARNING: The cached Hlms is for type " +
                        StringConverter::toString( mCache.type ) +
                        "but it is being applied to Hlms type: " +
                        StringConverter::toString( hlms->getType() ) +
                        ". HlmsDiskCache won't be applied." );
            return;
        }

        hlms->clearShaderCache();

        {
            //Compile shaders
            SourceCodeVec::const_iterator itor = mCache.sourceCode.begin();
            SourceCodeVec::const_iterator end  = mCache.sourceCode.end();

            while( itor != end )
            {
                if( !mTemplatesOutOfDate )
                {
                    //Templates haven't changed, send the Hlms-processed shader code for compilation
                    hlms->_compileShaderFromPreprocessedSource( itor->mergedCache, itor->sourceFile );
                }
                else
                {
                    //Templates have changed, they need to be run through the Hlms
                    //preprocessor again before they can be compiled again
                    Hlms::ShaderCodeCache shaderCodeCache( itor->mergedCache.pieces );
                    shaderCodeCache.mergedCache.setProperties = itor->mergedCache.setProperties;
                    hlms->compileShaderCode( shaderCodeCache );
                }
                ++itor;
            }
        }

        {
            PsoVec::const_iterator itor = mCache.pso.begin();
            PsoVec::const_iterator end  = mCache.pso.end();

            while( itor != end )
            {
                size_t renderableHash = hlms->addRenderableCache( itor->renderableCache.setProperties,
                                                                  itor->renderableCache.pieces );

                uint32 passHash = 0;
                {
                    assert( hlms->mPassCache.size() <= (uint32)HlmsBits::PassMask &&
                            "Too many passes combinations, we'll overflow "
                            "the bits assigned in the hash!" );

                    Hlms::PassCache passCache;
                    passCache.passPso       = itor->pso.pass;
                    passCache.properties    = itor->passProperties;

                    Hlms::PassCacheVec::iterator it = std::find( hlms->mPassCache.begin(),
                                                                 hlms->mPassCache.end(), passCache );
                    if( it == hlms->mPassCache.end() )
                    {
                        hlms->mPassCache.push_back( passCache );
                        it = hlms->mPassCache.end() - 1u;
                    }

                    passHash = (uint32)(it - hlms->mPassCache.begin()) << (uint32)HlmsBits::PassShift;
                }

                hlms->createShaderCacheEntry( renderableHash, passHash, , );

//                const uint32 inputLayout    = (finalHash >> HlmsBits::InputLayoutShift) &
//                                              (uint32)HlmsBits::InputLayoutMask;

                ++itor;
            }
        }
    }
    //-----------------------------------------------------------------------------------
    template <typename T> void write( DataStreamPtr &dataStream, const T &value )
    {
        dataStream->write( &value, sizeof(value) );
    }
    void HlmsDiskCache::save( DataStreamPtr &dataStream, const String &string )
    {
        write<uint32>( dataStream, static_cast<uint32>( string.size() ) );
        dataStream->write( string.c_str(), string.size() );
    }
    //-----------------------------------------------------------------------------------
    void HlmsDiskCache::save( DataStreamPtr &dataStream, const HlmsPropertyVec &properties )
    {
        write<uint32>( dataStream, static_cast<uint32>( properties.size() ) );

        HlmsPropertyVec::const_iterator itor = properties.begin();
        HlmsPropertyVec::const_iterator end  = properties.end();

        while( itor != end )
        {
            write( dataStream, itor->keyName.mHash );
            write( dataStream, itor->value );
            ++itor;
        }
    }
    //-----------------------------------------------------------------------------------
    void HlmsDiskCache::save( DataStreamPtr &dataStream, const Hlms::RenderableCache &renderableCache )
    {
        //Save the properties
        save( dataStream, renderableCache.setProperties );

        //Save the pieces
        for( size_t i=0; i<NumShaderTypes; ++i )
        {
            write<uint32>( dataStream, static_cast<uint32>( renderableCache.pieces[i].size() ) );

            PiecesMap::const_iterator itor = renderableCache.pieces[i].begin();
            PiecesMap::const_iterator end  = renderableCache.pieces[i].end();

            while( itor != end )
            {
                write( dataStream, itor->first.mHash );
                save( dataStream, itor->second );
                ++itor;
            }
        }
    }
    //-----------------------------------------------------------------------------------
    void HlmsDiskCache::saveTo( DataStreamPtr &dataStream )
    {
        write( dataStream, mCache.templateHash );
        write( dataStream, mCache.type );

        {
            //Save shaders
            write<uint32>( dataStream, static_cast<uint32>( mCache.sourceCode.size() ) );

            SourceCodeVec::const_iterator itor = mCache.sourceCode.begin();
            SourceCodeVec::const_iterator end  = mCache.sourceCode.end();

            while( itor != end )
            {
                save( dataStream, itor->mergedCache );
                for( size_t i=0; i<NumShaderTypes; ++i )
                    save( dataStream, itor->sourceFile[i] );

                ++itor;
            }
        }

        {
            //Save PSOs
            write<uint32>( dataStream, static_cast<uint32>( mCache.pso.size() ) );

            PsoVec::const_iterator itor = mCache.pso.begin();
            PsoVec::const_iterator end  = mCache.pso.end();

            while( itor != end )
            {
                save( dataStream, itor->renderableCache );
                save( dataStream, itor->passProperties );

                write<uint32>( dataStream, static_cast<uint32>( itor->pso.vertexElements.size() ) );
                VertexElement2VecVec::const_iterator itElem = itor->pso.vertexElements.begin();
                VertexElement2VecVec::const_iterator enElem = itor->pso.vertexElements.end();

                while( itElem != enElem )
                {
                    write<uint32>( dataStream, static_cast<uint32>( itElem->size() ) );
                    VertexElement2Vec::const_iterator itElem2 = itElem->begin();
                    VertexElement2Vec::const_iterator enElem2 = itElem->end();

                    while( itElem2 != enElem2 )
                    {
                        write( dataStream, itElem2->mType );
                        write( dataStream, itElem2->mSemantic );
                        write( dataStream, itElem2->mInstancingStepRate );
                        ++itElem2;
                    }

                    ++itElem;
                }

                write( dataStream, itor->pso.operationType );
                write( dataStream, itor->pso.enablePrimitiveRestart );
                write( dataStream, itor->pso.sampleMask );
                write( dataStream, itor->pso.pass );

                write( dataStream, itor->macroblock.mScissorTestEnabled );
                write( dataStream, itor->macroblock.mDepthCheck );
                write( dataStream, itor->macroblock.mDepthWrite );
                write( dataStream, itor->macroblock.mDepthFunc );
                write( dataStream, itor->macroblock.mDepthBiasConstant );
                write( dataStream, itor->macroblock.mDepthBiasSlopeScale );
                write( dataStream, itor->macroblock.mCullMode );
                write( dataStream, itor->macroblock.mPolygonMode );

                write( dataStream, itor->blendblock.mAlphaToCoverageEnabled );
                write( dataStream, itor->blendblock.mBlendChannelMask );
//                write( dataStream, itor->blendblock.mIsTransparent );
                write( dataStream, itor->blendblock.mSeparateBlend );
                write( dataStream, itor->blendblock.mSourceBlendFactor );
                write( dataStream, itor->blendblock.mDestBlendFactor );
                write( dataStream, itor->blendblock.mSourceBlendFactorAlpha );
                write( dataStream, itor->blendblock.mDestBlendFactorAlpha );
                write( dataStream, itor->blendblock.mBlendOperation );
                write( dataStream, itor->blendblock.mBlendOperationAlpha );

                ++itor;
            }
        }
    }
    //-----------------------------------------------------------------------------------
    template <typename T> void read( DataStreamPtr &dataStream, T &value )
    {
        dataStream->read( &value, sizeof(value) );
    }
    template <typename T> T read( DataStreamPtr &dataStream )
    {
        T value;
        dataStream->read( &value, sizeof(value) );
        return value;
    }
    void HlmsDiskCache::load( DataStreamPtr &dataStream, String &string )
    {
        const uint32 stringLength = read<uint32>( dataStream );
        string.resize( stringLength );
        if( stringLength > 0u )
            dataStream->read( &string[0], string.size() );
    }
    //-----------------------------------------------------------------------------------
    void HlmsDiskCache::load( DataStreamPtr &dataStream, HlmsPropertyVec &properties )
    {
        uint32 numEntries = read<uint32>( dataStream );
        properties.clear();
        properties.reserve( numEntries );

        HlmsPropertyVec::const_iterator itor = properties.begin();
        HlmsPropertyVec::const_iterator end  = properties.end();

        while( itor != end )
        {
            IdString keyName;
            int32 value;
            read( dataStream, keyName.mHash );
            read( dataStream, value );
            properties.push_back( HlmsProperty( keyName, value ) );
            ++itor;
        }
    }
    //-----------------------------------------------------------------------------------
    void HlmsDiskCache::load( DataStreamPtr &dataStream, Hlms::RenderableCache &renderableCache )
    {
        //Load the properties
        load( dataStream, renderableCache.setProperties );

        //Load the pieces
        for( size_t i=0; i<NumShaderTypes; ++i )
        {
            uint32 numEntries = read<uint32>( dataStream );
            //renderableCache.pieces[i].reserve( numEntries );

            for( size_t j=0; j<numEntries; ++j )
            {
                IdString key;
                String valueStr;
                read( dataStream, key.mHash );
                load( dataStream, valueStr );
                renderableCache.pieces[i][key] = valueStr;
            }
        }
    }
    //-----------------------------------------------------------------------------------
    void HlmsDiskCache::loadFrom( DataStreamPtr &dataStream )
    {
        clearCache();

        read( dataStream, mCache.templateHash );
        read( dataStream, mCache.type );

        {
            //Load shaders
            uint32 numEntries = read<uint32>( dataStream );
            mCache.sourceCode.reserve( numEntries );

            SourceCode sourceCode;
            for( size_t i=0; i<numEntries; ++i )
            {
                load( dataStream, sourceCode.mergedCache );
                for( size_t j=0; j<NumShaderTypes; ++j )
                    load( dataStream, sourceCode.sourceFile[j] );
                mCache.sourceCode.push_back( sourceCode );
            }
        }

        {
            //Load PSOs
            const uint32 numEntries = read<uint32>( dataStream );
            mCache.pso.reserve( numEntries );

            Pso pso;
            for( size_t i=0; i<numEntries; ++i )
            {
                load( dataStream, pso.renderableCache );
                load( dataStream, pso.passProperties );

                const uint32 numVertexElements = read<uint32>( dataStream );
                pso.pso.vertexElements.reserve( numVertexElements );

                for( size_t j=0; j<numVertexElements; ++j )
                {
                    pso.pso.vertexElements.push_back( VertexElement2Vec() );
                    VertexElement2Vec &vertexElements = pso.pso.vertexElements.back();

                    const uint32 numVertexElements2 = read<uint32>( dataStream );
                    vertexElements.reserve( numVertexElements2 );

                    for( size_t k=0; k<numVertexElements2; ++k )
                    {
                        VertexElementType type          = read<VertexElementType>( dataStream );
                        VertexElementSemantic semantic  = read<VertexElementSemantic>( dataStream );
                        uint32 instancingStepRate       = read<uint32>( dataStream );
                        vertexElements.push_back( VertexElement2( type, semantic ) );
                        vertexElements.back().mInstancingStepRate = instancingStepRate;
                    }
                }
            }

            read( dataStream, pso.pso.operationType );
            read( dataStream, pso.pso.enablePrimitiveRestart );
            read( dataStream, pso.pso.sampleMask );
            read( dataStream, pso.pso.pass );

            read( dataStream, pso.macroblock.mScissorTestEnabled );
            read( dataStream, pso.macroblock.mDepthCheck );
            read( dataStream, pso.macroblock.mDepthWrite );
            read( dataStream, pso.macroblock.mDepthFunc );
            read( dataStream, pso.macroblock.mDepthBiasConstant );
            read( dataStream, pso.macroblock.mDepthBiasSlopeScale );
            read( dataStream, pso.macroblock.mCullMode );
            read( dataStream, pso.macroblock.mPolygonMode );

            read( dataStream, pso.blendblock.mAlphaToCoverageEnabled );
            read( dataStream, pso.blendblock.mBlendChannelMask );
//          read( dataStream, pso.blendblock.mIsTransparent );
            read( dataStream, pso.blendblock.mSeparateBlend );
            read( dataStream, pso.blendblock.mSourceBlendFactor );
            read( dataStream, pso.blendblock.mDestBlendFactor );
            read( dataStream, pso.blendblock.mSourceBlendFactorAlpha );
            read( dataStream, pso.blendblock.mDestBlendFactorAlpha );
            read( dataStream, pso.blendblock.mBlendOperation );
            read( dataStream, pso.blendblock.mBlendOperationAlpha );

            //We retrieve the Macroblock & Blendblock from HlmsManager and immediately remove them
            //This allows us to create a permanent pointer, while the actual internal pointer is
            //released (i.e. it becomes inactive)
            pso.pso.macroblock = mHlmsManager->getMacroblock( pso.macroblock );
            mHlmsManager->destroyMacroblock( pso.pso.macroblock );

            pso.pso.blendblock = mHlmsManager->getBlendblock( pso.blendblock );
            mHlmsManager->destroyBlendblock( pso.pso.blendblock );

            mCache.pso.push_back( pso );
        }
    }
}

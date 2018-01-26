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

namespace Ogre {

    //--------------------------------------------------------------------------
    UserObjectBindings::UserObjectBindings() :
        mAttributes(NULL)
    {

    }

    //--------------------------------------------------------------------------
    UserObjectBindings::UserObjectBindings(const UserObjectBindings& other) :
        mAttributes(NULL)
    {
        if (other.mAttributes != NULL)
            mAttributes = OGRE_NEW UserObjectBindings::Attributes(*other.mAttributes);
    }

    //--------------------------------------------------------------------------
    UserObjectBindings::~UserObjectBindings()
    {
        clear();
    }

    //-----------------------------------------------------------------------
    void UserObjectBindings::setUserAny( const Any& anything )
    {
        // Allocate attributes on demand.
        if (mAttributes == NULL)
            mAttributes = OGRE_NEW UserObjectBindings::Attributes;

        mAttributes->mKeylessAny = anything;
    }

    //-----------------------------------------------------------------------
    const Any& UserObjectBindings::getUserAny( void ) const
    {
        // Allocate attributes on demand.
        if (mAttributes == NULL)
            mAttributes = OGRE_NEW UserObjectBindings::Attributes;

        return mAttributes->mKeylessAny;
    }

    //-----------------------------------------------------------------------
    void UserObjectBindings::setUserAny(const String& key, const Any& anything)
    {
        // Allocate attributes on demand.
        if (mAttributes == NULL)
            mAttributes = OGRE_NEW UserObjectBindings::Attributes;

        // Case map doesn't exists.
        if (mAttributes->mUserObjectsMap == NULL)
            mAttributes->mUserObjectsMap = OGRE_NEW_T(UserObjectsMap, MEMCATEGORY_GENERAL) ();

        (*mAttributes->mUserObjectsMap)[key] = anything;
    }

    //-----------------------------------------------------------------------
    const Any& UserObjectBindings::getUserAny(const String& key) const
    {
        static Any emptyAny;

        // Allocate attributes on demand.
        if (mAttributes == NULL)
            mAttributes = OGRE_NEW UserObjectBindings::Attributes;

        // Case map doesn't exists.
        if (mAttributes->mUserObjectsMap == NULL)
            return emptyAny;

        UserObjectsMapConstIterator it = mAttributes->mUserObjectsMap->find(key);

        // Case user data found.
        if (it != mAttributes->mUserObjectsMap->end())
        {
            return it->second;
        }

        return emptyAny;
    }

    //-----------------------------------------------------------------------
    void UserObjectBindings::eraseUserAny(const String& key)
    {
        // Case attributes and map allocated.
        if (mAttributes != NULL && mAttributes->mUserObjectsMap != NULL)
        {
            UserObjectsMapIterator it = mAttributes->mUserObjectsMap->find(key);

            // Case object found -> erase it from the map.
            if (it != mAttributes->mUserObjectsMap->end())
            {
                mAttributes->mUserObjectsMap->erase(it);
            }
        }
    }

    //-----------------------------------------------------------------------
    void UserObjectBindings::clear() const
    {
        if (mAttributes != NULL)
        {
            OGRE_DELETE mAttributes;
            mAttributes = NULL;
        }
    }
}

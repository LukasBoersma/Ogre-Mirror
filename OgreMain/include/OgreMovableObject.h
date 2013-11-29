/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2013 Torus Knot Software Ltd

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

#ifndef __MovableObject_H__
#define __MovableObject_H__

// Precompiler options
#include "OgrePrerequisites.h"
#include "OgreRenderQueue.h"
#include "OgreAxisAlignedBox.h"
#include "OgreSphere.h"
#include "OgreFactoryObj.h"
#include "OgreAnimable.h"
#include "OgreAny.h"
#include "OgreUserObjectBindings.h"
#include "OgreSceneNode.h"
#include "Math/Array/OgreObjectData.h"
#include "OgreId.h"
#include "OgreVisibilityFlags.h"
#include "OgreHeaderPrefix.h"

namespace Ogre {
	typedef vector<Frustum*>::type FrustumVec;

	// Forward declaration
	class MovableObjectFactory;

	/** \addtogroup Core
	*  @{
	*/
	/** \addtogroup Scene
	*  @{
	*/

	/** Abstract class defining a movable object in a scene.
        @remarks
            Instances of this class are discrete, relatively small, movable objects
            which are attached to SceneNode objects to define their position.
    */
	class _OgreExport MovableObject : public AnimableObject, public MovableAlloc, public IdObject
    {
    public:
        /** Listener which gets called back on MovableObject events.
        */
        class _OgreExport Listener
        {
        public:
            Listener(void) {}
            virtual ~Listener() {}
            /** MovableObject is being destroyed */
            virtual void objectDestroyed(MovableObject*) {}
            /** MovableObject has been attached to a node */
            virtual void objectAttached(MovableObject*) {}
            /** MovableObject has been detached from a node */
            virtual void objectDetached(MovableObject*) {}
            /** MovableObject has been moved */
            virtual void objectMoved(MovableObject*) {}
        };

    protected:
        /// node to which this object is attached
        Node* mParentNode;
		/// The render queue to use when rendering this object
        uint8 mRenderQueueID;
		/// The render queue group to use when rendering this object
		ushort mRenderQueuePriority;
		/// All the object data needed in SoA form
		ObjectData mObjectData;
		/// SceneManager holding this object (if applicable)
		SceneManager* mManager;

		//One for each submesh/material/Renderable
		FastArray<Real> const				*mLodMesh;
		FastArray< FastArray<Real> const * > mLodMaterial;
		unsigned char						mCurrentMeshLod;
		FastArray<unsigned char>			mCurrentMaterialLod;

		// Minimum pixel size to still render
		Real mMinPixelSize;
		/// User objects binding.
		UserObjectBindings mUserObjectBindings;

        /// MovableObject listener - only one allowed (no list) for size & performance reasons.
        Listener* mListener;

        /// List of lights for this object
        LightList mLightList;

		/// Is debug display enabled?
		bool mDebugDisplay;

		/// The memory manager used to allocate the ObjectData.
		ObjectMemoryManager *mObjectMemoryManager;

		/// Creator of this object (if created by a factory)
		MovableObjectFactory* mCreator;

#ifndef NDEBUG
		mutable bool mCachedAabbOutOfDate;
#endif

		/// Friendly name of this object, can be empty
		String mName;

		// Static members
		/// Default query flags
		static uint32 msDefaultQueryFlags;
		/// Default visibility flags
		static uint32 msDefaultVisibilityFlags;

	protected:
		Aabb updateSingleWorldAabb();
		float updateSingleWorldRadius();

    public:
		/** Index in the vector holding this MO reference (could be our parent node, or a global
			array tracking all movable objecst to avoid memory leaks). Used for O(1) removals.
		@remarks
			It is the parent (or our creator) the one that sets this value, not ourselves.
			Do NOT modify it manually.
		*/
		size_t mGlobalIndex;
		/// @copydoc mGlobalIndex
		size_t mParentIndex;

        /// Constructor
        MovableObject( IdType id, ObjectMemoryManager *objectMemoryManager,
						uint8 renderQueueId=RENDER_QUEUE_MAIN );

		/** Don't use this constructor unless you know what you're doing.
			@See ObjectMemoryManager::mDummyNode
		*/
		MovableObject( ObjectData *objectDataPtrs );

        /** Virtual destructor - read Scott Meyers if you don't know why this is needed.
        */
        virtual ~MovableObject();

		/** Notify the object of it's creator (internal use only) */
		virtual void _notifyCreator(MovableObjectFactory* fact) { mCreator = fact; }
		/** Get the creator of this object, if any (internal use only) */
		virtual MovableObjectFactory*  _getCreator(void) const { return mCreator; }
		/** Notify the object of it's manager (internal use only) */
		void _notifyManager(SceneManager* man) { mManager = man; }
		/** Get the manager of this object, if any (internal use only) */
		SceneManager* _getManager(void) const { return mManager; }

		/** Sets a custom name for this node. Doesn't have to be unique */
		void setName( const String &name )									{ mName = name; }

        /** Returns the name of this object. */
		const String& getName(void) const									{ return mName; }

        /** Returns the type name of this object. */
        virtual const String& getMovableType(void) const = 0;

        /// Returns the node to which this object is attached.
		Node* getParentNode(void) const										{ return mParentNode; }

		inline SceneNode* getParentSceneNode(void) const;

        /** Internal method called to notify the object that it has been attached to a node.
        */
        virtual void _notifyAttached( Node* parent );

        /** Returns true if this object is attached to a Node. */
		bool isAttached(void) const											{ return mParentNode != 0; }

		/** Detaches an object from a parent SceneNode if attached. */
		void detachFromParent(void);

        /** Internal method called to notify the object that it has been moved.
        */
        virtual void _notifyMoved(void);

		/// Checks whether this MovableObject is static. @See setStatic
		bool isStatic() const;

		/** Turns this Node into static or dynamic
		@remarks
			Switching between dynamic and static has some overhead and forces to update all
			static scene when converted to static. So don't do it frequently.
			Static objects are not updated every frame, only when requested explicitly. Use
			this feature if you plan to have this object unaltered for a very long times
		@par
			Note all MovableObjects support switching between static & dynamic after they
			have been created (eg. InstancedEntity)
		@par
			Changing this attribute will cause to switch the attribute to our parent node,
			and as a result, all of its other attached entities. @See Node::setStatic
		@return
			True if setStatic made an actual change. False otherwise. Can fail because the
			object was already static/dynamic, or because switching is not supported
		*/
		bool setStatic( bool bStatic );

		/// Called by SceneManager when it is telling we're a static MovableObject being dirty
		virtual void _notifyStaticDirty(void) const {}

        /** Retrieves the local axis-aligned bounding box for this object.
            @remarks
                This bounding box is in local coordinates.
        */
		Aabb getBoundingBox(void) const;

        /** Internal method by which the movable object must add Renderable subclass instances to the rendering queue.
            @remarks
                The engine will call this method when this object is to be rendered. The object must then create one or more
                Renderable subclass instances which it places on the passed in Queue for rendering.
        */
        virtual void _updateRenderQueue(RenderQueue* queue, Camera *camera, const Camera *lodCamera) = 0;

		/** @See SceneManager::updateAllBounds
		@remarks
			We don't pass by reference on purpose (avoid implicit aliasing)
		*/
		static void updateAllBounds( const size_t numNodes, ObjectData t );

		/** @See SceneManager::cullFrustum
		@remarks
			We don't pass by reference on purpose (avoid implicit aliasing)
			We perform frustum culling AND test visibility mask at the same time
		@param frustum
			Frustum to clip against
		@param sceneVisibilityFlags
			Combined scene's visibility flags (i.e. viewport | scene). Set LAYER_SHADOW_CASTER
			bit if you want to exclude non-shadow casters.
		@param outCulledObjects
			Out. List of objects that are (fully or partially) inside the frustum and
			should be rendered
		@param outReceiversBox [out]
			Bounds information from culled objects that are shadow receivers. Pointer can be null
		@param lodCamera
			Camera in which lod levels calculations are based (i.e. during shadow pass renders)
			Note however, we only use this camera to calulate if should be visible according to
			mUpperDistance
		*/
		typedef FastArray<MovableObject*> MovableObjectArray;
		static void cullFrustum( const size_t numNodes, ObjectData t, const Frustum *frustum,
								 uint32 sceneVisibilityFlags, MovableObjectArray &outCulledObjects,
								 AxisAlignedBox *outReceiversBox, const Camera *lodCamera );

		/// @See InstancingTheadedCullingMethod, @see InstanceBatch::instanceBatchCullFrustumThreaded
		virtual void instanceBatchCullFrustumThreaded( const Frustum *frustum,
														uint32 combinedVisibilityFlags ) {}

		/** Exactly the same as @see cullFrustum except that it doesn't produce outCulledObjects.
			Only useful when a shadow node needs to know the receiver boxes of other Render queue
			ranges that weren't calculated used in previous render scene passes.
		@remarks
			Because of the performance implication of this function, it's a copy-paste from cullFrustum,
			instead of refactoring in such a way that the code base is shared by both calulcations
			Ideally you shouldn't need this function (when a shadow node only renders the render queue
			ranges already calculated)
		@par
			There's a small difference with cullFrustum, as this routine assumes non-shadow casters are
			always included, since it's not meant to be called for casters-only (unlike cullFrustum).
		*/
		static void cullReceiversBox( const size_t numNodes, ObjectData t, const Frustum *frustum,
										uint32 sceneVisibilityFlags, AxisAlignedBox *outReceiversBox,
										const Camera *lodCamera );

		/** @See SceneManager::cullLights & @see MovableObject::cullFrustum
			Produces the global list of visible lights that is needed in buildLightList
		@remarks
			We don't pass ObjectData by reference on purpose (avoid implicit aliasing)
			It's declared here because all affected elements are from MovableObject
			IMPORTANT: It is assumed that all objects in ObjectData are Lights.
		@param outGlobalLightList
			Output, a list of lights, contiguously placed
		@param frustums
			An array of all frustums we need to check against
		*/
		static void cullLights( const size_t numNodes, ObjectData t, LightListInfo &outGlobalLightList,
								const FrustumVec &frustums );

		/** @See SceneManager::buildLightList
		@remarks
			We don't pass by reference on purpose (avoid implicit aliasing)
		@param globalLightList
			List of lights already culled against all possible frustums and
			reorganized contiguously for SoA
		*/
		static void buildLightList( const size_t numNodes, ObjectData t,
									const LightListInfo &globalLightList );

		static void calculateCastersBox( const size_t numNodes, ObjectData t,
										 uint32 sceneVisibilityFlags, AxisAlignedBox *outBox );

	protected:
		inline static void lodSet( ObjectData &t, Real lodValues[ARRAY_PACKED_REALS] );
	public:

		/** @See SceneManager::lodDistance
		@remarks
			Uses the distance to camera method to calculate the Lod value
			(will be cached in ObjectData::mOwner::mCurrentLod). @See mCurrentLod
		@param numNodes
			Total number of MovableObjects in ObjectData
		@param t
			SoA pointers to MovableObjects.
		@param camera
			Camera used for our Lod calculations.
		*/
		static void lodDistance( const size_t numNodes, ObjectData t, const Camera *camera );

		/** @See lodDistance
		@remarks
			Uses the visible pixel count method to calculate the Lod Value
		*/
		static void lodPixelCount( const size_t numNodes, ObjectData t, const Camera *camera );

	protected:
		static void lodPixelCountPerspective( const size_t numNodes, ObjectData t,
												const Camera *camera );
		static void lodPixelCountOrthographic( const size_t numNodes, ObjectData t,
												const Camera *camera );
	public:

        /** Tells this object whether to be visible or not, if it has a renderable component. 
		@note An alternative approach of making an object invisible is to detach it
			from it's SceneNode, or to remove the SceneNode entirely. 
			Detaching a node means that structurally the scene graph changes. 
			Once this change has taken place, the objects / nodes that have been 
			removed have less overhead to the visibility detection pass than simply
			making the object invisible, so if you do this and leave the objects 
			out of the tree for a long time, it's faster. However, the act of 
			detaching / reattaching nodes is in itself more expensive than 
			setting an object visibility flag, since in the latter case 
			structural changes are not made. Therefore, small or frequent visibility
			changes are best done using this method; large or more longer term
			changes are best done by detaching.
		*/
		inline void setVisible( bool visible );

        /** Gets this object whether to be visible or not, if it has a renderable component. 
        @remarks
            Returns the value set by MovableObject::setVisible only.
        */
		inline bool getVisible(void) const;

        /** Returns whether or not this object is supposed to be visible or not. 
		@remarks
			Takes into account visibility flags and the setVisible, but not rendering distance.
		*/
        bool isVisible(void) const;

		/** Sets the distance at which the object is no longer rendered.
		@param
			dist Distance beyond which the object will not be rendered (the default is FLT_MAX,
			which means objects are always rendered). Values equal or below zero will be ignored,
			and cause an assertion in debug mode.
		*/
		inline void setRenderingDistance(Real dist);

		/** Gets the distance at which batches are no longer rendered. */
		inline Real getRenderingDistance(void) const;

		/** Sets the minimum pixel size an object needs to be in both screen axes in order to be rendered
		@note Camera::setUseMinPixelSize() needs to be called for this parameter to be used.
		@param pixelSize Number of minimum pixels
			(the default is 0, which means objects are always rendered).
		*/
		void setRenderingMinPixelSize(Real pixelSize) { 
			mMinPixelSize = pixelSize; 
		}

		/** Returns the minimum pixel size an object needs to be in both screen axes in order to be
			rendered
		*/
		Real getRenderingMinPixelSize() const								{ return mMinPixelSize; }

		/** Return an instance of user objects binding associated with this class.
			You can use it to associate one or more custom objects with this class instance.
		@see UserObjectBindings::setUserAny.		
		*/
		UserObjectBindings&	getUserObjectBindings() { return mUserObjectBindings; }

		/** Return an instance of user objects binding associated with this class.
		You can use it to associate one or more custom objects with this class instance.
		@see UserObjectBindings::setUserAny.		
		*/
		const UserObjectBindings& getUserObjectBindings() const { return mUserObjectBindings; }

        /** Sets the render queue group this entity will be rendered through.
        @remarks
            Render queues are grouped to allow you to more tightly control the ordering
            of rendered objects. If you do not call this method, all Entity objects default
            to the default queue (RenderQueue::getDefaultQueueGroup), which is fine for most objects. You may want to alter this
            if you want this entity to always appear in front of other objects, e.g. for
            a 3D menu system or such.
        @par
            See RenderQueue for more details.
        @param queueID Enumerated value of the queue group to use. See the
			enum RenderQueueGroupID for what kind of values can be used here.
        */
        virtual void setRenderQueueGroup(uint8 queueID);

		/** Sets the render queue group and group priority this entity will be rendered through.
		@remarks
			Render queues are grouped to allow you to more tightly control the ordering
			of rendered objects. Within a single render group there another type of grouping
			called priority which allows further control.  If you do not call this method, 
			all Entity objects default to the default queue and priority 
			(RenderQueue::getDefaultQueueGroup, RenderQueue::getDefaultRenderablePriority), 
			which is fine for most objects. You may want to alter this if you want this entity 
			to always appear in front of other objects, e.g. for a 3D menu system or such.
		@par
			See RenderQueue for more details.
		@param queueID Enumerated value of the queue group to use. See the
			enum RenderQueueGroupID for what kind of values can be used here.
		@param priority The priority within a group to use.
		*/
		virtual void setRenderQueueGroupAndPriority(uint8 queueID, ushort priority);

        /** Gets the queue group for this entity, see setRenderQueueGroup for full details. */
        uint8 getRenderQueueGroup(void) const;

		/// Returns a direct access to the ObjectData state
		ObjectData& _getObjectData()										{ return mObjectData; }

		/// return the full transformation of the parent sceneNode or the attachingPoint node
		const Matrix4& _getParentNodeFullTransform(void) const;

		/** Gets the axis aligned box in world space.
		@remarks
			Assumes the caches are already updated. Will trigger an assert
			otherwise. @See getWorldAabbUpdated if you need the update process
			to be guaranteed
        */
		const Aabb getWorldAabb() const;

		/** Gets the axis aligned box in world space.
		@remarks
			Unlike getWorldAabb, this function guarantees the cache stays up to date.
			It is STRONGLY advised against calling this function for a large
			number of MovableObject. Refactor your queries so that they happen
			after SceneManager::updateAllBounds() has been called
		*/
		const Aabb getWorldAabbUpdated();

		/** Gets the bounding Radius scaled by max( scale.x, scale.y, scale.z ).
		@remarks
			Assumes the caches are already updated. Will trigger an assert
			otherwise. @See getWorldRadiusUpdated if you need the update process
			to be guaranteed
        */
		float getWorldRadius() const;

		/** Gets the bounding Radius scaled by max( scale.x, scale.y, scale.z ).
		@remarks
			Unlike getWorldRadius, this function guarantees the cache stays up to date.
			It is STRONGLY advised against calling this function for a large
			number of MovableObject. Refactor your queries so that they happen
			after SceneManager::updateAllBounds() has been called
		*/
		float getWorldRadiusUpdated();

        /** Sets the query flags for this object.
        @remarks
            When performing a scene query, this object will be included or excluded according
            to flags on the object and flags on the query. This is a bitwise value, so only when
            a bit on these flags is set, will it be included in a query asking for that flag. The
            meaning of the bits is application-specific.
        */
        inline void setQueryFlags(uint32 flags);

        /** As setQueryFlags, except the flags passed as parameters are appended to the
			existing flags on this object. */
        inline void addQueryFlags(uint32 flags);

        /** As setQueryFlags, except the flags passed as parameters are removed from the
			existing flags on this object. */
        inline void removeQueryFlags(uint32 flags);

        /// Returns the query flags relevant for this object
        inline uint32 getQueryFlags(void) const;

		/** Set the default query flags for all future MovableObject instances.
		*/
		static void setDefaultQueryFlags(uint32 flags) { msDefaultQueryFlags = flags; }

		/** Get the default query flags for all future MovableObject instances.
		*/
		static uint32 getDefaultQueryFlags() { return msDefaultQueryFlags; }


        /** Sets the visibility flags for this object.
        @remarks
			As well as a simple true/false value for visibility (as seen in setVisible), 
			you can also set visibility flags that is applied a binary 'and' with the SceneManager's
			mask and a compositor node pass. To exclude particular objects from rendering.
			Changes to reserved visibility flags are ignored (won't take effect).
        */
		inline void setVisibilityFlags(uint32 flags);

        /** As setVisibilityFlags, except the flags passed as parameters are appended to the
        existing flags on this object. */
        inline void addVisibilityFlags(uint32 flags);
            
        /** As setVisibilityFlags, except the flags passed as parameters are removed from the
        existing flags on this object. */
        inline void removeVisibilityFlags(uint32 flags);
        
        /** Returns the visibility flags relevant for this object. Reserved visibility flags are
			not returned.
		*/
        inline uint32 getVisibilityFlags(void) const;

		/** Set the default visibility flags for all future MovableObject instances.
		*/
		static void setDefaultVisibilityFlags(uint32 flags) { msDefaultVisibilityFlags = flags; }
		
		/** Get the default visibility flags for all future MovableObject instances.
		*/
		static uint32 getDefaultVisibilityFlags() { return msDefaultVisibilityFlags; }

        /** Sets a listener for this object.
        @remarks
            Note for size and performance reasons only one listener per object
            is allowed.
        */
        void setListener(Listener* listener) { mListener = listener; }

        /** Gets the current listener for this object.
        */
        Listener* getListener(void) const { return mListener; }

        /** Gets a list of lights, ordered relative to how close they are to this movable object.
        @remarks
            The lights are filled in @see buildLightList
        @return The list of lights use to lighting this object.
        */
		const LightList& queryLights(void) const								{ return mLightList; }

		/** Get a bitwise mask which will filter the lights affecting this object
		@remarks
			By default, this mask is fully set meaning all lights will affect this object
		*/
		inline uint32 getLightMask()const;
		/** Set a bitwise mask which will filter the lights affecting this object
		@remarks
		This mask will be compared against the mask held against Light to determine
			if a light should affect a given object. 
			By default, this mask is fully set meaning all lights will affect this object
		*/
		inline void setLightMask(uint32 lightMask);

		/** Returns a pointer to the current list of lights for this object.
		@remarks
			You should not modify this list outside of MovableObject::Listener::objectQueryLights
			(say if you want to use it to implement this method, and use the pointer
			as a return value) and for reading it's only accurate as at the last frame.
		*/
		LightList* _getLightList() { return &mLightList; }

        /** Sets whether or not this object will cast shadows.
        @remarks
        This setting simply allows you to turn on/off shadows for a given object.
        An object will not cast shadows unless the scene supports it in any case
        (see SceneManager::setShadowTechnique), and also the material which is
        in use must also have shadow casting enabled. By default all entities cast
        shadows. If, however, for some reason you wish to disable this for a single 
        object then you can do so using this method.
        @note This method normally refers to objects which block the light, but
        since Light is also a subclass of MovableObject, in that context it means
        whether the light causes shadows itself.
        */
        inline void setCastShadows( bool enabled );
        /** Returns whether shadow casting is enabled for this object. */
        inline bool getCastShadows(void) const;
		/** Returns whether the Material of any Renderable that this MovableObject will add to 
			the render queue will receive shadows. 
		*/
		bool getReceivesShadows();

		/** Get the 'type flags' for this MovableObject.
		@remarks
			A type flag identifies the type of the MovableObject as a bitpattern. 
			This is used for categorical inclusion / exclusion in SceneQuery
			objects. By default, this method returns all ones for objects not 
			created by a MovableObjectFactory (hence always including them); 
			otherwise it returns the value assigned to the MovableObjectFactory.
			Custom objects which don't use MovableObjectFactory will need to 
			override this if they want to be included in queries.
		*/
		virtual uint32 getTypeFlags(void) const;

		/** Method to allow a caller to abstractly iterate over the Renderable
			instances that this MovableObject will add to the render queue when
			asked, if any. 
		@param visitor Pointer to a class implementing the Renderable::Visitor 
			interface which will be called back for each Renderable which will
			be queued. Bear in mind that the state of the Renderable instances
			may not be finalised depending on when you call this.
		@param debugRenderables If false, only regular renderables will be visited
			(those for normal display). If true, debug renderables will be
			included too.
		*/
		virtual void visitRenderables(Renderable::Visitor* visitor, 
			bool debugRenderables = false) = 0;

		/** Sets whether or not the debug display of this object is enabled.
		@remarks
			Some objects aren't visible themselves but it can be useful to display
			a debug representation of them. Or, objects may have an additional 
			debug display on top of their regular display. This option enables / 
			disables that debug display. Objects that are not visible never display
			debug geometry regardless of this setting.
		*/
		virtual void setDebugDisplayEnabled(bool enabled) { mDebugDisplay = enabled; }
		/// Gets whether debug display of this object is enabled. 
		virtual bool isDebugDisplayEnabled(void) const { return mDebugDisplay; }





    };

	/** Interface definition for a factory class which produces a certain
		kind of MovableObject, and can be registered with Root in order
		to allow all clients to produce new instances of this object, integrated
		with the standard Ogre processing.
	*/
	class _OgreExport MovableObjectFactory : public MovableAlloc
	{
	protected:
		/// Type flag, allocated if requested
		uint32 mTypeFlag;

		/// Internal implementation of create method - must be overridden
		virtual MovableObject* createInstanceImpl( IdType id, ObjectMemoryManager *objectMemoryManager,
													const NameValuePairList* params = 0) = 0;
	public:
		MovableObjectFactory() : mTypeFlag(0xFFFFFFFF) {}
		virtual ~MovableObjectFactory() {}
		/// Get the type of the object to be created
		virtual const String& getType(void) const = 0;

		/** Create a new instance of the object.
		@param manager The SceneManager instance that will be holding the
			instance once created.
		@param params Name/value pair list of additional parameters required to 
			construct the object (defined per subtype). Optional.
		*/
		virtual MovableObject* createInstance( IdType id, ObjectMemoryManager *objectMemoryManager,
										SceneManager* manager, const NameValuePairList* params = 0);
		/** Destroy an instance of the object */
		virtual void destroyInstance(MovableObject* obj) = 0;

		/** Does this factory require the allocation of a 'type flag', used to 
			selectively include / exclude this type from scene queries?
		@remarks
			The default implementation here is to return 'false', ie not to 
			request a unique type mask from Root. For objects that
			never need to be excluded in SceneQuery results, that's fine, since
			the default implementation of MovableObject::getTypeFlags is to return
			all ones, hence matching any query type mask. However, if you want the
			objects created by this factory to be filterable by queries using a 
			broad type, you have to give them a (preferably unique) type mask - 
			and given that you don't know what other MovableObject types are 
			registered, Root will allocate you one. 
		*/
		virtual bool requestTypeFlags(void) const { return false; }
		/** Notify this factory of the type mask to apply. 
		@remarks
			This should normally only be called by Root in response to
			a 'true' result from requestTypeMask. However, you can actually use
			it yourself if you're careful; for example to assign the same mask
			to a number of different types of object, should you always wish them
			to be treated the same in queries.
		*/
		void _notifyTypeFlags(uint32 flag) { mTypeFlag = flag; }

		/** Gets the type flag for this factory.
		@remarks
			A type flag is like a query flag, except that it applies to all instances
			of a certain type of object.
		*/
		uint32 getTypeFlags(void) const { return mTypeFlag; }

	};

	class _OgreExport NullEntity : public MovableObject
	{
		static const String msMovableType;
	public:
		NullEntity() : MovableObject( 0 )
		{
		}

		virtual const String& getMovableType(void) const
		{
			return msMovableType;
		}
        virtual void _updateRenderQueue(RenderQueue* queue, Camera *camera, const Camera *lodCamera) {}
		virtual void visitRenderables(Renderable::Visitor* visitor, 
			bool debugRenderables = false) {}
	};

	/** @} */
	/** @} */

}

#include "OgreHeaderSuffix.h"

#include "OgreMovableObject.inl"

#endif

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

#ifndef __CompositorPass_H__
#define __CompositorPass_H__

#include "OgreHeaderPrefix.h"

#include "Compositor/Pass/OgreCompositorPassDef.h"

namespace Ogre
{
	class RenderTarget;
	struct CompositorChannel;

	/** \addtogroup Core
	*  @{
	*/
	/** \addtogroup Effects
	*  @{
	*/

	/** Abstract class for compositor passes. A pass can be a fullscreen quad, a scene
		rendering, a clear. etc.
		Derived classes are responsible for performing an actual job.
		Note that passes do not own RenderTargets, therefore we're not responsible
		for destroying it.
    @author
		Matias N. Goldberg
    @version
        1.0
    */
	class _OgreExport CompositorPass : public CompositorInstAlloc
	{
		CompositorPassDef const *mDefinition;
	protected:
		RenderTarget	*mTarget;
		Viewport		*mViewport;

	public:
		CompositorPass( const CompositorPassDef *definition, RenderTarget *target );
		virtual ~CompositorPass();

		virtual void execute() = 0;

		/// @See CompositorNode::notifyDestroyed
		virtual void notifyDestroyed( const CompositorChannel &channel );

		CompositorPassType getType() const	{ return mDefinition->getType(); }

		Viewport* getViewport() const		{ return mViewport; }
	};

	/** @} */
	/** @} */
}

#include "OgreHeaderSuffix.h"

#endif

/*  Berkelium Implementation
 *  MemoryRenderViewHost.hpp
 *
 *  Copyright (c) 2009, Patrick Reiter Horn
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Sirikata nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _BERKELIUM_MEMORYRENDERVIEWHOST_HPP_
#define _BERKELIUM_MEMORYRENDERVIEWHOST_HPP_

#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_factory.h"

class RenderWidgetHostView;
namespace Berkelium {
class WindowImpl;
class RenderWidget;

class MemoryRenderHostBase {
    MemoryRenderHostBase() {}
    virtual ~MemoryRenderHostBase() {}

    virtual void Memory_OnMsgUpdateRect(const ViewHostMsg_UpdateRect_Params&params)=0;
    virtual void Memory_PaintBackingStoreRect(TransportDIB* bitmap,
                                      const gfx::Rect& bitmap_rect,
                                      const std::vector<gfx::Rect>& copy_rects,
                                      const gfx::Size& view_size,
                                      int dx, int dy,
                                      const gfx::Rect& clip_rect)=0;

};

template <class RenderXHost> class MemoryRenderHostImpl: public RenderXHost {
    void init();
protected:
    template<class A, class B, class C, class D> MemoryRenderHostImpl(A a, B b, C c, D d):RenderXHost(a,b,c,d) {init();}
    template<class A, class B, class C> MemoryRenderHostImpl(A a, B b, C c):RenderXHost(a,b,c) {init();}
    template<class A, class B> MemoryRenderHostImpl(A a, B b):RenderXHost(a,b) {   init();}
    ~MemoryRenderHostImpl() {}

public:
    void Memory_WasResized();
    void Memory_OnMsgUpdateRect(const ViewHostMsg_UpdateRect_Params&params);
    virtual void Memory_PaintBackingStoreRect(TransportDIB* bitmap,
                                      const gfx::Rect& bitmap_rect,
                                      const std::vector<gfx::Rect>& copy_rects,
                                      const gfx::Size& view_size,
                                      int dx, int dy,
                                      const gfx::Rect& clip_rect);
protected:
    WindowImpl *mWindow;
    RenderWidget *mWidget;
    gfx::Size current_size_;
    bool mResizeAckPending;
    gfx::Size mInFlightSize;
};

class MemoryRenderWidgetHost : public MemoryRenderHostImpl<RenderWidgetHost> {
public:
    MemoryRenderWidgetHost(
        RenderViewHostDelegate *win,
        RenderWidgetHostView*wid,
        RenderProcessHost* process,
        int routing_id);
    ~MemoryRenderWidgetHost();
    virtual bool OnMessageReceived(const IPC::Message& msg);
    virtual void Memory_OnMsgUpdateRect(const ViewHostMsg_UpdateRect_Params&params);

};

class MemoryRenderViewHost : public MemoryRenderHostImpl <RenderViewHost> {
public:
    MemoryRenderViewHost(
        SiteInstance* instance,
        RenderViewHostDelegate* delegate,
        int routing_id,
        SessionStorageNamespace *session_storage_namespace_id);
    ~MemoryRenderViewHost();

    void Memory_OnAddMessageToConsole(
        const std::wstring& message,
        int32 line_no,
        const std::wstring& source_id);
    virtual bool OnMessageReceived(const IPC::Message& msg);
};

class MemoryRenderViewHostFactory : public RenderViewHostFactory {
public:

    ~MemoryRenderViewHostFactory();
    MemoryRenderViewHostFactory();

    virtual RenderViewHost* CreateRenderViewHost(
        SiteInstance* instance,
        RenderViewHostDelegate* delegate,
        int routing_id,
        SessionStorageNamespace *ssn_id);
};


}

#endif

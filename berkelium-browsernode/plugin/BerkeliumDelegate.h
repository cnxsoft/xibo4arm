
#ifndef _BerkeliumDelegate_H_
#define _BerkeliumDelegate_H_

#include "berkelium/Berkelium.hpp"
#include "berkelium/Window.hpp"
#include "berkelium/WindowDelegate.hpp"
#include "berkelium/Context.hpp"
#include "../../base/Point.h"
#include <boost/python.hpp>
#include <iostream>
#include <sstream>
#include <stdio.h>

namespace avg {

using namespace Berkelium;
using boost::python::object;

class ErrorDelegateWrapper : public ::Berkelium::ErrorDelegate {

public:
    ErrorDelegateWrapper ();

    virtual void onPureCall();
    virtual void onInvalidParameter(const wchar_t *expression, const wchar_t *function,
             const wchar_t *file, unsigned int line, unsigned int* reserved);
    virtual void onOutOfMemory();
    virtual void onAssertion(const char *assertMessage);
};


class BerkeliumDelegate : public WindowDelegate {

public:
    BerkeliumDelegate();
    virtual ~BerkeliumDelegate();

    virtual void onLoadingStateChanged(Window* pWindow, bool isLoading);
    virtual void onLoadError(Window* pWindow, WideString error);

    virtual void onPaint(Window* pWindowi, const unsigned char *pBitmapIn,
            const Rect &bitmapRect, size_t numCopyRects, const Rect *pCopyRects,
            int dx, int dy, const Rect &scrollRect);

    //virtual void onAddressBarChanged(Window* pWindow, URLString newURL);
    //virtual void onCreatedWindow(Window* pWindow, Window *newWindow,
    //        const Rect &initialRect);
    //virtual void onExternalHost(Window* pWindow, WideString message,
    //        URLString origin, URLString target);
    virtual void onPaintPluginTexture(Window* pWindow, void* sourceGLTexture,
            const std::vector<Rect> srcRects, const Rect &destRect);

    //Executing python callbacks
    virtual void onJavascriptCallback(Window* win, void* replyMsg, URLString origin,
            WideString funcName, Script::Variant *args, size_t numArgs);
    virtual void onFinishLoading(Window* pWindow);
    virtual void onCrashed(Window* pWindow);
    virtual void onCrashedPlugin(Window* pWindow, WideString pluginName);
    virtual void onResponsive(Window* pWindow);
    virtual void onUnresponsive(Window* pWindow);

    //Callback setters/getters
    void setJavascriptCb(const object& cb);
    const object& getJavascriptCb() const;
    void setFinishLoadingCb(const object& cb);
    const object& getFinishLoadingCb() const;
    void setCrashedCb(const object& cb);
    const object& getCrashedCb() const;
    void setCrashedPluginCb(const object& cb);
    const object& getCrashedPluginCb() const;
    void setResponsiveCb(const object& cb);
    const object& getResponsiveCb() const;
    void setUnresponsiveCb(const object& cb);
    const object& getUnresponsiveCb() const;

protected:
    void allocateBuffer(IntPoint size, IntPoint offset);
    unsigned char* getBuffer();
    int getBufferLength();
    bool isDirty();
    IntPoint getOffset();
    void cleanDirtyFlag();

private:
    std::string m_URL;
    unsigned char* m_pBuffer;
    unsigned char* m_pPrintBuffer;
    bool m_bDirty;
    IntPoint m_Size;
    IntPoint m_PrintSize;
    int m_BufferLength;
    IntPoint m_Offset;
    
    //python callbacks
    object m_JavascriptCallback;
    object m_FinishLoadingCallback;
    object m_CrashedCallback;
    object m_CrashedPluginCallback;
    object m_ResponsiveCallback;
    object m_UnresponsiveCallback;

};

}

#endif



#include "BerkeliumDelegate.h"
#include "../../base/Exception.h"
#include <sstream>

#define BYTES_PER_PIXEL 4


namespace avg {

using namespace Berkelium;
using std::cerr;
using std::endl;
using std::string;
using std::wstring;
using boost::python::object;
using boost::python::call;



ErrorDelegateWrapper::ErrorDelegateWrapper () {}

void ErrorDelegateWrapper::onPureCall() {
    cerr << "onPureCall " << endl;
}
void ErrorDelegateWrapper::onInvalidParameter(const wchar_t *expression, const wchar_t *function,
             const wchar_t *file, unsigned int line, unsigned int* reserved)
{
    cerr << "onInvalidParameter " << endl;
}
void ErrorDelegateWrapper::onOutOfMemory(){
    cerr << "onOutOfMemory" << endl;
}
void ErrorDelegateWrapper::onAssertion(const char *assertMessage){
    cerr << "onAssertion" << endl;
}

BerkeliumDelegate::BerkeliumDelegate()
    : m_pBuffer(0),
      m_pPrintBuffer(0),
      m_bDirty(true),
      m_Size(0, 0),
      m_PrintSize(0, 0),
      m_BufferLength(0),
      m_Offset(0, 0)
{
}

BerkeliumDelegate::~BerkeliumDelegate()
{    
    if (m_pPrintBuffer != m_pBuffer) {
        delete[] m_pPrintBuffer;
    }
    delete[] m_pBuffer;
}

void BerkeliumDelegate::onLoadError(Window* pWindow, WideString error)
{
    std::cerr << L"*** onLoadError " << m_URL << ": ";
    std::wcerr << error << std::endl;
}


void BerkeliumDelegate::allocateBuffer(IntPoint size, IntPoint offset)
{
    if (m_pPrintBuffer != m_pBuffer) {
        delete[] m_pPrintBuffer;
    }
    delete[] m_pBuffer;
    m_Offset = offset;
    m_PrintSize = size;
    m_Size = size + offset;
    m_BufferLength = BYTES_PER_PIXEL * m_PrintSize.x * m_PrintSize.y;
    m_pPrintBuffer = new unsigned char[m_BufferLength];
    memset(m_pPrintBuffer, 0, m_BufferLength);
    if (m_Offset.x || m_Offset.y) {
        m_pBuffer = new unsigned char[m_Size.x*m_Size.y*BYTES_PER_PIXEL];
    } else {
        m_pBuffer = m_pPrintBuffer;
    }
}

unsigned char* BerkeliumDelegate::getBuffer()
{
    return m_pPrintBuffer;
}

int BerkeliumDelegate::getBufferLength()
{
    return m_BufferLength;
}

IntPoint BerkeliumDelegate::getOffset()
{
    return m_Offset;
}

bool BerkeliumDelegate::isDirty()
{
    return m_bDirty;
}

void BerkeliumDelegate::cleanDirtyFlag()
{
    m_bDirty = false;
}

void BerkeliumDelegate::onPaint(Window* pWindow, const unsigned char *pBitmapIn,
        const Rect &bitmapRect, size_t numCopyRects, const Rect *pCopyRects,
        int dx, int dy, const Rect &scrollRect)
{
    if (!m_pBuffer) {
        return;
    }

    // Now, we first handle scrolling. We need to do this first since it
    // requires shifting existing data, some of which will be overwritten by
    // the regular dirty rect update.
    if (dx != 0 || dy != 0) {
        // scroll_rect contains the Rect we need to move
        // First we figure out where the the data is moved to by translating it
        Berkelium::Rect scrolled_rect = scrollRect.translate(-dx, -dy);
        // Next we figure out where they intersect, giving the scrolled
        // region
        Berkelium::Rect scrolled_shared_rect = scrollRect.intersect(scrolled_rect);
        // Only do scrolling if they have non-zero intersection
        if (scrolled_shared_rect.width() > 0 && scrolled_shared_rect.height() > 0) {
            // And the scroll is performed by moving shared_rect by (dx,dy)
            Berkelium::Rect shared_rect = scrolled_shared_rect.translate(dx, dy);
            int left = shared_rect.left();
            int top = shared_rect.top();
            int right = left + shared_rect.width();
            int bottom = top + shared_rect.height();
            int xInit,yInit,xInc,yInc;
            if (dx > 0) {
                xInit = right - 1;
                xInc = -1;
            } else {
                xInit = left;
                xInc = 1;
            }
            if (dy > 0) {
                yInit = bottom - 1;
                yInc = -1;
            } else {
                yInit = top;
                yInc = 1;
            }
            for (int y = yInit; y >= top && y < bottom; y += yInc) {
                for (int x = xInit; x >= left && x < right; x += xInc) {
                    int source = (x - dx) + (y - dy)*m_Size.x;
                    int dest = x + y*m_Size.x;
                    uint32_t *pBuffer = (uint32_t*)m_pBuffer;
                    uint32_t *pPrintBuffer = (uint32_t*)m_pPrintBuffer;
                    pBuffer[dest] = pBuffer[source];
                    if (m_pPrintBuffer != m_pBuffer && x >= m_Offset.x && y >= m_Offset.y) {
                        int pixel = (x-m_Offset.x) + (y-m_Offset.y)*m_PrintSize.x;
                        pPrintBuffer[pixel] = pBuffer[source];
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < numCopyRects; i++) {
        int top = pCopyRects[i].top();
        int bottom = top + pCopyRects[i].height();
        AVG_ASSERT(bottom <= m_Size.y);
        int left = pCopyRects[i].left();
        int right = left + pCopyRects[i].width();
        AVG_ASSERT(right <= m_Size.x);
        for (int y = top; y < bottom; ++y) {
            for (int x = left; x < right; ++x) {
                int source = (y-bitmapRect.top())*bitmapRect.width() + (x-bitmapRect.left());
                unsigned char r,g,b,a;
                b = pBitmapIn[source*BYTES_PER_PIXEL];
                g = pBitmapIn[source*BYTES_PER_PIXEL+1];
                r = pBitmapIn[source*BYTES_PER_PIXEL+2];
                a = pBitmapIn[source*BYTES_PER_PIXEL+3];
                int pixel = x + y*m_Size.x;
                m_pBuffer[pixel*BYTES_PER_PIXEL] = r;
                m_pBuffer[pixel*BYTES_PER_PIXEL+1] = g;
                m_pBuffer[pixel*BYTES_PER_PIXEL+2] = b;
                m_pBuffer[pixel*BYTES_PER_PIXEL+3] = a;
                if (m_pPrintBuffer != m_pBuffer && x >= m_Offset.x && y >= m_Offset.y) {
                    pixel = (x-m_Offset.x) + (y-m_Offset.y)*m_PrintSize.x;
                    m_pPrintBuffer[pixel*BYTES_PER_PIXEL] = r;
                    m_pPrintBuffer[pixel*BYTES_PER_PIXEL+1] = g;
                    m_pPrintBuffer[pixel*BYTES_PER_PIXEL+2] = b;
                    m_pPrintBuffer[pixel*BYTES_PER_PIXEL+3] = a;
                }
            }
        }
    }
    m_bDirty = true;

    /*static int call_count = 0;
    FILE *outfile;
    FILE *outfile2;
    {
        std::ostringstream os;
        os << "/tmp/chromium_render_" << time(NULL) << "_" << (call_count++) << ".ppm";
        std::string str (os.str());
        outfile = fopen(str.c_str(), "wb");
        std::ostringstream os2;
        os2 << "/tmp/chromium_render_" << time(NULL) << "_" << (call_count++) << ".a";
        std::string str2 (os2.str());
        outfile2 = fopen(str2.c_str(), "wb");
    }
    fprintf(outfile, "P6 %d %d 255\n", m_Size.x, m_Size.y);
    unsigned char*ptr = m_pBuffer;
    for (int y = 0; y < m_Size.y; ++y) {
        for (int x = 0; x < m_Size.x; ++x) {
            unsigned char r,g,b,a;
            r = *(ptr++);
            g = *(ptr++);
            b = *(ptr++);
            a = *(ptr++);
            fputc(r, outfile);  // Red
            fputc(g, outfile);  // Green
            fputc(b, outfile);  // Blue
            fputc(a, outfile2);  // Alpha
        }
    }
    fclose(outfile);
    fclose(outfile2);*/

}

/*
void BerkeliumDelegate::onCreatedWindow(Window* pWindow, Window *newWindow,
       const Rect &initialRect)
{
    std::cerr << "*** onCreatedWindow from source "<<m_URL<<std::endl;
}

void BerkeliumDelegate::onExternalHost(Window* pWindow, WideString message,
        URLString origin, URLString target)
{
    std::cerr << "*** onChromeSend at URL "<<m_URL<<" from "<<origin<<" to "<<target<<":"<<std::endl;
}

void BerkeliumDelegate::onAddressBarChanged(Window* pWindow, URLString newURL) {
    m_URL = newURL.get<std::string>();
    std::cerr << "*** onAddressChanged to "<<newURL<<std::endl;
}

void BerkeliumDelegate::onStartLoading(Window* pWindow, URLString newURL) {
    std::cerr << "*** Start loading "<< newURL << " from " << m_URL << std::endl;
}
*/

void BerkeliumDelegate::onPaintPluginTexture(Window* pWindow, void* sourceGLTexture,
        const std::vector<Rect> srcRects, // relative to destRect
        const Rect &destRect)
{
    std::cerr << "*** onPaintPluginTexture" << m_URL << std::endl;
}

void BerkeliumDelegate::onLoadingStateChanged(Window* pWindow, bool isLoading)
{
    //std::cerr << "*** Loading state changed "<<m_URL<<" to "<<(isLoading?"loading":"stopped")<<std::endl;
    if (!isLoading) {
        onFinishLoading(pWindow);
    }
}

// Callbacks
void BerkeliumDelegate::onJavascriptCallback(Window* pWindow, void* replyMsg,
        URLString origin, WideString funcName, Script::Variant* args, size_t numArgs)
{
    if (m_JavascriptCallback != object())
        call<void>(m_JavascriptCallback.ptr(), funcName, args, numArgs);
}

void BerkeliumDelegate::onFinishLoading(Window* pWindow)
{
    if (m_FinishLoadingCallback != object()) {
        call<void>(m_FinishLoadingCallback.ptr());
    }
}

void BerkeliumDelegate::onCrashed(Window* pWindow)
{
    std::cerr << "*** onCrashed "<<m_URL<<std::endl;
    if (m_CrashedCallback != object()) {
        call<void>(m_CrashedCallback.ptr());
    }
}

void BerkeliumDelegate::onCrashedPlugin(Window* pWindow, WideString pluginName)
{
    std::cerr << "*** onCrashedPlugin "<<m_URL<<std::endl;
    if (m_CrashedPluginCallback != object()) {
        call<void>(m_CrashedPluginCallback.ptr(), pluginName);
    }
}

void BerkeliumDelegate::onResponsive(Window* pWindow)
{
    std::cerr << "*** onResponsive "<<m_URL<<std::endl;
    if (m_ResponsiveCallback != object()) {
        call<void>(m_ResponsiveCallback.ptr());
    }
}

void BerkeliumDelegate::onUnresponsive(Window* pWindow)
{
    std::cerr << "*** onUnresponsive "<<m_URL<<std::endl;
    if (m_UnresponsiveCallback != object()) {
        call<void>(m_UnresponsiveCallback.ptr());
    }
}


// Callback Setters - Getters

void BerkeliumDelegate::setFinishLoadingCb(const object& callback)
{
    m_FinishLoadingCallback = callback;
}

const object& BerkeliumDelegate::getFinishLoadingCb() const
{
    return m_FinishLoadingCallback;
}

void BerkeliumDelegate::setJavascriptCb(const object& callback)
{
    m_JavascriptCallback = callback;
}

const object& BerkeliumDelegate::getJavascriptCb() const
{
    return m_JavascriptCallback;
}

void BerkeliumDelegate::setCrashedCb(const object& callback)
{
    m_CrashedCallback = callback;
}

const object& BerkeliumDelegate::getCrashedCb() const
{
    return m_CrashedCallback;
}

void BerkeliumDelegate::setCrashedPluginCb(const object& callback)
{
    m_CrashedPluginCallback = callback;
}

const object& BerkeliumDelegate::getCrashedPluginCb() const
{
    return m_CrashedPluginCallback;
}

void BerkeliumDelegate::setResponsiveCb(const object& callback)
{
    m_ResponsiveCallback = callback;
}

const object& BerkeliumDelegate::getResponsiveCb() const
{
    return m_ResponsiveCallback;
}

void BerkeliumDelegate::setUnresponsiveCb(const object& callback)
{
    m_UnresponsiveCallback = callback;
}

const object& BerkeliumDelegate::getUnresponsiveCb() const
{
    return m_UnresponsiveCallback;
}

}
 

//
//  libavg - Media Playback Engine. 
//  Copyright (C) 2003-2011 Ulrich von Zadow
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//  Current versions can be found at www.libavg.de
//

#include "GLContext.h"

#include "ShaderRegistry.h"
#include "StandardShader.h"

#include "../base/Exception.h"
#include "../base/Logger.h"
#include "../base/MathHelper.h"

#include <iostream>


namespace avg {

using namespace std;
using namespace boost;
thread_specific_ptr<GLContext*> GLContext::s_pCurrentContext;

#ifdef _WIN32
LONG WINAPI imagingWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{ 
    return DefWindowProc(hwnd, msg, wParam, lParam); 
} 

void registerWindowClass()
{
  static char * pClassName;
  if (pClassName) {
      return;
  }
  pClassName = "GL";

  HINSTANCE hInstance = GetModuleHandle(NULL);
  WNDCLASS  wc;
  memset(&wc, 0, sizeof(WNDCLASS));
  wc.style = CS_OWNDC;
  wc.lpfnWndProc = (WNDPROC)imagingWindowProc;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
  wc.hCursor = LoadCursor(hInstance, IDC_ARROW);
  wc.hbrBackground = NULL;
  wc.lpszMenuName = NULL;
  wc.lpszClassName = pClassName;
  
  BOOL bOK = RegisterClass(&wc);
  AVG_ASSERT(bOK);
}
#endif

#ifdef linux
static bool s_bX11Error;
static int (*s_DefaultErrorHandler) (Display *, XErrorEvent *);

int X11ErrorHandler(Display * pDisplay, XErrorEvent * pErrEvent)
{
    cerr << "X11 error creating offscreen context: " << (int)(pErrEvent->request_code)
            << ", " << (int)(pErrEvent->minor_code) << endl;
    s_bX11Error = true;
    return 0;
}
#endif

GLContext::VBMethod GLContext::s_VBMethod = VB_NONE;

GLContext::GLContext(bool bUseCurrent, const GLConfig& glConfig, 
        GLContext* pSharedContext)
    : m_Context(0),
      m_MaxTexSize(0),
      m_bCheckedGPUMemInfoExtension(false),
      m_bCheckedMemoryMode(false),
      m_bEnableTexture(false),
      m_bEnableGLColorArray(true),
      m_BlendMode(BLEND_ADD),
      m_bErrorCheckEnabled(false)
{
    if (bUseCurrent) {
        AVG_ASSERT(!pSharedContext);
    }
    if (s_pCurrentContext.get() == 0) {
        s_pCurrentContext.reset(new (GLContext*));
    }
    m_GLConfig = glConfig;
    m_bOwnsContext = !bUseCurrent;
    if (bUseCurrent) {
#if defined(__APPLE__)
        m_Context = CGLGetCurrentContext();
#elif defined(__linux__)
        m_pDisplay = glXGetCurrentDisplay();
        m_Drawable = glXGetCurrentDrawable();
        m_Context = glXGetCurrentContext();
#elif defined(_WIN32)
        m_hDC = wglGetCurrentDC();
        m_Context = wglGetCurrentContext();
#endif
        *s_pCurrentContext = this;
    } else {
#ifdef __APPLE__
        CGLPixelFormatObj   pixelFormatObj;
        GLint               numPixelFormats;

        CGLPixelFormatAttribute attribs[] = {(CGLPixelFormatAttribute)NULL};
        CGLContextObj cglSharedContext;
        if (pSharedContext) {
            cglSharedContext = pSharedContext->m_Context;
            pixelFormatObj = CGLGetPixelFormat(cglSharedContext);
        } else {
            cglSharedContext = 0;
            CGLChoosePixelFormat(attribs, &pixelFormatObj, &numPixelFormats);
        }

        CGLError err = CGLCreateContext(pixelFormatObj, cglSharedContext, &m_Context);
        if (err) {
            cerr << CGLErrorString(err) << endl;
            AVG_ASSERT(false);
        }
        CGLDestroyPixelFormat(pixelFormatObj);
#elif defined(__linux__)
        m_pDisplay = XOpenDisplay(0);
        if (!m_pDisplay) {
            throw Exception(AVG_ERR_VIDEO_GENERAL, "No X windows display available.");
        }
        XVisualInfo *vi;
        static int attributes[] = {GLX_RGBA,
            GLX_RED_SIZE, 1,
            GLX_GREEN_SIZE, 1,
            GLX_BLUE_SIZE, 1,
            0};
        vi = glXChooseVisual(m_pDisplay, DefaultScreen(m_pDisplay), attributes);
        m_Context = glXCreateContext(m_pDisplay, vi, 0, GL_TRUE);
        AVG_ASSERT(m_Context);
        Pixmap pmp = XCreatePixmap(m_pDisplay, RootWindow(m_pDisplay, vi->screen),
                8, 8, vi->depth);
        GLXPixmap pixmap = glXCreateGLXPixmap(m_pDisplay, vi, pmp);

        s_bX11Error = false;
        s_DefaultErrorHandler = XSetErrorHandler(X11ErrorHandler);
        glXMakeCurrent(m_pDisplay, pixmap, m_Context);
        XSetErrorHandler(s_DefaultErrorHandler);

        if (s_bX11Error) {
            throw Exception(AVG_ERR_VIDEO_GENERAL, "X error creating OpenGL context.");
        }
        m_Drawable = glXGetCurrentDrawable();
#elif defined(_WIN32)
        registerWindowClass();
        m_hwnd = CreateWindow("GL", "GL",
                WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                0, 0, 500, 300, 0, 0, GetModuleHandle(NULL), 0);
        checkWinError(m_hwnd != 0, "CreateWindow");

        m_hDC = GetDC(m_hwnd);
        winOGLErrorCheck(m_hDC != 0, "GetDC");

        PIXELFORMATDESCRIPTOR pfd;
        ZeroMemory(&pfd, sizeof(pfd));
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 32;
        pfd.iLayerType = PFD_MAIN_PLANE;

        int iFormat = ChoosePixelFormat(m_hDC, &pfd);
        checkWinError(iFormat != 0, "ChoosePixelFormat");
        SetPixelFormat(m_hDC, iFormat, &pfd);
        m_Context = wglCreateContext(m_hDC);
        checkWinError(m_Context != 0, "wglCreateContext");
#endif
    }

    init();
}

GLContext::~GLContext()
{
    m_pStandardShader = StandardShaderPtr();
    for (unsigned i=0; i<m_FBOIDs.size(); ++i) {
        glproc::DeleteFramebuffers(1, &(m_FBOIDs[i]));
    }
    m_FBOIDs.clear();
    if (*s_pCurrentContext == this) {
        *s_pCurrentContext = 0;
    }
    if (m_bOwnsContext && m_Context) {
#ifdef __APPLE__
        CGLSetCurrentContext(0);
        CGLDestroyContext(m_Context);
        m_Context = 0;
#elif defined _WIN32
        wglDeleteContext(m_Context);
        DeleteDC(m_hDC);
        DestroyWindow(m_hwnd);
#endif
    }
}

void GLContext::init()
{
    activate();
    glproc::init();
    m_pShaderRegistry = ShaderRegistryPtr(new ShaderRegistry());
    if (useGPUYUVConversion()) {
        m_pShaderRegistry->setPreprocessorDefine("ENABLE_YUV_CONVERSION", "");
    }
    enableGLColorArray(false);
    setBlendMode(BLEND_BLEND, false);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    if (!m_GLConfig.m_bUsePOTTextures) {
        m_GLConfig.m_bUsePOTTextures = 
                !queryOGLExtension("GL_ARB_texture_non_power_of_two");
    }
}

void GLContext::activate()
{
#ifdef __APPLE__
    CGLError err = CGLSetCurrentContext(m_Context);
    AVG_ASSERT(err == kCGLNoError);
#elif defined linux
    glXMakeCurrent(m_pDisplay, m_Drawable, m_Context);
#elif defined _WIN32
    BOOL bOk = wglMakeCurrent(m_hDC, m_Context);
    checkWinError(bOk, "wglMakeCurrent");
#endif
    *s_pCurrentContext = this;
}

ShaderRegistryPtr GLContext::getShaderRegistry() const
{
    return m_pShaderRegistry;
}

StandardShaderPtr GLContext::getStandardShader()
{
    if (m_pStandardShader == StandardShaderPtr()) {
        m_pStandardShader = StandardShaderPtr(new StandardShader());
    }
    return m_pStandardShader;
}

bool GLContext::useGPUYUVConversion() const
{
    int majorVer;
    int minorVer;
    getGLVersion(majorVer, minorVer);
    return (majorVer > 1);
}

bool GLContext::useMinimalShader() const
{
    int majorVer;
    int minorVer;
    getGLVersion(majorVer, minorVer);
    return (majorVer <= 1);
}

GLBufferCache& GLContext::getVertexBufferCache()
{
    return m_VertexBufferCache;
}

GLBufferCache& GLContext::getIndexBufferCache()
{
    return m_IndexBufferCache;
}

GLBufferCache& GLContext::getPBOCache()
{
    return m_PBOCache;
}

unsigned GLContext::genFBO()
{
    unsigned fboID;
    if (m_FBOIDs.empty()) {
        glproc::GenFramebuffers(1, &fboID);
    } else {
        fboID = m_FBOIDs.back();
        m_FBOIDs.pop_back();
    }
    return fboID;
}

void GLContext::returnFBOToCache(unsigned fboID) 
{
    m_FBOIDs.push_back(fboID);
}

void GLContext::enableTexture(bool bEnable)
{
    if (bEnable != m_bEnableTexture) {
        if (bEnable) {
            glEnable(GL_TEXTURE_2D);
        } else {
            glDisable(GL_TEXTURE_2D);
        }
        m_bEnableTexture = bEnable;
    }
}

void GLContext::enableGLColorArray(bool bEnable)
{
    if (bEnable != m_bEnableGLColorArray) {
        if (bEnable) {
            glEnableClientState(GL_COLOR_ARRAY);
        } else {
            glDisableClientState(GL_COLOR_ARRAY);
        }
        m_bEnableGLColorArray = bEnable;
    }
}

void checkBlendModeError(const char * sMode) 
{    
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        static bool bErrorReported = false;
        if (!bErrorReported) {
            AVG_TRACE(Logger::WARNING, "Blendmode "<< sMode <<
                    " not supported by OpenGL implementation.");
            bErrorReported = true;
        }
    }
}

void GLContext::setBlendMode(BlendMode mode, bool bPremultipliedAlpha)
{
    GLenum srcFunc;
    if (bPremultipliedAlpha) {
        srcFunc = GL_CONSTANT_ALPHA;
    } else {
        srcFunc = GL_SRC_ALPHA;
    }
    if (mode != m_BlendMode || m_bPremultipliedAlpha != bPremultipliedAlpha) {
        switch (mode) {
            case BLEND_BLEND:
                glproc::BlendEquation(GL_FUNC_ADD);
                glproc::BlendFuncSeparate(srcFunc, GL_ONE_MINUS_SRC_ALPHA, 
                        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                checkBlendModeError("blend");
                break;
            case BLEND_ADD:
                glproc::BlendEquation(GL_FUNC_ADD);
                glproc::BlendFuncSeparate(srcFunc, GL_ONE, GL_ONE, GL_ONE);
                checkBlendModeError("add");
                break;
            case BLEND_MIN:
                glproc::BlendEquation(GL_MIN);
                glproc::BlendFuncSeparate(srcFunc, GL_ONE_MINUS_SRC_ALPHA, 
                        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                checkBlendModeError("min");
                break;
            case BLEND_MAX:
                glproc::BlendEquation(GL_MAX);
                glproc::BlendFuncSeparate(srcFunc, GL_ONE_MINUS_SRC_ALPHA, 
                        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                checkBlendModeError("max");
                break;
            case BLEND_COPY:
                glproc::BlendEquation(GL_FUNC_ADD);
                glBlendFunc(GL_ONE, GL_ZERO);
                break;
            default:
                AVG_ASSERT(false);
        }

        m_BlendMode = mode;
        m_bPremultipliedAlpha = bPremultipliedAlpha;
    }
}

const GLConfig& GLContext::getConfig()
{
    return m_GLConfig;
}

void GLContext::logConfig() 
{
    AVG_TRACE(Logger::CONFIG, "OpenGL configuration: ");
    AVG_TRACE(Logger::CONFIG, "  OpenGL version: " << glGetString(GL_VERSION));
    AVG_TRACE(Logger::CONFIG, "  OpenGL vendor: " << glGetString(GL_VENDOR));
    AVG_TRACE(Logger::CONFIG, "  OpenGL renderer: " << glGetString(GL_RENDERER));
    m_GLConfig.log();
    switch (getMemoryModeSupported()) {
        case MM_PBO:
            AVG_TRACE(Logger::CONFIG, "  Using pixel buffer objects");
            break;
        case MM_OGL:
            AVG_TRACE(Logger::CONFIG, "  Not using GL memory extensions");
            break;
    }
    AVG_TRACE(Logger::CONFIG, "  Max. texture size: " << getMaxTexSize());
    string s;
    if (useGPUYUVConversion()) {
        s = "yes";
    } else {
        s = "no";
    }
    AVG_TRACE(Logger::CONFIG, string("  GPU-based YUV-RGB conversion: ")+s+".");
    if (useMinimalShader()) {
        s = "yes";
    } else {
        s = "no";
    }
    AVG_TRACE(Logger::CONFIG, string("  Minimal shader: ")+s+".");
    try {
        AVG_TRACE(Logger::CONFIG, "  Dedicated video memory: " << 
                getVideoMemInstalled()/(1024*1024) << " MB");
        AVG_TRACE(Logger::CONFIG, "  Video memory used at start: " << 
                getVideoMemUsed()/(1024*1024) << " MB");
    } catch (Exception) {
        AVG_TRACE(Logger::CONFIG, "  Dedicated video memory: Unknown");
        AVG_TRACE(Logger::CONFIG, "  Video memory used at start: Unknown");
    }
}

size_t GLContext::getVideoMemInstalled()
{
    checkGPUMemInfoSupport();
    int kbMemInstalled;
    glGetIntegerv(GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &kbMemInstalled);
    return (size_t)kbMemInstalled*1024;
}

size_t GLContext::getVideoMemUsed()
{
    checkGPUMemInfoSupport();
    int kbMemAvailable;
    glGetIntegerv(GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &kbMemAvailable);
    return getVideoMemInstalled()-(size_t)kbMemAvailable*1024;
}

bool GLContext::usePOTTextures()
{
    return m_GLConfig.m_bUsePOTTextures;
}

OGLMemoryMode GLContext::getMemoryModeSupported()
{
    if (!m_bCheckedMemoryMode) {
        if ((queryOGLExtension("GL_ARB_pixel_buffer_object") || 
             queryOGLExtension("GL_EXT_pixel_buffer_object")) &&
            m_GLConfig.m_bUsePixelBuffers) 
        {
            m_MemoryMode = MM_PBO;
        } else {
            m_MemoryMode = MM_OGL;
        }
        m_bCheckedMemoryMode = true;
    }
    return m_MemoryMode;
}

int GLContext::getMaxTexSize() 
{
    if (m_MaxTexSize == 0) {
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_MaxTexSize);
    }
    return m_MaxTexSize;
}

bool GLContext::initVBlank(int rate) 
{
    if (rate > 0) {
#ifdef __APPLE__
        initMacVBlank(rate);
        s_VBMethod = VB_APPLE;
#elif defined _WIN32
        if (queryOGLExtension("WGL_EXT_swap_control")) {
            glproc::SwapIntervalEXT(rate);
            s_VBMethod = VB_WIN;
        } else {
            AVG_TRACE(Logger::WARNING,
                    "Windows VBlank setup failed: OpenGL Extension not supported.");
            s_VBMethod = VB_NONE;
        }
#else
        if (getenv("__GL_SYNC_TO_VBLANK") != 0) {
            AVG_TRACE(Logger::WARNING, 
                    "__GL_SYNC_TO_VBLANK set. This interferes with libavg vblank handling.");
            s_VBMethod = VB_NONE;
        } else {
            if (queryGLXExtension("GLX_EXT_swap_control")) {
                s_VBMethod = VB_GLX;
                glproc::SwapIntervalEXT(m_pDisplay, m_Drawable, rate);

            } else {
                AVG_TRACE(Logger::WARNING,
                        "Linux VBlank setup failed: OpenGL Extension not supported.");
                s_VBMethod = VB_NONE;
            }
        }
#endif
    } else {
        switch (s_VBMethod) {
            case VB_APPLE:
                initMacVBlank(0);
                break;
            case VB_WIN:
#ifdef _WIN32
                glproc::SwapIntervalEXT(0);
#endif
                break;
            case VB_GLX:
#ifdef linux            
                glproc::SwapIntervalEXT(m_pDisplay, m_Drawable, 0);
#endif
                break;
            default:
                break;
        }
        s_VBMethod = VB_NONE;
    }
    switch(s_VBMethod) {
        case VB_GLX:
            AVG_TRACE(Logger::CONFIG, 
                    "  Using SGI OpenGL extension for vertical blank support.");
            break;
        case VB_APPLE:
            AVG_TRACE(Logger::CONFIG, "  Using Apple GL vertical blank support.");
            break;
        case VB_WIN:
            AVG_TRACE(Logger::CONFIG, "  Using Windows GL vertical blank support.");
            break;
        case VB_NONE:
            AVG_TRACE(Logger::CONFIG, "  Vertical blank support disabled.");
            break;
        default:
            AVG_TRACE(Logger::WARNING, "  Illegal vblank enum value.");
    }
    return s_VBMethod != VB_NONE;
}

void GLContext::enableErrorChecks(bool bEnable)
{
    m_bErrorCheckEnabled = bEnable;
}
    
void GLContext::checkError(const char* pszWhere) 
{
    // If there's no GL context anymore, we just ignore the error check.
    if (this && m_bErrorCheckEnabled) {
        mandatoryCheckError(pszWhere);
    }
}

void GLContext::mandatoryCheckError(const char* pszWhere) 
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        stringstream s;
        s << "OpenGL error in " << pszWhere <<": " << gluErrorString(err) 
            << " (#" << err << ") ";
        AVG_TRACE(Logger::ERROR, s.str());
        if (err != GL_INVALID_OPERATION) {
            checkError("  --");
        }
        AVG_ASSERT(false);
    }
}

GLContext::BlendMode GLContext::stringToBlendMode(const string& s)
{
    if (s == "blend") {
        return GLContext::BLEND_BLEND;
    } else if (s == "add") {
        return GLContext::BLEND_ADD;
    } else if (s == "min") {
        return GLContext::BLEND_MIN;
    } else if (s == "max") {
        return GLContext::BLEND_MAX;
    } else {
        throw(Exception(AVG_ERR_UNSUPPORTED, "Blend mode "+s+" not supported."));
    }
}

GLContext* GLContext::getCurrent()
{
    return *s_pCurrentContext;
}

void GLContext::checkGPUMemInfoSupport()
{
    if (!m_bCheckedGPUMemInfoExtension) {
        m_bGPUMemInfoSupported = queryOGLExtension("GL_NVX_gpu_memory_info");
        m_bCheckedGPUMemInfoExtension = true;
    }
    if (!m_bGPUMemInfoSupported) {
        throw Exception(AVG_ERR_UNSUPPORTED, 
                "Video memory query not supported on this system.");
    }
}

#ifdef _WIN32
void GLContext::checkWinError(BOOL bOK, const string& sWhere) 
{
    if (!bOK) {
        char szErr[512];
        FormatMessage((FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM),
                0, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                szErr, 512, 0);
        AVG_TRACE(Logger::ERROR, sWhere+":"+szErr);
        AVG_ASSERT(false);
    }
}
#endif

void GLContext::initMacVBlank(int rate)
{
#ifdef __APPLE__
    CGLContextObj context = CGLGetCurrentContext();
    AVG_ASSERT (context);
#if MAC_OS_X_VERSION_10_5
    GLint l = rate;
#else
    long l = rate;
#endif
    if (rate > 1) {
        AVG_TRACE(Logger::WARNING,
                "VBlank rate set to " << rate 
                << " but Mac OS X only supports 1. Assuming 1.");
        l = 1;
    }
    CGLError err = CGLSetParameter(context, kCGLCPSwapInterval, &l);
    AVG_ASSERT(!err);
#endif
}

}

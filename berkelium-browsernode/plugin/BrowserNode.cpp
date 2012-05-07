
#include "BerkeliumDelegate.h"
#include "BrowserNode.h"

#include "../../base/ProfilingZone.h"
#include "../../base/ScopeTimer.h"

#include "../../player/Player.h"
#include "../../player/MouseEvent.h"
#include "../../player/SDLDisplayEngine.h"

#include "../../base/Logger.h"
#include "../../graphics/OGLHelper.h"
#include "../../wrapper/WrapHelper.h"
#include <berkelium/Berkelium.hpp>
#include <berkelium/Window.hpp>
#include <berkelium/WindowDelegate.hpp>
#include <berkelium/Context.hpp>
#include <boost/python.hpp>

#include <iostream>

using namespace boost::python;
using std::endl;
using std::cerr;

#define MAX_ZOOM_LEVEL 10

namespace avg {


BrowserNode::BrowserNode(const ArgList& Args)
    : m_pBerkeliumWindow(0),
      m_LastSize(0, 0),
      m_bTransparent(false),
      m_bCreated(false),
      m_CurrentZoomLevel(0),
      m_InitZoomLevel(0),
      m_XOffset(0),
      m_YOffset(0),
      m_bEventHandler(false),
      m_bPainted(false)
{
    Args.setMembers(this);
    Berkelium::Context* context = Berkelium::Context::create();
    m_pBerkeliumWindow = Berkelium::Window::create(context);
    m_pBerkeliumWindow->resize(0, 0);
    context->destroy();
    Player::get()->registerPreRenderListener(this);
}

BrowserNode::~BrowserNode()
{
    Player::get()->unregisterPreRenderListener(this);
    m_pBerkeliumWindow->adjustZoom(0);
    m_pBerkeliumWindow->destroy();
}

void BrowserNode::onPaint(Berkelium::Window* pWindow, const unsigned char *pBitmapIn,
        const Berkelium::Rect &bitmapRect, size_t numCopyRects, 
        const Berkelium::Rect *pCopyRects, int dx, int dy,
        const Berkelium::Rect &scrollRect)
{
    BerkeliumDelegate::onPaint(pWindow,pBitmapIn,bitmapRect,numCopyRects,pCopyRects,
            dx,dy,scrollRect);
    if (numCopyRects <= 1) {
        m_bPainted = true;
    }
}

void BrowserNode::preRender()
{
    if (!m_bCreated || getSize() != m_LastSize) {
        m_bCreated = true;
        m_LastSize = getSize();
        if (m_XOffset < 0) {
            m_XOffset = 0;
        }
        if (m_YOffset < 0) {
            m_YOffset = 0;
        }
        int x = m_XOffset/100 * getWidth();
        int y = m_YOffset/100 * getHeight();
        IntPoint offset(x, y);
        IntPoint size(getWidth(), getHeight());
        m_pBerkeliumWindow->resize(getWidth() + offset.x, getHeight() + offset.y);
        m_pBerkeliumWindow->setDelegate(this);
        allocateBuffer(size, offset);
        bool bMipmap = getMaterial().getUseMipmaps();
        GLTexturePtr pTexture = GLTexturePtr(new GLTexture(size, R8G8B8A8, bMipmap));
        pTexture->enableStreaming();
        getSurface()->create(R8G8B8A8,pTexture);
    }
    RasterNode::preRender();
}

static ProfilingZoneID pzid("BrowserNode::render");

void BrowserNode::render(const DRect& Rect)
{
    ScopeTimer Timer(pzid);
    if (isDirty()){
        GLTexturePtr pTexture = getSurface()->getTex();
        BitmapPtr pBmp = pTexture->lockStreamingBmp();
        memcpy(pBmp->getPixels(), getBuffer(), getBufferLength());
        cleanDirtyFlag();
        pTexture->unlockStreamingBmp(true);
        bind();
    }
    blt32(getSize(), getEffectiveOpacity(), getBlendMode());
}

bool BrowserNode::handleEvent(EventPtr pEvent)
{
    if (m_bEventHandler) {
        if (MouseEventPtr pMouseEvent = boost::dynamic_pointer_cast<MouseEvent>(pEvent)) {
            DPoint coords = pMouseEvent->getPos();
            std::vector<NodeWeakPtr> vector = getParentChain();
            for (unsigned int i = 0; i < vector.size(); i++) {
                NodePtr n = vector[i].lock();
                coords = n->toLocal(coords);
            }

            coords.x = coords.x + getOffset().x;
            coords.y = coords.y + getOffset().y;
            unsigned int buttonID = 3;//NO BUTTON
            Event::Type type = pMouseEvent->getType();
            switch (pMouseEvent->getButton()) {
                case MouseEvent::LEFT_BUTTON:
                    buttonID = 0; //LEFT
                    break;
                case MouseEvent::MIDDLE_BUTTON:
                    buttonID = 1; //MIDDLE
                    break;
                case MouseEvent::RIGHT_BUTTON:
                    buttonID = 2; //RIGHT
                    break;
                case MouseEvent::WHEELDOWN_BUTTON:
                    buttonID = 4; //WHEELDOWN
                    break;
                case MouseEvent::WHEELUP_BUTTON:
                    buttonID = 5; //WHEELUP
                    break;
                default:
                    break;
            }
            if (buttonID < 4) {
                if (type == Event::CURSORDOWN) {
                    m_pBerkeliumWindow->mouseButton(buttonID, true);
                } else if (type == Event::CURSORUP) {
                    m_pBerkeliumWindow->mouseButton(buttonID, false);
                } else if (type == Event::CURSORMOTION) {
                    m_pBerkeliumWindow->mouseMoved(coords.x,coords.y);
                }
            }
        }
        /*else if (KeyEventPtr pKeyEvent = boost::dynamic_pointer_cast<KeyEvent>(pEvent)) {
            Event::Type type = pKeyEvent->getType();
            char val = toupper(pKeyEvent->getKeyString().c_str()[0]);
            AVG_TRACE(Logger::PLUGIN, "key: " << val);
            view->injectKeyEvent(type == Event::KEYDOWN, 0, val, pKeyEvent->getScanCode());
        }*/
    }
    return RasterNode::handleEvent(pEvent);
}

void BrowserNode::onPreRender()
{
    Berkelium::update();
}

void BrowserNode::loadUrl(const std::string& url)
{
    // TODO: need to support extra loadUrl parameters?
    m_pBerkeliumWindow->navigateTo(Berkelium::URLString::point_to(url));
}

void BrowserNode::onLoad(Berkelium::Window* pWindow)
{
    //std::cerr << "On load " << m_InitZoomLevel << std::endl;
    zoom(m_InitZoomLevel);
}

void BrowserNode::executeJavascript(const std::string& js)
{
    //cerr << "executeJavascript" << endl;
    int length = js.length();
    std::wstring wjs(length,'0');
    std::copy(js.begin(), js.end(), wjs.begin());
    m_pBerkeliumWindow->executeJavascript(Berkelium::WideString::point_to(wjs.c_str(),
            length));
}

void BrowserNode::refresh()
{
    m_pBerkeliumWindow->refresh();
}

void BrowserNode::zoom(int zoomLevel)
{
    if (zoomLevel >= MAX_ZOOM_LEVEL) {
        zoomLevel = MAX_ZOOM_LEVEL;
    }
    if (zoomLevel <= -MAX_ZOOM_LEVEL) {
        zoomLevel = -MAX_ZOOM_LEVEL;
    }
    while (m_CurrentZoomLevel < zoomLevel) {
        zoomIn();
    }
    while (m_CurrentZoomLevel > zoomLevel) {
        zoomOut();
    }
}

void BrowserNode::zoomIn()
{
    if (m_CurrentZoomLevel >= MAX_ZOOM_LEVEL) {
        return;
    }
    m_pBerkeliumWindow->adjustZoom(1);
    m_CurrentZoomLevel++;
}

void BrowserNode::zoomOut()
{
    if(m_CurrentZoomLevel <= -MAX_ZOOM_LEVEL) {
        return;
    }
    m_pBerkeliumWindow->adjustZoom(-1);
    m_CurrentZoomLevel--;
}

void BrowserNode::setTransparent(bool trans)
{
    m_bTransparent = trans;
    m_pBerkeliumWindow->setTransparent(trans);
}

bool BrowserNode::getTransparent() const
{
    return m_bTransparent;
}

void BrowserNode::setXOffset(float x)
{
    m_XOffset = x;
}

float BrowserNode::getXOffset() const
{
    return m_XOffset;
}

void BrowserNode::setYOffset(float y)
{
    m_YOffset = y;
}

float BrowserNode::getYOffset() const
{
    return m_YOffset;
}

bool BrowserNode::painted() const
{
    return m_bPainted;
}

NodeDefinition BrowserNode::createNodeDefinition()
{
    return NodeDefinition("browser",Node::buildNode<BrowserNode>)
        .extendDefinition(RasterNode::createDefinition())
        .addArg(Arg<bool>("transparent", false, false,
                offsetof(BrowserNode, m_bTransparent)))
        .addArg(Arg<float>("xoffset", false, false,
                offsetof(BrowserNode, m_XOffset)))
        .addArg(Arg<float>("yoffset", false, false,
                offsetof(BrowserNode, m_YOffset)))
        .addArg(Arg<bool>("eventHandler", false, false,
                offsetof(BrowserNode, m_bEventHandler)))
        .addArg(Arg<int>("zoomLevel", false, false,
                offsetof(BrowserNode, m_InitZoomLevel)))
        ;
}

}

using namespace avg;

BOOST_PYTHON_MODULE(libbrowsernode) 
{
    // TODO: add docstrings for all methods
    class_<BrowserNode, bases<RasterNode>, boost::noncopyable>("BrowserNode", no_init)
        .def("loadUrl", &BrowserNode::loadUrl)
        .def("refresh", &BrowserNode::refresh)
        .def("zoom", &BrowserNode::zoom)
        .def("zoomIn", &BrowserNode::zoomIn)
        .def("zoomOut", &BrowserNode::zoomOut)
        .def("executeJavascript", &BrowserNode::executeJavascript)
        .def("painted", &BrowserNode::painted)
        .add_property("transparent", &BrowserNode::getTransparent,
                &BrowserNode::setTransparent,
                "Forces all webpages to be rendered with a transparent background.\n")
        .add_property("onFinishLoading", make_function(&BrowserNode::getFinishLoadingCb,
                return_value_policy<copy_const_reference>()),
                &BrowserNode::setFinishLoadingCb, "TODO.\n")
        .add_property("onJavascript", make_function(&BrowserNode::getJavascriptCb,
                return_value_policy<copy_const_reference>()),
                &BrowserNode::setJavascriptCb, "TODO.\n")
        .add_property("onCrashed", make_function(&BrowserNode::getCrashedCb,
                return_value_policy<copy_const_reference>()), &BrowserNode::setCrashedCb,
                "TODO.\n")
        .add_property("onCrashedPlugin", make_function(&BrowserNode::getCrashedPluginCb,
                return_value_policy<copy_const_reference>()),
                &BrowserNode::setCrashedPluginCb, "TODO.\n")
        .add_property("onResponsive", make_function(&BrowserNode::getResponsiveCb,
                return_value_policy<copy_const_reference>()),
                &BrowserNode::setResponsiveCb, "TODO.\n")
        .add_property("onUnresponsive", make_function(&BrowserNode::getUnresponsiveCb,
                return_value_policy<copy_const_reference>()),
                &BrowserNode::setUnresponsiveCb, "TODO.\n")
        ;
}

AVG_PLUGIN_API void registerPlugin()
{

    Berkelium::init(Berkelium::FileString::point_to(".berkelium"));
    static avg::ErrorDelegateWrapper * Wrapper;
    Wrapper = new avg::ErrorDelegateWrapper();
    Berkelium::setErrorHandler(Wrapper);

    object avgModule(handle<>(borrowed(PyImport_AddModule("avg")))); 
    object browserNodeModule(handle<>(PyImport_ImportModule("libbrowsernode"))); 
    avgModule.attr("libbrowsernode") = browserNodeModule;


    avg::NodeDefinition myNodeDefinition = avg::BrowserNode::createNodeDefinition();
    const char* allowedParentNodeNames[] = {"avg", "div", 0};

    // Register this node type
    avg::Player::get()->registerNodeType(myNodeDefinition, allowedParentNodeNames);
}

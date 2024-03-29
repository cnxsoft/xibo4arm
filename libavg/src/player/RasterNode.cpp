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

#include "RasterNode.h"

#include "NodeDefinition.h"
#include "OGLSurface.h"
#include "FXNode.h"

#include "../graphics/ImagingProjection.h"

#include "../base/MathHelper.h"
#include "../base/Logger.h"
#include "../base/XMLHelper.h"
#include "../base/Exception.h"
#include "../base/ScopeTimer.h"

using namespace std;

namespace avg {

NodeDefinition RasterNode::createDefinition()
{
    return NodeDefinition("rasternode")
        .extendDefinition(AreaNode::createDefinition())
        .addArg(Arg<int>("maxtilewidth", -1, false, 
                offsetof(RasterNode, m_MaxTileSize.x)))
        .addArg(Arg<int>("maxtileheight", -1, false, 
                offsetof(RasterNode, m_MaxTileSize.y)))
        .addArg(Arg<string>("blendmode", "blend", false, 
                offsetof(RasterNode, m_sBlendMode)))
        .addArg(Arg<bool>("mipmap", false))
        .addArg(Arg<UTF8String>("maskhref", "", false, offsetof(RasterNode, m_sMaskHref)))
        .addArg(Arg<glm::vec2>("maskpos", glm::vec2(0,0), false,
                offsetof(RasterNode, m_MaskPos)))
        .addArg(Arg<glm::vec2>("masksize", glm::vec2(0,0), false,
                offsetof(RasterNode, m_MaskSize)))
        .addArg(Arg<glm::vec3>("gamma", glm::vec3(1.0f,1.0f,1.0f), false,
                offsetof(RasterNode, m_Gamma)))
        .addArg(Arg<glm::vec3>("contrast", glm::vec3(1.0f,1.0f,1.0f), false,
                offsetof(RasterNode, m_Contrast)))
        .addArg(Arg<glm::vec3>("intensity", glm::vec3(1.0f,1.0f,1.0f), false,
                offsetof(RasterNode, m_Intensity)));
}

RasterNode::RasterNode()
    : m_pSurface(0),
      m_Material(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, false),
      m_bBound(false),
      m_TileSize(-1,-1),
      m_pVertexes(0),
      m_bFXDirty(true)
{
}

RasterNode::~RasterNode()
{
    if (m_pSurface) {
        delete m_pSurface;
        m_pSurface = 0;
    }
}

void RasterNode::setArgs(const ArgList& args)
{
    AreaNode::setArgs(args);
    if ((!ispow2(m_MaxTileSize.x) && m_MaxTileSize.x != -1)
            || (!ispow2(m_MaxTileSize.y) && m_MaxTileSize.y != -1)) 
    {
        throw Exception(AVG_ERR_OUT_OF_RANGE, 
                "maxtilewidth and maxtileheight must be powers of two.");
    }
    bool bMipmap = args.getArgVal<bool>("mipmap");
    m_Material = MaterialInfo(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, bMipmap);
    m_pSurface = new OGLSurface();
}

void RasterNode::connectDisplay()
{
    AreaNode::connectDisplay();

    m_bBound = false;
    if (m_MaxTileSize != IntPoint(-1, -1)) {
        m_TileSize = m_MaxTileSize;
    }
    calcVertexGrid(m_TileVertices);
    setBlendModeStr(m_sBlendMode);
    if (m_pMaskBmp) {
        downloadMask();
        setMaskCoords();
    }
    m_pSurface->setColorParams(m_Gamma, m_Intensity, m_Contrast);
    setupFX(true);
}

void RasterNode::disconnect(bool bKill)
{
    if (m_pVertexes) {
        delete m_pVertexes;
        m_pVertexes = 0;
    }
    if (m_pSurface) {
        m_pSurface->destroy();
    }
    m_pFBO = FBOPtr();
    m_pImagingProjection = ImagingProjectionPtr();
    if (bKill) {
        m_pFXNode = FXNodePtr();
    } else {
        if (m_pFXNode) {
            m_pFXNode->disconnect();
        }
    }
    AreaNode::disconnect(bKill);
}

void RasterNode::checkReload()
{
    string sLastMaskFilename = m_sMaskFilename;
    string sMaskFilename = m_sMaskHref;
    initFilename(sMaskFilename);
    if (sLastMaskFilename != sMaskFilename) {
        m_sMaskFilename = sMaskFilename;
        try {
            if (m_sMaskFilename != "") {
                AVG_TRACE(Logger::MEMORY, "Loading " << m_sMaskFilename);
                m_pMaskBmp = BitmapPtr(new Bitmap(m_sMaskFilename));
                if (m_pMaskBmp->getPixelFormat() != I8) {
                    BitmapPtr pTempBmp = m_pMaskBmp;
                    m_pMaskBmp = BitmapPtr(new Bitmap(m_pMaskBmp->getSize(), I8));
                    m_pMaskBmp->copyPixels(*pTempBmp);
                }
                setMaskCoords();
            }
        } catch (Exception & ex) {
            if (ex.getCode() == AVG_ERR_VIDEO_GENERAL) {
                throw;
            }
            m_sMaskFilename = "";
            if (getState() != Node::NS_UNCONNECTED) {
                AVG_TRACE(Logger::ERROR, ex.getStr());
            } else {
                AVG_TRACE(Logger::MEMORY, ex.getStr());
            }
        }
        if (m_sMaskFilename == "") {
            m_pMaskBmp = BitmapPtr();
            getSurface()->setMask(GLTexturePtr());
        }
        if (getState() == Node::NS_CANRENDER && m_pMaskBmp) {
            downloadMask();
        }
    } else {
        setMaskCoords();
    }
}

VertexGrid RasterNode::getOrigVertexCoords()
{
    checkDisplayAvailable("getOrigVertexCoords");
    if (!m_bBound) {
        bind();
    }
    VertexGrid grid;
    calcVertexGrid(grid);
    return grid;
}

VertexGrid RasterNode::getWarpedVertexCoords() 
{
    checkDisplayAvailable("getWarpedVertexCoords");
    if (!m_bBound) {
        bind();
    }
    return m_TileVertices;
}

void RasterNode::setWarpedVertexCoords(const VertexGrid& grid)
{
    checkDisplayAvailable("setWarpedVertexCoords");
    if (!m_bBound) {
        bind();
    }
    bool bGridOK = true;
    IntPoint numTiles = getNumTiles();
    if (grid.size() != (unsigned)(numTiles.y+1)) {
        bGridOK = false;
    }
    for (unsigned i = 0; i < grid.size(); ++i) {
        if (grid[i].size() != (unsigned)(numTiles.x+1)) {
            bGridOK = false;
        }
    }
    if (!bGridOK) {
        throw Exception(AVG_ERR_OUT_OF_RANGE, 
                "setWarpedVertexCoords() called with incorrect grid size.");
    }
    m_TileVertices = grid;
    m_bVertexArrayDirty = true;
}

int RasterNode::getMaxTileWidth() const
{
    return m_MaxTileSize.x;
}

int RasterNode::getMaxTileHeight() const
{
    return m_MaxTileSize.y;
}

bool RasterNode::getMipmap() const
{
   return m_Material.getUseMipmaps();
}

const std::string& RasterNode::getBlendModeStr() const
{
    return m_sBlendMode;
}

void RasterNode::setBlendModeStr(const string& sBlendMode)
{
    m_sBlendMode = sBlendMode;
    m_BlendMode = GLContext::stringToBlendMode(sBlendMode);
}

const UTF8String& RasterNode::getMaskHRef() const
{
    return m_sMaskHref;
}

void RasterNode::setMaskHRef(const UTF8String& sHref)
{
    m_sMaskHref = sHref;
    checkReload();
}

const glm::vec2& RasterNode::getMaskPos() const
{
    return m_MaskPos;
}

void RasterNode::setMaskPos(const glm::vec2& pos)
{
    m_MaskPos = pos;
    setMaskCoords();
}

const glm::vec2& RasterNode::getMaskSize() const
{
    return m_MaskSize;
}

void RasterNode::setMaskSize(const glm::vec2& size)
{
    m_MaskSize = size;
    setMaskCoords();
}

void RasterNode::getElementsByPos(const glm::vec2& pos, vector<NodeWeakPtr>& pElements)
{
    // Node isn't pickable if it's warped.
    if (m_MaxTileSize == IntPoint(-1, -1)) {
        AreaNode::getElementsByPos(pos, pElements);
    }
}

glm::vec3 RasterNode::getGamma() const
{
    return m_Gamma;
}

void RasterNode::setGamma(const glm::vec3& gamma)
{
    m_Gamma = gamma;
    if (getState() == Node::NS_CANRENDER) {
        m_pSurface->setColorParams(m_Gamma, m_Intensity, m_Contrast);
    }
}

glm::vec3 RasterNode::getIntensity() const
{
    return m_Intensity;
}

void RasterNode::setIntensity(const glm::vec3& intensity)
{
    m_Intensity = intensity;
    if (getState() == Node::NS_CANRENDER) {
        m_pSurface->setColorParams(m_Gamma, m_Intensity, m_Contrast);
    }
}

glm::vec3 RasterNode::getContrast() const
{
    return m_Contrast;
}

void RasterNode::setContrast(const glm::vec3& contrast)
{
    m_Contrast = contrast;
    if (getState() == Node::NS_CANRENDER) {
        m_pSurface->setColorParams(m_Gamma, m_Intensity, m_Contrast);
    }
}

void RasterNode::setEffect(FXNodePtr pFXNode)
{
    if (m_pFXNode && m_pFXNode != pFXNode) {
        m_pFXNode->disconnect();
    }
    if (m_pFXNode && !pFXNode) {
        m_pFBO = FBOPtr();
    }
    m_pFXNode = pFXNode;
    if (getState() == NS_CANRENDER) {
        setupFX(true);
    }
}

void RasterNode::blt32(const glm::mat4& transform, const glm::vec2& destSize, 
        float opacity, GLContext::BlendMode mode, bool bPremultipliedAlpha)
{
    blt(transform, destSize, mode, opacity, Pixel32(255, 255, 255, 255),
            bPremultipliedAlpha);
}

void RasterNode::blta8(const glm::mat4& transform, const glm::vec2& destSize, 
        float opacity, const Pixel32& color, GLContext::BlendMode mode)
{
    blt(transform, destSize, mode, opacity, color, false);
}

GLContext::BlendMode RasterNode::getBlendMode() const
{
    return m_BlendMode;
}

OGLSurface * RasterNode::getSurface()
{
    return m_pSurface;
}

const MaterialInfo& RasterNode::getMaterial() const
{
    return m_Material;
}

bool RasterNode::hasMask() const
{
    return m_sMaskFilename != "";
}

void RasterNode::setMaskCoords()
{
    if (m_sMaskFilename != "") {
        calcMaskCoords();
    }
}

void RasterNode::calcMaskCoords()
{
    glm::vec2 maskSize;
    glm::vec2 mediaSize = glm::vec2(getMediaSize());
    if (m_MaskSize == glm::vec2(0,0)) {
        maskSize = glm::vec2(1,1);
    } else {
        maskSize = glm::vec2(m_MaskSize.x/mediaSize.x, m_MaskSize.y/mediaSize.y);
    }
    glm::vec2 maskPos = glm::vec2(m_MaskPos.x/mediaSize.x, m_MaskPos.y/mediaSize.y);
    m_pSurface->setMaskCoords(maskPos, maskSize);
}

void RasterNode::downloadMask()
{
    GLTexturePtr pTex(new GLTexture(m_pMaskBmp->getSize(), I8, 
            m_Material.getUseMipmaps()));
    pTex->moveBmpToTexture(m_pMaskBmp);
    m_pSurface->setMask(pTex);
}

void RasterNode::bind() 
{
    if (!m_bBound) {
        calcTexCoords();
    }
    m_bBound = true;
}

static ProfilingZoneID FXProfilingZone("RasterNode::renderFX");

void RasterNode::renderFX(const glm::vec2& destSize, const Pixel32& color, 
        bool bPremultipliedAlpha, bool bForceRender)
{
    ScopeTimer Timer(FXProfilingZone);
    setupFX(false);
    GLContext* pContext = GLContext::getCurrent();
    pContext->enableGLColorArray(false);
    pContext->enableTexture(true);
    if (m_pFXNode && (m_bFXDirty || m_pSurface->isDirty() || m_pFXNode->isDirty() ||
            bForceRender))
    {
        if (!m_bBound) {
            bind();
        }
        StandardShader::get()->setColor(glm::vec4(color.getR()/256.f, color.getG()/256.f,
                color.getB()/256.f, 1.f));
        m_pSurface->activate(getMediaSize());

        m_pFBO->activate();
        clearGLBuffers(GL_COLOR_BUFFER_BIT);


        if (bPremultipliedAlpha) {
            glproc::BlendColor(1.0f, 1.0f, 1.0f, 1.0f);
        }
        pContext->setBlendMode(GLContext::BLEND_BLEND, bPremultipliedAlpha);

        m_pImagingProjection->draw();

/*
        static int i=0;
        stringstream ss;
        ss << "foo" << i << ".png";
        BitmapPtr pBmp = m_pFBO->getImage(0);
        pBmp->save(ss.str());
  */  
        m_pFXNode->apply(m_pFBO->getTex());
        
/*        
        stringstream ss1;
        ss1 << "bar" << ".png";
        i++;
        m_pFXNode->getImage()->save(ss1.str());
*/
        m_bFXDirty = false;
        m_pSurface->resetDirty();
        m_pFXNode->resetDirty();
    }
}

void RasterNode::checkDisplayAvailable(std::string sMsg)
{
    if (!(getState() == Node::NS_CANRENDER)) {
        throw Exception(AVG_ERR_UNSUPPORTED,
            string(sMsg) + ": cannot access vertex coordinates before node is bound.");
    }
    if (!m_pSurface->isCreated()) {
        throw Exception(AVG_ERR_UNSUPPORTED,
            string(sMsg) + ": Surface not available.");
    }
}

void RasterNode::setupFX(bool bNewFX)
{
    if (m_pSurface && m_pSurface->getSize() != IntPoint(-1, -1) && m_pFXNode) {
        if (bNewFX || !m_pFBO || m_pFBO->getSize() != m_pSurface->getSize()) {
            m_pFXNode->setSize(m_pSurface->getSize());
            m_pFXNode->connect();
            m_bFXDirty = true;
        }
        if (!m_pFBO || m_pFBO->getSize() != m_pSurface->getSize()) {
            m_pFBO = FBOPtr(new FBO(IntPoint(m_pSurface->getSize()), B8G8R8A8, 1, 1,
                    false, getMipmap()));
            GLTexturePtr pTex = m_pFBO->getTex();
            pTex->setWrapMode(GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER);
            m_pImagingProjection = ImagingProjectionPtr(new ImagingProjection(
                    m_pSurface->getSize()));
        }
    }
}

void RasterNode::blt(const glm::mat4& transform, const glm::vec2& destSize, 
        GLContext::BlendMode mode, float opacity, const Pixel32& color,
        bool bPremultipliedAlpha)
{
    if (!m_bBound) {
        bind();
    }
    GLContext* pContext = GLContext::getCurrent();
    pContext->enableGLColorArray(false);
    pContext->enableTexture(true);
    FRect destRect;
    StandardShaderPtr pShader = pContext->getStandardShader();
    if (m_pFXNode) {
        m_pFXNode->getTex()->activate(GL_TEXTURE0);
        pShader->setColorModel(0);
        pShader->setColor(glm::vec4(1.0f, 1.0f, 1.0f, opacity));
        pShader->disableColorspaceMatrix();

        pContext->setBlendMode(mode, true);
        FRect relDestRect = m_pFXNode->getRelDestRect();
        destRect = FRect(relDestRect.tl.x*destSize.x, relDestRect.tl.y*destSize.y,
                relDestRect.br.x*destSize.x, relDestRect.br.y*destSize.y);
    } else {
        m_pSurface->activate(getMediaSize(), bPremultipliedAlpha);
        pContext->setBlendMode(mode, bPremultipliedAlpha);
        pShader->setColor(glm::vec4(color.getR()/256.f, color.getG()/256.f,
                color.getB()/256.f, opacity));
        destRect = FRect(glm::vec2(0,0), destSize);
    }
    pShader->activate();
    glproc::BlendColor(1.0f, 1.0f, 1.0f, float(opacity));
    glm::vec3 pos(destRect.tl.x, destRect.tl.y, 0);
    glm::vec3 scaleVec(destRect.size().x, destRect.size().y, 1);
    glm::mat4 localTransform = glm::translate(transform, pos);
    localTransform = glm::scale(localTransform, scaleVec);
    glLoadMatrixf(glm::value_ptr(localTransform));

    if (m_bVertexArrayDirty) {
        m_pVertexes->reset();
        for (unsigned y = 0; y < m_TileVertices.size()-1; y++) {
            for (unsigned x = 0; x < m_TileVertices[0].size()-1; x++) {
                int curVertex = m_pVertexes->getCurVert();
                m_pVertexes->appendPos(m_TileVertices[y][x], m_TexCoords[y][x]); 
                m_pVertexes->appendPos(m_TileVertices[y][x+1], m_TexCoords[y][x+1]); 
                m_pVertexes->appendPos(m_TileVertices[y+1][x+1], m_TexCoords[y+1][x+1]); 
                m_pVertexes->appendPos(m_TileVertices[y+1][x], m_TexCoords[y+1][x]); 
                m_pVertexes->appendQuadIndexes(
                        curVertex+1, curVertex, curVertex+2, curVertex+3);
            }
        }
        m_bVertexArrayDirty = false;
    }

    m_pVertexes->draw();

    PixelFormat pf = m_pSurface->getPixelFormat();
    AVG_TRACE(Logger::BLTS, "(" << destSize.x << ", " << destSize.y << ")" 
            << ", m_pf: " << pf);
}

IntPoint RasterNode::getNumTiles()
{
    IntPoint size = m_pSurface->getSize();
    if (m_TileSize.x == -1) {
        return IntPoint(1,1);
    } else {
        return IntPoint(safeCeil(float(size.x)/m_TileSize.x),
                safeCeil(float(size.y)/m_TileSize.y));
    }
}

void RasterNode::calcVertexGrid(VertexGrid& grid)
{
    IntPoint numTiles = getNumTiles();
    std::vector<glm::vec2> TileVerticesLine(numTiles.x+1);
    grid = std::vector<std::vector<glm::vec2> > (numTiles.y+1, TileVerticesLine);
    for (unsigned y = 0; y < grid.size(); y++) {
        for (unsigned x = 0; x < grid[y].size(); x++) {
            calcTileVertex(x, y, grid[y][x]);
        }
    }
    if (m_pVertexes) {
        delete m_pVertexes;
    }
    m_bVertexArrayDirty = true;
    m_pVertexes = new VertexArray(numTiles.x*numTiles.y*4, numTiles.x*numTiles.y*6);
}

void RasterNode::calcTileVertex(int x, int y, glm::vec2& Vertex) 
{
    IntPoint numTiles = getNumTiles();
    if (x < numTiles.x) {
        Vertex.x = float(m_TileSize.x*x) / m_pSurface->getSize().x;
    } else {
        Vertex.x = 1;
    }
    if (y < numTiles.y) {
        Vertex.y = float(m_TileSize.y*y) / m_pSurface->getSize().y;
    } else {
        Vertex.y = 1;
    }
}

void RasterNode::calcTexCoords()
{
    glm::vec2 textureSize = glm::vec2(m_pSurface->getTextureSize());
    glm::vec2 imageSize = glm::vec2(m_pSurface->getSize());
    glm::vec2 texCoordExtents = glm::vec2(imageSize.x/textureSize.x,
            imageSize.y/textureSize.y);

    glm::vec2 texSizePerTile;
    if (m_TileSize.x == -1) {
        texSizePerTile = texCoordExtents;
    } else {
        texSizePerTile = glm::vec2(float(m_TileSize.x)/imageSize.x*texCoordExtents.x,
                float(m_TileSize.y)/imageSize.y*texCoordExtents.y);
    }

    IntPoint numTiles = getNumTiles();
    vector<glm::vec2> texCoordLine(numTiles.x+1);
    m_TexCoords = std::vector<std::vector<glm::vec2> > 
            (numTiles.y+1, texCoordLine);
    for (unsigned y = 0; y < m_TexCoords.size(); y++) {
        for (unsigned x = 0; x < m_TexCoords[y].size(); x++) {
            if (y == m_TexCoords.size()-1) {
                m_TexCoords[y][x].y = texCoordExtents.y;
            } else {
                m_TexCoords[y][x].y = texSizePerTile.y*y;
            }
            if (x == m_TexCoords[y].size()-1) {
                m_TexCoords[y][x].x = texCoordExtents.x;
            } else {
                m_TexCoords[y][x].x = texSizePerTile.x*x;
            }
        }
    }
    m_bVertexArrayDirty = true;
}
}

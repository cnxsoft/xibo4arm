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

#include "VectorNode.h"

#include "NodeDefinition.h"
#include "OGLSurface.h"
#include "Image.h"

#include "../base/Exception.h"
#include "../base/Logger.h"
#include "../base/ScopeTimer.h"
#include "../base/Exception.h"
#include "../base/WideLine.h"
#include "../base/GeomHelper.h"
#include "../base/Triangle.h"
#include "../base/ObjectCounter.h"

#include "../graphics/VertexArray.h"
#include "../graphics/Filterfliprgb.h"

#include "../glm/gtx/norm.hpp"

#include <iostream>
#include <sstream>

using namespace std;
using namespace boost;

namespace avg {

NodeDefinition VectorNode::createDefinition()
{
    return NodeDefinition("vector")
        .extendDefinition(Node::createDefinition())
        .addArg(Arg<string>("color", "FFFFFF", false, offsetof(VectorNode, m_sColorName)))
        .addArg(Arg<float>("strokewidth", 1, false, offsetof(VectorNode, m_StrokeWidth)))
        .addArg(Arg<UTF8String>("texhref", "", false, offsetof(VectorNode, m_TexHRef)))
        .addArg(Arg<string>("blendmode", "blend", false, 
                offsetof(VectorNode, m_sBlendMode)))
        ;
}

VectorNode::VectorNode(const ArgList& args)
{
    m_pShape = ShapePtr(createDefaultShape());

    ObjectCounter::get()->incRef(&typeid(*this));
    m_TexHRef = args.getArgVal<UTF8String>("texhref"); 
    setTexHRef(m_TexHRef);
    m_sColorName = args.getArgVal<string>("color");
    m_Color = colorStringToColor(m_sColorName);
}

VectorNode::~VectorNode()
{
    ObjectCounter::get()->decRef(&typeid(*this));
}

void VectorNode::connectDisplay()
{
    setDrawNeeded();
    m_Color = colorStringToColor(m_sColorName);
    Node::connectDisplay();
    m_pShape->moveToGPU();
    setBlendModeStr(m_sBlendMode);
}

void VectorNode::connect(CanvasPtr pCanvas)
{
    Node::connect(pCanvas);
    checkReload();
}

void VectorNode::disconnect(bool bKill)
{
    if (bKill) {
        m_pShape->discard();
    } else {
        m_pShape->moveToCPU();
    }
    Node::disconnect(bKill);
}

void VectorNode::checkReload()
{
    Node::checkReload(m_TexHRef, m_pShape->getImage());
    if (getState() == Node::NS_CANRENDER) {
        m_pShape->moveToGPU();
        setDrawNeeded();
    }
}

const UTF8String& VectorNode::getTexHRef() const
{
    return m_TexHRef;
}

void VectorNode::setTexHRef(const UTF8String& href)
{
    m_TexHRef = href;
    checkReload();
    setDrawNeeded();
}

void VectorNode::setBitmap(BitmapPtr pBmp)
{
    m_TexHRef = "";
    m_pShape->setBitmap(pBmp);
    setDrawNeeded();
}

const string& VectorNode::getBlendModeStr() const
{
    return m_sBlendMode;
}

void VectorNode::setBlendModeStr(const string& sBlendMode)
{
    m_sBlendMode = sBlendMode;
    m_BlendMode = GLContext::stringToBlendMode(sBlendMode);
}

static ProfilingZoneID PrerenderProfilingZone("VectorNode::prerender");

void VectorNode::preRender()
{
    Node::preRender();

    VertexArrayPtr pVA = m_pShape->getVertexArray();
    {
        if (m_bDrawNeeded) {
            ScopeTimer timer(PrerenderProfilingZone);
            pVA->reset();
            Pixel32 color = getColorVal();
            calcVertexes(pVA, color);
            pVA->update();
            m_bDrawNeeded = false;
        }
    }
    
}

void VectorNode::maybeRender()
{
    AVG_ASSERT(getState() == NS_CANRENDER);
    if (isVisible()) {
        if (getID() != "") {
            AVG_TRACE(Logger::BLTS, "Rendering " << getTypeStr() << 
                    " with ID " << getID());
        } else {
            AVG_TRACE(Logger::BLTS, "Rendering " << getTypeStr()); 
        }
        glLoadMatrixf(glm::value_ptr(getParentTransform()));
        GLContext::getCurrent()->setBlendMode(m_BlendMode);
        render();
    }
}

static ProfilingZoneID RenderProfilingZone("VectorNode::render");

void VectorNode::render()
{
    ScopeTimer timer(RenderProfilingZone);
//    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    float curOpacity = getEffectiveOpacity();
    m_pShape->draw(getParentTransform(), curOpacity);
}

void VectorNode::setColor(const string& sColor)
{
    if (m_sColorName != sColor) {
        m_sColorName = sColor;
        m_Color = colorStringToColor(m_sColorName);
        m_bDrawNeeded = true;
    }
}

const string& VectorNode::getColor() const
{
    return m_sColorName;
}

void VectorNode::setStrokeWidth(float width)
{
    if (width != m_StrokeWidth) {
        m_bDrawNeeded = true;
        m_StrokeWidth = width;
    }
}

float VectorNode::getStrokeWidth() const
{
    return m_StrokeWidth;
}

Pixel32 VectorNode::getColorVal() const
{
    return m_Color;
}

GLContext::BlendMode VectorNode::getBlendMode() const
{
    return m_BlendMode;
}

VectorNode::LineJoin VectorNode::string2LineJoin(const string& s)
{
    if (s == "miter") {
        return LJ_MITER;
    } else if (s == "bevel") {
        return LJ_BEVEL;
    } else {
        throw(Exception(AVG_ERR_UNSUPPORTED, 
                "Vector linejoin "+s+" not supported."));
    }
}

string VectorNode::lineJoin2String(LineJoin lineJoin)
{
    switch(lineJoin) {
        case LJ_MITER:
            return "miter";
        case LJ_BEVEL:
            return "bevel";
        default:
            AVG_ASSERT(false);
            return 0;
    }
}

void VectorNode::setDrawNeeded()
{
    m_bDrawNeeded = true;
}
        
bool VectorNode::isDrawNeeded()
{
    return m_bDrawNeeded;
}

void VectorNode::calcPolyLineCumulDist(vector<float>& cumulDists, 
        const vector<glm::vec2>& pts, bool bIsClosed)
{
    cumulDists.clear();
    cumulDists.reserve(pts.size());
    if (!pts.empty()) {
        vector<float> distances;
        distances.reserve(pts.size());
        float totalDist = 0;
        for (unsigned i = 1; i < pts.size(); ++i) {
            float dist = glm::length(pts[i] - pts[i-1]);
            distances.push_back(dist);
            totalDist += dist;
        }
        if (bIsClosed) {
            float dist = glm::length(pts[pts.size()-1] - pts[0]);
            distances.push_back(dist);
            totalDist += dist;
        }

        float cumulDist = 0;
        cumulDists.push_back(0);
        for (unsigned i = 0; i < distances.size(); ++i) {
            cumulDist += distances[i]/totalDist;
            cumulDists.push_back(cumulDist);
        }
    }
}

void VectorNode::calcEffPolyLineTexCoords(vector<float>& effTC, 
        const vector<float>& tc, const vector<float>& cumulDist)
{
    if (tc.empty()) {
        effTC = cumulDist;
    } else if (tc.size() == cumulDist.size()) {
        effTC = tc;
    } else {
        effTC.reserve(cumulDist.size());
        effTC = tc;
        float minGivenTexCoord = tc[0];
        float maxGivenTexCoord = tc[tc.size()-1];
        float maxCumulDist = cumulDist[tc.size()-1];
        int baselineDist = 0;
        for (unsigned i = tc.size(); i < cumulDist.size(); ++i) {
            int repeatFactor = int(cumulDist[i]/maxCumulDist);
            float effCumulDist = fmod(cumulDist[i], maxCumulDist);
            while (cumulDist[baselineDist+1] < effCumulDist) {
                baselineDist++;
            }
            float ratio = (effCumulDist-cumulDist[baselineDist])/
                    (cumulDist[baselineDist+1]-cumulDist[baselineDist]);
            float rawTexCoord = (1-ratio)*tc[baselineDist] +ratio*tc[baselineDist+1];
            float texCoord = rawTexCoord
                    +repeatFactor*(maxGivenTexCoord-minGivenTexCoord);
            effTC.push_back(texCoord);
        }
    }

}

void VectorNode::calcPolyLine(const vector<glm::vec2>& origPts, 
        const vector<float>& origTexCoords, bool bIsClosed, LineJoin lineJoin, 
        VertexArrayPtr& pVertexArray, Pixel32 color)
{
    vector<glm::vec2> pts;
    pts.reserve(origPts.size());
    vector<float> texCoords;
    texCoords.reserve(origPts.size());

    pts.push_back(origPts[0]);
    texCoords.push_back(origTexCoords[0]);
    for (unsigned i = 1; i < origPts.size(); ++i) {
        if (glm::distance2(origPts[i], origPts[i-1]) > 0.1) {
            pts.push_back(origPts[i]);
            texCoords.push_back(origTexCoords[i]);
        }
    }
    if (bIsClosed) {
        texCoords.push_back(origTexCoords[origTexCoords.size()-1]);
    }

    int numPts = pts.size();

    // Create array of wide lines.
    vector<WideLine> lines;
    lines.reserve(numPts-1);
    for (int i = 0; i < numPts-1; ++i) {
        lines.push_back(WideLine(pts[i], pts[i+1], m_StrokeWidth));
    }
    if (bIsClosed) {
        lines.push_back(WideLine(pts[numPts-1], pts[0], m_StrokeWidth));
    }
    // First points
    if (bIsClosed) {
        WideLine lastLine = lines[lines.size()-1];
        glm::vec2 pli = getLineLineIntersection(lastLine.pl0, lastLine.dir, 
                lines[0].pl0, lines[0].dir);
        glm::vec2 pri = getLineLineIntersection(lastLine.pr0, lastLine.dir, 
                lines[0].pr0, lines[0].dir);
        Triangle tri(lastLine.pl1, lines[0].pl0, pri);
        if (tri.isClockwise()) {
            if (!LineSegment(lastLine.pr0, lastLine.pr1).isPointOver(pri) &&
                    !LineSegment(lines[0].pr0, lines[0].pr1).isPointOver(pri))
            {
                pri = lines[0].pr1;
            }
        } else {
            if (!LineSegment(lastLine.pl0, lastLine.pl1).isPointOver(pli) &&
                    !LineSegment(lines[0].pl0, lines[0].pl1).isPointOver(pli))
            {
                pli = lines[0].pl1;
            }
        }

        float curTC = texCoords[0];
        switch (lineJoin) {
            case LJ_MITER:
                pVertexArray->appendPos(pli, glm::vec2(curTC,1), color);
                pVertexArray->appendPos(pri, glm::vec2(curTC,0), color);
                break;
            case LJ_BEVEL: {
                    if (tri.isClockwise()) {
                        pVertexArray->appendPos(lines[0].pl0, glm::vec2(curTC,1), color);
                        pVertexArray->appendPos(pri, glm::vec2(curTC,0), color);
                    } else {
                        pVertexArray->appendPos(pli, glm::vec2(curTC,1), color);
                        pVertexArray->appendPos(lines[0].pr0, glm::vec2(curTC,0), color);
                    }
                }
                break;
            default:
                AVG_ASSERT(false);
                break;
        }
    } else {
        pVertexArray->appendPos(lines[0].pl0, glm::vec2(texCoords[0],1), color);
        pVertexArray->appendPos(lines[0].pr0, glm::vec2(texCoords[0],0), color);
    }

    // All complete line segments
    unsigned numNormalSegments;
    if (bIsClosed) {
        numNormalSegments = pts.size();
    } else {
        numNormalSegments = pts.size()-2;
    }
    for (unsigned i = 0; i < numNormalSegments; ++i) {
        const WideLine* pLine1 = &(lines[i]);
        const WideLine* pLine2;
        if (i == pts.size()-1) {
            pLine2 = &(lines[0]);
        } else {
            pLine2 = &(lines[i+1]);
        }
        glm::vec2 pli = getLineLineIntersection(pLine1->pl0, pLine1->dir, pLine2->pl0,
                pLine2->dir);
        glm::vec2 pri = getLineLineIntersection(pLine1->pr0, pLine1->dir, pLine2->pr0,
                pLine2->dir);
        Triangle tri(pLine1->pl1, pLine2->pl0, pri);
        if (tri.isClockwise()) {
            if (!LineSegment(pLine1->pr0, pLine1->pr1).isPointOver(pri) &&
                    !LineSegment(pLine2->pr0, pLine2->pr1).isPointOver(pri))
            {
                pri = pLine2->pr1;
            }
        } else {
            if (!LineSegment(pLine1->pl0, pLine1->pl1).isPointOver(pli) &&
                    !LineSegment(pLine2->pl0, pLine2->pl1).isPointOver(pli))
            {
                pli = pLine2->pl1;
            }
        }

        int curVertex = pVertexArray->getCurVert();
        float curTC = texCoords[i+1];
        switch (lineJoin) {
            case LJ_MITER:
                pVertexArray->appendPos(pli, glm::vec2(curTC,1), color);
                pVertexArray->appendPos(pri, glm::vec2(curTC,0), color);
                pVertexArray->appendQuadIndexes(
                        curVertex-1, curVertex-2, curVertex+1, curVertex);
                break;
            case LJ_BEVEL:
                {
                    float TC0;
                    float TC1;
                    if (tri.isClockwise()) {
                        calcBevelTC(*pLine1, *pLine2, true, texCoords, i+1, TC0, TC1);
                        pVertexArray->appendPos(pLine1->pl1, glm::vec2(TC0,1), color);
                        pVertexArray->appendPos(pLine2->pl0, glm::vec2(TC1,1), color);
                        pVertexArray->appendPos(pri, glm::vec2(curTC,0), color);
                        pVertexArray->appendQuadIndexes(
                                curVertex-1, curVertex-2, curVertex+2, curVertex);
                        pVertexArray->appendTriIndexes(
                                curVertex, curVertex+1, curVertex+2);
                    } else {
                        calcBevelTC(*pLine1, *pLine2, false,  texCoords, i+1, TC0, TC1);
                        pVertexArray->appendPos(pLine1->pr1, glm::vec2(TC0,0), color);
                        pVertexArray->appendPos(pli, glm::vec2(curTC,1), color);
                        pVertexArray->appendPos(pLine2->pr0, glm::vec2(TC1,0), color);
                        pVertexArray->appendQuadIndexes(
                                curVertex-2, curVertex-1, curVertex+1, curVertex);
                        pVertexArray->appendTriIndexes(
                                curVertex, curVertex+1, curVertex+2);
                    }
                }
                break;
            default:
                AVG_ASSERT(false);
        }
    }

    // Last segment (PolyLine only)
    if (!bIsClosed) {
        int curVertex = pVertexArray->getCurVert();
        float curTC = texCoords[numPts-1];
        pVertexArray->appendPos(lines[numPts-2].pl1, glm::vec2(curTC,1), color);
        pVertexArray->appendPos(lines[numPts-2].pr1, glm::vec2(curTC,0), color);
        pVertexArray->appendQuadIndexes(curVertex-1, curVertex-2, curVertex+1, curVertex);
    }
}

void VectorNode::calcBevelTC(const WideLine& line1, const WideLine& line2, 
        bool bIsLeft, const vector<float>& texCoords, unsigned i, 
        float& TC0, float& TC1)
{
    float line1Len = line1.getLen();
    float line2Len = line2.getLen();
    float triLen;
    if (bIsLeft) {
        triLen = glm::length(line1.pl1 - line2.pl0);
    } else {
        triLen = glm::length(line1.pr1 - line2.pr0);
    }
    float ratio0 = line1Len/(line1Len+triLen/2);
    TC0 = (1-ratio0)*texCoords[i-1]+ratio0*texCoords[i];
    float nextTexCoord;
    if (i == texCoords.size()-1) {
        nextTexCoord = texCoords[i];
    } else {
        nextTexCoord = texCoords[i+1];
    }
    float ratio1 = line2Len/(line2Len+triLen/2);
    TC1 = ratio1*texCoords[i]+(1-ratio1)*nextTexCoord;
}

int VectorNode::getNumDifferentPts(const vector<glm::vec2>& pts)
{
    int numPts = pts.size();
    for (unsigned i=1; i<pts.size(); ++i) {
        if (glm::distance2(pts[i], pts[i-1])<0.1) {
            numPts--;
        }
    }
    return numPts;
}

Shape* VectorNode::createDefaultShape() const
{
    return new Shape(MaterialInfo(GL_REPEAT, GL_CLAMP_TO_EDGE, false));
}

}

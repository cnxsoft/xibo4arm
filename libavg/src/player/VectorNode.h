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

#ifndef _VectorNode_H_
#define _VectorNode_H_

#include "../api.h"
#include "Node.h"
#include "Shape.h"

#include "../base/UTF8String.h"
#include "../graphics/Pixel32.h"
#include "../graphics/VertexArray.h"
#include "../graphics/GLContext.h"

namespace avg {

struct WideLine;

class AVG_API VectorNode : public Node
{
    public:
        enum LineJoin {LJ_MITER, LJ_BEVEL};

        static NodeDefinition createDefinition();
        
        VectorNode(const ArgList& args);
        virtual ~VectorNode();
        virtual void connectDisplay();
        virtual void connect(CanvasPtr pCanvas);
        virtual void disconnect(bool bKill);
        virtual void checkReload();

        const UTF8String& getTexHRef() const;
        void setTexHRef(const UTF8String& href);
        void setBitmap(BitmapPtr pBmp);

        const std::string& getBlendModeStr() const;
        void setBlendModeStr(const std::string& sBlendMode);

        virtual void preRender();
        virtual void maybeRender();
        virtual void render();

        virtual void calcVertexes(VertexArrayPtr& pVertexArray, Pixel32 color) = 0;

        void setColor(const std::string& sColor);
        const std::string& getColor() const;

        void setStrokeWidth(float width);
        float getStrokeWidth() const;

        static LineJoin string2LineJoin(const std::string& s);
        static std::string lineJoin2String(LineJoin lineJoin);

    protected:
        Pixel32 getColorVal() const;
        GLContext::BlendMode getBlendMode() const;

        void setDrawNeeded();
        bool isDrawNeeded();
        bool hasVASizeChanged();
        void calcPolyLineCumulDist(std::vector<float>& cumulDist, 
                const std::vector<glm::vec2>& pts, bool bIsClosed);
        void calcEffPolyLineTexCoords(std::vector<float>& effTC, 
        const std::vector<float>& tc, const std::vector<float>& cumulDist);

        void calcPolyLine(const std::vector<glm::vec2>& origPts, 
                const std::vector<float>& origTexCoords, bool bIsClosed, 
                LineJoin lineJoin, VertexArrayPtr& pVertexArray, Pixel32 color);
        void calcBevelTC(const WideLine& line1, const WideLine& line2, 
                bool bIsLeft, const std::vector<float>& texCoords, unsigned i, 
                float& TC0, float& TC1);
        int getNumDifferentPts(const std::vector<glm::vec2>& pts);

    private:
        Shape* createDefaultShape() const;

        std::string m_sColorName;
        Pixel32 m_Color;
        float m_StrokeWidth;
        UTF8String m_TexHRef;
        std::string m_sBlendMode;

        bool m_bDrawNeeded;
        bool m_bVASizeChanged;

        ShapePtr m_pShape;
        GLContext::BlendMode m_BlendMode;
};

typedef boost::shared_ptr<VectorNode> VectorNodePtr;

}

#endif


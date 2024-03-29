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

#ifndef _PolyLineNode_H_
#define _PolyLineNode_H_

#include "../api.h"
#include "VectorNode.h"

#include "../graphics/Pixel32.h"
#include "../base/WideLine.h"

#include <vector>

namespace avg {

class AVG_API PolyLineNode : public VectorNode
{
    public:
        static NodeDefinition createDefinition();
        
        PolyLineNode(const ArgList& args);
        virtual ~PolyLineNode();

        const std::vector<glm::vec2>& getPos() const;
        void setPos(const std::vector<glm::vec2>& pts);

        const std::vector<float>& getTexCoords() const;
        void setTexCoords(const std::vector<float>& coords);

        std::string getLineJoin() const;
        void setLineJoin(const std::string& s);

        virtual void calcVertexes(VertexArrayPtr& pVertexArray, Pixel32 color);

    private:
        std::vector<glm::vec2> m_Pts;
        std::vector<float> m_CumulDist;
        std::vector<float> m_TexCoords;
        std::vector<float> m_EffTexCoords;
        LineJoin m_LineJoin;
};

}

#endif


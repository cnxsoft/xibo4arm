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

#ifndef _LineNode_H_
#define _LineNode_H_

#include "../api.h"
#include "VectorNode.h"

#include "../graphics/Pixel32.h"

namespace avg {

class AVG_API LineNode : public VectorNode
{
    public:
        static NodeDefinition createDefinition();
        
        LineNode(const ArgList& args);
        virtual ~LineNode();

        const glm::vec2& getPos1() const;
        void setPos1(const glm::vec2& pt);

        const glm::vec2& getPos2() const;
        void setPos2(const glm::vec2& pt);

        float getTexCoord1() const;
        void setTexCoord1(float tc);

        float getTexCoord2() const;
        void setTexCoord2(float tc);

        virtual void calcVertexes(VertexArrayPtr& pVertexArray, Pixel32 color);

    private:
        glm::vec2 m_P1;
        glm::vec2 m_P2;
        float m_TC1;
        float m_TC2;
};

}

#endif


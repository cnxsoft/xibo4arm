
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

#ifndef _DisplayEngine_H_
#define _DisplayEngine_H_

#include "../api.h"
#include "DisplayParams.h"

#include "../graphics/Pixel32.h"
#include "../graphics/Bitmap.h"

#include "../base/Rect.h"

#include <boost/shared_ptr.hpp>

#include <string>

namespace avg {

class Region;

class AVG_API DisplayEngine
{   
    public:
        DisplayEngine();
        virtual ~DisplayEngine();
        virtual void teardown() = 0;
        void initRender();
        void deinitRender();
        void setFramerate(float rate);
        float getFramerate();
        float getEffectiveFramerate();
        void setVBlankRate(int rate);
        bool wasFrameLate();
        virtual float getRefreshRate() = 0;
        virtual void setGamma(float Red, float Green, float Blue) = 0;
        virtual void setMousePos(const IntPoint& pos) = 0;
        virtual int getKeyModifierState() const = 0;

        void frameWait();
        virtual void swapBuffers() = 0;
        void checkJitter();
        long long getDisplayTime();

        virtual IntPoint getSize() = 0;

        virtual void showCursor(bool bShow) = 0;

        virtual BitmapPtr screenshot(int buffer=0) = 0;

    protected:
        
    private:
        int m_NumFrames;
        int m_FramesTooLate;
        long long m_StartTime;
        long long m_TimeSpentWaiting;

        // Per-Frame timings.
        long long m_LastFrameTime;
        long long m_FrameWaitStartTime;
        long long m_TargetTime;
        int m_VBRate;
        float m_Framerate;
        bool m_bInitialized;
        bool m_bFrameLate;

        float m_EffFramerate;
};

typedef boost::shared_ptr<DisplayEngine> DisplayEnginePtr;

}

#endif //_DisplayEngine_H_

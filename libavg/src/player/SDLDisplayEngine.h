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

#ifndef _SDLDisplayEngine_H_
#define _SDLDisplayEngine_H_

#include "../api.h"
#include "IInputDevice.h"
#include "DisplayEngine.h"

#include "../graphics/GLConfig.h"
#include "../graphics/Bitmap.h"
#include "../graphics/Pixel32.h"
#include "../graphics/OGLHelper.h"
#include "../graphics/FBO.h"

#include <string>
#include <vector>

struct SDL_Surface;
union SDL_Event;

namespace avg {

class XInputMTInputDevice;
class MouseEvent;
typedef boost::shared_ptr<class MouseEvent> MouseEventPtr;
class GLContext;
typedef boost::shared_ptr<class GLContext> GLContextPtr;

class AVG_API SDLDisplayEngine: public DisplayEngine, public IInputDevice
{
    public:
        SDLDisplayEngine();
        virtual ~SDLDisplayEngine();
        virtual void init(const DisplayParams& dp, GLConfig glConfig);

        // From DisplayEngine
        virtual void teardown();
        virtual float getRefreshRate();
        virtual void setGamma(float red, float green, float blue);
        virtual void setMousePos(const IntPoint& pos);
        virtual int getKeyModifierState() const;

        virtual IntPoint getSize();

        virtual void showCursor(bool bShow);
        virtual BitmapPtr screenshot(int buffer=0);

        // From IInputDevice
        virtual std::vector<EventPtr> pollEvents();
        void setXIMTInputDevice(XInputMTInputDevice* pInputDevice);

        const IntPoint& getWindowSize() const;
        bool isFullscreen() const;
        IntPoint getScreenResolution();
        float getPixelsPerMM();
        glm::vec2 getPhysicalScreenDimensions();
        void assumePixelsPerMM(float ppmm);
        virtual void swapBuffers();

    private:
        void initSDL(int width, int height, bool isFullscreen, int bpp);
        void initTranslationTable();
        void calcScreenDimensions(float dotsPerMM=0);

        bool internalSetGamma(float red, float green, float blue);

        EventPtr createMouseEvent
                (Event::Type Type, const SDL_Event & SDLEvent, long Button);
        EventPtr createMouseButtonEvent(Event::Type Type, const SDL_Event & SDLEvent);
        EventPtr createKeyEvent(Event::Type Type, const SDL_Event & SDLEvent);
        
        IntPoint m_Size;
        bool m_bIsFullscreen;
        IntPoint m_WindowSize;
        IntPoint m_ScreenResolution;
        float m_PPMM;

        SDL_Surface * m_pScreen;

        static void calcRefreshRate();
        static float s_RefreshRate;

        // Event handling.
        bool m_bMouseOverApp;
        MouseEventPtr m_pLastMouseEvent;
        int m_NumMouseButtonsDown;
        static std::vector<long> KeyCodeTranslationTable;
        XInputMTInputDevice * m_pXIMTInputDevice;

        GLContextPtr m_pGLContext;

        float m_Gamma[3];
};

typedef boost::shared_ptr<SDLDisplayEngine> SDLDisplayEnginePtr;

}

#endif

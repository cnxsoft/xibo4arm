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
//  Original author of this file is Nick Hebner (hebnern@gmail.com).
//

#ifndef _SDLAudioEngine_H_
#define _SDLAudioEngine_H_

#include "../api.h"
#include "AudioEngine.h"
#include "AudioBuffer.h"
#include "IProcessor.h"

#include <SDL/SDL.h>

#include <boost/thread/mutex.hpp>

namespace avg {

class AVG_API SDLAudioEngine : public AudioEngine
{   
    public:
        static SDLAudioEngine* get();
        SDLAudioEngine();
        virtual ~SDLAudioEngine();
        
        virtual int getChannels();
        virtual int getSampleRate();
        virtual const AudioParams * getParams();
        
        virtual void init(const AudioParams& AP, float volume);
        virtual void teardown();
        
        virtual void setAudioEnabled(bool bEnabled);
        
        virtual void play();
        virtual void pause();
        
        virtual void addSource(IAudioSource* pSource);
        virtual void removeSource(IAudioSource* pSource);
        virtual void setVolume(float volume);
        
    private:
        void mixAudio(Uint8 *pDestBuffer, int destBufferLen);
        static void audioCallback(void *userData, Uint8 *audioBuffer, int audioBufferLen);
        void addBuffers(float *pDest, AudioBufferPtr pSrc);
        void calcVolume(float *pBuffer, int numSamples, float volume);
        
        AudioParams m_AP;
        AudioBufferPtr m_pTempBuffer;
        float * m_pMixBuffer;
        IProcessor<float>* m_pLimiter;
        boost::mutex m_Mutex;

        static SDLAudioEngine* s_pInstance;
};

}

#endif //_SDLAudioEngine_H_

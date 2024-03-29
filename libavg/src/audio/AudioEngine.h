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

#ifndef _AudioEngine_H_
#define _AudioEngine_H_

#include "../api.h"
#include "IAudioSource.h"
#include "AudioParams.h"

#include <vector>

namespace avg {

typedef std::vector<IAudioSource*> AudioSourceList;

class AVG_API AudioEngine
{
    public:
        AudioEngine();
        virtual ~AudioEngine();

        virtual int getChannels() = 0;
        virtual int getSampleRate() = 0;
        virtual const AudioParams * getParams() = 0;

        virtual void setAudioEnabled(bool bEnabled);
        
        virtual void init(const AudioParams& ap, float volume);
        virtual void teardown() = 0;
        
        virtual void play() = 0;
        virtual void pause() = 0;
        
        AudioSourceList& getSources();
        virtual void addSource(IAudioSource* pSource);
        virtual void removeSource(IAudioSource* pSource);
        virtual void setVolume(float volume);
        float getVolume() const;
        bool isEnabled() const;
        
    private:
        bool m_bEnabled;
        AudioSourceList m_AudioSources;
        float m_Volume;
};

}

#endif

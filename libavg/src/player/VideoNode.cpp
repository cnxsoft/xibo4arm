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
#include "VideoNode.h"
#include "Player.h"
#include "OGLSurface.h"
#include "NodeDefinition.h"
#include "Canvas.h"

#include "../base/Exception.h"
#include "../base/Logger.h"
#include "../base/ScopeTimer.h"
#include "../base/XMLHelper.h"
#include "../base/ObjectCounter.h"

#include "../graphics/Filterfill.h"
#include "../graphics/GLTexture.h"
#include "../graphics/TextureMover.h"

#include "../audio/SDLAudioEngine.h"

#include "../video/AsyncVideoDecoder.h"
#include "../video/FFMpegDecoder.h"

#include <iostream>
#include <sstream>

#ifndef _WIN32
#include <unistd.h>
#endif

using namespace boost::python;
using namespace std;

namespace avg {

NodeDefinition VideoNode::createDefinition()
{
    return NodeDefinition("video", Node::buildNode<VideoNode>)
        .extendDefinition(RasterNode::createDefinition())
        .addArg(Arg<UTF8String>("href", "", false, offsetof(VideoNode, m_href)))
        .addArg(Arg<bool>("loop", false, false, offsetof(VideoNode, m_bLoop)))
        .addArg(Arg<bool>("threaded", true, false, offsetof(VideoNode, m_bThreaded)))
        .addArg(Arg<float>("fps", 0.0, false, offsetof(VideoNode, m_FPS)))
        .addArg(Arg<int>("queuelength", 8, false, 
                offsetof(VideoNode, m_QueueLength)))
        .addArg(Arg<float>("volume", 1.0, false, offsetof(VideoNode, m_Volume)))
        .addArg(Arg<bool>("accelerated", false, false,
                offsetof(VideoNode, m_bUsesHardwareAcceleration)))
        ;
}

VideoNode::VideoNode(const ArgList& args)
    : m_VideoState(Unloaded),
      m_bFrameAvailable(false),
      m_bFirstFrameDecoded(false),
      m_Filename(""),
      m_bEOFPending(false),
      m_pEOFCallback(0),
      m_FramesTooLate(0),
      m_FramesPlayed(0),
      m_SeekBeforeCanRenderTime(0),
      m_pDecoder(0),
      m_Volume(1.0),
      m_bUsesHardwareAcceleration(false)
{
    args.setMembers(this);
    m_Filename = m_href;
    initFilename(m_Filename);
    if (!m_bThreaded && m_QueueLength != 8) {
        throw Exception(AVG_ERR_INVALID_ARGS, 
                "Can't set queue length for unthreaded videos because there is no decoder queue in this case.");
    }
    if (m_bThreaded) {
        VideoDecoderPtr pSyncDecoder = VideoDecoderPtr(new FFMpegDecoder());
        m_pDecoder = new AsyncVideoDecoder(pSyncDecoder, m_QueueLength);
    } else {
        m_pDecoder = new FFMpegDecoder();
    }
    ObjectCounter::get()->incRef(&typeid(*this));
}

VideoNode::~VideoNode()
{
    if (m_pDecoder) {
        delete m_pDecoder;
        m_pDecoder = 0;
    }
    if (m_pEOFCallback) {
        Py_DECREF(m_pEOFCallback);
    }
    ObjectCounter::get()->decRef(&typeid(*this));
}

void VideoNode::connectDisplay()
{
    checkReload();
    RasterNode::connectDisplay();
    long long CurTime = Player::get()->getFrameTime(); 
    if (m_VideoState != Unloaded) {
        startDecoding();
    }
    if (m_VideoState == Paused) {
        m_PauseStartTime = CurTime;
    } 
}

void VideoNode::connect(CanvasPtr pCanvas)
{
    pCanvas->registerFrameEndListener(this);
    checkReload();
    RasterNode::connect(pCanvas);
}

void VideoNode::disconnect(bool bKill)
{
    getCanvas()->unregisterFrameEndListener(this);
    if (bKill) {
        setEOFCallback(Py_None);
    }
    changeVideoState(Unloaded);
    RasterNode::disconnect(bKill);
}

void VideoNode::play()
{
    changeVideoState(Playing);
}

void VideoNode::stop()
{
    changeVideoState(Unloaded);
}

void VideoNode::pause()
{
    changeVideoState(Paused);
}

int VideoNode::getNumFrames() const
{
    exceptionIfUnloaded("getNumFrames");
    return m_pDecoder->getVideoInfo().m_NumFrames;
}

int VideoNode::getCurFrame() const
{
    exceptionIfUnloaded("getCurFrame");
    int curFrame = m_pDecoder->getCurFrame();
    if (curFrame > 0) {
        return curFrame;
    } else {
        return 0;
    }
}

int VideoNode::getNumFramesQueued() const
{
    exceptionIfUnloaded("getNumFramesQueued");
    return m_pDecoder->getNumFramesQueued();
}

void VideoNode::seekToFrame(int frameNum)
{
    if (frameNum < 0) {
        throw Exception(AVG_ERR_OUT_OF_RANGE,
                "Can't seek to a negative frame in a video.");
    }
    exceptionIfUnloaded("seekToFrame");
    if (getCurFrame() != frameNum) {
        long long destTime = (long long)(frameNum*1000.0/m_pDecoder->getNominalFPS());
        seek(destTime);
    }
}

std::string VideoNode::getStreamPixelFormat() const
{
    exceptionIfUnloaded("getStreamPixelFormat");
    return m_pDecoder->getVideoInfo().m_sPixelFormat;
}

long long VideoNode::getDuration() const
{
    exceptionIfUnloaded("getDuration");
    return (long long)(m_pDecoder->getVideoInfo().m_Duration*1000);
}

long long VideoNode::getVideoDuration() const
{
    exceptionIfUnloaded("getVideoDuration");
    return (long long)(m_pDecoder->getVideoInfo().m_VideoDuration*1000);
}

long long VideoNode::getAudioDuration() const
{
    exceptionIfUnloaded("getAudioDuration");
    if (!hasAudio()) {
        throw Exception(AVG_ERR_INVALID_ARGS, "Video has no audio track.");
    }

    return (long long)(m_pDecoder->getVideoInfo().m_AudioDuration*1000);
}

int VideoNode::getBitrate() const
{
    exceptionIfUnloaded("getBitrate");
    return m_pDecoder->getVideoInfo().m_Bitrate;
}

string VideoNode::getVideoCodec() const
{
    exceptionIfUnloaded("getVideoCodec");
    return m_pDecoder->getVideoInfo().m_sVCodec;
}

std::string VideoNode::getAudioCodec() const
{
    exceptionIfNoAudio("getAudioCodec");
    return m_pDecoder->getVideoInfo().m_sACodec;
}

int VideoNode::getAudioSampleRate() const
{
    exceptionIfNoAudio("getAudioSampleRate");
    return m_pDecoder->getVideoInfo().m_SampleRate;
}

int VideoNode::getNumAudioChannels() const
{
    exceptionIfNoAudio("getNumAudioChannels");
    return m_pDecoder->getVideoInfo().m_NumAudioChannels;
}

long long VideoNode::getCurTime() const
{
    if (m_VideoState == Unloaded) {
        return 0;
    } else {
        long long curTime = (long long)(m_pDecoder->getCurTime()*1000);
        if (curTime > 0) {
            return curTime;
        } else {
            return 0;
        }
    }
}

void VideoNode::seekToTime(long long time)
{
    if (time < 0) {
        throw Exception(AVG_ERR_OUT_OF_RANGE,
                "Can't seek to a negative time in a video.");
    }
    exceptionIfUnloaded("seekToTime");
    seek(time);
    m_bSeekPending = true;
}

bool VideoNode::getLoop() const
{
    return m_bLoop;
}

bool VideoNode::isThreaded() const
{
    return m_bThreaded;
}

bool VideoNode::hasAudio() const
{
    exceptionIfUnloaded("hasAudio");
    return m_pDecoder->getVideoInfo().m_bHasAudio;
}

bool VideoNode::hasAlpha() const
{
    exceptionIfUnloaded("hasAlpha");
    PixelFormat pf = getPixelFormat();
    return pixelFormatHasAlpha(pf); 
}

void VideoNode::setEOFCallback(PyObject * pEOFCallback)
{
    if (m_pEOFCallback) {
        Py_DECREF(m_pEOFCallback);
    }
    if (pEOFCallback == Py_None) {
        m_pEOFCallback = 0;
    } else {
        Py_INCREF(pEOFCallback);
        m_pEOFCallback = pEOFCallback;
    }
}

bool VideoNode::isAccelerated() const
{
    exceptionIfUnloaded("isAccelerated");
    return m_bUsesHardwareAcceleration;
}

const UTF8String& VideoNode::getHRef() const
{
    return m_href;
}

void VideoNode::setHRef(const UTF8String& href)
{
    m_href = href;
    checkReload();
}

float VideoNode::getVolume()
{
    return m_Volume;
}

void VideoNode::setVolume(float Volume)
{
    if (Volume < 0) {
        Volume = 0;
    }
    m_Volume = Volume;
    if (m_VideoState != Unloaded && hasAudio()) {
        m_pDecoder->setVolume(Volume);
    }
}

void VideoNode::checkReload()
{
    string fileName (m_href);
    if (m_href != "") {
        initFilename(fileName);
        if (fileName != m_Filename && m_VideoState != Unloaded) {
            changeVideoState(Unloaded);
            m_Filename = fileName;
            changeVideoState(Paused);
        } else {
            m_Filename = fileName;
        }
    } else {
        changeVideoState(Unloaded);
        m_Filename = "";
    }
    RasterNode::checkReload();
}

void VideoNode::onFrameEnd()
{
    if (m_bEOFPending) {
        // If the VideoNode is unlinked by python in onEOF, the following line prevents
        // the object from being deleted until we return from this function.
        NodePtr pTempThis = shared_from_this();
        m_bEOFPending = false;
        onEOF();
    }
}

int VideoNode::fillAudioBuffer(AudioBufferPtr pBuffer)
{
    AVG_ASSERT(m_bThreaded);
    if (m_VideoState == Playing) {
        return m_pDecoder->fillAudioBuffer(pBuffer);
    } else {
        return 0;
    }
}

void VideoNode::changeVideoState(VideoState NewVideoState)
{
    long long curTime = Player::get()->getFrameTime(); 
    if (m_VideoState == NewVideoState) {
        return;
    }
    if (m_VideoState == Unloaded) {
        m_PauseStartTime = curTime;
        open();
    }
    if (NewVideoState == Unloaded) {
        close();
    }
    if (getState() == NS_CANRENDER) {
        if (m_VideoState == Unloaded) {
            startDecoding();
        }
        if (NewVideoState == Paused) {
            m_PauseStartTime = curTime;
        } else if (NewVideoState == Playing && m_VideoState == Paused) {
/*            
            cerr << "Play after pause:" << endl;
            cerr << "  getFrameTime()=" << curTime << endl;
            cerr << "  m_PauseStartTime=" << m_PauseStartTime << endl;
            cerr << "  offset=" << (1000.0/m_pDecoder->getFPS()) << endl;
*/
            m_PauseTime += (curTime-m_PauseStartTime
                    - (long long)(1000.0/m_pDecoder->getFPS()));
        }
    }
    m_VideoState = NewVideoState;
}

void VideoNode::seek(long long destTime) 
{
    if (getState() == NS_CANRENDER) {    
        m_pDecoder->seek(float(destTime)/1000.0f);
        m_StartTime = Player::get()->getFrameTime() - destTime;
        m_JitterCompensation = 0.5;
        m_PauseTime = 0;
        m_PauseStartTime = Player::get()->getFrameTime();
        m_bFrameAvailable = false;
        m_bSeekPending = true;
    } else {
        // If we get a seek command before decoding has really started, we need to defer 
        // the actual seek until the decoder is ready.
        m_SeekBeforeCanRenderTime = destTime;
    }
}

void VideoNode::open() 
{
    m_FramesTooLate = 0;
    m_FramesInRowTooLate = 0;
    m_FramesPlayed = 0;
    m_pDecoder->open(m_Filename, m_bThreaded, m_bUsesHardwareAcceleration);
    m_pDecoder->setVolume(m_Volume);
    VideoInfo videoInfo = m_pDecoder->getVideoInfo();
    if (!videoInfo.m_bHasVideo) {
        m_pDecoder->close();
        throw Exception(AVG_ERR_VIDEO_GENERAL, 
                string("Video: Opening "+m_Filename+" failed. No video stream found."));
    }
    m_StartTime = Player::get()->getFrameTime();
    m_JitterCompensation = 0.5;
    m_PauseTime = 0;

    m_bFirstFrameDecoded = false;
    m_bFrameAvailable = false;
    m_bUsesHardwareAcceleration = videoInfo.m_bUsesVDPAU;
}

void VideoNode::startDecoding()
{
    const AudioParams * pAP = 0;
    SDLAudioEngine* pAudioEngine = SDLAudioEngine::get();
    if (pAudioEngine) {
        pAP = pAudioEngine->getParams();
    }
    m_pDecoder->startDecoding(GLContext::getCurrent()->useGPUYUVConversion(), pAP);
    VideoInfo videoInfo = m_pDecoder->getVideoInfo();
    if (m_FPS != 0.0) {
        if (videoInfo.m_bHasAudio) {
            AVG_TRACE(Logger::WARNING, 
                    getID() + ": Can't set FPS if video contains audio. Ignored.");
        } else {
            m_pDecoder->setFPS(m_FPS);
        }
    }
    if (videoInfo.m_bHasAudio && pAudioEngine) {
        pAudioEngine->addSource(this);
    }
    m_bSeekPending = true;
    
    setViewport(-32767, -32767, -32767, -32767);
    createTextures(videoInfo.m_Size);
   
    if (m_SeekBeforeCanRenderTime != 0) {
        seek(m_SeekBeforeCanRenderTime);
        m_SeekBeforeCanRenderTime = 0;
    }
}

void VideoNode::createTextures(IntPoint size)
{
    PixelFormat pf = getPixelFormat();
    bool bMipmap = getMaterial().getUseMipmaps();
    if (pixelFormatIsPlanar(pf)) {
        m_pTextures[0] = GLTexturePtr(new GLTexture(size, I8, bMipmap));
        IntPoint halfSize(size.x/2, size.y/2);
        m_pTextures[1] = GLTexturePtr(new GLTexture(halfSize, I8, bMipmap));
        m_pTextures[2] = GLTexturePtr(new GLTexture(halfSize, I8, bMipmap));
        if (pixelFormatHasAlpha(pf)) {
            m_pTextures[3] = GLTexturePtr(new GLTexture(size, I8, bMipmap));
        }
    } else {
        m_pTextures[0] = GLTexturePtr(new GLTexture(size, pf, bMipmap));
    }
    for (unsigned i=0; i<getNumPixelFormatPlanes(pf); ++i) {
        m_pTextures[i]->enableStreaming();
    }
    if (pf == B8G8R8X8 || pf == B8G8R8A8) {
        FilterFill<Pixel32> Filter(Pixel32(0,0,0,255));
        BitmapPtr pBmp = m_pTextures[0]->lockStreamingBmp();
        Filter.applyInPlace(pBmp);
        m_pTextures[0]->unlockStreamingBmp(true);
    }
    if (pixelFormatIsPlanar(pf)) {
        if (pixelFormatHasAlpha(pf)) {
            getSurface()->create(pf, m_pTextures[0], m_pTextures[1], m_pTextures[2],
                    m_pTextures[3]);
        } else {
            getSurface()->create(pf, m_pTextures[0], m_pTextures[1], m_pTextures[2]);
        }
    } else {
        getSurface()->create(pf, m_pTextures[0]);
    }
}

void VideoNode::close()
{
    SDLAudioEngine* pAudioEngine = SDLAudioEngine::get();
    if (hasAudio() && pAudioEngine) {
        pAudioEngine->removeSource(this);
    }
    m_pDecoder->close();
    if (m_FramesTooLate > 0) {
        string sID;
        if (getID() == "") {
            sID = m_href; 
        } else {
            sID = getID();
        }
        AVG_TRACE(Logger::PROFILE_VIDEO, "Missed video frames for '" << sID << "': " 
                << m_FramesTooLate << " of " << m_FramesPlayed);
        m_FramesTooLate = 0;
    }
}

PixelFormat VideoNode::getPixelFormat() const 
{
    return m_pDecoder->getPixelFormat();
}

IntPoint VideoNode::getMediaSize()
{
    if (m_pDecoder && m_pDecoder->getState() != VideoDecoder::CLOSED) {
        return m_pDecoder->getSize();
    } else {
        return IntPoint(0,0);
    }
}

float VideoNode::getFPS() const
{
    return m_pDecoder->getFPS();
}

int VideoNode::getQueueLength() const
{
    return m_QueueLength;
}

long long VideoNode::getNextFrameTime() const
{
    switch (m_VideoState) {
        case Unloaded:
            return 0;
        case Paused:
            return m_PauseStartTime-m_StartTime;
        case Playing:
            {
                if (Player::get()->getFrameTime()-m_StartTime-m_PauseTime < 0) {
                    cerr << "getNextFrameTime < 0" << endl;
                    cerr << "getFrameTime(): " << Player::get()->getFrameTime() << endl;
                    cerr << "m_StartTime: " << m_StartTime << endl;
                    cerr << "m_PauseTime: " << m_PauseTime << endl;
                }
                long long nextFrameTime = Player::get()->getFrameTime()-m_StartTime
                        -m_PauseTime
                        -(long long)(m_JitterCompensation*1000.0/
                                Player::get()->getFramerate());
                if (nextFrameTime < 0) {
                    nextFrameTime = 0;
                }
                return nextFrameTime;
            }
        default:
            AVG_ASSERT(false);
            return 0;
    }
}

void VideoNode::exceptionIfNoAudio(const std::string& sFuncName) const
{
    exceptionIfUnloaded(sFuncName);
    if (!hasAudio()) {
        throw Exception(AVG_ERR_VIDEO_GENERAL, 
                string("VideoNode.")+sFuncName+" failed: no audio stream.");
    }
}

void VideoNode::exceptionIfUnloaded(const std::string& sFuncName) const
{
    if (m_VideoState == Unloaded) {
        throw Exception(AVG_ERR_VIDEO_GENERAL, 
                string("VideoNode.")+sFuncName+" failed: video not loaded.");
    }
}

static ProfilingZoneID PrerenderProfilingZone("VideoNode::prerender");

void VideoNode::preRender()
{
    ScopeTimer timer(PrerenderProfilingZone);
    Node::preRender();
    if (isVisible()) {
        if (m_VideoState != Unloaded) {
            if (m_VideoState == Playing) {
                bool bNewFrame = renderFrame();
                m_bFrameAvailable |= bNewFrame;
            } else { // Paused
                if (!m_bFrameAvailable) {
                    m_bFrameAvailable = renderFrame();
                }
            }
            m_bFirstFrameDecoded |= m_bFrameAvailable;
            if (m_bFirstFrameDecoded) {
                renderFX(getSize(), Pixel32(255, 255, 255, 255), false);
            }
        }
    } else {
        if (m_bSeekPending && m_bFirstFrameDecoded && m_VideoState != Unloaded) {
            renderFrame();
        }
        if (m_VideoState == Playing) {
            // Throw away frames that are not visible to make sure the video 
            // stays in sync.
            m_pDecoder->throwAwayFrame(getNextFrameTime()/1000.0f);

            if (m_pDecoder->isEOF()) {
                updateStatusDueToDecoderEOF();
            }
        }
    }
}

static ProfilingZoneID RenderProfilingZone("VideoNode::render");

void VideoNode::render()
{
    ScopeTimer timer(RenderProfilingZone);
    if (m_VideoState != Unloaded && m_bFirstFrameDecoded) {
        blt32(getTransform(), getSize(), getEffectiveOpacity(), getBlendMode());
    }
}

VideoNode::VideoAccelType VideoNode::getVideoAccelConfig()
{
#ifdef AVG_ENABLE_VDPAU
    if (VDPAU::isAvailable()) {
        return VDPAU;
    }
#endif
    return NONE;
}

bool VideoNode::renderFrame()
{
    FrameAvailableCode frameAvailable = renderToSurface();
    if (m_pDecoder->isEOF()) {
//        AVG_TRACE(Logger::PROFILE, "------------------ EOF -----------------");
        updateStatusDueToDecoderEOF();
        if (m_bLoop) {
            frameAvailable = renderToSurface();
        }
    }

    switch (frameAvailable) {
        case FA_NEW_FRAME:
            m_FramesPlayed++;
            m_FramesInRowTooLate = 0;
            bind();
            m_bSeekPending = false;
            setMaskCoords();
//            AVG_TRACE(Logger::PROFILE, "New frame.");
            break;
        case FA_STILL_DECODING:
            {
                m_FramesPlayed++;
                m_FramesTooLate++;
                m_FramesInRowTooLate++;
                float framerate = Player::get()->getEffectiveFramerate();
                long long frameTime = Player::get()->getFrameTime();
                if (m_VideoState == Playing) {
                    if (m_FramesInRowTooLate > 3 && framerate != 0) {
                        // Heuristic: If we've missed more than 3 frames in a row, we stop
                        // advancing movie time until the decoder has caught up.
                        m_PauseTime += (long long)(1000/framerate);
                    }
                    if (m_bSeekPending) {
                        // The movie time also stays still while waiting for a seek to 
                        // complete.
                        m_PauseTime = frameTime-m_PauseStartTime;
                    }
                    long long curMovieTime = 
                            Player::get()->getFrameTime()-m_StartTime-m_PauseTime;
                    if (curMovieTime < 0) {
                        cerr << "----------- curTime < 0 -------------" << endl;
                        cerr << "FramesPlayed=" << m_FramesPlayed << endl;
                        cerr << "getFrameTime()=" << Player::get()->getFrameTime()
                                << endl;
                        cerr << "m_StartTime=" << m_StartTime << endl;
                        cerr << "m_PauseTime=" << m_PauseTime << endl;
                        m_PauseTime = Player::get()->getFrameTime()-m_StartTime;
                    }
                }
            }
//            AVG_TRACE(Logger::PROFILE, "Missed video frame.");
            break;
        case FA_USE_LAST_FRAME:
            m_FramesInRowTooLate = 0;
            m_bSeekPending = false;
//            AVG_TRACE(Logger::PROFILE, "Video frame reused.");
            break;
        default:
            AVG_ASSERT(false);
    }

    return (frameAvailable == FA_NEW_FRAME);
    return false;
}

FrameAvailableCode VideoNode::renderToSurface()
{
    FrameAvailableCode frameAvailable;
    PixelFormat pf = m_pDecoder->getPixelFormat();
    std::vector<BitmapPtr> pBmps;
    for (unsigned i=0; i<getNumPixelFormatPlanes(pf); ++i) {
        pBmps.push_back(m_pTextures[i]->lockStreamingBmp());
    }
    if (pixelFormatIsPlanar(pf)) {
        frameAvailable = m_pDecoder->renderToBmps(pBmps, getNextFrameTime()/1000.0f);
    } else {
        frameAvailable = m_pDecoder->renderToBmp(pBmps[0], getNextFrameTime()/1000.0f);
    }
    for (unsigned i=0; i<getNumPixelFormatPlanes(pf); ++i) {
        m_pTextures[i]->unlockStreamingBmp(frameAvailable == FA_NEW_FRAME);
    }

    // Even with vsync, frame duration has a bit of jitter. If the video frames rendered
    // are at the border of a frame's time, this can cause irregular display times.
    // So, if we detect this condition, we adjust the frame time by a small fraction
    // to move it towards the center of the time slot.
    long long jitter = (long long)(getNextFrameTime()-m_pDecoder->getCurTime()*1000);
    if (jitter > (long long)(0.4*(1000/m_pDecoder->getFPS()))) {
        m_JitterCompensation += 0.05;
        if (m_JitterCompensation > 1) {
            m_JitterCompensation -= 1;
        }
    }
    return frameAvailable;
}

void VideoNode::onEOF()
{
    if (m_pEOFCallback) {
        PyObject * arglist = Py_BuildValue("()");
        PyObject * result = PyEval_CallObject(m_pEOFCallback, arglist);
        Py_DECREF(arglist);    
        if (!result) {
            throw error_already_set();
        }
        Py_DECREF(result);
    }
}


void VideoNode::updateStatusDueToDecoderEOF()
{
    m_bEOFPending = true;
    if (m_bLoop) {
        m_StartTime = Player::get()->getFrameTime();
        m_JitterCompensation = 0.5;
        m_PauseTime = 0;
        m_FramesInRowTooLate = 0;
        m_bFrameAvailable = false;
        m_pDecoder->loop();
    } else {
        changeVideoState(Paused);
    }
}


}


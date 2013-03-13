/****************************************************************************
 Author: Luma (stubma@gmail.com)
 
 https://github.com/stubma/cocos2dx-wisound
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/
#if BACKEND_OPENAL

#include "CCAudioPlayer.h"
#include "CCOpenAL.h"
#include "SimpleAudioEngine_openal.h"

#define BUFFER_SIZE 40960
#define MAX_BUFFER 3

namespace WiSound {

CCAudioPlayer::CCAudioPlayer(CCAudioStream* stream) :
        m_stream(NULL),
        m_buffers(NULL),
        m_source(0),
        m_renderedSeconds(0),
        m_secondsPerBuffer(0),
        m_tempBuffer(NULL),
        m_soundId(0),
        m_loop(0),
        m_playing(false),
        m_paused(false) {
    m_stream = stream;
    CC_SAFE_RETAIN(m_stream);
            
    // create buffer
    m_buffers = (ALuint*)calloc(MAX_BUFFER, sizeof(ALuint));
    alGenBuffers(m_stream->isSingleBuffer() ? 1 : MAX_BUFFER, m_buffers);
            
    // get buffer seconds
    m_secondsPerBuffer = (float)BUFFER_SIZE / (m_stream->getBitsPerSample() / 8) / m_stream->getChannel() / m_stream->getSampleRate();
    
    // load audio all if single buffer
    if(m_stream->isSingleBuffer()) {
        // read until no data
        char* buf = (char*)malloc(BUFFER_SIZE * sizeof(char));
        int bufSize = BUFFER_SIZE;
        int total = 0;
        int readLen = 0;
        while((readLen = m_stream->read(buf + total, BUFFER_SIZE)) > 0) {
            total += readLen;
            if(total + BUFFER_SIZE > bufSize) {
                buf = (char*)realloc(buf, bufSize * 2);
                bufSize *= 2;
            }
        }
        
        // set to buffer
        alBufferData(m_buffers[0], getALFormat(), buf, total, m_stream->getSampleRate());
        free(buf);
    }
}

CCAudioPlayer::~CCAudioPlayer() {
    // ensure audio is stopped
    stop();
    
    // free temp buffer
	if(m_tempBuffer) {
		free(m_tempBuffer);
		m_tempBuffer = NULL;
	}
    
	// free source
	if(m_stream->isSingleBuffer() && m_source != 0) {
		CCOpenAL::freeSource(m_source);
	}
    
	// free buffer
	alDeleteBuffers(m_stream->isSingleBuffer() ? 1 : MAX_BUFFER, m_buffers);
	free(m_buffers);
    
    // release stream
    CC_SAFE_RELEASE(m_stream);
}

CCAudioPlayer* CCAudioPlayer::create(CCAudioStream* stream) {
    CCAudioPlayer* p = new CCAudioPlayer(stream);
    return (CCAudioPlayer*)p->autorelease();
}

ALenum CCAudioPlayer::getALFormat() {
	switch(m_stream->getBitsPerSample()) {
		case 8:
			return m_stream->getChannel() == 1 ? AL_FORMAT_MONO8 : AL_FORMAT_STEREO8;
		case 16:
			return m_stream->getChannel() == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
		default:
			return 0;
	}
}

void CCAudioPlayer::play(float volume) {
	if(!m_playing) {
		// play
		if(m_stream->isSingleBuffer()) {
			// set source
			m_source = CCOpenAL::obtainSource();
			if(m_source == 0)
				return;
			alSourcei(m_source, AL_BUFFER, m_buffers[0]);
			alSourcei(m_source, AL_LOOPING, AL_FALSE);
			alSourcef(m_source, AL_GAIN, volume);
            
			// play
			alSourcePlay(m_source);
		} else {
			// set source
			m_source = CCOpenAL::obtainSource();
			if(m_source == 0)
				return;
			alSourcei(m_source, AL_LOOPING, AL_FALSE);
			alSourcef(m_source, AL_GAIN, volume);
            
            // force to clear the error code, due to that there would be some unpredicted error happened here 
            int error = alGetError();
            if(error != AL_NO_ERROR) {
				CCLOGWARN("CCAudioPlayer::play: AL error occured: 0x%X", error);
			}
            
			// queue buffers
			for (int i = 0; i < MAX_BUFFER; i++) {
				if (!fill(m_buffers[i]))
					break;
				alSourceQueueBuffers(m_source, 1, &m_buffers[i]);
			}
            
			// check error
			error = alGetError();
			if(error != AL_NO_ERROR) {
				CCLOGWARN("CCAudioPlayer::play: AL error occured: 0x%X", error);
				stop();
				return;
			}
            
			// play
			alSourcePlay(m_source);
		}
        
		// set flag
		m_playing = true;
	}
}

void CCAudioPlayer::stop() {
	if(m_playing && m_source) {
        CCOpenAL::freeSource(m_source);
        
		m_stream->reset();
		m_source = 0;
		m_playing = false;
		m_paused = false;
        
		// notify manager
		((SimpleAudioEngine_openal*)SimpleAudioEngine::sharedEngine())->onAudioStop(this);
	}
}

void CCAudioPlayer::pause() {
	if(!m_paused && m_playing) {
		if(m_source != 0) {
			alSourcePause(m_source);
			m_paused = true;
		}
	}
}

void CCAudioPlayer::resume() {
	if(m_paused && m_playing) {
		if(m_source != 0) {
			alSourcePlay(m_source);
			m_paused = false;
		}
	}
}

void CCAudioPlayer::update() {
	if(m_source == 0)
		return;
    
	if(!m_stream->isSingleBuffer()) {
		bool end = false;
		ALint buffers;
		alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &buffers);
		while(buffers-- > 0) {
			// unqueue a buffer
			ALuint buffer;
			alSourceUnqueueBuffers(m_source, 1, &buffer);
			if(buffer == AL_INVALID_VALUE)
				break;
            
			// adjust rendered seconds
			m_renderedSeconds += m_secondsPerBuffer;
            
			// try to fill buffer, if failed, let it end
			if(fill(buffer))
				alSourceQueueBuffers(m_source, 1, &buffer);
			else {
				end = true;
				break;
			}
		}
        
		// if no queued buffer or end flag is set
		alGetSourcei(m_source, AL_BUFFERS_QUEUED, &buffers);
		if(end && buffers == 0) {
			stop();
			return;
		}
        
		// A buffer underflow will cause the source to stop.
		ALint state;
		alGetSourcei(m_source, AL_SOURCE_STATE, &state);
		if(m_playing && !m_paused && state != AL_PLAYING)
			alSourcePlay(m_source);
	} else {
		if(m_playing) {
			ALint state;
			alGetSourcei(m_source, AL_SOURCE_STATE, &state);
			if(state != AL_PLAYING)
				stop();
		}
	}
    
    // check error
    int error = alGetError();
    if(error != AL_NO_ERROR) {
        CCLOGWARN("CCAudioPlayer::update: AL error occured: 0x%X", error);
    }
}

void CCAudioPlayer::setVolume(float volume) {
	if(m_source != 0)
		alSourcef(m_source, AL_GAIN, volume);
}

bool CCAudioPlayer::fill(ALuint buffer) {
	// create buffer if not
	if(!m_tempBuffer) {
		m_tempBuffer = (char*)malloc(BUFFER_SIZE * sizeof(char));
	}
    
	// read max buffer size
	size_t length = m_stream->read(m_tempBuffer, BUFFER_SIZE);
	if(length <= 0) {
		if(isLoop()) {
			// decrease loop count
			if(m_loop > 0)
				m_loop--;
            
			// reset to first and read again
			m_stream->reset();
			length = m_stream->read(m_tempBuffer, BUFFER_SIZE);
			if(length <= 0)
				return false;
		} else {
			return false;
		}
	}
    
	// set to buffer
	alBufferData(buffer, getALFormat(), m_tempBuffer, length, m_stream->getSampleRate());
    
	return true;
}
    
} // end of namespace WiSound

#endif // #if BACKEND_OPENAL

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

#include "cocos2d.h"
#include "CCOpenAL.h"
#include <stdlib.h>

USING_NS_CC;

// max sources
#define MAX_SOURCE 10

bool CCOpenAL::s_inited = false;
ALCcontext* CCOpenAL::s_context = NULL;
ALCdevice* CCOpenAL::s_device = NULL;

void CCOpenAL::init(const char* deviceArguments, int contextFrequency, int contextRefresh, bool contextSynchronized) {
	if(!s_inited) {
		s_inited = true;

		// create device
		s_device = alcOpenDevice((const ALCchar*)deviceArguments);
		if(!s_device) {
			CCLOGERROR("Failed to create OpenAL device");
			return;
		}

		// create context
		if(contextFrequency == -1) {
			s_context = alcCreateContext(s_device, NULL);
		} else {
			ALCint attrList[] = {
				ALC_FREQUENCY, contextFrequency,
				ALC_REFRESH, contextRefresh,
				ALC_SYNC, contextSynchronized,
				0
			};
			s_context = alcCreateContext(s_device, attrList);
		}

		// make current context
		alcMakeContextCurrent(s_context);

		// add listener
		ALfloat orient[] = {
			0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f
		};
		ALfloat zero[] = {
			0.0f, 0.0f, 0.0f
		};
		alListenerfv(AL_ORIENTATION, orient);
		alListenerfv(AL_VELOCITY, zero);
		alListenerfv(AL_POSITION, zero);
	}
}

void CCOpenAL::destroy() {
	if(s_inited) {
		// free context
		if(s_context) {
			alcMakeContextCurrent(NULL);
			alcDestroyContext(s_context);
			s_context = NULL;
		}

		// free device
		if(s_device) {
			alcCloseDevice(s_device);
			s_device = NULL;
		}

		s_inited = false;
	}
}

ALuint CCOpenAL::obtainSource() {
	ALuint source = 0;
	alGenSources(1, &source);
	if(source == 0)
		CCLOGWARN("Failed to generate audio source");
	return source;
}

void CCOpenAL::freeSource(ALuint source) {
	if(source == 0)
		return;

	// stop source
    alSourceStop(source);
    
    // unqueue source buffer or just clear it
    ALint bufferCount = 0;
    alGetSourcei(source, AL_BUFFERS_QUEUED, &bufferCount);
    if(bufferCount > 0) {
        for(int i = 0; i < bufferCount; ++i) {
            ALuint buffer = 0;
            alSourceUnqueueBuffers(source, 1, &buffer); 
        }
    } else {
        alSourcei(source, AL_BUFFER, 0);
    }

    // delete source
	alDeleteSources(1, &source);
}

#endif // #if BACKEND_OPENAL

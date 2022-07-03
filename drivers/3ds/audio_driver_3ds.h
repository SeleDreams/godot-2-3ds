/*************************************************************************/
/*  audio_driver_3ds.h                                                   */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                    http://www.godotengine.org                         */
/*************************************************************************/
/* Copyright (c) 2007-2016 Juan Linietsky, Ariel Manzur.                 */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
#ifndef AUDIO_DRIVER_3DS_H
#define AUDIO_DRIVER_3DS_H

#include "servers/audio/audio_server_sw.h"

#include "core/os/thread.h"
#include "core/os/mutex.h"

extern "C" {
#include <3ds/types.h>
#include <3ds/ndsp/ndsp.h>
#include <3ds/ndsp/channel.h>
#include <3ds/allocator/linear.h>
#include <3ds/services/dsp.h>
}


class AudioDriver3ds : public AudioDriverSW {
	
	enum {
		NDSP_BUFFER_COUNT = 3,
	};

	Thread* thread;
	Mutex* mutex;

	int32_t* samples_in;
	
	ndspWaveBuf ndsp_buffers[NDSP_BUFFER_COUNT];

	static void thread_func(void* p_udata);
	int buffer_size;

	unsigned int mix_rate;
	OutputFormat output_format;

	int channels;

	bool active;
	bool thread_exited;
	mutable bool exit_thread;
	bool pcm_open;

public:

	const char* get_name() const {
		return "3DS NDSP";
	};

	virtual Error init();
	virtual void start();
	virtual int get_mix_rate() const;
	virtual OutputFormat get_output_format() const;
	virtual void lock();
	virtual void unlock();
	virtual void finish();

	AudioDriver3ds();
	~AudioDriver3ds();
};

#endif

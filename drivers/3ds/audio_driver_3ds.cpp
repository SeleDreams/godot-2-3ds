/*************************************************************************/
/*  audio_driver_3ds.cpp                                                 */
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
#ifdef _3DS
#include "audio_driver_3ds.h"

#include "globals.h"
#include "os/os.h"
#include <string.h>

static int channel_num = 1;


Error AudioDriver3ds::init() {
	
	if (ndspInit() < 0)
		return FAILED;

	active=false;
	thread_exited=false;
	exit_thread=false;
	pcm_open = false;
	samples_in = NULL;

	mix_rate = 44100;
	output_format = OUTPUT_STEREO;
	channels = 2;

	int latency = GLOBAL_DEF("audio/output_latency",25);
	buffer_size = nearest_power_of_2( latency * mix_rate / 1000 );

	samples_in = memnew_arr(int32_t, buffer_size*channels);
	
	ndspWaveBuf* buffer = ndsp_buffers;
	for (int i = 0; i < NDSP_BUFFER_COUNT; ++i) {
		memset(buffer, 0, sizeof(ndspWaveBuf));
		buffer->data_vaddr = linearAlloc(buffer_size * channels * sizeof(int16_t));
		buffer->nsamples = buffer_size;
		buffer->looping = false;
		buffer->status = NDSP_WBUF_DONE;
		buffer++;
	}
	
	ndspChnReset(channel_num);
	ndspChnSetInterp(channel_num, NDSP_INTERP_LINEAR);
	ndspChnSetRate(channel_num, mix_rate);
	ndspChnSetFormat(channel_num, (channels == 1) ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16);

	mutex = Mutex::create();
	thread = Thread::create(AudioDriver3ds::thread_func, this);

	return OK;
};

void AudioDriver3ds::thread_func(void* p_udata) {
	
	int buffer_index = 0;
	ndspWaveBuf* buffer;

	AudioDriver3ds* ad = (AudioDriver3ds*)p_udata;
	
	int sample_count = ad->buffer_size * ad->channels;

	while (!ad->exit_thread) {
		
		buffer = &ad->ndsp_buffers[buffer_index % NDSP_BUFFER_COUNT];
		
		while (!ad->exit_thread && (!ad->active || buffer->status != NDSP_WBUF_DONE))
			OS::get_singleton()->delay_usec(10000);
		
		if (ad->exit_thread)
			break;

		buffer->status = NDSP_WBUF_FREE;

		if (ad->active) {
			ad->lock();
			ad->audio_server_process(ad->buffer_size, ad->samples_in);
			ad->unlock();
			
			for(int i = 0; i < sample_count; ++i)
				buffer->data_pcm16[i] = ad->samples_in[i] >> 16;
			
		} else
			memset(buffer->data_pcm16, 0, sample_count * sizeof(int16_t));
		
		DSP_FlushDataCache(buffer->data_pcm16, sample_count * sizeof(int16_t));
		ndspChnWaveBufAdd(channel_num, buffer);

		buffer_index++;
	}

	ndspChnWaveBufClear(channel_num);

	ad->thread_exited=true;
};

void AudioDriver3ds::start() {

	active = true;
};

int AudioDriver3ds::get_mix_rate() const {

	return mix_rate;
};

AudioDriverSW::OutputFormat AudioDriver3ds::get_output_format() const {

	return output_format;
};
void AudioDriver3ds::lock() {

	if (!thread || !mutex)
		return;
	mutex->lock();
};
void AudioDriver3ds::unlock() {

	if (!thread || !mutex)
		return;
	mutex->unlock();
};

void AudioDriver3ds::finish() {

	if (!thread)
		return;

	exit_thread = true;
	Thread::wait_to_finish(thread);

	if (samples_in) {
		memdelete_arr(samples_in);
	};
	
	for (int i = 0; i < NDSP_BUFFER_COUNT; ++i)
		linearFree(ndsp_buffers[i].data_pcm16);

	memdelete(thread);
	if (mutex)
		memdelete(mutex);
	thread = NULL;
	
	ndspExit();
};

AudioDriver3ds::AudioDriver3ds() {

	mutex = NULL;
	thread = NULL;
};

AudioDriver3ds::~AudioDriver3ds() {

};

#endif

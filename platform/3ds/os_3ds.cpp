/*************************************************************************/
/*  os_3ds.cpp                                                           */
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
#include "servers/visual/visual_server_raster.h"
// #include "servers/visual/rasterizer_dummy.h"
#include "drivers/3ds/citro3d/rasterizer_citro3d.h"
#include "os_3ds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "print_string.h"
#include "servers/physics/physics_server_sw.h"
#include "errno.h"

#include "drivers/unix/memory_pool_static_malloc.h"
#include "os/memory_pool_dynamic_static.h"
// #include "thread_posix.h"
// #include "semaphore_posix.h"
// #include "mutex_posix.h"
#include "drivers/3ds/thread_3ds.h"

//#include "core/io/file_access_buffered_fa.h"
#include "drivers/unix/file_access_unix.h"
#include "drivers/unix/dir_access_unix.h"
// #include "tcp_server_posix.h"
// #include "stream_peer_tcp_posix.h"
// #include "packet_peer_udp_posix.h"

#include "main/main.h"

extern "C" {
#include <3ds/svc.h>
#include <3ds/gfx.h>
#include <3ds/console.h>
#include <3ds/services/apt.h>
#include <3ds/services/hid.h>
// Big stack thanks to CANVAS_ITEM_Z_MAX among other things
u32 __stacksize__ = 1024 * 128;
}

static aptHookCookie apt_hook_cookie;

static void apt_hook_callback(APT_HookType hook, void* param)
{
	if (hook == APTHOOK_ONRESTORE || hook == APTHOOK_ONWAKEUP) {
		
	}
}


OS_3DS::OS_3DS()
: video_mode(800, 480, true, false, false)
{	
	gfxInitDefault();
	consoleInit(GFX_BOTTOM, NULL);
	
	aptHook(&apt_hook_cookie, apt_hook_callback, this);
	
// 	set_low_processor_usage_mode(true);
	_render_thread_mode=RENDER_THREAD_UNSAFE;
	AudioDriverManagerSW::add_driver(&audio_driver);
	
	use_vsync = true;
	last_id = 1;
}

OS_3DS::~OS_3DS()
{
	gfxExit();
}

void OS_3DS::run()
{
	if (!main_loop)
		return;
	
	main_loop->init();

	while (aptMainLoop())
	{
		processInput();
		
		if (hidKeysDown() & KEY_SELECT)
			break;
		
		if (Main::iteration()==true)
			break;
		
		printf("fps:%f\n", get_frames_per_second());
	}
	
	main_loop->finish();
}

static MemoryPoolStaticMalloc *mempool_static=NULL;
static MemoryPoolDynamicStatic *mempool_dynamic=NULL;

void OS_3DS::initialize_core()
{
	Thread3ds::make_default();
	Semaphore3ds::make_default();
	Mutex3ds::make_default();
	
	mempool_static = new MemoryPoolStaticMalloc;
	mempool_dynamic = memnew( MemoryPoolDynamicStatic );
	
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_FILESYSTEM);
	//FileAccessBufferedFA<FileAccessUnix>::make_default();
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_FILESYSTEM);

#ifndef NO_NETWORK
// 	TCPServerPosix::make_default();
// 	StreamPeerTCPPosix::make_default();
// 	PacketPeerUDPPosix::make_default();
// 	IP_Unix::make_default();
#endif

	ticks_start=svcGetSystemTick();
}

void OS_3DS::initialize(const VideoMode& p_desired,int p_video_driver,int p_audio_drive)
{
	main_loop=NULL;
	
	rasterizer = memnew( RasterizerCitro3d );
// 	rasterizer = memnew( RasterizerDummy );
	
	visual_server = memnew( VisualServerRaster(rasterizer) );
	
	if (get_render_thread_mode()!=RENDER_THREAD_UNSAFE) {
		visual_server =memnew(VisualServerWrapMT(visual_server,get_render_thread_mode()==RENDER_SEPARATE_THREAD));
	}
	
	AudioDriverManagerSW::get_driver(0)->set_singleton();
	if (AudioDriverManagerSW::get_driver(0)->init()!=OK)
	{
		ERR_PRINT("Initializing audio failed.");
	}

	sample_manager = memnew( SampleManagerMallocSW );
	audio_server = memnew( AudioServerSW(sample_manager) );
	audio_server->init();
	spatial_sound_server = memnew( SpatialSoundServerSW );
	spatial_sound_server->init();
	spatial_sound_2d_server = memnew( SpatialSound2DServerSW );
	spatial_sound_2d_server->init();

	ERR_FAIL_COND(!visual_server);
	
	visual_server->init();
	
	physics_server = memnew( PhysicsServerSW );
	physics_server->init();
// 	physics_2d_server = memnew( Physics2DServerSW );
	physics_2d_server = Physics2DServerWrapMT::init_server<Physics2DServerSW>();
	physics_2d_server->init();

	input = memnew( InputDefault );
}

void OS_3DS::delete_main_loop()
{
	if (main_loop)
		memdelete(main_loop);
	main_loop=NULL;
}

void OS_3DS::set_main_loop( MainLoop * p_main_loop )
{
	main_loop=p_main_loop;
	input->set_main_loop(p_main_loop);
}

void OS_3DS::finalize()
{
	if(main_loop)
		memdelete(main_loop);
	main_loop=NULL;

	spatial_sound_server->finish();
	memdelete(spatial_sound_server);
	spatial_sound_2d_server->finish();
	memdelete(spatial_sound_2d_server);

	memdelete(input);
	
	memdelete(sample_manager);

	audio_server->finish();
	memdelete(audio_server);

	visual_server->finish();
	memdelete(visual_server);
	memdelete(rasterizer);

	physics_server->finish();
	memdelete(physics_server);

	physics_2d_server->finish();
	memdelete(physics_2d_server);
}

void OS_3DS::finalize_core()
{
	if (mempool_dynamic)
		memdelete( mempool_dynamic );
	delete mempool_static;
}

void OS_3DS::vprint(const char* p_format, va_list p_list,bool p_stder)
{
	if (p_stder) {
		vfprintf(stderr,p_format,p_list);
		fflush(stderr);
	} else {
		vprintf(p_format,p_list);
		fflush(stdout);
	}
}

void OS_3DS::alert(const String& p_alert,const String& p_title)
{
	fprintf(stderr,"ERROR: %s\n",p_alert.utf8().get_data());
}

Error OS_3DS::set_cwd(const String& p_cwd)
{
	printf("set cwd: %s", p_cwd.utf8().get_data());
	if (chdir(p_cwd.utf8().get_data())!=0)
		return ERR_CANT_OPEN;

	return OK;
}

OS::Date OS_3DS::get_date(bool utc) const
{

	time_t t=time(NULL);
	struct tm *lt;
	if (utc)
		lt=gmtime(&t);
	else
		lt=localtime(&t);
	Date ret;
	ret.year=1900+lt->tm_year;
	// Index starting at 1 to match OS_Unix::get_date
	//   and Windows SYSTEMTIME and tm_mon follows the typical structure 
	//   of 0-11, noted here: http://www.cplusplus.com/reference/ctime/tm/
	ret.month=(Month)(lt->tm_mon + 1);
	ret.day=lt->tm_mday;
	ret.weekday=(Weekday)lt->tm_wday;
	ret.dst=lt->tm_isdst;
	
	return ret;
}

OS::Time OS_3DS::get_time(bool utc) const
{
	time_t t=time(NULL);
	struct tm *lt;
	if (utc)
		lt=gmtime(&t);
	else
		lt=localtime(&t);
	Time ret;
	ret.hour=lt->tm_hour;
	ret.min=lt->tm_min;
	ret.sec=lt->tm_sec;
	get_time_zone_info();
	return ret;
}

OS::TimeZoneInfo OS_3DS::get_time_zone_info() const
{
	time_t t = time(NULL);
	struct tm *lt = localtime(&t);
	char name[16];
	strftime(name, 16, "%Z", lt);
	name[15] = 0;
	TimeZoneInfo ret;
	ret.name = name;

	char bias_buf[16];
	strftime(bias_buf, 16, "%z", lt);
	int bias;
	bias_buf[15] = 0;
	sscanf(bias_buf, "%d", &bias);

	// convert from ISO 8601 (1 minute=1, 1 hour=100) to minutes
	int hour = (int)bias / 100;
	int minutes = bias % 100;
	if (bias < 0)
		ret.bias = hour * 60 - minutes;
	else
		ret.bias = hour * 60 + minutes;

	return ret;
}

void OS_3DS::delay_usec(uint32_t p_usec) const
{
// 	printf("delay_usec: %lu\n", p_usec);
	svcSleepThread(1000ULL * p_usec);
}

#define TICKS_PER_SEC 268123480ULL
#define TICKS_PER_USEC 268

uint64_t OS_3DS::get_ticks_usec() const
{
	return (svcGetSystemTick() - ticks_start) / TICKS_PER_USEC;
}

uint64_t OS_3DS::get_unix_time() const
{
	return time(NULL);
}

uint64_t OS_3DS::get_system_time_secs() const
{
	struct timeval tv_now;
	gettimeofday(&tv_now, NULL);
	//localtime(&tv_now.tv_usec);
	//localtime((const long *)&tv_now.tv_usec);
	return uint64_t(tv_now.tv_sec);
}

void OS_3DS::swap_buffers()
{
// 	gfxFlushBuffers();
	gfxSwapBuffersGpu();
	if (use_vsync)
		gspWaitForVBlank();
}

int OS_3DS::get_processor_count() const
{
	return 1;
}

static u32 buttons[16] = {
	KEY_B,
	KEY_A,
	KEY_Y,
	KEY_X,
	KEY_L,
	KEY_R,
	KEY_ZL,    // L2
	KEY_ZR,    // R2
	KEY_TOUCH, // L3 substitute
	KEY_TOUCH, // R3 substitute
	KEY_SELECT,
	KEY_START,
	KEY_DUP,
	KEY_DDOWN,
	KEY_DLEFT,
	KEY_DRIGHT,
};

void OS_3DS::processInput()
{
	hidScanInput();
	u32 kDown = hidKeysDown();
	u32 kUp = hidKeysUp();
	last_id++;
	
	for (int i = 0; i < 16; ++i)
	{
		if (buttons[i] & kDown)
			last_id = input->joy_button(last_id, 0, i, true);
		else if (buttons[i] & kUp)
			last_id = input->joy_button(last_id, 0, i, false);
	}
}

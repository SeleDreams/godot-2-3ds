/*************************************************************************/
/*  os_x11.h                                                             */
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
#ifndef OS_3DS_H
#define OS_3DS_H


#include "os/os.h"
#include "os/input.h"
#include "servers/visual_server.h"
#include "servers/visual/visual_server_wrap_mt.h"
#include "servers/visual/rasterizer.h"
#include "servers/physics_server.h"
#include "servers/audio/audio_server_sw.h"
#include "servers/audio/audio_driver_dummy.h"
#include "servers/audio/sample_manager_sw.h"
#include "servers/spatial_sound/spatial_sound_server_sw.h"
#include "servers/spatial_sound_2d/spatial_sound_2d_server_sw.h"
#include "servers/physics_2d/physics_2d_server_sw.h"
#include "servers/physics_2d/physics_2d_server_wrap_mt.h"
#include "main/input_default.h"
#include "drivers/3ds/audio_driver_3ds.h"

/**
	@author Thomas Edvalson <machin3@gmail.com>
*/

class OS_3DS : public OS {
	
	MainLoop *main_loop;
	Rasterizer *rasterizer;
	VisualServer *visual_server;
	InputDefault *input;
	
	PhysicsServer *physics_server;
	Physics2DServer *physics_2d_server;
	
	AudioServerSW *audio_server;
	SampleManagerMallocSW *sample_manager;
	SpatialSoundServerSW *spatial_sound_server;
	SpatialSound2DServerSW *spatial_sound_2d_server;
	
	AudioDriver3ds audio_driver;
	
	Point2i last_mouse_pos;
	VideoMode video_mode;
	
	bool use_vsync;
	
	uint64_t ticks_start;
	int last_id;

protected:
friend class Main;

	// functions used by main to initialize/deintialize the OS
	virtual int get_video_driver_count() const { return 1; }
	virtual const char * get_video_driver_name(int p_driver) const { return "citro3d"; }

	virtual VideoMode get_default_video_mode() const { return video_mode; }

	virtual int get_audio_driver_count() const { return 1; }
	virtual const char * get_audio_driver_name(int p_driver) const { return "ndsp"; }

	virtual void initialize_core();
	virtual void initialize(const VideoMode& p_desired,int p_video_driver,int p_audio_driver);

	virtual void set_main_loop( MainLoop * p_main_loop );
	virtual void delete_main_loop();

	virtual void finalize();
	virtual void finalize_core();

// 	virtual void set_cmdline(const char* p_execpath, const List<String>& p_args);

public:

// 	virtual void print_error(const char* p_function,const char* p_file,int p_line,const char *p_code,const char*p_rationale,ErrorType p_type=ERR_ERROR);

// 	virtual void print(const char *p_format, ... );
// 	virtual void printerr(const char *p_format, ... );
	virtual void vprint(const char* p_format, va_list p_list, bool p_stderr=false);
	virtual void alert(const String& p_alert,const String& p_title="ALERT!");
	virtual String get_stdin_string(bool p_block = true) { return ""; }

// 	virtual void set_last_error(const char* p_error);
// 	virtual const char *get_last_error() const;
// 	virtual void clear_last_error();


// 	virtual void set_mouse_mode(MouseMode p_mode);
// 	virtual MouseMode get_mouse_mode() const;


// 	virtual void warp_mouse_pos(const Point2& p_to)  {}
	virtual Point2 get_mouse_pos() const { return last_mouse_pos; }
	virtual int get_mouse_button_state() const { return 0; }
	virtual void set_window_title(const String& p_title) {};

	virtual void set_video_mode(const VideoMode& p_video_mode,int p_screen=0) {}
	virtual VideoMode get_video_mode(int p_screen=0) const { return video_mode; }
	virtual void get_fullscreen_mode_list(List<VideoMode> *p_list,int p_screen=0) const {}


// 	virtual int get_screen_count() const{ return 1; }
// 	virtual int get_current_screen() const { return 0; }
// 	virtual void set_current_screen(int p_screen) { }
// 	virtual Point2 get_screen_position(int p_screen=0) const { return Point2(); }
// 	virtual Size2 get_screen_size(int p_screen=0) const { return get_window_size(); }
// 	virtual int get_screen_dpi(int p_screen=0) const { return 72; }
	virtual Size2 get_window_size() const { return Size2(400, 240); }


// 	virtual void set_iterations_per_second(int p_ips);
// 	virtual int get_iterations_per_second() const;

// 	virtual void set_target_fps(int p_fps);
// 	virtual float get_target_fps() const;

// 	virtual float get_frames_per_second() const { return _fps; }

// 	virtual void set_keep_screen_on(bool p_enabled);
// 	virtual bool is_keep_screen_on() const;
// 	virtual void set_low_processor_usage_mode(bool p_enabled);
// 	virtual bool is_in_low_processor_usage_mode() const;

// 	virtual String get_installed_templates_path() const { return ""; }
	virtual String get_executable_path() const {return "test"; }
	virtual Error execute(const String& p_path, const List<String>& p_arguments,bool p_blocking,ProcessID *r_child_id=NULL,String* r_pipe=NULL,int *r_exitcode=NULL) { return FAILED; }
	virtual Error kill(const ProcessID& p_pid) { return FAILED; }
// 	virtual int get_process_ID() const;

// 	virtual Error shell_open(String p_uri);
	virtual Error set_cwd(const String& p_cwd);

	virtual bool has_environment(const String& p_var) const { return false; }
	virtual String get_environment(const String& p_var) const { return ""; }

	virtual String get_name() { return "3DS"; }
// 	virtual List<String> get_cmdline_args() const { return _cmdline; }
// 	virtual String get_model_name() const;

	virtual MainLoop *get_main_loop() const { return main_loop; }

// 	String get_custom_level() const { return _custom_level; }

// 	virtual void yield();


	virtual Date get_date(bool local=false) const;
	virtual Time get_time(bool local=false) const;
	virtual TimeZoneInfo get_time_zone_info() const;
	virtual uint64_t get_unix_time() const;
	virtual uint64_t get_system_time_secs() const;

	virtual void delay_usec(uint32_t p_usec) const;
	virtual uint64_t get_ticks_usec() const;
	uint32_t get_ticks_msec() const;
	uint64_t get_splash_tick_msec() const;

	void set_frame_delay(uint32_t p_msec);
	uint32_t get_frame_delay() const;

	virtual bool can_draw() const { return true; }

// 	uint64_t get_frames_drawn();

// 	uint64_t get_fixed_frames() const { return _fixed_frames; }
// 	uint64_t get_idle_frames() const { return _idle_frames; }
// 	bool is_in_fixed_frame() const { return _in_fixed; }

	bool is_stdout_verbose() const;


// 	virtual bool has_virtual_keyboard() const;
// 	virtual void show_virtual_keyboard(const String& p_existing_text,const Rect2& p_screen_rect=Rect2());
// 	virtual void hide_virtual_keyboard();

	virtual void set_cursor_shape(CursorShape p_shape) {}

	virtual bool get_swap_ok_cancel() { return false; }
// 	virtual void dump_memory_to_file(const char* p_file);
// 	virtual void dump_resources_to_file(const char* p_file);
// 	virtual void print_resources_in_use(bool p_short=false);
// 	virtual void print_all_resources(String p_to_file="");

// 	virtual int get_static_memory_usage() const;
// 	virtual int get_static_memory_peak_usage() const;
// 	virtual int get_dynamic_memory_usage() const;
// 	virtual int get_free_static_memory() const;

// 	RenderThreadMode get_render_thread_mode() const { return RENDER_THREAD_UNSAFE; }

// 	virtual String get_locale() const;

// 	String get_safe_application_name() const;
// 	virtual String get_data_dir() const { return "."; }
// 	virtual String get_resource_dir() const { printf("res dir: %s\n", Globals::get_singleton()->get_resource_path().utf8().get_data()); return Globals::get_singleton()->get_resource_path(); }


// 	virtual String get_system_dir(SystemDir p_dir) const;


// 	virtual void set_no_window_mode(bool p_enable);
// 	virtual bool is_no_window_mode_enabled() const;

// 	virtual bool has_touchscreen_ui_hint() const;


// 	virtual void set_screen_orientation(ScreenOrientation p_orientation);
// 	ScreenOrientation get_screen_orientation() const;

// 	virtual void move_window_to_foreground() {}

// 	virtual void debug_break();

// 	virtual void release_rendering_thread();
// 	virtual void make_rendering_thread();
	virtual void swap_buffers();

// 	virtual int get_exit_code() const;
// 	virtual void set_exit_code(int p_code);

	virtual int get_processor_count() const;

// 	virtual String get_unique_ID() const;

// 	virtual Error native_video_play(String p_path, float p_volume, String p_audio_track, String p_subtitle_track);
// 	virtual bool native_video_is_playing() const;
// 	virtual void native_video_pause();
// 	virtual void native_video_unpause();
// 	virtual void native_video_stop();

	virtual bool can_use_threads() const { return true; }

// 	virtual Error dialog_show(String p_title, String p_description, Vector<String> p_buttons, Object* p_obj, String p_callback);
// 	virtual Error dialog_input_text(String p_title, String p_description, String p_partial, Object* p_obj, String p_callback);


// 	virtual LatinKeyboardVariant get_latin_keyboard_variant() const;

// 	void set_time_scale(float p_scale);
// 	float get_time_scale() const;


// 	virtual bool is_joy_known(int p_device) { return p_device == 1; )
// 	virtual String get_joy_guid(int p_device)const;

// 	virtual void set_context(int p_context);

	virtual void set_use_vsync(bool p_enable) { use_vsync = p_enable; }
	virtual bool is_vsnc_enabled() const { return use_vsync; }

// 	Dictionary get_engine_version() const;

	void run();
	void processInput();

	OS_3DS();
	virtual ~OS_3DS();
};

#endif

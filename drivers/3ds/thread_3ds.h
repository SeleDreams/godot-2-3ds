/*************************************************************************/
/*  thread_3ds.h                                                         */
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
#ifndef THREAD_3DS_H
#define THREAD_3DS_H

#include "core/os/thread.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "thread_ctr_wrapper.h"

extern "C" {
#include <3ds/types.h>
#include <3ds/svc.h>
#include <3ds/synchronization.h>
}


class Thread3ds : public Thread {

	static Thread* create_func_3ds(ThreadCreateCallback p_callback,void * p_user,const Settings& p_settings=Settings());
	static void wait_to_finish_func_3ds(Thread* p_thread);
	
	ThreadCtrWrapper* thread;
	ID id;

public:
	Thread3ds(ThreadCtrWrapper* p_thread);
	~Thread3ds();
	
	virtual ID get_ID() const;

	static void make_default();
};


class Mutex3ds : public Mutex {

	static Mutex* create(bool p_recursive);
	
	bool is_recursive;
	LightLock lightLock;
	RecursiveLock recursiveLock;

public:

	virtual void lock();
	virtual void unlock();
	virtual Error try_lock();

	static void make_default();
	
	Mutex3ds(bool p_recursive);
	~Mutex3ds();
};


class Semaphore3ds : public Semaphore {

	static Semaphore* create();

public:
	virtual Error wait() { return OK; };
	virtual Error post() { return OK; };
	virtual int get() const { return 0; }; ///< get semaphore value

	static void make_default();

};

#endif

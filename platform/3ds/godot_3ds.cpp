/*************************************************************************/
/*  godot_3ds.cpp                                                        */
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
#include "main/main.h"
#include "os_3ds.h"
#include <stdio.h>


int main(int argc, char* argv[])
{
	OS_3DS os;

	char* args[] = {"-path", "sat_test"};
// 	char* args[] = {"-path", "motion", "-test", "render"};
// 	char* args[] = {"motion"};

// 	Error err  = Main::setup(argv[0],argc-1,&argv[1]);
	Error err  = Main::setup("3ds", 2, args);
// 	Error err  = Main::setup("3ds", 0, NULL);
	if (err==OK)
	{
		printf("Running...\n");

		if (Main::start())
			os.run(); // it is actually the OS that decides how to run
		Main::cleanup();
	}

	// OS_3DS destructor to exit ctrulib stuff
	return 0;
}

/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009,2010  Alessandro Pignotti (a.pignotti@sssup.it)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#ifndef SWF_H
#define SWF_H

#include "compat.h"
#include <iostream>
#include <fstream>
#include <list>
#include <map>
#include <semaphore.h>
#include "swftypes.h"
#include "frame.h"
#include "vm.h"
#include "flashdisplay.h"
#include "timer.h"

#include <GL/glew.h>
#ifndef WIN32
#include <X11/Xlib.h>
#include <GL/glx.h>
#else
//#include <windows.h>
#endif

class zlib_file_filter;
namespace lightspark
{

class DownloadManager;
class DisplayListTag;
class DictionaryTag;
class PlaceObject2Tag;
class EventDispatcher;
class ABCVm;
class InputThread;
class RenderThread;

typedef void* (*thread_worker)(void*);
long timeDiff(timespec& s, timespec& d);

class SWF_HEADER
{
public:
	UI8 Signature[3];
	UI8 Version;
	UI32 FileLength;
	RECT FrameSize;
	UI16 FrameRate;
	UI16 FrameCount;
public:
	SWF_HEADER(std::istream& in);
	const RECT& getFrameSize(){ return FrameSize; }
};

class ExecutionContext;

struct fps_profiling
{
	uint64_t render_time;
	uint64_t action_time;
	uint64_t cache_time;
	uint64_t fps;
	uint64_t event_count;
	uint64_t event_time;
	fps_profiling():render_time(0),action_time(0),cache_time(0),fps(0),event_count(0),event_time(0){}
};

//RootMovieClip is used as a ThreadJob for timed rendering purpose
class RootMovieClip: public MovieClip, public IThreadJob
{
friend class ParseThread;
protected:
	sem_t mutex;
	//Semaphore to wait for new frames to be available
	sem_t new_frame;
	bool initialized;
private:
	RGB Background;
	std::list < DictionaryTag* > dictionary;
	//frameSize and frameRate are valid only after the header has been parsed
	RECT frameSize;
	float frameRate;
	mutable sem_t sem_valid_size;
	mutable sem_t sem_valid_rate;
	//Frames mutex (shared with drawing thread)
	sem_t sem_frames;
	bool toBind;
	tiny_string bindName;
	void execute();

public:
	RootMovieClip(LoaderInfo* li);
	~RootMovieClip();
	unsigned int version;
	unsigned int fileLenght;
	RGB getBackground();
	void setBackground(const RGB& bg);
	void setFrameSize(const RECT& f);
	RECT getFrameSize() const;
	float getFrameRate() const;
	void setFrameRate(float f);
	void setFrameCount(int f);
	void addToDictionary(DictionaryTag* r);
	DictionaryTag* dictionaryLookup(int id);
	void addToFrame(DisplayListTag* t);
	void addToFrame(ControlTag* t);
	void commitFrame(bool another);
	void revertFrame();
	void Render();
	bool getBounds(number_t& xmin, number_t& xmax, number_t& ymin, number_t& ymax) const;
	void bindToName(const tiny_string& n);
	void initialize();
/*	ASObject* getVariableByQName(const tiny_string& name, const tiny_string& ns);
	void setVariableByQName(const tiny_string& name, const tiny_string& ns, ASObject* o);
	void setVariableByMultiname(multiname& name, ASObject* o);
	void setVariableByString(const std::string& s, ASObject* o);*/
};


class SystemState: public RootMovieClip
{
private:
	ThreadPool* threadPool;
	TimerThread* timerThread;
	sem_t terminated;
#ifndef WIN32
	timespec ts,td;
#endif
	int frameCount;
	int secsCount;
public:
	void setUrl(const tiny_string& url);

	bool shutdown;
	bool error;
	void setShutdownFlag();
	void execute();
	void draw();
	void wait();

	//Be careful, SystemState constructor does some global initialization that must be done
	//before any other thread gets started
	SystemState();
	~SystemState();
	fps_profiling* fps_prof;
	Stage* stage;
	ABCVm* currentVm;
	InputThread* cur_input_thread;
	RenderThread* cur_render_thread;
	//Application starting time in milliseconds
	uint64_t startTime;

	//Class map
	std::map<tiny_string, Class_base*> classes;

	//DEBUG
	std::vector<tiny_string> events_name;
	void dumpEvents()
	{
		for(unsigned int i=0;i<events_name.size();i++)
			std::cout << events_name[i] << std::endl;
	}

	//Flags for command line options
	bool useInterpreter;
	bool useJit;

	void parseParameters(std::istream& i);
	void setParameters(ASObject* p);
	void addJob(IThreadJob* j);
	void addTick(uint32_t tickTime, IThreadJob* job);
	void addWait(uint32_t waitTime, IThreadJob* job);

	DownloadManager* downloadManager;
};

class ParseThread: public IThreadJob
{
private:
	std::istream& f;
	sem_t ended;
	void execute();
public:
	RootMovieClip* root;
	int version;
	ParseThread(RootMovieClip* r,std::istream& in);
	void wait();

	//DEPRECATED
	Sprite* parsingTarget;
};

enum ENGINE { SDL=0, NPAPI, GLX};
#ifndef WIN32
struct NPAPI_params
{
	Display* display;
	VisualID visual;
	Window window;
	int width;
	int height;
};
#else
struct NPAPI_params
{
};
#endif


class InputThread
{
private:
	SystemState* m_sys;
	NPAPI_params* npapi_params;
	pthread_t t;
	bool terminated;
	static void* sdl_worker(InputThread*);
	static void* npapi_worker(InputThread*);

	std::multimap< tiny_string, EventDispatcher* > listeners;
	sem_t sem_listeners;

public:
	InputThread(SystemState* s,ENGINE e, void* param=NULL);
	~InputThread();
	void wait();
	void addListener(const tiny_string& type, EventDispatcher* tag);
	void broadcastEvent(const tiny_string& type);
};

class RenderThread
{
private:
	SystemState* m_sys;
	NPAPI_params* npapi_params;
	pthread_t t;
	bool terminated;
	static void* sdl_worker(RenderThread*);
	static void* npapi_worker(RenderThread*);
	static void* glx_worker(RenderThread*);
	void commonGLInit(int width, int height, unsigned int t2[3]);
	sem_t render;

#ifndef WIN32
	Display* mDisplay;
	GLXFBConfig mFBConfig;
	GLXContext mContext;
	GLXPbuffer mPbuffer;
	Window mWindow;
	GC mGC;
#endif
	static int load_program();
	float* interactive_buffer;
	bool fbAcquired;
public:
	RenderThread(SystemState* s,ENGINE e, void* param=NULL);
	~RenderThread();
	void draw();
	void wait();
	float getIdAt(int x, int y);
	//The calling context MUST preserve current matrix with a wraping pushMatrix, popMatrix combo
	void glAcquireFramebuffer();
	void glBlitFramebuffer();

	//OpenGL fragment programs
	int gpu_program;
	GLuint fboId;
	GLuint spare_tex;
	GLuint data_tex;
	int width;
	int height;
};

};
#endif

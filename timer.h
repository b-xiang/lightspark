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

#ifndef _TIMER_H
#define _TIMER_H

#include <list>
#include <pthread.h>
#include <time.h>
#include <semaphore.h>
#include <inttypes.h>
#include "thread_pool.h"

namespace lightspark
{

typedef void* (*thread_worker)(void*);
class TimerThread
{
private:
	class TimingEvent
	{
	public:
		bool isTick;
		IThreadJob* job;
		//Timing are in milliseconds
		uint64_t timing;
		uint32_t tickTime;
	};
	sem_t mutex;
	sem_t newEvent;
	pthread_t t;
	std::list<TimingEvent*> pendingEvents;
	SystemState* m_sys;
	bool waiting;
	static void* timer_worker(TimerThread*);
	static uint64_t timespecToMsecs(timespec t);
	static timespec msecsToTimespec(uint64_t time);
	void insertNewEvent(TimingEvent* e);
public:
	TimerThread(SystemState* s);
	void addTick(uint32_t tickTime, IThreadJob* job);
	void addWait(uint32_t waitTime, IThreadJob* job);
};

};
#endif

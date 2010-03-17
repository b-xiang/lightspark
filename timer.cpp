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

#include <errno.h>
#include <stdlib.h>

#include "timer.h"
#include "compat.h"

using namespace lightspark;
using namespace std;

extern TLSDATA SystemState* sys;

uint64_t TimerThread::timespecToMsecs(timespec t)
{
	uint64_t ret=0;
	ret+=(t.tv_sec*1000);
	ret+=(t.tv_nsec/1000000);
	return ret;
}

timespec TimerThread::msecsToTimespec(uint64_t time)
{
	timespec ret;
	ret.tv_sec=time/1000;
	ret.tv_nsec=(time%1000)*1000000;
	return ret;
}

TimerThread::TimerThread(SystemState* s):m_sys(s)
{
	sem_init(&mutex,0,1);
	sem_init(&newEvent,0,0);

	pthread_create(&t,NULL,(thread_worker)timer_worker,this);
}

void TimerThread::insertNewEvent(TimingEvent* e)
{
	sem_wait(&mutex);
	list<TimingEvent*>::iterator it=pendingEvents.begin();
	//If there are no events pending, or this is earlier than the first, signal newEvent
	if(pendingEvents.empty() || (*it)->timing > e->timing)
	{
		pendingEvents.insert(it, e);
		sem_post(&newEvent);
		sem_post(&mutex);
		return;
	}
	++it;

	for(;it!=pendingEvents.end();++it)
	{
		if((*it)->timing > e->timing)
		{
			pendingEvents.insert(it, e);
			sem_post(&mutex);
			return;
		}
	}
	//Event has to be inserted after all the others
	pendingEvents.insert(pendingEvents.end(), e);
	sem_post(&mutex);
}

void* TimerThread::timer_worker(TimerThread* th)
{
	sys=th->m_sys;
	while(1)
	{
		//Wait until a time expires
		sem_wait(&th->mutex);
		//Check if there is any event
		if(th->pendingEvents.empty())
		{
			sem_post(&th->mutex);
			sem_wait(&th->newEvent);
			sem_wait(&th->mutex);
		}

		//Get expiration of first event
		uint32_t timing=th->pendingEvents.front()->timing;
		//Wait for the previous absolute time, or a newEvent signal
		timespec tmpt=msecsToTimespec(timing);
		sem_post(&th->mutex);
		int ret=sem_timedwait(&th->newEvent, &tmpt);
		if(ret==0)
			continue;

		//The first event is expired
		if(errno!=ETIMEDOUT)
			abort();

		//Note: it may happen that between the sem_timewait and this code another event gets inserted in the front. In this 
		//case it's not an error to execute it now, as it's expiration time is the first anyway and before the one expired
		sem_wait(&th->mutex);
		TimingEvent* e=th->pendingEvents.front();
		th->pendingEvents.pop_front();
		sem_post(&th->mutex);

		bool destroyEvent=true;
		if(e->isTick) //Let's schedule the event again
		{
			e->timing+=e->tickTime;
			th->insertNewEvent(e); //newEvent may be signaled, and will be waietd by the timedwait
			destroyEvent=false;
		}

		//Now execute the job
		e->job->execute();

		//Cleanup
		if(destroyEvent)
			delete e;
	}
}

void TimerThread::addTick(uint32_t tickTime, IThreadJob* job)
{
	TimingEvent* e=new TimingEvent;
	e->isTick=true;
	e->job=job;
	e->tickTime=tickTime;
	timespec tp;
	//Get current clock to schedule next wakeup
	clock_gettime(CLOCK_REALTIME,&tp);
	e->timing=timespecToMsecs(tp)+tickTime;
	insertNewEvent(e);
}

void TimerThread::addWait(uint32_t waitTime, IThreadJob* job)
{
	TimingEvent* e=new TimingEvent;
	e->isTick=false;
	e->job=job;
	e->tickTime=0;
	timespec tp;
	//Get current clock to schedule next wakeup
	clock_gettime(CLOCK_REALTIME,&tp);
	e->timing=timespecToMsecs(tp)+waitTime;
	insertNewEvent(e);
}

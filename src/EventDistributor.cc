// $Id$

#include "EventDistributor.hh"
#include "openmsx.hh"
#include <iostream>

#include "config.h"

EventDistributor::EventDistributor()
{
}

EventDistributor::~EventDistributor()
{
}

EventDistributor *EventDistributor::instance()
{
	if (oneInstance == NULL) {
		oneInstance = new EventDistributor();
	}
	return oneInstance;
}
EventDistributor *EventDistributor::oneInstance = NULL;


/**
 * This is the main-loop. It waits for events and 
 *   - delivers them to asynchronous listeners
 *   - queues them for later synchronous delivery
 * Note: this method runs in a different thread!!
 */
void EventDistributor::run()
{
	SDL_Event event;
	while (SDL_WaitEvent(&event)) {
		PRT_DEBUG("SDL event received");
		
		std::multimap<int, EventListener*>::iterator it;

		asyncMutex.grab();
		for (it = asyncMap.lower_bound(event.type);
		     (it != asyncMap.end()) && (it->first == event.type);
		     it++) {
			it->second->signalEvent(event);
		}
		asyncMutex.release();

		syncMutex.grab();
		queueMutex.grab();
		for (it = syncMap.lower_bound(event.type);
		     (it != syncMap.end()) && (it->first == event.type);
		     it++) {
			queue.push(std::pair<SDL_Event, EventListener*>(event, it->second));
		}
		queueMutex.release();
		syncMutex.release();
	}
	PRT_ERROR("Error while waiting for event");
}

void EventDistributor::registerAsyncListener(int type, EventListener *listener)
{
	asyncMutex.grab();
	asyncMap.insert(std::pair<int, EventListener*>(type, listener));
	asyncMutex.release();
}

void EventDistributor::registerSyncListener (int type, EventListener *listener)
{
	syncMutex.grab();
	syncMap.insert(std::pair<int, EventListener*>(type, listener));
	syncMutex.release();
}

void EventDistributor::pollSyncEvents()
{
	queueMutex.grab();
	while (!queue.empty()) {
		std::pair<SDL_Event, EventListener*> pair = queue.front();
		queue.pop();
		queueMutex.release();	// temporary release queue mutex
		pair.second->signalEvent(pair.first);
		queueMutex.grab();	// retake queue mutex
	}
	queueMutex.release();
}


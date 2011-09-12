/**
 * file: FixEvent.cpp
 * Author: Maoliang <kceiwH@gmail.com>
 * Created Time: Mon Oct 19 14:01:17 2009
 * Description: 
 */

#include "FixEvent.h"

FixEvent::FixEvent() : 
    events(NULL),
    triggering_event(NULL)
{
}

FixEvent::~FixEvent()
{
    delete this->triggering_event;
    this->triggering_event = NULL;

    delete this->events;
    this->events = NULL;
}

FixEvent::FixEvent(const FixEvent &other) : events(NULL),
        triggering_event(NULL)
{
    if (other.triggering_event!=NULL) {
        Event *new_triggering_event = new Event(*other.triggering_event);
        this->triggering_event = new_triggering_event;
    }
    if (other.events!=NULL) {
        EventNode *new_events = new EventNode(*other.events);
        this->events = new_events;
    }
}

FixEvent & FixEvent::operator = (const FixEvent &other)
{
    FixEvent fe(other);
    Event *t = this->triggering_event;
    this->triggering_event = fe.triggering_event;
    fe.triggering_event = t;

    EventNode *te = this->events;
    this->events = fe.events;
    fe.events = te;
    return *this;
}

void FixEvent::set_triggering_event(const Event &event)
{
    delete this->triggering_event;
    this->triggering_event = new Event(event);
}

void FixEvent::set_events(const EventNode &events)
{
    delete this->events;
    this->events = new EventNode(events);
}

const Event * FixEvent::get_trigger_event() const
{
	return this->triggering_event;
}

const EventNode * FixEvent::get_events() const
{
	return this->events;
}


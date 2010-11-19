/**
 * file: FixEvent.h
 * Author: Maoliang <kceiwH@gmail.com>
 * Created Time: Mon Oct 19 13:59:17 2009
 * Description: 
 */

#ifndef _FIXEVENT_H__
#define _FIXEVENT_H__

#include "EventNode.h"
#include "Event.h"

class FixEvent
{
public:
	FixEvent();
	FixEvent(const FixEvent &other);
	FixEvent & operator = (const FixEvent &other);
	~FixEvent();

	void set_triggering_event(const Event &event);
	void set_events(const EventNode &events);
	const Event * get_trigger_event() const;
	const EventNode * get_events() const;

private:

private:
	Event *triggering_event;
	EventNode *events;
};

#endif /* _FIXEVENT_H__ */


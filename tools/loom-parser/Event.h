/**
 * file: Event.h
 * Author: Maoliang <kceiwH@gmail.com>
 * Created Time: Mon Oct 19 13:56:54 2009
 * Description: 
 */

#ifndef _EVENT_H__
#define _EVENT_H__

#include "EventDesc.h"

enum EventType { event_type_groups, event_type_item };

class Event
{
public:
        Event() : type(event_type_item), events() {}
	Event(EventType type) { this->type = type; }
	Event(const Event &e) { this->events = e.events; this->type = e.type; }
	Event & operator = (const Event &);
	const EventType get_type() const { return this->type; }
	void add_description(const EventDescContainer &edc);
	void add_description(const EventDesc &ed);
	const EventDescContainer::DescContainer & get_descs() const;
        EventDescContainer::DescContainerSizeType get_amount_descs() const;

private:
	EventDescContainer events;
	EventType type;
};

#endif /* _EVENT_H__ */


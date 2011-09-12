/**
 * file: EventNode.h
 * Author: Maoliang <kceiwH@gmail.com>
 * Created Time: Mon Oct 19 13:58:05 2009
 * Description: 
 */

#ifndef _EVENTNODE_H__
#define _EVENTNODE_H__

#include "Event.h"

enum EventOperation
{
	event_op_noop = -1,
	event_op_order,
	event_op_critical,
	event_op_barrier,
        event_op_atomic,
};

class EventNode
{
public:
	EventNode(EventOperation event_op);
	EventNode(EventOperation event_op, const EventNode &first_event, const
		EventNode &second_event);
	EventNode(EventOperation event_op, const EventNode &first_event, const
		EventNode &second_event, const Event &event);
	EventNode(const EventNode &other);
	EventNode & operator = (const EventNode &other);
	~EventNode();
	void add_first(const EventNode &first);
	void add_last(const EventNode &last);
	void set_event(const Event &event);
        EventOperation get_event_op() const;
        const EventNode * get_first() const;
        const EventNode * get_last() const;
        const Event * get_event() const;

private:
	void clone(const EventNode &other);
private:
	EventOperation event_op;
	EventNode *first_event;
	EventNode *last_event;
	Event *event;
};

#endif /* _EVENTNODE_H__ */

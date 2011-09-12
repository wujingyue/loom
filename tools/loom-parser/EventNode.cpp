/**
 * file: EventNode.cpp
 * Author: Maoliang <kceiwH@gmail.com>
 * Created Time: Mon Oct 19 14:02:10 2009
 * Description: 
 */

#include "EventNode.h"

EventNode::EventNode(EventOperation event_op) 
            : event_op(event_op),first_event(NULL),last_event(NULL),event(NULL)
{
}

EventNode::EventNode(EventOperation event_op, const EventNode &first_event, const
            EventNode &second_event) 
            : event_op(event_op), first_event(NULL), last_event(NULL),event(NULL)
{
    add_first(first_event);
    add_last(second_event);
}

EventNode::EventNode(EventOperation event_op, const EventNode &first_event, const
            EventNode &second_event, const Event &event) 
            : event_op(event_op), first_event(NULL), last_event(NULL),event(NULL)
{
    add_first(first_event);
    add_last(second_event);
    set_event(event);
}

EventNode::EventNode(const EventNode &other)
    : event_op(other.event_op), first_event(NULL), last_event(NULL),
        event(NULL)
{
    clone(other);
}

EventNode & EventNode::operator = (const EventNode &other)
{
    clone(other);
    return *this;
}

EventNode::~EventNode()
{
    delete first_event;
    delete last_event;
    delete event;
}

void EventNode::add_first(const EventNode &first)
{
    first_event = new EventNode(first);
}

void EventNode::add_last(const EventNode &second)
{
    last_event = new EventNode(second);
}

void EventNode::set_event(const Event &event)
{
    this->event = new Event(event);
}

EventOperation EventNode::get_event_op() const
{
    return this->event_op;
}

const EventNode * EventNode::get_first() const
{
    return this->first_event;
}

const EventNode * EventNode::get_last() const
{
    return this->last_event;
}

const Event * EventNode::get_event() const 
{
    return this->event;
}

void EventNode::clone(const EventNode &other)
{
    EventNode *new_first = NULL;
    if (other.first_event!=NULL) {
        new_first = new EventNode(other.first_event->event_op);
        if (other.first_event->first_event!=NULL) {
            new_first->add_first(*other.first_event->first_event);
        }

        if (other.first_event->last_event!=NULL) {
            new_first->add_last(*other.first_event->last_event);
        }

        if (other.first_event->event!=NULL) {
            new_first->set_event(*other.first_event->event);
        }
    }

    EventNode *new_second = NULL;
    if (other.last_event!=NULL) {
        new_second = new EventNode(other.last_event->event_op);
        if (other.last_event->first_event!=NULL) {
            new_second->add_first(*other.last_event->first_event);
        }

        if (other.last_event->last_event!=NULL) {
            new_second->add_last(*other.last_event->last_event);
        }

        if (other.last_event->event!=NULL) {
            new_second->set_event(*other.last_event->event);
        }
    }

    Event *new_event = NULL;
    if (other.event!=NULL) {
        new_event = new Event(*other.event);
    }

    delete first_event;
    delete last_event;
    delete event;

    this->event_op = other.event_op;
    this->first_event = new_first;
    this->last_event = new_second;
    this->event = new_event;
}


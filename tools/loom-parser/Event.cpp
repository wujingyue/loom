/**
 * file: Event.cpp
 * Author: Maoliang <kceiwH@gmail.com>
 * Created Time: Mon Oct 19 14:02:48 2009
 * Description: 
 */

#include "Event.h"

Event & Event::operator = (const Event &other)
{
    this->events = other.events;
    this->type = other.type;
}

void Event::add_description(const EventDescContainer &event)
{
    this->events.add_desc(event);
}

void Event::add_description(const EventDesc &ed)
{
    this->events.add_desc(ed);
}

const EventDescContainer::DescContainer & Event::get_descs() const
{
    return this->events.get_descs();
}

EventDescContainer::DescContainerSizeType Event::get_amount_descs() const
{
    return this->events.get_descs().size();
}



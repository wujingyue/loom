/**
 * file: EventDesc.cpp
 * Author: Maoliang <kceiwH@gmail.com>
 * Created Time: Mon Oct 19 14:03:30 2009
 * Description: 
 */

#include <string>

#include "EventDesc.h"

EventDesc::EventDesc() : op(NULL), object(NULL), location(NULL), context(NULL)
{
}

EventDesc::EventDesc(const EventDesc &other) 
    : op(NULL), object(NULL), location(NULL), context(NULL)
{
    if (other.op!=NULL) {
        this->op = new std::string(*other.op);
    }
    if (other.object!=NULL) {
        this->object = new std::string(*other.object);
    }
    if (other.location!=NULL) {
        this->location = new std::string(*other.location);
    }
    if (other.context!=NULL) {
        this->context = new std::string(*other.context);
    }
}

EventDesc& EventDesc::operator = (const EventDesc &other)
{
    EventDesc tmp(other);
    std::string *strtmp;
    
    strtmp = this->op;
    this->op = tmp.op;
    tmp.op = strtmp;
    
    strtmp = this->object;
    this->object = tmp.object;
    tmp.object = strtmp;
    
    strtmp = this->location;
    this->location = tmp.location;
    tmp.location = strtmp;
    
    strtmp = this->context;
    this->context = tmp.context;
    tmp.context = strtmp;
}


EventDesc::~EventDesc()
{
    cleanup();
}

void EventDesc::cleanup()
{
    delete op;
    delete object;
    delete location;
    delete context;

    op = NULL;
    object = NULL;
    location = NULL;
    context = NULL;
}

void EventDesc::set_op(const std::string &op)
{
    delete this->op;
    this->op = new std::string(op);
}

void EventDesc::set_object(const std::string &object)
{
    delete this->object; 
    this->object = new std::string(object);
}

void EventDesc::set_location(const std::string &location)
{
    delete this->location;
    this->location = new std::string(location);
}

void EventDesc::set_context(const std::string &context)
{
    delete this->context;
    this->context = new std::string(context);
}

const std::string * EventDesc::get_op() const
{
    return this->op;
}

const std::string * EventDesc::get_object() const
{
    return this->object;
}

const std::string * EventDesc::get_location() const
{
    return this->location;
}

const std::string * EventDesc::get_context() const
{
    return this->context;
}

EventDescContainer::EventDescContainer() : descs()
{
}

void EventDescContainer::add_desc(const EventDescContainer &descs)
{
    this->descs.insert(this->descs.end(), descs.descs.begin(),
                    descs.descs.end());
}

void EventDescContainer::add_desc(const EventDesc &desc)
{
    this->descs.push_back(desc);
}

EventDescContainer::DescContainerSizeType EventDescContainer::get_size() const
{
    return this->descs.size();
}

const EventDescContainer::DescContainer & EventDescContainer::get_descs() const
{
    return this->descs;
}


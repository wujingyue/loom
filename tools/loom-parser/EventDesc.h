/**
 * file: EventDesc.h
 * Author: Maoliang <kceiwH@gmail.com>
 * Created Time: Mon Oct 19 13:51:15 2009
 * Description: 
 */

#ifndef _EVENTDESC_H__
#define _EVENTDESC_H__

#include <deque>
#include <string>

class EventDesc
{
public:
	EventDesc();
	~EventDesc();
	EventDesc(const EventDesc &other);
	EventDesc& operator = (const EventDesc &other);
	void set_op(const std::string &op);
	void set_object(const std::string &object);
	void set_location(const std::string &location);
	void set_context(const std::string &context);

	const std::string * get_op() const;
	const std::string * get_object () const;
	const std::string * get_location() const;
	const std::string * get_context() const;

private:
	void cleanup();

private:
	std::string *op;
	std::string *object;
	std::string *location;
	std::string *context;
};

class EventDescContainer
{
public:
    typedef std::deque< EventDesc > DescContainer;    
    typedef std::deque< EventDesc >::size_type DescContainerSizeType;

    EventDescContainer();
    void add_desc(const EventDescContainer &descs);
    void add_desc(const EventDesc &desc);
    DescContainerSizeType get_size() const;
    const DescContainer & get_descs() const;

private:
    DescContainer descs;
};

#endif /* _EVENTDESC_H__ */


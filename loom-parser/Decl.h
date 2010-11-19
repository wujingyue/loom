/**
 * file: Decl.h
 * Author: Maoliang <kceiwH@gmail.com>
 * Created Time: Mon Oct 19 13:49:39 2009
 * Description: 
 */

#ifndef _DECL_H__
#define _DECL_H__

#include <string>
#include <map>

class Decl
{
public:
	bool add_method(const std::string &method, const std::string &function);
	void set_name(const std::string &n);
	void clear_methods();
	const std::string & get_name() const { return name; }
	const std::map< std::string, std::string > & get_methods() const {
return this->methods; }

private:
	std::string name;
	std::map< std::string, std::string > methods;
};


#endif /* _DECL_H__ */


/**
 * file: Decl.cpp
 * Author: Maoliang <kceiwH@gmail.com>
 * Created Time: Mon Oct 19 14:04:18 2009
 * Description: 
 */

#include "Decl.h"

void Decl::set_name(const std::string &n)
{
	this->name = n;
}

void Decl::clear_methods()
{
	this->methods.clear();
}

bool Decl::add_method(const std::string &method, const std::string &function)
{
	if (this->methods.find(method)==this->methods.end()) {
		this->methods.insert(make_pair(method, function));
		return true;
	} else {
		return false;
	}
}

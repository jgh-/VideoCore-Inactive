/*
 *  UriParser_cpp.cpp
 *  UriParser-cpp
 *
 *  Created by human on 4/13/16.
 *
 *
 */

#include <iostream>
#include "UriParser_cpp.hpp"
#include "UriParser_cppPriv.hpp"

void UriParser_cpp::HelloWorld(const char * s)
{
	 UriParser_cppPriv *theObj = new UriParser_cppPriv;
	 theObj->HelloWorldPriv(s);
	 delete theObj;
};

void UriParser_cppPriv::HelloWorldPriv(const char * s) 
{
	std::cout << s << std::endl;
};


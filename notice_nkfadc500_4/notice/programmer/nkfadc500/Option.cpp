/*
 * Option.cpp
 *
 *  Created on: Oct 3, 2012
 *      Author: evgeniy
 */
#include "Option.h"
#include <cstdlib>

Option::Option(const char * _name, int _argument_size,Option* _parent, void(*_runAction)(Option* option, void *)) {
  name = _name;
  children = NULL;
  chidrensize = 0;
  parent= _parent;
  argument_size=_argument_size;
  args=NULL;
  enabled=false;
  runAction = _runAction;
}

/*
 * Actions.h
 *
 *  Created on: Oct 2, 2012
 *      Author: Milman Evgeniy
 */

#ifndef OPTION_H_
#define OPTION_H_

struct Option {
  const char * name;
  Option** children;
  int chidrensize;
  Option* parent;
  int argument_size;
  char** args;
  bool enabled;
  void (*runAction)(Option* option, void *);

  Option(const char * _name, int _argument_size,Option* _parent, void(*_runAction)(Option* option, void *)) ;
};

#endif

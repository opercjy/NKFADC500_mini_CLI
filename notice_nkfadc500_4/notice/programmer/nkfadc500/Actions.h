/*
 * Actions.h
 *
 *  Created on: Oct 2, 2012
 *      Author: Milman Evgeniy
 */

#ifndef ACTIONS_H_
#define ACTIONS_H_

#include "Option.h"

void uploadFX3_action(Option* option, void *data);
void uploadFPGA_action(Option* option, void *data);
void devinfo_action(Option* option, void *data);
void write_sid_action(Option* option, void *data);
void write_version_action(Option* option, void *data);
void help_action(Option* option, void *data);
void list_action(Option* option, void *data);
void dev_action(Option* option, void *data);



#endif /* ACTIONS_H_ */

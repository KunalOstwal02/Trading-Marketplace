#ifndef TRADER_H
#define TRADER_H

#include "spx_common.h"

int* connect_pipes(char* trader_id, int* pipe);
void sigusr1_handler(int signo);

#endif
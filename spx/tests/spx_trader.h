#ifndef SPX_TRADER_H
#define SPX_TRADER_H

#include "spx_common.h"

typedef struct {
    int type;
    int order_id;
    char* product;
    int quantity;
    int price;
} order_t;

void command_maker(char* command, order_t* order);
int command_handler(char* command, order_t* order);
int* connect_pipes(char* trader_id, int* pipe);
void sigusr1_handler(int signo, siginfo_t* info, void* context);
void sigchld_handler(int signo, siginfo_t* info, void* context);



#endif

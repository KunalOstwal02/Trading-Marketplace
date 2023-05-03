// TODO
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include "cmocka.h"

#include "spx_exchange.h"

static void test_free_malloced(void **state) 
{
    int num_traders = 2;
    char*** named_pipes = malloc(sizeof(char**) * num_traders);
    int** malloced_pipes = malloc(sizeof(int*) * num_traders);
    for (int i = 0; i < num_traders; i++) {
        named_pipes[i] = malloc(sizeof(char*) * 2);
        named_pipes[i][0] = malloc(sizeof(char) * 32);
        named_pipes[i][1] = malloc(sizeof(char) * 32);
        malloced_pipes[i] = malloc(sizeof(int*) * 2);
    }

    int num_products = 2;
    char** products = malloc(sizeof(char*) * num_products);

    order_t *order = malloc(sizeof(order_t));
    orderbook_t* orderbook = malloc(sizeof(orderbook_t));
    orderbook->orders = malloc(sizeof(order_t*));

    trader_t **traders = malloc(sizeof(trader_t*) * num_traders);
    for (int i = 0; i < num_traders; i++) {
        traders[i] = malloc(sizeof(trader_t));
    }

    

    
    
    


}
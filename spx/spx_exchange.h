#ifndef SPX_EXCHANGE_H
#define SPX_EXCHANGE_H

#include "spx_common.h"

#define LOG_PREFIX "[SPX]"
#define MAX_PRODUCT_NAME_LENGTH 16
#define DEFAULT_MALLOC_SIZE 16

typedef struct {
    int type;
    int trader_id;
    int global_id;
    int order_id;
    char* product;
    int remaining_quantity;
    int completed_quantity;
    int price;
    int completed;
} order_t;

typedef struct {
    order_t** orders;
    int num_orders;
    int max_orders;
} orderbook_t;

typedef struct {
    int trader_id;
    int current_order;
    int* money_balance;
    int* product_balance;
} trader_t;

void sigusr1_handler(int sig, siginfo_t* info, void* context);
void sigchld_handler(int sig, siginfo_t* info, void* context);
char *strsep(char **stringp, const char *delim);
void remove_named_pipes(int argc, char*** malloced_named_pipes);
void free_malloced(int argc, char*** malloced_named_pipes, int** malloced_pipes, char** products, int num_products, orderbook_t *orderbook, order_t* order, int* pid, trader_t** traders, int num_traders);
void child_handler(char **named_pipes, int *malloced_pipes, int trader_num, char* trader_binary);
char*** malloc_fds(int argc);
int validate_args(int argc, char **argv);
char** read_product_file(char *product_file_path);
int return_zero();
int command_handler(char *buffer, orderbook_t *orderbook, int* pid, trader_t** traders, int num_traders, int trader_id, int** malloced_pipes, int num_products, char** products, order_t *order);
void manage_orderbook(orderbook_t* orderbook, order_t* order, int num_products, char** products, trader_t** traders, int num_traders, int** malloced_pipes);
int compare_orders_price (const void *a, const void *b);
int send_command_to_users(int **malloced_pipes, int* pid, int num_traders, int trader_id, orderbook_t *orderbook, order_t *order);


#endif
#include "spx_common.h"
#include "trader.h"

int pipes[2];

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Not enough arguments\n");
        return 1;
    }

    printf("%s\n", argv[1]);
    // register signal handler

    // connect to named pipes
    connect_pipes(argv[1], pipes);

    printf("Pipes: %d, %d\n", pipes[0], pipes[1]);

    struct sigaction sa_sigusr1 = { 0 };
    sa_sigusr1.sa_flags = 0;
    sa_sigusr1.sa_handler = &sigusr1_handler;
    sigaction(SIGUSR1, &sa_sigusr1, NULL);

    // event loop:
    // 1. write command to pipe
    // 2. read response from pipe
    // 3. print response

    write(pipes[1], "SELL 0 GPU 35 200;", strlen("SELL 0 GPU 30 500;"));

    sigaction(SIGUSR1, &sa_sigusr1, NULL);
    
    write(pipes[1], "BUY 0 GPU 20 500;", strlen("BUY 0 GPU 30 500;"));

    sigaction(SIGUSR1, &sa_sigusr1, NULL);

    close(pipes[0]);
    close(pipes[1]);

    // wait for exchange update (MARKET message)
    // send order
    // wait for exchange confirmation (ACCEPTED message)

    return 0;
}

void sigusr1_handler(int signo) {
    if (signo == SIGUSR1) {
        // printf("SIGUSR1 received %s\n", "SIGUSR1");
    }

}


// Connect named pipes
int* connect_pipes(char* trader_id, int* pipe) {
    char exchange_pipe_name[32];
    char trader_pipe_name[32];
    sprintf(exchange_pipe_name, "%s%s", "/tmp/spx_exchange_", trader_id);
    sprintf(trader_pipe_name, "%s%s", "/tmp/spx_trader_", trader_id);

    // printf("Connecting to FIFO %s\n", exchange_pipe_name);

    pipe[0] = open(exchange_pipe_name, O_RDONLY);
    pipe[1] = open(trader_pipe_name, O_WRONLY);

    if (pipe[0] == -1) {
        printf("Error opening exchange pipe %s\n", exchange_pipe_name);
        return NULL;
    }
    if (pipe[1] == -1) {
        printf("Error opening trader pipe %s\n", trader_pipe_name);
        return NULL;
    }

    printf("Child connected to exchange pipe %s\n", exchange_pipe_name);
    printf("Child connected to trader pipe %s\n", trader_pipe_name);

    return pipe;

}


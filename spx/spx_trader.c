#include "spx_trader.h"

int pipes[2];

struct pollfd pfds[2];

struct sigaction sa_sigusr1 = {0};

struct sigaction sa_sigchld = {0};

int order_id = 0;

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Not enough arguments\n");
        return 1;
    }

    // register signal handler
    sa_sigusr1.sa_flags = SA_SIGINFO;
    sa_sigusr1.sa_sigaction = &sigusr1_handler;
    sigaction(SIGUSR1, &sa_sigusr1, NULL);

    sa_sigchld.sa_flags = SA_SIGINFO;
    sa_sigchld.sa_sigaction = &sigchld_handler;
    sigaction(SIGCHLD, &sa_sigchld, NULL);

    // connect to named pipes
    connect_pipes(argv[1], pipes);

    pfds[0].fd = pipes[0];
    pfds[0].events = POLLIN;
    pfds[1].fd = pipes[1];
    pfds[1].events = POLLIN;

    int bytes_read;
    char buffer[64];
    char *return_command = malloc(sizeof(char) * 64);

    order_t *order = malloc(sizeof(order_t));
    order->product = malloc(sizeof(char) * 16);

    poll(pfds, 2, -1);
    // event loop:
    while ((bytes_read = read(pfds[0].fd, buffer, sizeof(buffer)) > 0))
    {
        poll(pfds, 2, 10);
        read(pfds[0].fd, buffer, sizeof(buffer));
        if (strncmp(buffer, "MARKET SELL", 11) == 0)
        {
            int command_return = command_handler(buffer, order);
            if (command_return == 1)
            {
                kill(getppid(), SIGCHLD);
                continue;
            }
            else
            {
                command_maker(return_command, order);

                write(pfds[1].fd, return_command, strlen(return_command));
                kill(getppid(), SIGUSR1);
            }
        }
        else if (strncmp(buffer, "FILL", 4) == 0)
        {
            poll(pfds, 2, 10);
            continue;
        }
        else if (strncmp(buffer, "ACCEPTED", 8) == 0)
        {
            poll(pfds, 2, 10);
            continue;
        }
        else {
            kill(getppid(), SIGCHLD);
            poll(pfds, 2, 10);
            continue;
        }
    }
    free(order->product);
    free(order);
    free(return_command);

    return 0;
}

void command_maker(char *command, order_t *order)
{
    sprintf(command, "BUY %d %s %d %d;", order_id, order->product, order->quantity, order->price);
    return;
}

int command_handler(char *command, order_t *order)
{
    char *order_type[4];
    sscanf(command, "MARKET %s %s %d %d", *order_type, order->product, &(order->quantity), &(order->price));
    if (order->quantity > 1000)
    {
        return 1;
    }
    order->type = BUY;
    return 0;
}

void sigchld_handler(int signo, siginfo_t *info, void *context)
{
    return;
}

void sigusr1_handler(int signo, siginfo_t *info, void *context)
{
    printf("%d\n", info->si_pid);
}

// Connect named pipes
int *connect_pipes(char *trader_id, int *pipe)
{
    char exchange_pipe_name[32];
    char trader_pipe_name[32];
    sprintf(exchange_pipe_name, "%s%s", "/tmp/spx_exchange_", trader_id);
    sprintf(trader_pipe_name, "%s%s", "/tmp/spx_trader_", trader_id);

    // printf("Connecting to FIFO %s\n", exchange_pipe_name);

    pipe[0] = open(exchange_pipe_name, O_RDONLY);
    pipe[1] = open(trader_pipe_name, O_WRONLY);

    if (pipe[0] == -1)
    {
        printf("Error opening exchange pipe %s\n", exchange_pipe_name);
        return NULL;
    }
    if (pipe[1] == -1)
    {
        printf("Error opening trader pipe %s\n", trader_pipe_name);
        return NULL;
    }
    printf("Child connected to exchange pipe %s\n", exchange_pipe_name);
    printf("Child connected to trader pipe %s\n", trader_pipe_name);

    return pipe;
}
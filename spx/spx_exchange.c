/**
 * comp2017 - assignment 3
 * Kunal Ostwal
 * kost6112
 */

#include "spx_exchange.h"
#define EXCHANGE 0
#define TRADER 1

struct sigaction sa_sigusr1 = {0};

struct sigaction sa_sigchld = {0};

int global_id = 0;

int global_trader_pid = -1;

int exchange_fee = 0;

int main(int argc, char **argv)
{
	printf("%s Starting\n", LOG_PREFIX);

	sa_sigusr1.sa_sigaction = sigusr1_handler;
	sa_sigusr1.sa_flags = SA_SIGINFO;
	sigaction(SIGUSR1, &sa_sigusr1, NULL);

	sa_sigchld.sa_sigaction = sigchld_handler;
	sa_sigchld.sa_flags = SA_SIGINFO;
	sigaction(SIGCHLD, &sa_sigchld, NULL);

	// validate args

	if (validate_args(argc, argv) == 1)
	{
		printf("Invalid arguments.\n");
		return 1;
	}

	int num_traders = argc - 2;

	// read product file
	char **products = read_product_file(argv[1]);
	if (products == NULL)
	{
		perror("Error reading product file.\n");
		return 1;
	}
	int num_products = atoi(products[0]);
	printf("%s Trading %d products:", LOG_PREFIX, num_products);
	for (int i = 1; i < atoi(products[0]) + 1; i++)
	{
		printf(" %s", products[i]);
	}
	printf("\n");

	// create pipes
	char ***malloced_named_pipes = malloc_fds(argc);

	if (malloced_named_pipes == NULL)
	{
		return 1;
	}

	int **malloced_pipes = malloc(sizeof(int *) * num_traders);
	trader_t **traders = malloc(sizeof(trader_t *) * num_traders);
	for (int i = 0; i < num_traders; i++)
	{
		malloced_pipes[i] = malloc(sizeof(int) * 2);
		traders[i] = malloc(sizeof(trader_t));
		traders[i]->trader_id = i;
		traders[i]->money_balance = calloc(num_products, sizeof(int) * num_products);
		traders[i]->product_balance = calloc(num_products, sizeof(int) * num_products);
	}

	orderbook_t *orderbook = malloc(sizeof(orderbook_t));

	orderbook->orders = malloc(sizeof(order_t) * DEFAULT_MALLOC_SIZE);
	orderbook->num_orders = 0;
	orderbook->max_orders = DEFAULT_MALLOC_SIZE;

	int *pid = malloc(sizeof(int) * num_traders);
	int fees_collected = 0;
	order_t *order = NULL;

	for (int i = 0; i < num_traders; i++)
	{
		if (mkfifo(malloced_named_pipes[i][EXCHANGE], 0777) == -1)
		{
			if (errno != EEXIST)
			{
				perror("Error creating named pipe");
				free_malloced(argc, malloced_named_pipes, malloced_pipes, products, num_products, orderbook, order, pid, traders, num_traders);
				return 1;
			}
		}
		printf("%s Created FIFO %s\n", LOG_PREFIX, malloced_named_pipes[i][EXCHANGE]);

		if (mkfifo(malloced_named_pipes[i][TRADER], 0777) == -1)
		{
			if (errno != EEXIST)
			{
				perror("Error creating named pipe");
				free_malloced(argc, malloced_named_pipes, malloced_pipes, products, num_products, orderbook, order, pid, traders, num_traders);
				return 1;
			}
		}
		printf("%s Created FIFO %s\n", LOG_PREFIX, malloced_named_pipes[i][TRADER]);
		printf("%s Starting trader %d (%s)\n", LOG_PREFIX, i, argv[i + 2]);

		pid[i] = fork();

		if (pid[i] < 0)
		{
			perror("Something went wrong with fork.\n");
			return 1;
		}
		else if (pid[i] == 0)
		{
			char *trader_binary = argv[i + 2];
			child_handler(malloced_named_pipes[i], malloced_pipes[i], i, trader_binary);
		}
		else
		{
			// parent

			// parent_handler(malloced_named_pipes, i, pipe, pid);

			if ((malloced_pipes[i][0] = open(malloced_named_pipes[i][EXCHANGE], O_WRONLY)) < 0)
			{
				perror("Error opening named pipe");
				return -1;
			}
			printf("%s Connected to %s\n", LOG_PREFIX, malloced_named_pipes[i][EXCHANGE]);

			if ((malloced_pipes[i][1] = open(malloced_named_pipes[i][TRADER], O_RDONLY)) < 0)
			{
				perror("Unable to open fifo");
				return -1;
			}
			printf("%s Connected to %s\n", LOG_PREFIX, malloced_named_pipes[i][TRADER]);

			if ((write(malloced_pipes[i][0], "MARKET OPEN;", strlen("MARKET OPEN;")) < 0))
			{
				perror("Error writing to pipe");
				return -1;
			}

			kill(pid[i], SIGUSR1);
		}
	}

	for (int i = 0; i < num_traders; i++)
	{

		// read from pipe[1]
		char buffer[128];

		int bytes_read = read(malloced_pipes[i][TRADER], buffer, 128);
		while (bytes_read > 0)
		{
			pause();
			buffer[bytes_read] = '\0';
			command_handler(buffer, orderbook, pid, traders, num_traders, i, malloced_pipes, num_products, products, order);

			bytes_read = read(malloced_pipes[i][1], buffer, 128);
		}

		if (malloced_pipes[i][0] == -1 || malloced_pipes[i][1] == -1)
		{
			perror("opening pipes.\n");
			return 1;
		}

		kill(pid[i], SIGCHLD);


	}

	for (int i = 0; i < argc - 2; i++)
	{
		close(malloced_pipes[i][0]);
		close(malloced_pipes[i][1]);
		printf("%s Trader %d disconnected\n", LOG_PREFIX, i);
	}

	printf("%s Trading completed\n", LOG_PREFIX);
	printf("%s Exchange fees collected: $%d\n", LOG_PREFIX, fees_collected);
	remove_named_pipes(num_traders, malloced_named_pipes);
	free_malloced(argc, malloced_named_pipes, malloced_pipes, products, num_products, orderbook, order, pid, traders, num_traders);
	return 0;
}

int send_command_to_users(int **malloced_pipes, int *pid, int num_traders, int trader_id, orderbook_t *orderbook, order_t *order)
{

	char *buffer = malloc(sizeof(char) * 128);
	strcpy(buffer, "");
	if (order->type == BUY)
	{
		sprintf(buffer, "MARKET BUY %s %d %d;", order->product, order->remaining_quantity, order->price);
	}
	else
	{
		sprintf(buffer, "MARKET SELL %s %d %d;", order->product, order->remaining_quantity, order->price);
	}

	for (int i = 0; i < num_traders; i++)
	{
		if (i == trader_id)
		{
			continue;
		}
		write(malloced_pipes[i][0], buffer, strlen(buffer));
		kill(pid[i], SIGUSR1);
	}
	free(buffer);
	return 0;
}

// https://stackoverflow.com/questions/58244300/getting-the-error-undefined-reference-to-strsep-with-clang-and-mingw
char *strsep(char **stringp, const char *delim)
{
	char *rv = *stringp;
	if (rv)
	{
		*stringp += strcspn(*stringp, delim);
		if (**stringp)
			*(*stringp)++ = '\0';
		else
			*stringp = 0;
	}
	return rv;
}

void sigusr1_handler(int sig, siginfo_t *info, void *context)
{
	global_trader_pid = info->si_pid;
}

void sigchld_handler(int sig, siginfo_t *info, void *context)
{
	global_trader_pid = info->si_pid;
}

void remove_named_pipes(int num_traders, char ***malloced_named_pipes)
{
	for (int i = 0; i < num_traders; i++)
	{
		remove(malloced_named_pipes[i][EXCHANGE]);
		remove(malloced_named_pipes[i][TRADER]);
	}
	return;
}

int compare_orders_price(const void *a, const void *b)
{
	order_t *order_a = (order_t *)a;
	order_t *order_b = (order_t *)b;

	return (order_a->price - order_b->price);
}

void free_malloced(int argc, char ***malloced_named_pipes, int **malloced_pipes, char **products, int num_products, orderbook_t *orderbook, order_t *order, int *pid, trader_t **traders, int num_traders)
{
	for (int i = 0; i < orderbook->num_orders; i++)
	{
		free(orderbook->orders[i]->product);
		free(orderbook->orders[i]);
	}

	free(order);

	free(orderbook->orders);
	free(orderbook);

	for (int i = 0; i < num_traders; i++)
	{
		free(traders[i]->product_balance);
		free(traders[i]->money_balance);
		free(traders[i]);
	}
	free(traders);

	for (int i = 0; i < argc - 2; i++)
	{
		for (int j = 0; j < 2; j++)
		{
			free(malloced_named_pipes[i][j]);
		}
		free(malloced_pipes[i]);
		free(malloced_named_pipes[i]);
	}
	free(malloced_pipes);
	free(malloced_named_pipes);

	for (int i = 0; i < num_products + 1; i++)
	{
		free(products[i]);
	}
	free(products);
	free(pid);
	return;
}

char ***malloc_fds(int argc)
{

	char ***malloced_named_pipes = malloc(sizeof(char **) * argc);
	for (int i = 0; i < argc - 2; i++)
	{
		malloced_named_pipes[i] = malloc(sizeof(char *) * 2);
		malloced_named_pipes[i][0] = malloc(sizeof(char) * (strlen(FIFO_EXCHANGE) + 1));
		malloced_named_pipes[i][1] = malloc(sizeof(char) * (strlen(FIFO_TRADER) + 1));

		sprintf(malloced_named_pipes[i][0], FIFO_EXCHANGE, i);
		sprintf(malloced_named_pipes[i][1], FIFO_TRADER, i);
	}
	return malloced_named_pipes;
}

order_t *extract_order(char *buffer, int trader_id, order_t *order)
{
	char *type = strsep(&buffer, " ");

	if (strcmp(type, "BUY") == 0)
	{
		order->type = BUY;
	}
	else if (strcmp(type, "SELL") == 0)
	{
		order->type = SELL;
	}
	else
	{
		printf("%s Invalid order type\n", LOG_PREFIX);
		return NULL;
	}

	order->trader_id = trader_id;

	char *order_id_str = strsep(&buffer, " ");
	order->order_id = atoi(order_id_str);

	char *product = strsep(&buffer, " ");
	order->product = malloc(sizeof(char) * DEFAULT_MALLOC_SIZE);
	strcpy(order->product, product);

	char *quantity_str = strsep(&buffer, " ");
	order->remaining_quantity = atoi(quantity_str);
	order->completed_quantity = 0;

	char *price_str = strsep(&buffer, " ");
	order->price = atoi(price_str);

	order->global_id = global_id;
	order->completed = 0;

	printf("%s [T%d] Parsing command: <%s %d %s %d %d>\n", LOG_PREFIX, trader_id, type, order->order_id, order->product, order->remaining_quantity, order->price);
	return order;
}

int validate_order(order_t *order, int num_products, char **products)
{

	int product_index = -1;
	for (int i = 0; i < num_products + 1; i++)
	{
		if (strcmp(order->product, products[i]) == 0)
		{
			product_index = i;
			break;
		}
	}
	if (product_index == -1)
	{
		// printf("Did not find product %s\n", order->product);
		return 1;
	}

	if (order->order_id < 0 || order->order_id > 999999)
	{
		return 1;
	}

	if (order->price <= 0 || order->price > 999999)
	{
		return 1;
	}

	if (order->remaining_quantity <= 0 || order->remaining_quantity > 999999)
	{
		return 1;
	}

	if (strlen(order->product) > 16)
	{
		return 1;
	}
	return 0;
}

void orderbook_add_order(order_t *order, orderbook_t *orderbook)
{
	orderbook->orders[orderbook->num_orders] = malloc(sizeof(order_t));
	orderbook->orders[orderbook->num_orders]->type = order->type;
	orderbook->orders[orderbook->num_orders]->trader_id = order->trader_id;
	orderbook->orders[orderbook->num_orders]->order_id = order->order_id;
	orderbook->orders[orderbook->num_orders]->product = malloc(sizeof(char) * (strlen(order->product) + 1));
	strcpy(orderbook->orders[orderbook->num_orders]->product, order->product);
	orderbook->orders[orderbook->num_orders]->remaining_quantity = order->remaining_quantity;
	orderbook->orders[orderbook->num_orders]->completed_quantity = order->completed_quantity;
	orderbook->orders[orderbook->num_orders]->price = order->price;
	orderbook->orders[orderbook->num_orders]->global_id = order->global_id;
	orderbook->orders[orderbook->num_orders]->completed = order->completed;

	orderbook->num_orders++;

	if (orderbook->num_orders == orderbook->max_orders)
	{
		orderbook->max_orders *= 2;
		orderbook->orders = realloc(orderbook->orders, sizeof(order_t *) * orderbook->max_orders);
	}
	return;
}

void match_orders(orderbook_t *orderbook, order_t *order, int num_products, char **products, trader_t **traders, int trader_id, int num_traders)
{
	int product_index = -1;
	for (int i = 0; i < num_products + 1; i++)
	{
		if (strcmp(order->product, products[i]) == 0)
		{
			product_index = i - 1;
			break;
		}
	}

	for (int i = 0; i < orderbook->num_orders; i++)
	{
		if (order->global_id == orderbook->orders[i]->global_id || order->type == orderbook->orders[i]->type || strcmp(order->product, orderbook->orders[i]->product) != 0 || orderbook->orders[i]->completed == 1)
		{
			continue;
		}

		if (order->type == BUY)
		{
			if (orderbook->orders[i]->price > order->price)
			{
				continue;
			}

			if (orderbook->orders[i]->remaining_quantity > order->remaining_quantity)
			{
				orderbook->orders[i]->remaining_quantity -= order->remaining_quantity;
				orderbook->orders[i]->completed_quantity += order->remaining_quantity;
				order->completed_quantity += order->remaining_quantity;
				order->remaining_quantity = 0;
				order->completed = 1;

				traders[trader_id]->money_balance[product_index] -= orderbook->orders[i]->price * order->completed_quantity;
				traders[trader_id]->product_balance[product_index] += order->completed_quantity;

				traders[orderbook->orders[i]->trader_id]->money_balance[product_index] += orderbook->orders[i]->price * order->completed_quantity;
				traders[orderbook->orders[i]->trader_id]->product_balance[product_index] -= order->completed_quantity;
			}
			else if (orderbook->orders[i]->remaining_quantity < order->remaining_quantity)
			{
				order->remaining_quantity -= orderbook->orders[i]->remaining_quantity;
				order->completed_quantity += orderbook->orders[i]->remaining_quantity;
				orderbook->orders[i]->completed_quantity = orderbook->orders[i]->remaining_quantity;
				orderbook->orders[i]->remaining_quantity = 0;
				orderbook->orders[i]->completed = 1;

				traders[trader_id]->money_balance[product_index] -= orderbook->orders[i]->price * orderbook->orders[i]->completed_quantity;
				traders[trader_id]->product_balance[product_index] += orderbook->orders[i]->completed_quantity;

				traders[orderbook->orders[i]->trader_id]->money_balance[product_index] += orderbook->orders[i]->price * orderbook->orders[i]->completed_quantity;
				traders[orderbook->orders[i]->trader_id]->product_balance[product_index] -= orderbook->orders[i]->completed_quantity;
			}
			else
			{
				order->completed_quantity = order->remaining_quantity;
				order->remaining_quantity = 0;
				order->completed = 1;

				orderbook->orders[i]->completed_quantity = orderbook->orders[i]->remaining_quantity;
				orderbook->orders[i]->remaining_quantity = 0;
				orderbook->orders[i]->completed = 1;

				traders[trader_id]->money_balance[product_index] -= orderbook->orders[i]->price * orderbook->orders[i]->completed_quantity;
				traders[trader_id]->product_balance[product_index] += orderbook->orders[i]->completed_quantity;

				traders[orderbook->orders[i]->trader_id]->money_balance[product_index] += orderbook->orders[i]->price * orderbook->orders[i]->completed_quantity;
				traders[orderbook->orders[i]->trader_id]->product_balance[product_index] -= orderbook->orders[i]->completed_quantity;
			}
		}
		else
		{
			if (orderbook->orders[i]->price < order->price)
			{
				continue;
			}

			if (orderbook->orders[i]->remaining_quantity > order->remaining_quantity)
			{
				orderbook->orders[i]->remaining_quantity -= order->remaining_quantity;
				orderbook->orders[i]->completed_quantity += order->remaining_quantity;
				order->completed_quantity += order->remaining_quantity;
				order->remaining_quantity = 0;
				order->completed = 1;

				traders[trader_id]->money_balance[product_index] += orderbook->orders[i]->price * order->completed_quantity;
				traders[trader_id]->product_balance[product_index] -= order->completed_quantity;

				traders[orderbook->orders[i]->trader_id]->money_balance[product_index] -= orderbook->orders[i]->price * order->completed_quantity;
				traders[orderbook->orders[i]->trader_id]->product_balance[product_index] += order->completed_quantity;
			}
			else if (orderbook->orders[i]->remaining_quantity < order->remaining_quantity)
			{
				order->remaining_quantity -= orderbook->orders[i]->remaining_quantity;
				order->completed_quantity += orderbook->orders[i]->remaining_quantity;
				orderbook->orders[i]->completed_quantity = orderbook->orders[i]->remaining_quantity;
				orderbook->orders[i]->remaining_quantity = 0;
				orderbook->orders[i]->completed = 1;

				traders[trader_id]->money_balance[product_index] += orderbook->orders[i]->price * orderbook->orders[i]->completed_quantity;
				traders[trader_id]->product_balance[product_index] -= orderbook->orders[i]->completed_quantity;

				traders[orderbook->orders[i]->trader_id]->money_balance[product_index] -= orderbook->orders[i]->price * orderbook->orders[i]->completed_quantity;
				traders[orderbook->orders[i]->trader_id]->product_balance[product_index] += orderbook->orders[i]->completed_quantity;
			}
			else
			{
				order->completed_quantity = order->remaining_quantity;
				order->remaining_quantity = 0;
				order->completed = 1;

				orderbook->orders[i]->completed_quantity = orderbook->orders[i]->remaining_quantity;
				orderbook->orders[i]->remaining_quantity = 0;
				orderbook->orders[i]->completed = 1;

				traders[trader_id]->money_balance[product_index] += orderbook->orders[i]->price * orderbook->orders[i]->completed_quantity;
				traders[trader_id]->product_balance[product_index] -= orderbook->orders[i]->completed_quantity;

				traders[orderbook->orders[i]->trader_id]->money_balance[product_index] -= orderbook->orders[i]->price * orderbook->orders[i]->completed_quantity;
				traders[orderbook->orders[i]->trader_id]->product_balance[product_index] += orderbook->orders[i]->completed_quantity;
			}
		}
	}
}

void manage_orderbook(orderbook_t *orderbook, order_t *order, int num_products, char **products, trader_t **traders, int num_traders, int **malloced_pipes)
{

	qsort(orderbook->orders, orderbook->num_orders, sizeof(order_t *), compare_orders_price);

	match_orders(orderbook, order, num_products, products, traders, order->trader_id, num_traders);

	printf("%s	--ORDERBOOK--\n", LOG_PREFIX);

	for (int i = 1; i < num_products + 1; i++)
	{
		int buy_levels = 0;
		int sell_levels = 0;

		for (int j = 0; j < orderbook->num_orders; j++)
		{
			if (j < orderbook->num_orders - 1 && orderbook->orders[j]->price == orderbook->orders[j + 1]->price)
			{
				continue;
			}
			if (strcmp(orderbook->orders[j]->product, products[i]) == 0 && orderbook->orders[j]->remaining_quantity > 0)
			{
				if (orderbook->orders[j]->type == BUY)
				{
					buy_levels++;
				}
				else
				{
					sell_levels++;
				}
			}
		}

		printf("%s	Product: %s; Buy levels: %d; Sell levels: %d\n", LOG_PREFIX, products[i], buy_levels, sell_levels);
		int same_orders = 0;
		int same_quantity = 0;
		for (int j = orderbook->num_orders - 1; j >= 0; j--)
		{
			if (strcmp(orderbook->orders[j]->product, products[i]) == 0 && orderbook->orders[j]->remaining_quantity > 0)
			{
				if ((j != 0) && orderbook->orders[j]->type == orderbook->orders[j - 1]->type && orderbook->orders[j]->price == orderbook->orders[j - 1]->price)
				{
					same_orders++;
					same_quantity += orderbook->orders[j]->remaining_quantity;
					if (orderbook->orders[j]->type == BUY)
					{
						buy_levels--;
					}
					else
					{
						sell_levels--;
					}
					continue;
				}
				else
				{
					same_quantity += orderbook->orders[j]->remaining_quantity;
					same_orders++;
					printf("%s		%s %d @ $%d (%d %s)\n", LOG_PREFIX, orderbook->orders[j]->type == BUY ? "BUY" : "SELL", same_quantity, orderbook->orders[j]->price, same_orders, same_orders > 1 ? "orders" : "order");
					same_quantity = 0;
					same_orders = 0;
				}
			}
		}
	}

	printf("%s	--POSITIONS--\n", LOG_PREFIX);
	for (int i = 0; i < num_traders; i++)
	{
		printf("%s	Trader %d: %s %d ($%d)", LOG_PREFIX, traders[i]->trader_id, products[1], traders[i]->product_balance[1], traders[i]->money_balance[i]);
		for (int j = 2; j < num_products + 1; j++)
		{
			printf(", %s %d ($%d)", products[j], traders[i]->product_balance[j], traders[i]->money_balance[j]);
		}
		printf("\n");
	}

	for (int i = 0; i < num_products; i++)
	{
		for (int j = 0; j < orderbook->num_orders; j++)
		{
			if (strcmp(orderbook->orders[j]->product, products[i]) == 0 && orderbook->orders[j]->remaining_quantity > 0)
			{
			}
		}
	}
}

void process_order(orderbook_t *orderbook, order_t *order, int num_products, char **products, int *pid, int num_traders, int trader_id, int **malloced_pipes)
{
	orderbook_add_order(order, orderbook);

	char *buffer = malloc(sizeof(char) * 18);
	sprintf(buffer, "ACCEPTED %d;", order->order_id);
	write(malloced_pipes[trader_id][EXCHANGE], buffer, strlen(buffer));
	kill(global_trader_pid, SIGUSR1);

	send_command_to_users(malloced_pipes, pid, num_traders, trader_id, orderbook, order);

	free(buffer);

	return;
}

int command_handler(char *buffer, orderbook_t *orderbook, int *pid, trader_t **traders, int num_traders, int trader_id, int **malloced_pipes, int num_products, char **products, order_t *order)
{
	order = malloc(sizeof(order_t));
	if (strncmp(buffer, "BUY", 3) == 0 || strncmp(buffer, "SELL", 4) == 0)
	{
		order = extract_order(buffer, trader_id, order);
		if (validate_order(order, num_products, products) == 0)
		{
			process_order(orderbook, order, num_products, products, pid, num_traders, trader_id, malloced_pipes);
			manage_orderbook(orderbook, order, num_products, products, traders, num_traders, malloced_pipes);
			free(order->product);
			free(order);
			return 0;
		}
		else
		{
			write(malloced_pipes[trader_id][EXCHANGE], "INVALID;", strlen("INVALID;"));
			kill(global_trader_pid, SIGUSR1);
			free(order->product);
			free(order);

			return 1;
		}
	}
	else if (strncmp(buffer, "AMEND", 5) == 0)
	{
		strsep(&buffer, " ");
	}
	free(order);
	return 0;
}

void child_handler(char **named_pipes, int *malloced_pipes, int trader_id, char *trader_binary)
{

	// open fifos
	char str_trader_id[6];
	sprintf(str_trader_id, "%d", trader_id);

	execl(trader_binary, trader_binary, str_trader_id, (char *)NULL);
	// parse buffer
}

int validate_args(int argc, char **argv)
{
	// check for correct number of arguments
	if (argc < 2)
	{
		fprintf(stderr, "Usage: provide product file and trader program\n");
		return 1;
	}

	return 0;
}

char **read_product_file(char *product_file_path)
{
	// read product file

	// open file
	FILE *product_file = fopen(product_file_path, "r");

	char **products = malloc(sizeof(char *) * DEFAULT_MALLOC_SIZE);
	int i = 0;
	char line[16];

	while (fgets(line, 16, product_file) != NULL)
	{
		strtok(line, "\n");
		products[i] = malloc(sizeof(char) * DEFAULT_MALLOC_SIZE);
		strcpy(products[i], line);
		i++;
		if (i % DEFAULT_MALLOC_SIZE == 0)
		{
			products = realloc(products, sizeof(char *) * (i + DEFAULT_MALLOC_SIZE));
		}
	}
	fclose(product_file);

	return products;
}
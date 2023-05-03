CC=gcc
CFLAGS=-Wall -Werror -Wvla -O0 -std=c11 -g -fsanitize=address,leak 
LDFLAGS=-lm
BINARIES=spx_exchange spx_trader trader_a trader_b
EXCHANGE=spx_exchange.c spx_exchange.h
TRADER=trader_a.c trader.h

all: $(BINARIES)

spx_exchange: spx_exchange.c spx_exchange.h
	make trader_a
	make trader_b
	$(CC) $(CFLAGS) $(EXCHANGE) -o $@ $(LDFLAGS)

trader_a: trader_a.c trader.h
	$(CC) $(CFLAGS) $(^) -o $@ $(LDFLAGS)

trader_b: trader_b.c trader.h
	$(CC) $(CFLAGS) $(^) -o $@ $(LDFLAGS)

git:
	make all
	git add --all
	git commit -m "update"
	git push

run:
	./spx_exchange products.txt ./trader_a ./trader_b

.PHONY: clean
clean:
	rm -f $(BINARIES)
	rm -f ./tmp/*


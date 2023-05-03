1. Describe how your exchange works.

From the number of arguments provided, the exchange stores the names of named_pipes for each trader binaries in a nested dynamic array which is in the form of: `[ [ "Exchange_1", "Trader_1" ], [ "Exchange_2", "Trader_2" ]... ]`.  
Then in a loop till the number of trader binaries provided, named pipes for each trader are created. After this, the process is forked. The child process replaces the program with trader binary using `execl()`. The parent now opens the pipes and sends sigusr1 to the trader, from which point connection between trader and exchange is established. The exchange reads content from named pipes whenever a trader sends a signal (handled by sig_handler).
The trader details are stored in a dynamic array of a struct called traders. Order details are stored in an orderbook struct as a dynamic array. Each order also has a global ID to ensure the time component of price-time algorithm.  
While matching, orderbook is sorted using qsort according to prices and then if the prices are similar, global ID is compared.    


2. Describe your design decisions for the trader and how it's fault-tolerant.

Auto-trader uses poll to wait and receive signals from exchange. Signal receiving is done in a while loop that continues reading until the exchange sends a message. at the end of each loop, the trader waits for a signal from exchange and when recieved, it'll rerun the loop.  
Whenever a signal is received, the trader reads from the exchange pipe. If its a `MARKET SELL` signal, it will create an identical return statement that gets sent to exchange (given that the quantity < 1000). This statement is sent as a command and `SIGUSR1` is sent after that.  
If a command is accepted, the trader waits and if no signal is received from exchange, it will continue to send `SIGUSR1` until it is recieved.

3. Describe your tests and how to run them.


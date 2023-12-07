This program is designed to take in 4 paramters
-n [number of processes to launch]
-s [number of processes to run at one time]
-t [time interval in nanoseconds to launch processes]
-f [file to output log]
This program will fork off n processes while maintaining the limit set by s
Each process will request read or write to pages in its page table
The main process will fulfil these requests by maintaining a frame table
The main process will check if the requested page is in the page table
If it is not, it will page fault and do a blocked wait until it either populates an empty frame or replaces the Frame at the head of the FIFO queue
Finally, it will output some stats on the simulation

Repos: https://github.com/jhsq2r/OS_Project5

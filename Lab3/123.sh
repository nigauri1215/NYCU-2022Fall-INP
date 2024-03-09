#!/bin/sh
timeout 20 ./tcp_2 1;   sleep 5  # send at 1 MBps
timeout 20 ./tcp_2 1.5; sleep 5  # send at 1.5 MBps
timeout 20 ./tcp_2 2;   sleep 5  # send at 2 MBps
timeout 20 ./tcp_2 3
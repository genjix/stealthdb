#!/bin/bash
g++ -o stealth *.cpp $(pkg-config --cflags --libs libbitcoin)


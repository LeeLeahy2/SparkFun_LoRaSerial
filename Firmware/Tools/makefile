######################################################################
# makefile
#
# Builds the LoRaSerial support programs
######################################################################

##########
# Source files
##########

EXECUTABLES += Pipe_1
EXECUTABLES += ProcessLog
EXECUTABLES += Slow_Read
EXECUTABLES += SprinklerServer
EXECUTABLES += VcServerTest

INCLUDES  = settings.h
INCLUDES += ../LoRaSerial/Virtual_Circuit_Protocol.h
INCLUDES += ../LoRaSerial/Weather_Station_Protocol.h

COMMON_LIB = Common_Lib.a

LIB_OBJS = RadioV2.o Terminal.o

##########
# Buid tools and rules
##########

GCC = gcc
CFLAGS = -flto -O3 -Wpedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-parameter
CC = $(GCC) $(CFLAGS)

%.o: %.c $(INCLUDES)
	$(CC) -c -o $@ $<

%: %.c $(INCLUDES)
	$(CC) $(CFLAGS) -o $@ $<

##########
# Buid all the sources - must be first
##########

.PHONY: all

all: $(EXECUTABLES)

##########
# Buid the libraries
##########

$(COMMON_LIB): $(LIB_OBJS)
	gcc --version
	ar rvs $@ $^

##########
# Build the executables
##########

Pipe_1: Pipe_1.c $(COMMON_LIB)
	$(CC) -o $@ $^
	chmod o=rx $@

ProcessLog: ProcessLog.c $(COMMON_LIB)
	$(CC) -o $@ $^
	chmod o=rx $@

Slow_Read: Slow_Read.c $(COMMON_LIB)
	$(CC) -o $@ $^

SprinklerServer: SprinklerServer.c WeatherStation.c
	$(CC) -o $@ $^ -lmariadbclient

VcServerTest: VcServerTest.c $(COMMON_LIB)
	$(CC) -o $@ $^

########
# Clean the build directory
##########

.PHONY: clean

clean:
	rm -f *.o *.a $(EXECUTABLES)

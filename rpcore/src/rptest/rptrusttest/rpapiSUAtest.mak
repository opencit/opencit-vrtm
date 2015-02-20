RPROOT=../..
#BIN=        $(RPROOT)/bin/debug
#OBJ=        $(RPROOT)/build/debug/rptrustobjects
BIN=        $(RPROOT)/bin/release
OBJ=        $(RPROOT)/build/release/rptrustobjects
#LIBHOME=    /home/s1
#LIB=        $(LIBHOME)/rp/lib
LIB=        $(RPROOT)/lib
INC=        $(RPROOT)/inc

S=          .
T=          .
SC=         ../fileProxy/Code/commonCode
SCC=        ../fileProxy/Code/jlmcrypto
BSC=        ../fileProxy/Code/oldjlmbignum
TH=         ../fileProxy/Code/tao
TRS=        .
TS=         ../TPMDirect
CH=         ../channels
TCS =       ../fileProxy/Code/tcService

DEBUG_CFLAGS     := -Wall  -Wno-format -g -DDEBUG
RELEASE_CFLAGS   := -Wall  -Wno-unknown-pragmas -Wno-format -O3
O1RELEASE_CFLAGS   := -Wall  -Wno-unknown-pragmas -Wno-format -O1
#CFLAGS=     -D LINUX  -D TEST -D TIXML_USE_STL -D __FLUSHIO__ $(DEBUG_CFLAGS)
CFLAGS=     -D LINUX  -D TIXML_USE_STL -D __FLUSHIO__ $(RELEASE_CFLAGS)
LDFLAGS          := $(RELEASE_LDFLAGS)
O1CFLAGS=    -D LINUX -D TEST -D TIXML_USE_STL -D __FLUSHIO__ $(O1RELEASE_CFLAGS)

CC=         g++
LINK=       g++

dobjs=      $(OBJ)/rpapiSUAtest.o 
qobjs=      $(OBJ)/rpapiQVtest.o 
vobjs=      $(OBJ)/rpapiVtest.o 

all: $(BIN)/rpapiSUAtest  $(BIN)/rpapiQVtest  $(BIN)/rpapiVtest

$(BIN)/rpapiSUAtest: $(dobjs)
	@echo "rpapiSUAtest"
	$(LINK) -o $(BIN)/rpapiSUAtest $(dobjs) $(LDFLAGS) -lpthread -L$(LIB) -lrpcrypto
$(OBJ)/rpapiSUAtest.o: $(TRS)/rpapiSUAtest.cpp  $(INC)/rpapi.h
	$(CC) $(CFLAGS) -I$(INC) -I$(TRS) -I$(SC) -I$(TCS) -c -o $(OBJ)/rpapiSUAtest.o $(TRS)/rpapiSUAtest.cpp

$(BIN)/rpapiQVtest: $(qobjs)
	@echo "rpapiQVtest"
	$(LINK) -o $(BIN)/rpapiQVtest $(qobjs) $(LDFLAGS) -lpthread -L$(LIB) -lrpcrypto
$(OBJ)/rpapiQVtest.o: $(TRS)/rpapiQVtest.cpp  $(INC)/rpapi.h
	$(CC) $(CFLAGS) -I$(INC) -I$(TRS) -I$(SC) -I$(TCS) -c -o $(OBJ)/rpapiQVtest.o $(TRS)/rpapiQVtest.cpp

$(BIN)/rpapiVtest: $(vobjs)
	@echo "rpapiVtest"
	$(LINK) -o $(BIN)/rpapiVtest $(vobjs) $(LDFLAGS) -lpthread -L$(LIB) -lrpcrypto
$(OBJ)/rpapiVtest.o: $(TRS)/rpapiVtest.cpp  $(INC)/rpapi.h
	$(CC) $(CFLAGS) -I$(INC) -I$(TRS) -I$(SC) -I$(TCS) -c -o $(OBJ)/rpapiVtest.o $(TRS)/rpapiVtest.cpp
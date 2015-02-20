RPROOT=../../..
BIN=        $(RPROOT)/bin/debug
OBJ=        $(RPROOT)/build/debug/rptrustobjects
#BIN=        $(RPROOT)/bin/release
#OBJ=        $(RPROOT)/build/release/rptrustobjects
#LIBHOME=    /home/s1
#LIB=        $(LIBHOME)/rp/lib
LIB=        $(RPROOT)/lib
INC=        $(RPROOT)/inc

S=          .
T=	    .
SC=         ../../fileProxy/Code/commonCode
SCC=        ../../fileProxy/Code/jlmcrypto
BSC=        ../../fileProxy/Code/jlmbignum
TH=         ../../fileProxy/Code/tao
TRS=        .
TS=         ../../rptpm/TPMDirect
CYP=        ../../rpcrypto
DEBUG_CFLAGS     := -Wall  -Wno-format -g -DDEBUG
RELEASE_CFLAGS   := -Wall  -Wno-unknown-pragmas -Wno-format -O3
O1RELEASE_CFLAGS   := -Wall  -Wno-unknown-pragmas -Wno-format -O1
CFLAGS=     -D LINUX  -D TEST -D TIXML_USE_STL -D __FLUSHIO__ -D RPMMIO $(DEBUG_CFLAGS)
LDFLAGS          := $(RELEASE_LDFLAGS)
O1CFLAGS=    -D LINUX -D TEST -D TIXML_USE_STL -D __FLUSHIO__ $(O1RELEASE_CFLAGS)

TBOOTINCLUDE= ../../fileProxy/tboot-1.7.3/tboot/include/ 
TBOOTINCLUDE2=../../fileProxy/tboot-1.7.3/include
SHA1SRC=../../fileProxy/tboot-1.7.3/tboot/common

CC=         g++
LINK=       g++

dobjs=      $(OBJ)/tpmextend.o

all: $(BIN)/tpmextend

$(BIN)/tpmextend: $(dobjs)
	@echo "tpmextend"
	$(LINK) -o $(BIN)/tpmextend $(dobjs) $(LDFLAGS) -lpthread -L$(LIB) -lrptpm-dir-g -lrpcrypto-g

$(OBJ)/tpmextend.o: $(TRS)/tpmextend.cpp 
	$(CC) $(CFLAGS) -I$(TAO_MODS) -I$(BSC) -I$(SCC) -I$(CYP) -I$(TRS) -I$(SC) -I$(TS) -c -o $(OBJ)/tpmextend.o $(TRS)/tpmextend.cpp

clean:
	rm -rf $(OBJ)/tpmextend.o $(BIN)/tpmextend

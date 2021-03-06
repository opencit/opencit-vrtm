RPROOT=../..

ifeq ($(debug),1)
	DEBUG_CFLAGS     := -Wall  -Wno-format -g -DDEBUG
else
        DEBUG_CFLAGS     := -Wall -Wno-unknown-pragmas -Wno-format -O3 -Wformat -Wformat-security
endif

BIN=        $(RPROOT)/bin
OBJ=        $(RPROOT)/build/vrtmcoreobjects
LIB=        $(RPROOT)/lib
INC=        $(RPROOT)/inc

UTIL=       ../util
SC=         ../util
CH=         ../vrtmchannel
TM=	    	../vrtmcore
LXML=		/usr/include/libxml2/
OPENSSL=    	/usr/include/openssl/
LOG4CPP=	/usr/include/log4cpp/
SAFESTRING=     ../util/SafeStringLibrary/
SAFESTRING_INCLUDE=    $(SAFESTRING)/include/

#DEBUG_CFLAGS     := -Wall  -Wno-format -g -DDEBUG
#RELEASE_CFLAGS   := -Wall  -Wno-unknown-pragmas -Wno-format -O3
O1RELEASE_CFLAGS   := -Wall  -Wno-unknown-pragmas -Wno-format -O1
RELEASE_LDFLAGS  := -pie -z noexecstack -z relro -z now
LDFLAGS          := ${RELEASE_LDFLAGS}
CFLAGS=    -fPIE -fPIC -fstack-protector-strong -O2 -D FORTIFY_SOURCE=2 -D TPMSUPPORT -D QUOTE2_DEFINED -D TEST -D TEST1 -D TCSERVICE -D __FLUSHIO__ $(DEBUG_CFLAGS) -DNEWANDREORGANIZED -D TPMTEST -DTEST_SEG -DCSR_REQ
#CFLAGS=     -D TPMSUPPORT -D TEST -D QUOTE2_DEFINED -D TCSERVICE -D __FLUSHIO__ $(RELEASE_CFLAGS) -DNEWANDREORGANIZED
O1CFLAGS=    -D TPMSUPPORT -D QUOTE2_DEFINED -D TEST -D __FLUSHIO__ $(O1RELEASE_CFLAGS)

CC=         g++
LINK=       g++

sobjs=    	$(OBJ)/vrtmcoremain.o $(OBJ)/vrtminterface.o \
            $(OBJ)/modtcService.o $(OBJ)/logging.o \
            $(OBJ)/vrtm_listener.o \
            $(OBJ)/vrtmCommon.o
			
#	    $(OBJ)/dombuilder.o $(OBJ)/tcpchan.o

all: $(BIN)/vrtmcore

$(BIN)/vrtmcore: $(sobjs)
	@echo "vrtmcoreservice"
	$(LINK) -o $(BIN)/vrtmcore $(sobjs) $(LDFLAGS) -L$(LIB) -L$(SAFESTRING) -L/usr/local/lib/ -lvrtmchannel -lpthread -lxml2 -llog4cpp -lvirt -lssl -lcrypto -lSafeStringRelease
ifneq "$(debug)" "1"
	strip -s $(BIN)/vrtmcore
endif

#$(LINK) -o $(BIN)/rpcoreservice $(sobjs) $(LDFLAGS) -lxenlight -lxlutil -lxenctrl -lxenguest -lblktapctl -lxenstore -luuid -lutil -lpthread -L$(LIB) -lrpdombldr-g

$(OBJ)/vrtmCommon.o: $(SC)/vrtmCommon.cpp $(SC)/vrtmCommon.h
	$(CC) $(CFLAGS) -I$(SC) -I$(LOG4CPP) -c -o $(OBJ)/vrtmCommon.o $(SC)/vrtmCommon.cpp

$(OBJ)/logging.o: $(SC)/logging.cpp $(SC)/logging.h 
	$(CC) $(CFLAGS) -I$(SC) -I$(LOG4CPP) -I$(SAFESTRING_INCLUDE) -c -o $(OBJ)/logging.o $(SC)/logging.cpp

$(OBJ)/vrtmcoremain.o: $(TM)/vrtmcoremain.cpp
	@echo $(debug)
	$(CC)  $(CFLAGS) -I$(UTIL) -I$(SC) -I$(LOG4CPP) -I$(CH) -I$(SAFESTRING_INCLUDE) -c -o $(OBJ)/vrtmcoremain.o $(TM)/vrtmcoremain.cpp

$(OBJ)/vrtminterface.o: $(TM)/vrtminterface.cpp 
	$(CC) $(CFLAGS)  -I$(CH) -I$(UTIL)   -I$(SC) -I$(LOG4CPP) -I$(SAFESTRING_INCLUDE) -c -o $(OBJ)/vrtminterface.o $(TM)/vrtminterface.cpp

#original tcService
$(OBJ)/modtcService.o: $(TM)/modtcService.cpp
	$(CC) $(CFLAGS)  -I$(LXML) -I$(OPENSSL) -I$(UTIL) -I$(CH) -I$(SC) -I$(LOG4CPP) -I$(SAFESTRING_INCLUDE) -c -o $(OBJ)/modtcService.o $(TM)/modtcService.cpp


$(OBJ)/vrtm_listener.o: $(TM)/vrtm_listener.cpp
	$(CC) $(CFLAGS) -I$(SC) -I$(UTIL) -I$(LOG4CPP) -I$(CH) -I$(SAFESTRING_INCLUDE) -c -o $(OBJ)/vrtm_listener.o $(TM)/vrtm_listener.cpp

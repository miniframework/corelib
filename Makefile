WORKROOT=../
LIBNAME = minicore


BIN_DIR = bin
SRC_DIR = src
LIB_DIR = lib

INCLUDE = -I./include 

INSTALL_PATH=$(WORKROOT)lib/minilib

SOURCES = $(wildcard $(SRC_DIR)/*.cpp) 
OBJS = $(patsubst %.cpp,%.o,$(SOURCES))


COMMON_DEFINES = -DLINUX -D_REENTERANT
GCC4_IGNORE_WARNINGS= -Wno-write-strings


ifeq "$(MAKECMDGOALS)" "release"
	DEFINES=$(COMMON_DEFINES) -DNDEBUG
	CXXFLAGS= -fPIC -O2 -fsigned-char -Wall -W $(DEFINES) $(INCLUDE) $(GCC4_IGNORE_WARNINGS)
else
    ifeq "$(MAKECMDGOALS)" "withpg"
	DEFINES=$(COMMON_DEFINES)
	CXXFLAGS= -fPIC -g -pg -fsigned-char -Wall -W $(DEFINES) $(INCLUDE) $(GCC4_IGNORE_WARNINGS)
    else
	DEFINES=$(COMMON_DEFINES)
	CXXFLAGS= -fPIC -g -fsigned-char -Wall -W $(DEFINES) $(INCLUDE) $(GCC4_IGNORE_WARNINGS)
    endif
endif


CXX = g++ 
AR  = ar


OUTPUT_LIB=lib$(LIBNAME).a
OUTPUT_PATH=./output



all: clean outputdir

outputdir: output
	if [ ! -d $(OUTPUT_PATH) ]; then mkdir $(OUTPUT_PATH); fi
	if [ ! -d $(OUTPUT_PATH)/lib ]; then mkdir $(OUTPUT_PATH)/lib; fi
	if [ ! -d $(OUTPUT_PATH)/include ]; then mkdir $(OUTPUT_PATH)/include; fi
	cp lib/$(OUTPUT_LIB) $(OUTPUT_PATH)/lib
	cp include/*.h $(OUTPUT_PATH)/include
	rm -f $(OBJS)


output: $(OBJS) 
	if [ ! -d $(LIB_DIR) ]; then mkdir $(LIB_DIR); fi
	ar -ruv $(LIB_DIR)/$(OUTPUT_LIB) $(OBJS)

$(OBJS): %.o: %.cpp
	$(CXX) -c $< -o $@ $(INCLUDE) $(LIB) $(CXXFLAGS)


clean:
	rm -f $(OBJS) $(LIB_DIR)/*.a
	if [ -d $(OUTPUT_PATH) ]; then rm  -r $(OUTPUT_PATH); fi


install: outputdir
	if [ ! -d $(INSTALL_PATH) ]; then mkdir -p $(INSTALL_PATH); fi
	cp -rf $(OUTPUT_PATH)/* $(INSTALL_PATH)

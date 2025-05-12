SHELL = bash

DEPEND = $() CollectionsPlus Try
DLL_BIN = ../Bin
BIN = Bin
SOURCE = Source/*.c
TESTS = Tests/*.c
NAME = Config

DLL := $(DLL_BIN)/lib$(NAME).dll
TESTS_EXE := $(BIN)/Tests.exe
RUN := $(TESTS_EXE)

HEADERS_WILDCARD = ../*/Header
HEADERS := $(subst $() , -I , $(wildcard $(HEADERS_WILDCARD)))

Debug: COMPILE_FLAGS = -g
Debug: Compile

Release: COMPILE_FLAGS = -s
Release: Compile

Debugger: RUN = gdb $(TESTS_EXE)
Debugger: Debug

Compile: $(DLL) $(TESTS_EXE)
	$(RUN)

$(DLL): $(SOURCE) $(HEADERS_WILDCARD)/*.h
	gcc $(COMPILE_FLAGS) -fPIC -shared $(SOURCE) $(HEADERS) -L$(DLL_BIN) $(subst $() , -l,$(DEPEND)) -o $(DLL)

$(TESTS_EXE): $(DLL) $(TESTS) $(HEADERS_WILDCARD)/*.h
	gcc $(COMPILE_FLAGS) $(TESTS) $(HEADERS) -L $(DLL_BIN) -l$(NAME) $(subst $() , -l,$(DEPEND)) -o $(TESTS_EXE)

Clean:
	rm $(TESTS_EXE) $(DLL)

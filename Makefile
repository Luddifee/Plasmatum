FILES = $(wildcard src/*.cc)
OBJ_FILES = $(FILES:.cc=.o)
COMPILER = clang++

CXXFLAGS = `llvm-config --cxxflags` -O3
LDFLAGS = `llvm-config --ldflags --system-libs --libs all` -flto -lLLVM

all: bin/plsm

bin/plsm: $(OBJ_FILES)
	mkdir -p bin
	$(COMPILER) $(OBJ_FILES) $(LDFLAGS) -o bin/plsm
	strip bin/plsm

%.o: %.cc
	$(COMPILER) $(CXXFLAGS) -o $@ -c $<


clean:
	rm -rf bin $(OBJ_FILES)

rebuild: clean bin/plsm

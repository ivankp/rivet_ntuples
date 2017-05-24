STD := -std=c++14
DF := $(STD) -Isrc
CF := $(STD) -Wall -Isrc
LF := $(STD)

ifeq (0, $(words $(findstring $(MAKECMDGOALS), rel)))
# development mode
CF += -O2 -g -fmax-errors=3
else
# release mode
CF += -O3 -flto -funroll-loops -march=native
LF += -flto
endif

NPROC := $(shell nproc --all)

# ROOT_CFLAGS := $(shell root-config --cflags)
ROOT_CFLAGS := -Wno-deprecated-declarations -pthread -m64
ROOT_CFLAGS += -I$(shell root-config --incdir)
ROOT_LIBS   := $(shell root-config --libs)

# RPATH
rpath_script := ldd `root-config --libdir`/libTreePlayer.so \
  | sed -n 's/.*=> \(.*\)\/.\+\.so[^ ]* (.*/\1/p' \
  | sort | uniq \
  | sed '/^\/lib/d;/^\/usr\/lib/d' \
  | sed 's/^/-Wl,-rpath=/'
ROOT_LIBS += $(shell $(rpath_script))

# FJ_DIR    := $(shell fastjet-config --prefix)
# FJ_CFLAGS := -I$(FJ_DIR)/include
# FJ_LIBS   := -L$(FJ_DIR)/lib -lfastjet -Wl,-rpath=$(FJ_DIR)/lib

# LHAPDF_DIR    := $(shell fastjet-config --prefix)
# LHAPDF_CFLAGS := $(shell lhapdf-config --cppflags)
# LHAPDF_LIBS   := $(shell lhapdf-config --ldflags) -Wl,-rpath=$(LHAPDF_DIR)/lib

RIVET_CFLAGS := $(shell rivet-config --cppflags)
RIVET_LIBS   := -lHepMC $(shell rivet-config --libs | sed 's/-L\/usr\/[^ ]\+ //g')

C_hj_rivet := $(ROOT_CFLAGS) $(RIVET_CFLAGS)
L_hj_rivet := $(ROOT_LIBS) -lTreePlayer $(RIVET_LIBS)

C_Higgs2diphoton := $(ROOT_CFLAGS)

SRC := src
BIN := bin
BLD := .build

SRCS := $(shell find $(SRC) -type f -name '*.cc')
DEPS := $(patsubst $(SRC)%.cc,$(BLD)%.d,$(SRCS))

GREP_EXES := grep -rl '^ *int \+main *(' $(SRC)
EXES := $(patsubst $(SRC)%.cc,$(BIN)%,$(filter %.cc,$(shell $(GREP_EXES))))

NODEPS := clean
.PHONY: all clean

all: $(EXES)
rel: $(EXES)

#Don't create dependencies when we're cleaning, for instance
ifeq (0, $(words $(findstring $(MAKECMDGOALS), $(NODEPS))))
-include $(DEPS)
endif

$(BIN)/hj_rivet: $(BLD)/Higgs2diphoton.o

$(DEPS): $(BLD)/%.d: $(SRC)/%.cc | $(BLD)
	$(CXX) $(DF) -MM -MT '$(@:.d=.o)' $< -MF $@

$(BLD)/%.o: | $(BLD)
	$(CXX) $(CF) $(C_$*) -c $(filter %.cc,$^) -o $@

$(EXES): $(BIN)/%: $(BLD)/%.o | $(BIN)
	$(CXX) $(LF) $(filter %.o,$^) -o $@ $(L_$*)

$(BLD) $(BIN):
	mkdir $@

clean:
	@rm -rfv $(BLD) $(BIN)

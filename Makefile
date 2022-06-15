CXXFLAGS = $(shell geant4-config --cflags)
LDLIBS = $(shell geant4-config --libs) -lxerces-c

# Enable HDF5 if it is enabled in GEANT4
ifeq ($(shell geant4-config --has-feature hdf5),yes)
  CPPFLAGS += -DGEANT4_USE_HDF5
endif

# set rpath on MacOS
ifeq ($(shell uname),Darwin)
  CXXFLAGS += -Wl,-rpath,$(shell geant4-config --prefix)/lib
endif

.PHONY: all clean

all: g4simple

g4simple: g4simple.cc
	@echo building g4simple...
	@$(CXX) $(CXXFLAGS) -o g4simple g4simple.cc $(LDLIBS)

clean:
	@rm -f g4simple

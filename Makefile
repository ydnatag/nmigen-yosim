
YOSYS_PATH ?= $(PWD)/yosys

simulation: adder.il pywrapper.cc $(YOSYS_PATH)/yosys
	$(YOSYS_PATH)/yosys -q adder.il -o adder.cc
	clang++ -I$(YOSYS_PATH)/share/include/backends/cxxrtl $(shell python3-config --cflags) -shared -o simulation.so pywrapper.cc -fPIC

$(YOSYS_PATH)/yosys:
	make -C $(YOSYS_PATH)

	

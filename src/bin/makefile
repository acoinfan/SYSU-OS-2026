include ../../build/config.mk

CPP_SRCS := $(wildcard *.cpp)
CPP_OBJS := $(patsubst %.cpp,$(OBJDIR)/bin/%.o,$(CPP_SRCS))
ELF_TARGETS := $(patsubst %.cpp,$(RUNDIR)/root/bin/%,$(CPP_SRCS))

.PHONY: all clean

all: $(ELF_TARGETS)

$(OBJDIR)/bin/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(USER_INCLUDE) -c $< -o $@

$(RUNDIR)/root/bin/%: $(OBJDIR)/bin/%.o $(LIBC_LIB)
	@mkdir -p $(dir $@)
	$(LINKER) -melf_i386 -N -Ttext 0x08048000 -e _start -o $@ $< $(LIBC_LIB)

clean:
	rm -rf $(OBJDIR)/bin
	rm -f $(ELF_TARGETS)

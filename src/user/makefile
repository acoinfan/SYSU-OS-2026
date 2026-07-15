include ../../build/config.mk

CPP_SRCS := $(wildcard *.cpp)
CPP_OBJS := $(patsubst %.cpp,$(OBJDIR)/user/%.o,$(CPP_SRCS))
ELF_TARGETS := $(patsubst %.cpp,$(RUNDIR)/user/%,$(CPP_SRCS))

.PHONY: all clean

all: $(ELF_TARGETS)
	@mkdir -p $(RUNDIR)/user
	@for file in $(RUNDIR)/user/*; do \
		if [ -f "$$file" ] && [ ! -f "$(CURDIR)/$$(basename "$$file").cpp" ]; then \
			rm -f "$$file"; \
		fi; \
	done

$(OBJDIR)/user/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(USER_INCLUDE) -c $< -o $@

$(RUNDIR)/user/%: $(OBJDIR)/user/%.o $(LIBC_LIB)
	@mkdir -p $(dir $@)
	$(LINKER) -melf_i386 -N -Ttext 0x08048000 -e _start -o $@ $< $(LIBC_LIB)

clean:
	rm -rf $(OBJDIR)/user
	rm -f $(ELF_TARGETS)

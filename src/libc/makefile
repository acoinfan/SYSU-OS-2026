include ../../build/config.mk

CPP_SRCS := $(wildcard *.cpp)
ASM_SRCS := $(wildcard *.asm)

CPP_OBJS := $(patsubst %.cpp,$(OBJDIR)/libc/%.o,$(CPP_SRCS))
ASM_OBJS := $(patsubst %.asm,$(OBJDIR)/libc/%.o,$(ASM_SRCS))

OBJS := $(CPP_OBJS) $(ASM_OBJS)

TARGET := $(LIBDIR)/libc.a

.PHONY: all clean

all: $(TARGET)

# build lib
$(TARGET): $(OBJS)
	@mkdir -p $(LIBDIR)
	$(AR) $(ARFLAGS) $@ $^

# CXX
$(OBJDIR)/libc/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(LIBC_INCLUDE) -c $< -o $@

# ASM
$(OBJDIR)/libc/%.o: %.asm
	@mkdir -p $(dir $@)
	$(ASM_COMPILER) $(ASM_FLAGS) #(LIBC_INCLUDE) -o $@ $<

clean:
	rm -rf $(OBJDIR)/libc
	rm -f $(TARGET)
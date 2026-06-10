include ../../build/config.mk

CPP_SRCS := $(wildcard *.cpp)
ASM_SRCS := $(wildcard *.asm)

CPP_OBJS := $(patsubst %.cpp,$(OBJDIR)/builtin/%.o,$(CPP_SRCS))
ASM_OBJS := $(patsubst %.asm,$(OBJDIR)/builtin/%.o,$(ASM_SRCS))

OBJS := $(CPP_OBJS) $(ASM_OBJS)

TARGET := $(LIBDIR)/builtin.a

.PHONY: all clean

all: $(TARGET)

# build lib
$(TARGET): $(OBJS)
	@mkdir -p $(LIBDIR)
	$(AR) $(ARFLAGS) $@ $^

# CXX
$(OBJDIR)/builtin/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(BUILTIN_INCLUDE) -c $< -o $@

# ASM
$(OBJDIR)/builtin/%.o: %.asm
	@mkdir -p $(dir $@)
	$(ASM_COMPILER) $(ASM_FLAGS) -o $@ $<

clean:
	rm -rf $(OBJDIR)/builtin
	rm -f $(TARGET)
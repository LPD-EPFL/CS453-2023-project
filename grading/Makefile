BIN := ./$(notdir $(lastword $(abspath .)))

EXT_H    := h
EXT_HPP  := h hh hpp hxx h++
EXT_C    := c
EXT_CXX  := C cc cpp cxx c++

INCLUDE_DIRS := ../include .
SOURCE_DIRS  := .

WILD_EXT  = $(strip $(foreach EXT,$($(1)),$(wildcard $(2)/*.$(EXT))))

HDRS_C   := $(foreach INCLUDE_DIR,$(INCLUDE_DIRS),$(call WILD_EXT,EXT_H,$(INCLUDE_DIR)))
HDRS_CXX := $(foreach INCLUDE_DIR,$(INCLUDE_DIRS),$(call WILD_EXT,EXT_HPP,$(INCLUDE_DIR)))
SRCS_C   := $(foreach SOURCE_DIR,$(SOURCE_DIRS),$(call WILD_EXT,EXT_C,$(SOURCE_DIR)))
SRCS_CXX := $(foreach SOURCE_DIR,$(SOURCE_DIRS),$(call WILD_EXT,EXT_CXX,$(SOURCE_DIR)))
OBJS     := $(SRCS_C:%=%.o) $(SRCS_CXX:%=%.o)

CC       := $(CC)
CCFLAGS  := -Wall -Wextra -Wfatal-errors -O2 -std=c11 $(foreach INCLUDE_DIR,$(INCLUDE_DIRS),-I$(INCLUDE_DIR))
CXX      := $(CXX)
CXXFLAGS := -Wall -Wextra -Wfatal-errors -O2 -std=c++17 $(foreach INCLUDE_DIR,$(INCLUDE_DIRS),-I$(INCLUDE_DIR))
LD       := $(if $(SRCS_CXX),$(CXX),$(CC))
LDFLAGS  :=
LDLIBS   := -ldl -lpthread

LIB_DIRS := $(filter-out ../include/ ../grading/ ../playground/ ../template/,$(filter-out $(wildcard ../*),$(wildcard ../*/)))
LIB_SOS  := $(patsubst %/,%.so,$(filter-out ../reference/,$(LIB_DIRS)))

.PHONY: build build-libs clean clean-libs run

build: $(BIN)
build-libs:
	@$(foreach DIR,$(LIB_DIRS),make -C $(DIR) build; )
clean:
	$(RM) $(OBJS) $(BIN)
clean-libs:
	@$(foreach DIR,$(LIB_DIRS),make -C $(DIR) clean; )
run: $(BIN)
	$(BIN) 453 ../reference.so $(LIB_SOS)

define BUILD_C
%.$(1).o: %.$(1) $$(HDRS_C) Makefile
	$$(CC) $$(CCFLAGS) -c -o $$@ $$<
endef
$(foreach EXT,$(EXT_C),$(eval $(call BUILD_C,$(EXT))))

define BUILD_CXX
%.$(1).o: %.$(1) $$(HDRS_CXX) Makefile
	$$(CXX) $$(CXXFLAGS) -c -o $$@ $$<
endef
$(foreach EXT,$(EXT_CXX),$(eval $(call BUILD_CXX,$(EXT))))

$(BIN): $(OBJS) Makefile
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

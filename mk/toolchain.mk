##############################################
### Compiler Detection (clang/gcc supported)
##############################################
# BitInt is no longer required; allow both clang and gcc. Auto-detect a usable
# compiler when the user does not provide CXX.

$(info [gsim] Compiler detection: start)

# Record whether user explicitly set CXX (environment or command line)
ORIGIN_CXX := $(origin CXX)
$(info [gsim] CXX is setting by: $(ORIGIN_CXX))

# If user provided a C compiler name, switch to its C++ frontend for proper C++ linking.
ifneq ($(filter environment command line,$(ORIGIN_CXX)),)
  CXX_BASENAME := $(notdir $(firstword $(CXX)))
  ifeq ($(CXX_BASENAME),clang)
    ifneq ($(shell command -v clang++ >/dev/null 2>&1 && echo yes),)
      $(info [gsim] CXX=clang provided; switching to clang++ for C++ build)
      CXX := clang++
    endif
  endif
  ifeq ($(CXX_BASENAME),gcc)
    ifneq ($(shell command -v g++ >/dev/null 2>&1 && echo yes),)
      $(info [gsim] CXX=gcc provided; switching to g++ for C++ build)
      CXX := g++
    endif
  endif
  ifeq ($(CXX_BASENAME),cc)
    ifneq ($(shell command -v c++ >/dev/null 2>&1 && echo yes),)
      $(info [gsim] CXX=cc provided; switching to c++ for C++ build)
      CXX := c++
    endif
  endif
endif

# When CXX is not explicitly set, pick the first available compiler from the
# candidate list (clang preferred, then gcc).
ifeq ($(filter environment command line,$(ORIGIN_CXX)),)
  ifeq ($(strip $(CXX)),)
    CXX_CANDIDATES := clang++-19 clang++ g++ c++
    define pick_compiler
      ifeq ($(strip $(CXX)),)
        ifneq ($(shell command -v $(1) >/dev/null 2>&1 && echo yes),)
          CXX := $(1)
        endif
      endif
    endef
    $(foreach cc,$(CXX_CANDIDATES),$(eval $(call pick_compiler,$(cc))))
    ifeq ($(strip $(CXX)),)
      $(error No suitable C++ compiler found. Please install clang++ or g++, or set CXX to your compiler.)
    else
      $(info [gsim] Auto-selected CXX=$(CXX))
    endif
    ORIGIN_CXX := auto
  else
    $(info [gsim] Using default make CXX=$(CXX))
  endif
else
  ifeq ($(shell command -v $(firstword $(CXX)) >/dev/null 2>&1 && echo yes),)
    $(error CXX is set to '$(CXX)' but the compiler is not found. Please install it or update CXX.)
  endif
  $(info [gsim] CXX explicitly set by user; use as is)
endif

$(info [gsim] Using CXX=$(CXX) (origin: $(ORIGIN_CXX)))

# Read compiler version string (best-effort)
CXX_VERSION_STR := $(shell $(CXX) --version 2>/dev/null)
CXX_VERSION_FIRSTLINE := $(shell $(CXX) --version 2>/dev/null | head -n 1)
ifneq ($(strip $(CXX_VERSION_FIRSTLINE)),)
  $(info [gsim] $(CXX) version: $(CXX_VERSION_FIRSTLINE))
else
  $(warning Unable to query compiler version from $(CXX); build will continue.)
endif

# Detect compiler family
CXX_IS_CLANG := $(if $(findstring clang,$(CXX_VERSION_FIRSTLINE)),1,0)
CXX_IS_GCC := $(if $(findstring GCC,$(CXX_VERSION_FIRSTLINE)),1,$(if $(findstring g++,$(CXX_VERSION_FIRSTLINE)),1,0))

ifeq ($(CXX_IS_CLANG),1)
  CLANG_MAJOR := $(shell echo '$(CXX_VERSION_FIRSTLINE)' | sed -n 's/.*clang version \([0-9][0-9]*\).*/\1/p')
  ifneq ($(CLANG_MAJOR),)
    $(info [gsim] Detected clang major from CXX: $(CLANG_MAJOR))
  endif
endif

ifeq ($(CXX_IS_GCC),1)
  GCC_MAJOR := $(shell echo '$(CXX_VERSION_FIRSTLINE)' | sed -n 's/.* \([0-9][0-9]*\)\.[0-9]*\.[0-9]*/\1/p')
  ifneq ($(GCC_MAJOR),)
    $(info [gsim] Detected GCC major from CXX: $(GCC_MAJOR))
  endif
endif

$(info [gsim] Compiler detection: done)

.DEFAULT_GOAL := help

CORE_BUILD_DIR ?= build
ELUNA_DEV_DIR ?= build-eluna
ELUNA_FLAGS_FILE ?= $(CORE_BUILD_DIR)/modules/CMakeFiles/modules.dir/flags.make
ELUNA_OBJECT_DIR ?= $(ELUNA_DEV_DIR)/objects
CXX ?= c++

ELUNA_SOURCES := $(wildcard modules/mod-eluna/src/*.cpp)
ELUNA_OBJECTS := $(patsubst modules/mod-eluna/src/%.cpp,$(ELUNA_OBJECT_DIR)/%.o,$(ELUNA_SOURCES))

# Reuse the include paths and defines from the configured core module target.
# This keeps the fast check aligned with the real build without running a
# second dependency-discovery configure.
ifneq ($(wildcard $(ELUNA_FLAGS_FILE)),)
include $(ELUNA_FLAGS_FILE)
endif

# mod-dungeon-clear adds a force-include that is only valid for that module.
# Do not pull its playerbot compatibility prelude into Eluna's fast check.
ELUNA_CXX_FLAGS := $(filter-out -include %/modules/mod-dungeon-clear/src/AcCompat.h,$(CXX_FLAGS))

-include $(ELUNA_OBJECTS:.o=.d)

.PHONY: help dev-eluna eluna-dev-check

help:
	@printf '%s\n' \
		'Available targets:' \
		'  make dev-eluna  Compile only mod-eluna sources for fast diagnostics'

eluna-dev-check:
	@test -f "$(ELUNA_FLAGS_FILE)" || { \
		printf '%s\n' \
			"Missing $(ELUNA_FLAGS_FILE). Configure a normal module-enabled build first." \
			'Example: cmake -S . -B build -DMODULE_MOD_ELUNA=static'; \
		exit 1; \
	}
	@test -n "$(ELUNA_SOURCES)" || { \
		printf '%s\n' 'No C++ sources found under modules/mod-eluna/src.'; \
		exit 1; \
	}

dev-eluna: eluna-dev-check $(ELUNA_OBJECTS)

$(ELUNA_OBJECTS): | eluna-dev-check

$(ELUNA_OBJECT_DIR)/%.o: modules/mod-eluna/src/%.cpp
	@mkdir -p "$(@D)"
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(ELUNA_CXX_FLAGS) \
		-MMD -MP -MF "$(@:.o=.d)" -MT "$@" -c "$<" -o "$@"

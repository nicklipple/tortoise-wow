# Build Tortoise WoW without Docker.
#
# The defaults mirror the Docker builder, except that the install prefix is
# user-writable. Override any variable on the make command line, for example:
#
#   make CMAKE_INSTALL_PREFIX=/opt/turtle BUILD_JOBS=8

.DEFAULT_GOAL := all

CMAKE ?= cmake
APT ?= apt-get
SUDO ?= sudo
BUILD_DIR ?= build
CMAKE_BUILD_TYPE ?= Release
CMAKE_INSTALL_PREFIX ?= $(HOME)/turtle
# Ubuntu/Debian installs ACE below /usr; override this for a custom ACE build.
ACE_ROOT ?= /usr
BUILD_PLAYERBOTS ?= ON
USE_EXTRACTORS ?= ON
MODULE_MOD_DYNAMIC_XP ?= static
ALLOW_TURTLE_ADDONS ?= ON
BUILD_JOBS ?= $(shell nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || printf 1)

# ccache is used when it is installed, but is not required for local builds.
CCACHE ?= ccache
USE_CCACHE ?= auto
CCACHE_DIR ?= $(HOME)/.cache/ccache
export CCACHE_DIR

EXTRACTOR_TARGETS := mapextractor vmapextractor vmap_assembler MoveMapGen

CMAKE_ARGS := \
	-S . \
	-B "$(BUILD_DIR)" \
	-DCMAKE_BUILD_TYPE="$(CMAKE_BUILD_TYPE)" \
	-DCMAKE_INSTALL_PREFIX="$(CMAKE_INSTALL_PREFIX)" \
	-DBUILD_PLAYERBOTS="$(BUILD_PLAYERBOTS)" \
	-DMODULE_MOD_DYNAMIC_XP="$(MODULE_MOD_DYNAMIC_XP)" \
	-DUSE_EXTRACTORS="$(USE_EXTRACTORS)" \
	-DALLOW_TURTLE_ADDONS="$(ALLOW_TURTLE_ADDONS)"

ifneq ($(strip $(ACE_ROOT)),)
CMAKE_ARGS += -DACE_ROOT="$(ACE_ROOT)"
endif

ifeq ($(USE_CCACHE),auto)
ifneq ($(shell command -v "$(CCACHE)" 2>/dev/null),)
CMAKE_ARGS += -DCMAKE_C_COMPILER_LAUNCHER="$(CCACHE)" \
	-DCMAKE_CXX_COMPILER_LAUNCHER="$(CCACHE)"
endif
else ifneq ($(filter ON on YES yes TRUE true 1,$(USE_CCACHE)),)
CMAKE_ARGS += -DCMAKE_C_COMPILER_LAUNCHER="$(CCACHE)" \
	-DCMAKE_CXX_COMPILER_LAUNCHER="$(CCACHE)"
endif

.PHONY: all install-deps configure build install extractors extractors-only clean help

all: install

# Install the Ubuntu/Debian packages used by the Docker builder. This target is
# intentionally opt-in because it requires elevated privileges and apt.
install-deps:
	$(SUDO) $(APT) update
	$(SUDO) $(APT) install -y \
		build-essential \
		ca-certificates \
		cmake \
		git \
		libace-dev \
		libboost-all-dev \
		default-libmysqlclient-dev \
		libssl-dev \
		zlib1g-dev \
		libbz2-dev \
		ccache \
		pkg-config

configure:
	$(CMAKE) $(CMAKE_ARGS)

build: configure
	$(CMAKE) --build "$(BUILD_DIR)" --parallel "$(BUILD_JOBS)"

install: build
	$(CMAKE) --install "$(BUILD_DIR)" --config "$(CMAKE_BUILD_TYPE)"

# Build only the tools used to extract client data. The resulting binaries are
# left in the CMake build tree, ready to be run from the client directory.
extractors: configure
	$(CMAKE) --build "$(BUILD_DIR)" --parallel "$(BUILD_JOBS)" --target $(EXTRACTOR_TARGETS)

# Match the Dockerfile's EXTRACTORS_ONLY path by placing just the extractor
# binaries in the configured install prefix.
extractors-only: extractors
	mkdir -p "$(CMAKE_INSTALL_PREFIX)/bin"
	find "$(BUILD_DIR)" -type f -executable \( $(foreach target,$(EXTRACTOR_TARGETS),-name $(target) -o) -false \) -exec cp -a '{}' "$(CMAKE_INSTALL_PREFIX)/bin/" ';'

clean:
	$(CMAKE) -E rm -rf "$(BUILD_DIR)"

help:
	@printf '%s\n' \
		'Available targets:' \
		'  all             Configure, build, and install the server and extractors (default)' \
		'  install-deps    Install the Ubuntu/Debian build dependencies, including ACE' \
		'  configure       Configure the CMake build tree' \
		'  build           Build all configured targets' \
		'  install         Install the completed build' \
		'  extractors      Build mapextractor, vmapextractor, vmap_assembler, and MoveMapGen' \
		'  extractors-only Build extractors and copy them to the install prefix' \
		'  clean           Remove the CMake build tree' \
		'' \
		'Override BUILD_JOBS, CMAKE_BUILD_TYPE, CMAKE_INSTALL_PREFIX, BUILD_PLAYERBOTS,' \
		'USE_EXTRACTORS, MODULE_MOD_DYNAMIC_XP, ALLOW_TURTLE_ADDONS, ACE_ROOT, or USE_CCACHE.'

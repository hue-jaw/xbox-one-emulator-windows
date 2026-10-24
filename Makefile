# Build hote (PC) : parseur XDVDFS + extraction + cache + CLI de test.
# Le build console (XEX) est decrit dans docs/BUILD.md (Xbox 360 XDK / xkelib).

CC      ?= gcc
CXX     ?= g++
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?=

CORE_SRC := src/core/pathutil.c src/core/xiso.c src/core/cache.c \
            src/core/extract.c src/core/session.c
HOST_SRC := src/host/fs_posix.c src/host/iosrc_stdio.c src/host/cli.c
TEST_SRC := tests/test_core.c src/host/fs_posix.c src/host/iosrc_stdio.c

BIN  := build/xiboot-host
TEST := build/test-core

.PHONY: all test clean fixtures check-xbox

all: $(BIN)

$(BIN): $(CORE_SRC) $(HOST_SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) -I. -o $@ $^ $(LDFLAGS)

$(TEST): $(CORE_SRC) $(TEST_SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) -I. -o $@ $^ $(LDFLAGS)

fixtures: 
	python3 tools/make_fixtures.py

test: $(TEST)
	./$(TEST)

clean:
	rm -rf build

# ---------------------------------------------------------------------------
# Verification de compilation du code console (sans XDK).
# On compile src/xbox/*.c|*.cpp contre src/xbox/stub/xbox_stub.h.
# Ce n'est PAS un build XEX : voir docs/BUILD.md pour le vrai build console.
# ---------------------------------------------------------------------------
XBOX_C_SRC   := src/xbox/fs_xbox.c src/xbox/iosrc_xbox.c src/xbox/launch_xbox.c
XBOX_MAIN    := src/xbox/main.c
XBOX_CXX_SRC := src/xbox/ui_d3d.cpp
STUB_OBJ     := $(patsubst %.c,build/stub/%.o,$(patsubst %.cpp,build/stub/%.opp,$(XBOX_C_SRC) $(XBOX_MAIN) $(XBOX_CXX_SRC)))

check-xbox:
	@mkdir -p build/stub
	for f in $(XBOX_C_SRC) $(XBOX_MAIN); do \
	  echo "  CC(stub) $$f"; \
	  $(CC) -std=c99 -Wall -Wextra -Wno-unused-parameter -DXBOX_STUB \
	    -Isrc/core -o build/stub/$$(basename $$f .c).o -c $$f || exit 1; \
	done
	for f in $(XBOX_CXX_SRC); do \
	  echo "  CXX(stub) $$f"; \
	  $(CXX) -std=c++11 -Wall -Wextra -Wno-unused-parameter -DXBOX_STUB \
	    -Isrc/core -o build/stub/$$(basename $$f .cpp).o -c $$f || exit 1; \
	done
	@echo "OK : code console compilable (verification de types uniquement)"

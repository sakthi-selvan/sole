CXX      ?= g++
PREFIX   ?= $(HOME)/.local
BINDIR   ?= $(PREFIX)/bin
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -I src -I third_party
LDFLAGS  ?= -lX11 -lm

SRCS := src/main.cpp src/config.cpp src/font.cpp src/nvidia_api.cpp src/overlay.cpp
OBJS := $(SRCS:.cpp=.o)

.PHONY: all clean install uninstall autostart hotkey

all: overlay-chat

third_party/stb_truetype.h:
	mkdir -p third_party
	curl -fsSL -o third_party/stb_truetype.h https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h

overlay-chat: third_party/stb_truetype.h $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

src/%.o: src/%.cpp src/overlay.hpp src/font.hpp src/config.hpp src/nvidia_api.hpp src/utf8.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) overlay-chat

install: overlay-chat
	-pkill -x overlay-chat || true
	mkdir -p $(BINDIR)
	cp overlay-chat $(BINDIR)/overlay-chat
	chmod 755 $(BINDIR)/overlay-chat
	@echo "Installed $(BINDIR)/overlay-chat"
	@echo "START: $(BINDIR)/overlay-chat"
	@echo "END:   $(BINDIR)/overlay-chat --quit"
	-bash scripts/install-hotkey.sh

hotkey:
	bash scripts/install-hotkey.sh

uninstall:
	rm -f $(BINDIR)/overlay-chat
	rm -f $(HOME)/.config/autostart/overlay-chat.desktop
	@echo "Removed binary and autostart entry"

autostart: install
	bash scripts/install-autostart.sh

PROG = PowerPriceSyncronizer

-include power-price-syncronizer.env
export

BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj
SRC_DIR   := source

CXXFLAGS += -std=c++20
CXXFLAGS += -Wall

LDFLAGS += -pthread
LDFLAGS += -lcurl
LDFLAGS += -lpq
LDFLAGS += -lcjson

OBJS = $(addprefix $(OBJ_DIR)/, main.o helper_functions.o database_handler.o api_handler.o)

.PHONY: all
all:  $(BUILD_DIR)/$(PROG)

$(BUILD_DIR)/$(PROG): $(OBJS)
	$(CXX) $^ $(LDFLAGS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp 
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: run
run: $(BUILD_DIR)/$(PROG)
	./$(BUILD_DIR)/$(PROG)


SCRIPTS_DIR = scripts
install: $(BUILD_DIR)/$(PROG)
	mkdir -p $(DESTDIR)/etc/power-price-syncronizer
	install -m 600 power-price-syncronizer.env $(DESTDIR)/etc/power-price-syncronizer/power-price-syncronizer.env
	mkdir -p $(DESTDIR)/etc/systemd/system
	install -m 644 $(SCRIPTS_DIR)/power-price-syncronizer.service $(DESTDIR)/etc/systemd/system/power-price-syncronizer.service
	mkdir -p $(DESTDIR)/usr/bin
	install -m 755 $(BUILD_DIR)/$(PROG) $(DESTDIR)/usr/bin/power-price-syncronizer

ARCHITECTURE=$(shell dpkg --print-architecture)
PACKET_DIR=$(BUILD_DIR)/staging
VERSION_FILE = VERSION
PACKET_CONTROL = $(PACKET_DIR)/DEBIAN/control

# Read current version and increment build number
.PHONY: increment-version
increment-version:
	@if [ -f $(VERSION_FILE) ]; then \
		VERSION=$$(cat $(VERSION_FILE)); \
		MAJOR=$$(echo $$VERSION | cut -d. -f1); \
		MINOR=$$(echo $$VERSION | cut -d. -f2); \
		BUILD=$$(echo $$VERSION | cut -d. -f3); \
		BUILD=$$((BUILD + 1)); \
		echo "$$MAJOR.$$MINOR.$$BUILD" > $(VERSION_FILE); \
		echo "Version incremented to $$MAJOR.$$MINOR.$$BUILD"; \
	else \
		echo "1.0.0" > $(VERSION_FILE); \
		echo "Version file created: 1.0.0"; \
	fi

deb: $(BUILD_DIR)/$(PROG) increment-version
	$(eval PACKET_VERSION := $(shell cat $(VERSION_FILE)))
	install -D $(SCRIPTS_DIR)/control $(PACKET_CONTROL)
	install -m 755 -D $(SCRIPTS_DIR)/postinst $(PACKET_DIR)/DEBIAN/postinst  
	install -m 755 -D $(SCRIPTS_DIR)/prerm $(PACKET_DIR)/DEBIAN/prerm  
	install -m 755 -D $(SCRIPTS_DIR)/conffiles $(PACKET_DIR)/DEBIAN/conffiles
	$(MAKE) install DESTDIR=$(PACKET_DIR)
	sed -i 's|@PACKAGE@|$(PROG)|' $(PACKET_CONTROL)
	sed -i 's|@VERSION@|$(PACKET_VERSION)|' $(PACKET_CONTROL)
	sed -i 's|@ARCHITECTURE@|$(ARCHITECTURE)|' $(PACKET_CONTROL)
	dpkg-deb --build $(PACKET_DIR) $(BUILD_DIR)/$(PROG)-$(PACKET_VERSION).deb
	@echo "Package created: $(BUILD_DIR)/$(PROG)-$(PACKET_VERSION).deb"
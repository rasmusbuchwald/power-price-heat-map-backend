PROG = PowerPriceHeatMap

-include .env
export

BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj
SRC_DIR   := source

CXXFLAGS += -std=c++17
CXXFLAGS += -Wall

LDFLAGS += -pthread
LDFLAGS += -lcurl
LDFLAGS += -lpq
LDFLAGS += -lcjson

OBJS = $(addprefix $(OBJ_DIR)/, main.o)

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


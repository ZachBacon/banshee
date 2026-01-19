# Compiler settings
CC = cc
CFLAGS = -Wall -Wextra -std=gnu11 -O2
LDFLAGS =

# Package config for dependencies
PKG_CONFIG = pkg-config
PACKAGES = gtk4 gstreamer-1.0 gstreamer-video-1.0 gstreamer-pbutils-1.0 gstreamer-tag-1.0 glib-2.0 sqlite3 libxml-2.0 libcurl json-glib-1.0

# Get flags from pkg-config
CFLAGS += $(shell $(PKG_CONFIG) --cflags $(PACKAGES))
LDFLAGS += $(shell $(PKG_CONFIG) --libs $(PACKAGES))

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

# Source and object files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))

# Target executable
TARGET = $(BUILD_DIR)/banshee

# Include directories
INCLUDES = -I$(INC_DIR)

# Default target
all: $(TARGET)

# Create build directories
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Link executable
$(TARGET): $(OBJECTS) | $(BUILD_DIR)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	@echo "Build complete: $(TARGET)"

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	@echo "Clean complete"

# Install (optional)
install: $(TARGET)
	install -D -m 755 $(TARGET) /usr/local/bin/banshee
	@echo "Installation complete"

# Uninstall (optional)
uninstall:
	rm -f /usr/local/bin/banshee
	@echo "Uninstallation complete"

# Run the application
run: $(TARGET)
	./$(TARGET)

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: clean all

# Help
help:
	@echo "Banshee Media Player - Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build the application (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install to /usr/local/bin"
	@echo "  uninstall - Remove from /usr/local/bin"
	@echo "  run       - Build and run the application"
	@echo "  debug     - Build with debug symbols"
	@echo "  help      - Show this help message"

.PHONY: all clean install uninstall run debug help

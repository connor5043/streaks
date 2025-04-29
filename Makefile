# Name of the source file and output executable
SRC = streaks.c
OUT = streaks

# Detect the operating system
UNAME := $(shell uname)

# Determine the installation directory based on the OS
ifeq ($(UNAME), Darwin)
  INSTALL_DIR = $(HOME)/Library/Application\ Support/bin
else
  INSTALL_DIR = $(HOME)/.local/bin
endif

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Wno-format-truncation -O2

.PHONY: all clean install uninstall

# Default target: Build the program
all: $(OUT)

# Compile the program
$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

# Install the program to the specified directory
install: $(OUT)
	@mkdir -p $(INSTALL_DIR)
	@cp $(OUT) $(INSTALL_DIR)/
	@echo "Installed $(OUT) to $(INSTALL_DIR)"

# Uninstall the program
uninstall:
	@rm -f $(INSTALL_DIR)/$(OUT)
	@echo "Uninstalled $(OUT) from $(INSTALL_DIR)"

# Clean up build artifacts
clean:
	@rm -f $(OUT)
	@echo "Cleaned up build artifacts"

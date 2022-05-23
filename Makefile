QUIET = @

ifeq ($(ENV),x86)
$(info Current ENV:$(ENV))
else ifeq ($(ENV),jetson)
$(info Current ENV:$(ENV))
else ifeq ($(ENV),nuvoton)
$(info Current ENV:$(ENV))
else ifeq ($(ENV),sunix)
$(info Current ENV:$(ENV))
else
$(error please set environment variable ENV)
endif

CC = g++
GCC_GXX_WARNINGS = -Wall -Wno-error -Wno-packed -Wpointer-arith -Wredundant-decls -Wstrict-aliasing=3 -Wswitch-enum -Wundef -Wwrite-strings -Wextra -Wno-unused-parameter
CFLAGS = -Os -DDEBUG
LDFLAGS = -lm

BUILD_INFO_INCLUDE_FILE = $(PWD)/src/Util/build_info.h
BUILD_DATE := $(shell date '+%Y%m%d-%H%M%S')
BUILD_VERSION := '1.00.2003'

BUILD_DIR = build
SOURCE_DIR = src
CPP_SOURCES = $(wildcard $(SOURCE_DIR)/*.cpp) $(wildcard $(SOURCE_DIR)/Util/*.cpp)
CPP_OBJS = $(addprefix $(BUILD_DIR)/, $(patsubst %.cpp, %.o, $(CPP_SOURCES)))

PLUGIN_SDK = dep/linux-plugin-sdk
CLIB = $(PLUGIN_SDK)/lib/libadmplugin.a \
	-lrt -lcrypto -lpthread -lboost_system -lboost_chrono -lboost_random -lssl
CINC = -Idep/websocketpp/include -I$(SOURCE_DIR) \
	-I$(PLUGIN_SDK)/include -I$(PLUGIN_SDK)/dep/cJSON/include -I$(PLUGIN_SDK)/dep/phc-winner-argon2/include

OUTPUT_PACKAGE_SUFFIX = _$(ENV)
OUTPUT_PACKAGE_SOURCE = output_package$(OUTPUT_PACKAGE_SUFFIX)

# Check if output_package only one subdirectory
APP_GUID_DIR=$(shell find $(OUTPUT_PACKAGE_SOURCE) -mindepth 1 -maxdepth 1 -type d)
ifeq ($(shell echo $(APP_GUID_DIR) | wc -l),1)
	APP_GUID = $(notdir $(APP_GUID_DIR))
else 
$(error output_package folder structure is not valid)
endif

$(info APP_GUID: $(APP_GUID))
TEMP_OUTPUT_PACKAGE = $(APP_GUID)

TARGET_NAME = plugIN-upterm
TARGET_BINARY = $(BUILD_DIR)/$(TARGET_NAME) $(OUTPUT_PACKAGE_SOURCE)/$(APP_GUID)/$(TARGET_NAME)

default: init build
	$(info ###### Compile $(CPP_OBJS) $(C_OBJS)done!! ######)

build: $(TARGET_BINARY)

$(TARGET_BINARY): $(CPP_OBJS)
	$(CC) -no-pie $^ -o $@ $(LDFLAGS) $(CLIB)

$(BUILD_DIR)/%.o: %.cpp
	$(info Compiling $<)
	$(QUIET)echo "$(CC) $(CFLAGS) -DLINUX $(GCC_GXX_WARNINGS) -c -o2 $(CINC) $< -o $@"
	$(QUIET)$(CC) $(CFLAGS) -DLINUX $(GCC_GXX_WARNINGS) -c -o2 $(CINC) $< -o $@

.PHONY: clean
clean:
	$(QUIET)rm -rf $(BUILD_DIR) $(TARGET_BINARY)

init:
	echo '#define BUILD_INFO "'$(BUILD_VERSION)_$(BUILD_DATE)'"' > $(BUILD_INFO_INCLUDE_FILE);
	mkdir -p $(BUILD_DIR)/src/Util

.PHONY: package
package: $(TARGET_BINARY) $(OUTPUT_PACKAGE_SOURCE)
	test ! -d $(TEMP_OUTPUT_PACKAGE) || rm -rf $(TEMP_OUTPUT_PACKAGE)
	$(QUIET)cp -r $(OUTPUT_PACKAGE_SOURCE) $(TEMP_OUTPUT_PACKAGE)/
	$(QUIET)cd $(TEMP_OUTPUT_PACKAGE); tar -czf $(PWD)/$(APP_GUID).tar.gz .
	$(QUIET)rm -rf $(TEMP_OUTPUT_PACKAGE)
	$(info The $(TARGET_NAME) app related files are packaged to ./$(APP_GUID).tar.gz)

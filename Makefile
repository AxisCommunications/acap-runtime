INSTALL ?= install
RM ?= rm -f
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

# Paths
#OUT_PATH ?= $(CURDIR)/build_$(shell $(CXX) -dumpmachine)
OUT_PATH ?= $(CURDIR)
API_PATH := $(CURDIR)/apis
SRC_PATH := $(CURDIR)/src
TEST_PATH := $(CURDIR)/test
INSTALL_PATH := $(DESTDIR)/usr/bin
GRPC_CPP_PLUGIN := grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= $(shell which $(GRPC_CPP_PLUGIN))

# Output binary name matches the repository name
BINARY := $(subst -,_,$(shell basename -s .git $$(git config --get remote.origin.url)))
TEST := $(addsuffix _test, $(BINARY))

# Build files
PROTOBUF_FILES := $(call rwildcard, $(API_PATH),*.proto)
PROTOBUF_H := $(patsubst %.proto,%.pb.h,$(patsubst $(API_PATH)%,$(OUT_PATH)%,$(PROTOBUF_FILES)))
PROTOBUF_O := $(patsubst %.pb.h,%.pb.o,$(PROTOBUF_H))
PROTOBUF_GRPC_O := $(patsubst %.pb.h,%.grpc.pb.o,$(PROTOBUF_H))
SRC_FILES := $(wildcard $(SRC_PATH)/*.cpp $(SRC_PATH)/*.cc)
TEST_FILES := $(wildcard $(TEST_PATH)/*.cpp $(TEST_PATH)/*.cc)

# Compiler flags
PKGS = protobuf grpc grpc++
PKG_CONFIG_CFLAGS_I := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_LIBDIR) pkg-config --cflags-only-I $(PKGS))
PKG_CONFIG_CFLAGS_OTHER := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_LIBDIR) pkg-config --cflags-only-other $(PKGS))
PKG_CONFIG_LDFLAGS := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_LIBDIR) pkg-config --libs-only-L $(PKGS))
PKG_CONFIG_LDLIBS := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_LIBDIR) pkg-config --libs-only-l $(PKGS))
CXXFLAGS += -DLAROD_API_VERSION_2 -std=c++17 -I$(OUT_PATH) $(PKG_CONFIG_CFLAGS_OTHER) $(PKG_CONFIG_CFLAGS_I)
LDLIBS   += -llarod -lrt $(PKG_CONFIG_LDLIBS)
LDFLAGS  += $(PKG_CONFIG_LDFLAGS)

.PHONY: clean install install/strip

# Do not remove intermediate files
.SECONDARY:

all: install/strip

# Main binary
$(OUT_PATH)/$(BINARY): $(SRC_FILES) $(PROTOBUF_H) $(PROTOBUF_O) $(PROTOBUF_GRPC_O)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) \
	-I$(OUT_PATH)/tensorflow_serving/apis \
	-o $@ src/main.c $(SRC_FILES) $(PROTOBUF_O) $(PROTOBUF_GRPC_O) $(LDLIBS)

# Test binary -fsanitize=leak
$(OUT_PATH)/$(TEST): $(TEST_FILES) $(SRC_FILES) $(PROTOBUF_H) $(PROTOBUF_O) $(PROTOBUF_GRPC_O)
	$(CXX) -g $(CXXFLAGS) $(LDFLAGS) \
	-I$(SRC_PATH) \
	-I$(OUT_PATH)/tensorflow_serving/apis \
	-I/usr/src/googletest/googletest/include \
	-I/usr/src/googletest/googlemock/include \
	-o $@ $(TEST_FILES) $(SRC_FILES) $(PROTOBUF_O) $(PROTOBUF_GRPC_O) -lgtest_main -lgtest  $(LDLIBS)

# Build directory
$(OUT_PATH) $(INSTALL_PATH):
	$(INSTALL) -d $@

# Protobuf object files
%.pb.o: %.pb.cc
	$(CXX) -c $(CXXFLAGS) -I$(OUT_PATH) $^ -o $@

# Generate protobuf gRPC source files
$(OUT_PATH)/%.grpc.pb.cc $(OUT_PATH)/%grpc.pb.h: $(API_PATH)/%.proto | $(OUT_PATH)
	protoc $(PKG_CONFIG_CFLAGS_I) \
	-I$(API_PATH) \
	--grpc_out=$(OUT_PATH) \
	--plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

# Generate protobuf source files
$(OUT_PATH)/%.pb.cc $(OUT_PATH)/%.pb.h: $(API_PATH)/%.proto | $(OUT_PATH)
	protoc $(PKG_CONFIG_CFLAGS_I) -I$(API_PATH) --cpp_out=$(OUT_PATH) $<

$(INSTALL_PATH)/$(BINARY): $(OUT_PATH)/$(BINARY)
	$(INSTALL) $^ $@

$(INSTALL_PATH)/$(TEST): $(OUT_PATH)/$(TEST)
	$(INSTALL) $^ $@

install: $(INSTALL_PATH)/$(BINARY) $(INSTALL_PATH)/$(TEST)

install/strip: $(INSTALL_PATH)/$(BINARY) $(INSTALL_PATH)/$(TEST)
	$(STRIP) $^

clean:
	$(RM) -r $(OUT_PATH)

test: $(OUT_PATH)/$(TEST)
	$(OUT_PATH)/$(TEST)

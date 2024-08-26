INSTALL ?= install
RM ?= rm -f
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

# Paths
#OUT_PATH ?= $(CURDIR)/build_$(shell $(CXX) -dumpmachine)
OUT_PATH ?= $(CURDIR)/build
API_PATH := $(CURDIR)/apis
SRC_PATH := $(CURDIR)/src
TEST_PATH := $(CURDIR)/test
INSTALL_PATH := $(DESTDIR)/usr/bin
GRPC_CPP_PLUGIN := grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= $(shell which $(GRPC_CPP_PLUGIN))

# Output binary name matches the repository name
BINARY := $(subst -,,$(shell basename -s .git $$(git config --get remote.origin.url)))
TEST := $(addsuffix test, $(BINARY))

# Build files
PROTOBUF_FILES := $(call rwildcard, $(API_PATH),*.proto)
PROTOBUF_H := $(patsubst %.proto,%.pb.h,$(patsubst $(API_PATH)%,$(OUT_PATH)%,$(PROTOBUF_FILES)))
PROTOBUF_O := $(patsubst %.pb.h,%.pb.o,$(PROTOBUF_H))
PROTOBUF_GRPC_O := $(patsubst %.pb.h,%.grpc.pb.o,$(PROTOBUF_H))
SRC_FILES := $(wildcard $(SRC_PATH)/*.cpp $(SRC_PATH)/*.cc)
TEST_FILES := $(wildcard $(TEST_PATH)/*.cpp $(TEST_PATH)/*.cc)

# Compiler flags
# grpc and protobuf (and deps) don't play nice with pkg-config so we tediously list (in order) everything needed
PROTO_PKG_CONFIG_CFLAGS_I := -I$(TARGETSYSROOT)/usr/include
PROTO_PKG_CONFIG_CFLAGS_OTHER := -DCARES_STATICLIB -pthread -DNOMINMAX
# Derived from grpc cross-compile example
PROTO_PKG_CONFIG_LDLIBS := -lgrpc++ -lprotobuf -lgrpc -lupb_json_lib -lupb_textformat_lib \
                        -lutf8_range -lupb_message_lib -lupb_base_lib -lupb_mem_lib -lre2 \
                        -lz -lcares -lgpr -labsl_random_distributions -labsl_random_seed_sequences \
                        -labsl_random_internal_pool_urbg -labsl_random_internal_randen \
                        -labsl_random_internal_randen_hwaes -labsl_random_internal_randen_hwaes_impl \
                        -labsl_random_internal_randen_slow -labsl_random_internal_platform \
                        -labsl_random_internal_seed_material -labsl_random_seed_gen_exception \
                        -lrt -laddress_sorting -labsl_log_internal_check_op -labsl_leak_check \
                        -labsl_die_if_null -labsl_log_internal_conditions -labsl_log_internal_message \
                        -labsl_log_internal_nullguard -labsl_examine_stack -labsl_log_internal_format \
                        -labsl_log_internal_proto -labsl_log_internal_log_sink_set -labsl_log_sink \
                        -labsl_log_entry -labsl_log_initialize -labsl_log_internal_globals \
                        -labsl_log_globals -labsl_vlog_config_internal -labsl_log_internal_fnmatch \
                        -labsl_statusor -labsl_status -labsl_strerror -lutf8_validity \
                        -labsl_flags_internal -labsl_flags_marshalling -labsl_flags_reflection \
                        -labsl_flags_config -labsl_cord -labsl_cordz_info -labsl_cord_internal \
                        -labsl_cordz_functions -labsl_cordz_handle -labsl_crc_cord_state -labsl_crc32c \
                        -labsl_str_format_internal -labsl_crc_internal -labsl_crc_cpu_detect \
                        -labsl_raw_hash_set -labsl_hash -labsl_bad_variant_access -labsl_city \
                        -labsl_low_level_hash -labsl_hashtablez_sampler -labsl_exponential_biased \
                        -labsl_flags_private_handle_accessor -labsl_flags_commandlineflag \
                        -labsl_bad_optional_access -labsl_flags_commandlineflag_internal \
                        -labsl_flags_program_name -labsl_synchronization -labsl_graphcycles_internal \
                        -labsl_kernel_timeout_internal -labsl_time -labsl_civil_time -labsl_time_zone \
                        -labsl_stacktrace -labsl_symbolize -labsl_strings -labsl_strings_internal \
                        -labsl_string_view -labsl_int128 -labsl_throw_delegate -labsl_malloc_internal \
                        -labsl_debugging_internal -labsl_demangle_internal -labsl_base \
                        -labsl_raw_logging_internal -labsl_log_severity -labsl_spinlock_wait
PROTO_PKG_CONFIG_LDLIBS += $(TARGETSYSROOT)/usr/lib/libabsl_flags_parse.a \
                        $(TARGETSYSROOT)/usr/lib/libssl.a \
                        $(TARGETSYSROOT)/usr/lib/libcrypto.a \
                        -ldl -lm -lsystemd $(TARGETSYSROOT)/usr/lib/libsystemd.so \
                        $(TARGETSYSROOT)/usr/lib/libabsl_flags_usage.a \
                        $(TARGETSYSROOT)/usr/lib/libabsl_flags_usage_internal.a

PROTO_CXXFLAGS := $(CXXFLAGS) -std=c++17 $(PROTO_PKG_CONFIG_CFLAGS_OTHER)

PKGS = gio-2.0 glib-2.0 vdostream axparameter
PKG_CONFIG_CFLAGS_I := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags-only-I $(PKGS))
PKG_CONFIG_CFLAGS_OTHER := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags-only-other $(PKGS))
PKG_CONFIG_LDFLAGS := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs-only-L $(PKGS))
PKG_CONFIG_LDLIBS := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs-only-l $(PKGS))

CXXFLAGS += -DLAROD_API_VERSION_2 -std=c++17 -I$(OUT_PATH) $(PKG_CONFIG_CFLAGS_OTHER) $(PKG_CONFIG_CFLAGS_I) $(PROTO_PKG_CONFIG_CFLAGS_OTHER) $(PROTO_PKG_CONFIG_CFLAGS_I)
LDLIBS += -llarod $(PKG_CONFIG_LDLIBS) $(PROTO_PKG_CONFIG_LDLIBS)
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
	-I/usr/src/googletest/googletest/include \
	-I/usr/src/googletest/googlemock/include \
	-I$(OUT_PATH)/tensorflow_serving/apis \
	-o $@ $(TEST_FILES) $(SRC_FILES) $(PROTOBUF_O) $(PROTOBUF_GRPC_O) -lgtest_main -lgtest  $(LDLIBS)

# Build directory
$(OUT_PATH) $(INSTALL_PATH):
	$(INSTALL) -d $@

# Protobuf object files
%.pb.o: %.pb.cc
	$(CXX) -c $(PROTO_CXXFLAGS) -I$(OUT_PATH) $^ -o $@

# Generate protobuf gRPC source files
$(OUT_PATH)/%.grpc.pb.cc $(OUT_PATH)/%grpc.pb.h: $(API_PATH)/%.proto | $(OUT_PATH)
	protoc $(PROTO_PKG_CONFIG_CFLAGS_I) \
	-I$(API_PATH) \
	--grpc_out=$(OUT_PATH) \
	--plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

# Generate protobuf source files
$(OUT_PATH)/%.pb.cc $(OUT_PATH)/%.pb.h: $(API_PATH)/%.proto | $(OUT_PATH)
	protoc $(PROTO_PKG_CONFIG_CFLAGS_I) -I$(API_PATH) --cpp_out=$(OUT_PATH) $<

$(BINARY): $(OUT_PATH)/$(BINARY)
	cp $(OUT_PATH)/$(BINARY) $(CURDIR)

$(TEST): $(OUT_PATH)/$(TEST)
	cp $(OUT_PATH)/$(TEST) $(CURDIR)

install/strip: $(BINARY) $(TEST) 
	$(STRIP) $^

clean:
	$(RM) -r $(OUT_PATH)

test: $(OUT_PATH)/$(TEST)
	$(OUT_PATH)/$(TEST)

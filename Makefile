TARGET := iTLS-Enso
TITLE_ID := SKGTLSE12

PREFIX := arm-vita-eabi
CC := $(PREFIX)-gcc
HOST_CC ?= cc
PYTHON ?= python3

APP_OBJS := main.o font.o graphics.o installer/ca_bundle.o \
	installer/file_transaction.o installer/tai_config.o
APP_LIBS := -lSceCtrl_stub -lSceDisplay_stub -lScePower_stub -lSceVshBridge_stub
CFLAGS := -std=gnu99 -Wall -Wextra -O3
LDFLAGS := -Wl,-q

BUILD_DIR := build
KERNEL_BUILD_DIR := $(BUILD_DIR)/kernel
DNS_BUILD_DIR := $(BUILD_DIR)/old_dns_fix
HOST_TEST_DIR := $(BUILD_DIR)/host-tests
KERNEL_MODULE := $(KERNEL_BUILD_DIR)/itls.skprx
DNS_MODULE := $(DNS_BUILD_DIR)/old_dns_fix.suprx

CA_SONY := misc/sony_certs.cer
CA_MOZILLA := misc/mozilla_cacert.pem
CA_BUNDLE := misc/certs.cer
CA_GENERATOR := tools/update_ca_bundle.py

HOST_CA_LOADER := $(HOST_TEST_DIR)/ca_loader_test
HOST_PATH_REWRITE := $(HOST_TEST_DIR)/test_kernel_path_rewrite
HOST_FILE_TRANSACTION := $(HOST_TEST_DIR)/test_file_transaction
HOST_TAI_CONFIG := $(HOST_TEST_DIR)/test_tai_config
HOST_CA_BUNDLE := $(HOST_TEST_DIR)/test_ca_bundle_transaction

.PHONY: all clean clean-host-tests host-tests python-tests check-certs update-certs test

all: $(TARGET).vpk

$(TARGET).vpk: eboot.bin $(KERNEL_MODULE) $(DNS_MODULE) $(CA_BUNDLE) \
		misc/compat.suprx misc/lhttp.suprx misc/lssl.suprx misc/comm2.suprx \
		sce_sys/icon0.png sce_sys/livearea/contents/bg.png \
		sce_sys/livearea/contents/template.xml
	vita-mksfoex -s TITLE_ID=$(TITLE_ID) "iTLS-Enso" param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin \
		-a sce_sys/icon0.png=sce_sys/icon0.png \
		-a sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png \
		-a sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml \
		-a $(KERNEL_MODULE)=kernel.skprx \
		-a $(DNS_MODULE)=dnsfix.suprx \
		-a misc/compat.suprx=compat.suprx \
		-a misc/lhttp.suprx=lhttp.suprx \
		-a misc/lssl.suprx=lssl.suprx \
		-a misc/comm2.suprx=comm2.suprx \
		-a $(CA_BUNDLE)=certs.cer $@

eboot.bin: $(TARGET).velf
	vita-make-fself -c $< $@

$(TARGET).velf: $(TARGET).elf
	vita-elf-create $< $@

$(TARGET).elf: $(APP_OBJS)
	$(CC) $(LDFLAGS) $^ $(APP_LIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_MODULE): kernel/CMakeLists.txt kernel/itls.yml kernel/main.c \
		kernel/path_rewrite.c kernel/path_rewrite.h
	cmake -S kernel -B $(KERNEL_BUILD_DIR)
	+cmake --build $(KERNEL_BUILD_DIR) --target itls.skprx

$(DNS_MODULE): old_dns_fix/CMakeLists.txt old_dns_fix/exports.yml old_dns_fix/main.c
	cmake -S old_dns_fix -B $(DNS_BUILD_DIR)
	+cmake --build $(DNS_BUILD_DIR) --target old_dns_fix.suprx-self

$(CA_BUNDLE): $(CA_SONY) $(CA_MOZILLA) $(CA_GENERATOR)
	$(PYTHON) $(CA_GENERATOR) \
		--sony $(CA_SONY) \
		--mozilla $(CA_MOZILLA) \
		--output $(CA_BUNDLE)

$(HOST_TEST_DIR):
	mkdir -p $@

$(HOST_CA_LOADER): tests/ca_loader_test.c | $(HOST_TEST_DIR)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror $< \
		$$(pkg-config --cflags --libs openssl) -o $@

$(HOST_PATH_REWRITE): tests/test_kernel_path_rewrite.c \
		kernel/path_rewrite.c kernel/path_rewrite.h | $(HOST_TEST_DIR)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror \
		tests/test_kernel_path_rewrite.c kernel/path_rewrite.c -o $@

$(HOST_FILE_TRANSACTION): tests/test_file_transaction.c \
		installer/file_transaction.c installer/file_transaction.h | $(HOST_TEST_DIR)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror \
		tests/test_file_transaction.c installer/file_transaction.c -o $@

$(HOST_TAI_CONFIG): tests/test_tai_config.c \
		installer/tai_config.c installer/tai_config.h | $(HOST_TEST_DIR)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror \
		tests/test_tai_config.c installer/tai_config.c -o $@

$(HOST_CA_BUNDLE): tests/test_ca_bundle_transaction.c \
		installer/ca_bundle.c installer/ca_bundle.h \
		installer/file_transaction.c installer/file_transaction.h | $(HOST_TEST_DIR)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror \
		tests/test_ca_bundle_transaction.c installer/ca_bundle.c \
		installer/file_transaction.c -o $@

python-tests:
	$(PYTHON) -m unittest -v tests.test_update_ca_bundle

host-tests: python-tests $(HOST_CA_LOADER) $(HOST_PATH_REWRITE) \
		$(HOST_FILE_TRANSACTION) $(HOST_TAI_CONFIG) $(HOST_CA_BUNDLE)
	$(HOST_PATH_REWRITE)
	$(HOST_FILE_TRANSACTION)
	$(HOST_TAI_CONFIG)
	$(HOST_CA_BUNDLE)

check-certs: $(HOST_CA_LOADER)
	$(PYTHON) $(CA_GENERATOR) \
		--sony $(CA_SONY) \
		--mozilla $(CA_MOZILLA) \
		--output $(CA_BUNDLE) \
		--check
	$(HOST_CA_LOADER) $(CA_BUNDLE)

update-certs:
	tools/update_ca_bundle.sh

test: host-tests check-certs

clean-host-tests:
	rm -rf $(HOST_TEST_DIR)

clean:
	rm -rf $(TARGET).vpk $(TARGET).velf $(TARGET).elf eboot.bin param.sfo \
		$(APP_OBJS) $(KERNEL_BUILD_DIR) $(DNS_BUILD_DIR) $(HOST_TEST_DIR)

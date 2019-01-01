deps_config := \
	/home/saffron/esp/esp-idf/components/app_trace/Kconfig \
	/home/saffron/esp/esp-idf/components/aws_iot/Kconfig \
	/home/saffron/esp/esp-idf/components/bt/Kconfig \
	/home/saffron/esp/esp-idf/components/driver/Kconfig \
	/home/saffron/esp/esp-idf/components/esp32/Kconfig \
	/home/saffron/esp/esp-idf/components/esp_adc_cal/Kconfig \
	/home/saffron/esp/esp-idf/components/esp_event/Kconfig \
	/home/saffron/esp/esp-idf/components/esp_http_client/Kconfig \
	/home/saffron/esp/esp-idf/components/esp_http_server/Kconfig \
	/home/saffron/esp/esp-idf/components/ethernet/Kconfig \
	/home/saffron/esp/esp-idf/components/fatfs/Kconfig \
	/home/saffron/esp/esp-idf/components/freemodbus/Kconfig \
	/home/saffron/esp/esp-idf/components/freertos/Kconfig \
	/home/saffron/esp/esp-idf/components/heap/Kconfig \
	/home/saffron/esp/esp-idf/components/libsodium/Kconfig \
	/home/saffron/esp/esp-idf/components/log/Kconfig \
	/home/saffron/esp/esp-idf/components/lwip/Kconfig \
	/home/saffron/esp/esp-idf/components/mbedtls/Kconfig \
	/home/saffron/esp/esp-idf/components/mdns/Kconfig \
	/home/saffron/esp/esp-idf/components/mqtt/Kconfig \
	/home/saffron/esp/esp-idf/components/nvs_flash/Kconfig \
	/home/saffron/esp/esp-idf/components/openssl/Kconfig \
	/home/saffron/esp/esp-idf/components/pthread/Kconfig \
	/home/saffron/esp/esp-idf/components/spi_flash/Kconfig \
	/home/saffron/esp/esp-idf/components/spiffs/Kconfig \
	/home/saffron/esp/esp-idf/components/tcpip_adapter/Kconfig \
	/home/saffron/esp/esp-idf/components/unity/Kconfig \
	/home/saffron/esp/esp-idf/components/vfs/Kconfig \
	/home/saffron/esp/esp-idf/components/wear_levelling/Kconfig \
	/home/saffron/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/saffron/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/saffron/esp/CH5-WIFI/main/Kconfig.projbuild \
	/home/saffron/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/saffron/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_TARGET)" "esp32"
include/config/auto.conf: FORCE
endif
ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;

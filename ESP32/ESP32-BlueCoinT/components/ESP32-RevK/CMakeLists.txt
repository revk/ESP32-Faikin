set(SOURCES "revk.c" "jo.c" "lwmqtt.c")
set(RECS nvs_flash app_update esp_http_client esp-tls esp_http_server spi_flash esp_wifi esp_timer esp_system driver bt)

# Add extra dependancies

# Unfortunately we can't simply test for ${CONFIG_IDF_TARGET_ESP8266} here
# because requirements are evaluated in a separate pass, where any CONFIG_
# settings haven't been evaluated yet.
# The same applied to mdns check below
if(EXISTS $ENV{IDF_PATH}/components/esp8266)
	set(SOURCES ${SOURCES} "esp8266_compat.c")
	set(RECS nvs_flash app_update esp_http_client esp-tls esp_http_server esp8266 spi_flash)
endif()

if(EXISTS "../managed_components/espressif__mdns")
	set(RECS ${RECS} mdns)
endif()

idf_component_register(
			SRCS ${SOURCES}
			INCLUDE_DIRS "include"
			REQUIRES ${RECS}
			EMBED_FILES "../../build/partition_table/partition-table.bin"
)

add_definitions(-DBUILD_ESP32_USING_CMAKE)
set_property(SOURCE "../../build/partition_table/partition-table.bin" PROPERTY GENERATED 1)

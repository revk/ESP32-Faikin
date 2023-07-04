# ESP32 RevK support library

The `ESP32-RevK` support library is used in almost all of the ESP32 projects on this repository. It provides basic management of WiFi connections, settings, MQTT, and JSON.

This manual covers the use of teh library in an app.

## Software update server

If you are running your own server for software upgrades you need to ensure `http://` is allowed with access to the `.bin` file at the top level. This is because multiple TLS connections are a memory issue on many ESP32 modules. One trick is to allow `http://` if the `User-Agent` is `ESP32 HTTP Client/1.0` and redirect to `https://` for all other cases. As it is not `https://`, you should always build with code signing and checking. Your server also needs to allow `Range:` header for the version check to be done.

The upgrade checks the `.bin` file using `Range:`, and does not upgrade if no change, so make sure new code has a new `version`, `product_name`, `date` or `time` set, touching `${IDF_PATH}/components/esp_app_format/esp_app_desc.c` as part of your build can ensure this. The `Makefile` for apps usually uses the `buildsuffix` script provided, which does this for you.

## RevK

This is how an application uses the RevK library.

### Include

To use the libraries simply add the include file
```
#include "revk.h"
```
This includes necessary system includes and `lwmqtt.h` and `jo.h` include files needed.

### CMake

In you `main/CMakeLists.txt ` have
```
set (COMPONENT_REQUIRES "ESP32-RevK)
```

### Init

In `void app_main()` you need, close to the start
```
revk_boot(&app_callback);
```
And once you have done any settings (see below) do
```
revk_start();
```

The `app_callback` is `const char *app_callback(int client, const char *prefix, const char *target, const char *suffix, jo_t j)` which is called for any received MQTT messages. The return value is an empty string for all OK, or an error message. NULL means not handled and not an error (which usually means an error as unknown command).

### Settings

Between `revk_boot` and `revk_start` you should add necessary calls to `revk_register(...)` to add any settings you need.
```
  void revk_register(const char *name,    // Setting name (note max 15 characters inc any number suffix)
                     uint8_t array,       // If non zero then settings are suffixed numerically 1 to array
                     uint16_t size,       // Base setting size, -8/-4/-2/-1 signed, 1/2/4/8 unsigned, 0=null terminated string.
                     void *data,  // The setting itself (for string this points to a char* pointer)
                     const char *defval,  // default value (default value text, or bitmask[space]default)
                     uint8_t flags);      // Setting flags
```
The way this is typically done is a list of settings in a macro, allowing the definition of the settings and the calling of the `revk_register` all to be done from the same list.

For example, define settings like this

```
#define settings                \
        u8(webcontrol,2)        \
        bl(debug)               \
```

You can then define the values, e.g.

```
#define u8(n,d) uint8_t n;
#define bl(n) uint8_t n;
settings
#undef u8
#undef bl
```

And in `app_main` call the `revk_register` like this.

```
#define bl(n) revk_register(#n,0,sizeof(n),&n,NULL,SETTING_BOOLEAN|SETTING_LIVE);
#define u8(n,d) revk_register(#n,0,sizeof(n),&n,str(d),0);
   settings
#undef u8
#undef bl
```

Obviously there could be way more types and flags you can use for different types of settings. This example uses `bl()` for "Boolean, live update", and `u8()` for `uint8_t` with a default value.

It is possible to flag fields as needing various bit tags based on a string of characters, and even as needing hex encoding of the value. Settings can live, and apply when sent, or by default they are stored and a reboot is done in a few seconds. It is a good idea to look at some of the existing apps to see the ways settings can be used.

#### Flags

|Flag|meaning|
|----|-------|
|`LIVE`|The setting is always applied live, it also gets written to flash but does not cause a reboot.|
|`BINDATA`|This is a binary block. If fixed size then this is just a malloc'd block. If variable then this is a pointer to `revk_bin_t` which incldes a length|
|`SIGNED`|Numeric value is signed|
|`BOOLEAN`|Boolean - if an array then an array of bits in a value|
|`BITFIELD`|Numeric value has high bits reserved for a series of character flags (initial string in the default value string followed by a space and actual default)|
|`HEX`|Source is hex coded|
|`SET`|Top bit of numeric value is reserved - set if the value has been set|
|`SECRET`|Don't output this setting|
|`FIX`|This setting is always written to flash, meaning any defaults are only every used on first run|

### Useful functions tracking state

There are a number of functions to keep track of things...
```
uint32_t revk_link_down(void);  // How long link down (no IP or no parent if mesh)
const char *revk_wifi(void);	// Return wifi SSID
void revk_wifi_close(void); // Close WiFi
int revk_wait_wifi(int seconds); // Wait for WiFi to be ready
char *revk_setting(jo_t); // Store settings
const char *revk_command(const char *tag, jo_t);        // Do an internal command
const char *revk_restart(const char *reason, int delay);        // Restart cleanly
const char *revk_ota(const char *host, const char *target);     // OTA and restart cleanly (target NULL for self as root node)
uint32_t revk_shutting_down(void); // If we are shutting down (how many seconds to go)
```

## LWMQTT

The `lwmqtt` library is a *light weight MQTT* server and client library used internally by the RevK library.
```
lwmqtt_t revk_mqtt(int);	// Return the current LWMQTT handle
void revk_mqtt_close(const char *reason);       // Clean close MQTT
int revk_wait_mqtt(int seconds);	// Wait for MQTT to connect
```

Generally you do not need to directly interact with MQTT, but there are some simple functions to generate MQTT messages.

To use these you construct a JSON object then call these to send the message and free the object.
```
revk_state(const char *suffix,jo_t j); // Send a state message
revk_event(const char *suffix,jo_t j); // Send a event message
revk_info(const char *suffix,jo_t j); // Send a info message
revk_error(const char *suffix,jo_t j); // Send a error message
```
Additional lower level functions are defined in `revk.h` and `lwmqtt.h`

### Example

```
jo_t j = jo_object_alloc();
jo_string(j, "field", tag);
jo_string(j, "error", err);
revk_error("control", &j);
```

## Build tools

There are also some useful scripts.

### `buildsuffix`

This returns a build suffix, based on the `sdkconfig`. The idea is that you can build different versions for different target chips and accessories, and make a build file for each case. e.g.

E.g. `Makefile` or other build script having:

```
PROJECT_NAME := LED
SUFFIX := $(shell components/ESP32-RevK/buildsuffix)

all:
        @echo Make: build/$(PROJECT_NAME)$(SUFFIX).bin
        @idf.py build
        @cp --remove-destination build/$(PROJECT_NAME).bin $(PROJECT_NAME)$(SUFFIX).bin
        @echo Done: build/$(PROJECT_NAME)$(SUFFIX).bin
```

### `setbuildsuffix`

This sets `sdkconfig` based on a requested build suffix. This can link in to some other components in some cases (see [GFX](https://github.com/revk/ESP32-GFX)), but the simple factors are `S1` for the `S1` silicon, or `S2`, `S3`, etc, and `PICO` for the `ESP32-PICO` chips, and `SOLO` for single processor chips as used in Shelly modules. This means you can make targets, e.g.

```
pico:
        components/ESP32-RevK/setbuildsuffix -S1-PICO
        @make
        
wroom:
        components/ESP32-RevK/setbuildsuffix -S1
        @make

solo:
        components/ESP32-RevK/setbuildsuffix -S1-SOLO
        @make
```

The suffix is then known in the code as `CONFIG_REVK_BUILD_SUFFIX` and used as part of the upgrade process, e.g. in the above examples, the binary file would be `LED-S1-PICO.bin` for the `pico` build.

## JO

The JSON OBJECT library is designed to allow a JSON object to be constructed or parsed to/from a simple in memory string. This is intended to be very memory efficient as it does not create memory structures for the JSON object itself, and for parsing it literally just scans the string itself with minimum overhead.

The main type used is `jo_t` which is an object to handle either constructing or parsing a JSON string. It is possible to construct a string and then rewind and parse the same string.

The `jo_t` type has a pointer in to the object. For creating this is constantly moving forward adding fields and values. For parsing it is possible to rewind and go back and move forward through the object.

Use `jo_free` to free an object, but some functions such as `revk_info`, etc, free for you.

### Error handling

If at any point there is an error, e.g. bad parsing, or bad creating, or memory overrun creating then an error flag is set. You can check with `jo_error` to see if an error. Once set parsing stops (`JO_END` returned) and creating stops (no action).

### Parsing JSON

To parse a JSON object in memory you start with `jo_parse_mem` which is given a buffer and length.

To move through the object you can use `jo_here` to tell what is at this point, `jo_next` to move to next point and tell what is at that point, `jo_skip` to skip the next value at the same level, e.g. if the next value is an object it skips the whole object, and finally `jo_find` to find a named field in the JSON where you pass a tag that can be *tag*, or *tag.tag*, etc, returning the type of the value it finds.

The type of where you are can be one of:-

|Type|Meaning|
|----|-------|
|`JO_END`|End of JSON parsing|
|`JO_CLOSE`|End of object or array, i.e. `}` or `]`|
|`JO_TAG`|The name/tag in an object|
|`JO_OBJECT`|The `{` at the start of an object|
|`JO_ARRAY`|The `[` at the start of an array|
|`JO_STRING`|A string value|
|`JO_NUMBER`|A number value|
|`JO_NULL`|A `null` value|
|`JO_TRUE`|A `true` value|
|`JO_FALSE`|A `faluse` value|

Walking through an object using `jo_next` sees the start (`JO_OBJECT` or `JO_ARRAY`) and end (`JO_CLOSE`) of objects and arrays, and in objects it sees the `JO_TAG` and then the value type of that tag.

Once on the value, e.g. after a `JO_TAG` or within an array, you can get the value using functions. For literals you see if `JO_TRUE`/`JO_FALSE`/`JO_NULL` and can get values using `jo_read_int` or `jo_read_float`.

However strings are more complex as the raw JSON has escaping. `jo_strlen` gives the length of a `JO_STRING` value after de-escaping. `jo_strncpy` can be used to copy and de-escape. `jo_strncmp` can be used to compare to a normal string. `jo_strdup` can be used to copy and de-escape in to malloc'd memory. These string functions can be used at a `JO_STRING` or `JO_TAG` point. There are also `jo_strncpy64` (and `32` and `16`) for decoding base64 string and copying.

There are additional functions such as `jo_level` to tell you what level of nesting you are at, `jo_rewind` to start parsing again.

The function `jo_copy` can copy a whole JSON object if needed.

### Creating JSON

You create a new object for creating a JSON object using either `jo_create_alloc` which returns an empty JSON object ready to create, or `jo_object_alloc` which returns a JSON structure which has opened an object, i.e. the initial `{` exists and a final `}` will be added when closed. These allocate memory using `malloc` and `realloc` as needed. You can also create one using static memory using `jo_create_mem` which is passed a buffer and length.

The functions to create JSON handle the commands and `{`/`}` and `[`/`]` and tags and so on for you. When done you use `jo_finish` (for static JSON) or `jo_finisha` for malloc'd JSON to get the formatted JSON string. If not sure which then `jo_isalloc` will tell you. The reason for two separate calls is that for malloc'd you have to `free()` the value but not for static, so you are expected to know which it is you are getting.

You build up the JSON with functions... These functions take a *tag* which is needed if adding within an object and must be NULL when adding within an array.

|Functions|Meaning|
|---------|-------|
|`jo_array`|Start an array (i.e. add `[`)|
|`jo_object`|Start an object (i.e. add '{')|
|`jo_close`|Close currect object or array (i.e. add `}` or `]`)|
|`jo_json`|Add a JSON value from another JSON pointer|
|`jo_int`|Add an integer|
|`jo_bool`|Add a Boolean, e.g. `true` or `false`|
|`jo_null`|Add a `null`|
|`jo_string`|Add a string|
|`jo_stringn`|Add a string with specified length, so allows nulls in the string|
|`jo_stringf`|Add printf formatted string|
|`jo_lit`|Add a literal, e.g. `"true"` or `"null`", or a numeric literal, etc.|
|`jo_litf`|Add a literal using printf formatting, usually for adding a number of some sort.|
|`jo_datetime`|Add a `time_t` as ISO datetime string|
|`jo_base64`|Add a base 64 coded value, also `jo_base32` and `jo_base16`|

You do not need to close everything, when you finish the construction all necessary closes are applied for you.

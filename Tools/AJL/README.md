# AJL
Adrian's JSON library is "yes another JSON library", coding for GCC under linux, and designed to be lightweight and easy to use.

This is similar to AXL (Adrian's XML library) but focused on JSON rather than XML. The key differences being a lack of namespace, and the handling of the fact JSON allows different types of value (string, number, Boolean), and arrays.

It is comprised of two parts: (1) A low level parse and generation library, (2) a higher level object management library (which includes the parse library too).

The parsing and generation are designed around managing JSON at a lexical level - e.g. if you want to create a JSON object on the fly programmatically, such as start an object, add a tagged string, start an array, etc. This can be to memory or a stream. The parsing side is similarly simple - allowing you to detect start of an object, a tagged value in an object, the next value in an array and so on. This low level also provides simple formatted parse, output, and compare for the basic data types, e.g. for strings and numbers and Booleans.

The object management library allows a whole JSON file to be loaded and then processed in memory. You can also create a new object in memory. In either case you can walk around the object, find tagged entries by name or path. Add or remove or change entries by name or path. And, of course, write out the object. This is intended for code that needs to work on an whole JSON object, either processing or generating or editing. It treats most data values as a simple string with the functions to make and parse numbers and Boolean as well.

There is also a function for output of a JSON object in an XML format if needed.

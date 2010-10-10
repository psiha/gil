This is an attempt at an alternate GIL.IO implementation that
- provides native backends/implementations
- provides a more powerfull and low(er) level interface to underlying formats
- allows more source types for reading 'formatted' images (as opposed to only char const * specified filenames)
- uses a public class approach (as opposed to current global function wrappers around private implementation classes) with classes that directly provide atleast some sort of a GIL Image/View concept
- provides adaptors for image/bitmap classes from popular GUI frameworks that turn them into GIL Image/View concepts
- has other generic improvements in the fields of space/time efficiency and code bloat reduction.

Currently this (alternate) code is based on original Boost.GIL.IO code from the Boost trunk.

Development is currently being done with MSVC++ 10.0 so no jamfiles are provided yet.

You can contact me/"the author" at dsaritz at gmail.com.
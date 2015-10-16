# encoding:utf-8 ;

# extbrotli - ruby bindings for Brotli

This is ruby bindings for compression library
[Brotli (https://github.com/google/brotli)](https://github.com/google/brotli).

  * package name: extbrotli
  * author: dearblue (mailto:dearblue@users.osdn.me)
  * version: 0.0.1.PROTOTYPE
  * software quality: EXPERIMENTAL
  * license: 2-clause BSD License
  * report issue to: <https://osdn.jp/projects/rutsubo/ticket/>
  * dependency ruby: ruby-2.0+
  * dependency ruby gems: (none)
  * dependency libraries: (none)
  * bundled external libraries:
      * brotli-0.2 <https://github.com/google/brotli/tree/v0.2.0>


## HOW TO USE (***a stub***)

### basic usage (one pass encode/decode)

``` ruby:ruby
# First, load library
require "extbrotli"

# Prepair data
source = "sample data..." * 50

# Directly compression
encdata = Brotli.encode(source)
puts "encdata.bytesize=#{encdata.bytesize}"

# Directly decompression
decdata = Brotli.decode(encdata)
puts "decdata.bytesize=#{decdata.bytesize}"

# Comparison source and decoded data
p source == decdata # => true
```

### Fastest encoding

``` ruby:ruby
best_speed = 0
Brotli.encode(source, quality: best_speed)
```

### Highest encoding

``` ruby:ruby
best_compression = 11
Brotli.encode(source, quality: best_compression)
```

### Data decoding with limitation size

``` ruby:ruby
maxdestsize = 5 # as byte size
Brotli.decode(brotli_data, maxdestsize)
```

----

[a stub]

#!ruby
#vim: set fileencoding:utf-8

require "mkmf"

dir = File.dirname(__FILE__).gsub(/[\[\{\?\*]/, "[\\0]")

encfiles = Dir.glob(File.join(dir, "../contrib/brotli/enc/*.cc")).sort
encsrc = "brotli_encoder.cc"
File.write encsrc, (encfiles.map {|n| "#include \"#{n}\"" }.join("\n") << "\n")
decfiles = Dir.glob(File.join(dir, "../contrib/brotli/dec/*.c")).sort
decsrc = "brotli_decoder.c"
File.write decsrc, (decfiles.map {|n| "#include \"#{n}\"" }.join("\n") << "\n")

filepattern = "*.c{,c,pp}"
target = File.join(dir, filepattern)
files = Dir.glob(target).map { |n| File.basename n }
rejects = (RbConfig::CONFIG["arch"] =~ /mswin|mingw/) ? /_pthreads_/ : /_win32_/
files.reject! { |n| n =~ rejects }
$srcs = files + [encsrc, decsrc]
$srcs.uniq!

#$VPATH.push "$(srcdir)/../contrib"

#find_header "brotli/enc/encode.h", "$(srcdir)/../contrib" or abort
find_header "brotli/dec/decode.h", "$(srcdir)/../contrib" or abort

if RbConfig::CONFIG["arch"] =~ /mingw/
  $CPPFLAGS << " -D__forceinline=__attribute__\\(\\(always_inline\\)\\)"
  $CXXFLAGS << " -std=gnu++11"
  $LDFLAGS << " -static-libgcc -static-libstdc++"
end

try_link "void main(void){}", " -Wl,-Bsymbolic " and $LDFLAGS << " -Wl,-Bsymbolic "

create_makefile("extbrotli")

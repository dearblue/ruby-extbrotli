#vim: set fileencoding:utf-8

require_relative "lib/extbrotli/version"

GEMSTUB = Gem::Specification.new do |s|
  s.name = "extbrotli"
  s.version = Brotli::VERSION
  s.summary = "ruby bindings for brotli"
  s.description = <<EOS
unofficial ruby bindings for brotli <https://github.com/google/brotli> the compression library.
EOS
  s.homepage = "https://osdn.jp/projects/rutsubo/"
  s.license = "2-clause BSD License"
  s.author = "dearblue"
  s.email = "dearblue@users.osdn.me"

  s.required_ruby_version = ">= 2.0"
  s.add_development_dependency "rake", "~> 10.0"
end

EXTRA.concat(FileList["contrib/brotli/{LICENSE,README.md,{enc,dec}/**/*.{h,hh,hxx,hpp,c,cc,cxx,cpp,C}}"])

require "mkmf"
require 'rbconfig'
# List directories to search for PDFium headers and library files to link against
def append_pdfium_directory_to paths
  paths.map do |dir|
    [
      File.join(dir, 'pdfium'),
      File.join(dir, 'pdfium', 'fpdfsdk', 'include'),
      File.join(dir, 'pdfium', 'third_party', 'base', 'numerics')
    ]
  end.flatten + paths
end

LIB_DIRS    = append_pdfium_directory_to %w[
  /usr/local/lib/
  /usr/lib/
]
HEADER_DIRS = append_pdfium_directory_to %w[
  /usr/local/include/
  /usr/include/
]

# Tell ruby we want to search in the specified paths
dir_config("pdfium", HEADER_DIRS, LIB_DIRS)

LIB_FILES= %w[
  javascript
  bigint
  freetype
  fpdfdoc
  fpdftext
  formfiller
  icudata
  icuuc
  icui18n
  v8_libbase
  v8_base
  v8_snapshot
  v8_libplatform
  jsapi
  pdfwindow
  fxedit
  fxcrt
  fxcodec
  fpdfdoc
  fdrm
  fxge
  fpdfapi
  freetype
  pdfium
  pthread
  freeimage
]

LIB_FILES.each do | lib |
  have_library(lib) or abort "Couldn't find library lib#{lib} in #{LIB_DIRS.join(', ')}"
end

$CPPFLAGS += " -fPIC -std=c++11 -Wall"
if RUBY_PLATFORM =~ /darwin/
  have_library('objc')
  FRAMEWORKS = %w{AppKit CoreFoundation}
  $LDFLAGS << FRAMEWORKS.map { |f| " -framework #{f}" }.join
end

if ENV['DEBUG'] == '1'
  $defs.push "-DDEBUG=1"
  $CPPFLAGS += " -g"
else
  $CPPFLAGS += " -O2"
end

create_makefile "pdfium_ruby"

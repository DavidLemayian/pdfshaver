module PDFShaver
  class EncryptionError < StandardError; end
  class InvalidFormatError < StandardError; end
  class MissingHandlerError < StandardError; end
end

%w[
  document
  page
  page_set
  version
].each { |file| require_relative File.join('pdfshaver', file) }
require_relative 'pdfium_ruby'

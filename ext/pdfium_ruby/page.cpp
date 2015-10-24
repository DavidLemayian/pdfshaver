#include "page.h"
#include <FreeImage.h>

/********************************************
* C++ Page definition
*********************************************/

// When created make sure C++ pages are marked as unopened.
Page::Page() { this->opened = false; }

// When destroying a C++ Page, make sure to dispose of the internals properly.
// And notify the parent document that this page will no longer be used.
Page::~Page() {
  if (this->opened) {
    this->unload();
    this->document->notifyPageClosed(this);
  }
}

// When the page is initialized through the Ruby lifecycle, store a reference
// to its parent Document, the page number and notify the Document that this page
// is available to be loaded.
void Page::initialize(Document* document, int page_index) {
  this->document = document;
  this->page_index = page_index;
  this->document->notifyPageOpened(this);
}

// Load the page through PDFium and flag the document as currently open.
bool Page::load() {
  if (!this->opened) {
    this->fpdf_page = FPDF_LoadPage(this->document->fpdf_document, this->page_index);
    this->text_page = FPDFText_LoadPage(this->fpdf_page);
    this->opened = true;
  }
  return this->opened;
}

// Unload the page (freeing the page's memory) and mark it as not open.
void Page::unload() {
  if (this->opened){ 
    FPDFText_ClosePage(this->text_page);
    FPDF_ClosePage(this->fpdf_page);
  }
  this->opened = false;
}

// readers for the page's dimensions.
double Page::width()  {      return FPDF_GetPageWidth(this->fpdf_page); }
double Page::height() {      return FPDF_GetPageHeight(this->fpdf_page); }
double Page::aspect() {      return width() / height(); }
//int    Page::text_length() { return FPDFText_CountChars(this->text_page); }

// Render the page to a destination path with the dimensions
// specified by width & height (or appropriate defaults).
bool Page::render(char* path, int width, int height) {
  // If no height or width is supplied, render at natural dimensions.
  if (!width && !height) {
    width  = (int)(this->width());
    height = (int)(this->height());
  }
  // When given only a height or a width, 
  // infer the other by preserving page aspect ratio.
  if ( width && !height) { height = width  / this->aspect(); }
  if (!width &&  height) { width  = height * this->aspect(); }
  
  // Create bitmap.  width, height, alpha 1=enabled,0=disabled
  bool alpha = false;
  FPDF_BITMAP bitmap = FPDFBitmap_Create(width, height, alpha);
  if (!bitmap) { return false; }

  // and fill all pixels with white for the background color
  FPDFBitmap_FillRect(bitmap, 0, 0, width, height, 0xFFFFFFFF);

  // Render a page into the bitmap in RGBA format
  // flags are:
  //      0 for normal display, or combination of flags defined below
  //   0x01 Set if annotations are to be rendered
  //   0x02 Set if using text rendering optimized for LCD display
  //   0x04 Set if you don't want to use GDI+
  int start_x = 0;
  int start_y = 0;
  int rotation = 0;
  int flags = FPDF_PRINTING; // A flag defined in PDFium's codebase.
  FPDF_RenderPageBitmap(bitmap, this->fpdf_page, start_x, start_y, width, height, rotation, flags);
  
  // Calculate the page's stride.
  // The stride holds the width of one row in bytes.  It may not be an exact
  // multiple of the pixel width because the data may be packed to always end on a byte boundary
  int stride = FPDFBitmap_GetStride(bitmap);

  // Safety checks to make sure that the bitmap
  // is properly sized and can be safely manipulated
  bool bitmapIsntValid = (
    (stride < 0) || 
    (width > INT_MAX / height) || 
    ((stride * height) > (INT_MAX / 3))
  );
  if (bitmapIsntValid){
      FPDFBitmap_Destroy(bitmap);
      return false;
  }

  // Hand off the PDFium bitmap data to FreeImage for additional processing.
  unsigned bpp        = 32;
  unsigned red_mask   = 0xFF0000;
  unsigned green_mask = 0x00FF00;
  unsigned blue_mask  = 0x0000FF;
  bool     topdown    = true;
  FIBITMAP *raw = FreeImage_ConvertFromRawBits(
    (BYTE*)FPDFBitmap_GetBuffer(bitmap), width, height, stride, bpp, 
    red_mask, green_mask, blue_mask, topdown);

  // With bitmap handoff complete the FPDF bitmap can be destroyed.
  FPDFBitmap_Destroy(bitmap);

  // Conversion to jpg or gif require that the bpp be set to 24
  // since we're not exporting using alpha transparency above in FPDFBitmap_Create
  FIBITMAP *image = FreeImage_ConvertTo24Bits(raw);
  FreeImage_Unload(raw);

  // figure out the desired format from the file extension
  FREE_IMAGE_FORMAT format = FreeImage_GetFIFFromFilename(path);

  bool success = false;
  if ( FIF_GIF == format ){
      // Gif requires quantization to drop to 8bpp
      FIBITMAP *gif = FreeImage_ColorQuantize(image, FIQ_WUQUANT);
      success = FreeImage_Save(FIF_GIF, gif, path, GIF_DEFAULT);
      FreeImage_Unload(gif);
  } else {
      // All other formats should be just a save call
      success = FreeImage_Save(format, image, path, 0);
  }

  // unload the image
  FreeImage_Unload(image);

  return success;
}

/********************************************
* Ruby class definition and initialization
*********************************************/

void Define_Page() {
  // Get the PDFShaver namespace and get the `Page` class inside it.
  VALUE rb_PDFShaver = rb_const_get(rb_cObject, rb_intern("PDFShaver"));
  VALUE rb_PDFShaver_Page = rb_const_get(rb_PDFShaver, rb_intern("Page"));
  
  // Define the C allocator function so that when a new PDFShaver::Page instance
  // is created, our C/C++ data structures are initialized into the Ruby lifecycle.
  rb_define_alloc_func(rb_PDFShaver_Page, *page_allocate);
  
  // Wire the C functions we need/want into Ruby land.
  // We're using the CPP_RUBY_METHOD_FUNC to wrap functions for C++'s comfort.
  rb_define_private_method(rb_PDFShaver_Page, "initialize_page_internals", 
                            CPP_RUBY_METHOD_FUNC(initialize_page_internals),-1);
  rb_define_method(rb_PDFShaver_Page, "render", CPP_RUBY_METHOD_FUNC(page_render), -1);
  rb_define_private_method(rb_PDFShaver_Page, "load_data",   CPP_RUBY_METHOD_FUNC(page_load_data), 0);
  rb_define_private_method(rb_PDFShaver_Page, "unload_data", CPP_RUBY_METHOD_FUNC(page_unload_data), 0);
}

// the C++ page can be deleted when we're done with the Ruby page.
static void destroy_page(Page* page) { delete page; }

// Whenever a PDFShaver::Page is created, we'll create a new C++ Page object 
// and store it in the newly created Ruby page instances, and inform it to
// clean the page up using `destroy_page`.
VALUE page_allocate(VALUE rb_PDFShaver_Page) {
  Page* page = new Page();
  return Data_Wrap_Struct(rb_PDFShaver_Page, NULL, destroy_page, page);
}

// This function does the actual initialization of the C++ page's internals
// defining which page of the document will be opened when `load_data` is called.
VALUE initialize_page_internals(int arg_count, VALUE* args, VALUE self) {
  // use Ruby's argument scanner to pull out a required
  VALUE rb_document, page_index, options;
  int number_of_args = rb_scan_args(arg_count, args, "21", &rb_document, &page_index, &options);
  if (number_of_args > 2) { /* there are options */ }
  
  // fetch the C++ document from the Ruby document the page has been initialized with
  Document* document;
  Data_Get_Struct(rb_document, Document, document);
  // And fetch the C++ page
  Page* page;
  Data_Get_Struct(self, Page, page);
  // and associate them by initializing the C++ page.
  page->initialize(document, FIX2INT(page_index));
  return self;
}

VALUE page_load_data(VALUE self) {
  Page* page;
  Data_Get_Struct(self, Page, page);
  if (! page->load() ) { rb_raise(rb_eRuntimeError, "Unable to load page data"); }
  rb_ivar_set(self, rb_intern("@extension_data_is_loaded"),  Qtrue);
  rb_ivar_set(self, rb_intern("@width"),  INT2FIX(page->width()));
  rb_ivar_set(self, rb_intern("@height"), INT2FIX(page->height()));
  rb_ivar_set(self, rb_intern("@aspect"), rb_float_new(page->aspect()));
  //rb_ivar_set(self, rb_intern("@length"), INT2FIX(page->text_length()));
  return Qtrue;
}

VALUE page_unload_data(VALUE self) {
  Page* page;
  Data_Get_Struct(self, Page, page);
  page->unload();
  rb_ivar_set(self, rb_intern("@extension_data_is_loaded"),  Qfalse);
  return Qtrue;
}

//VALUE page_text_length(VALUE self) {
//  Page* page;
//  Data_Get_Struct(self, Page, page);
//  return INT2FIX(page->text_length());
//}

//VALUE page_text(VALUE self) {
//  Page* page;
//  Data_Get_Struct(self, Page, page);
//  return INT2FIX(page->text());
//}

//bool page_render(int arg_count, VALUE* args, VALUE self) {
VALUE page_render(int arg_count, VALUE* args, VALUE self) {
  VALUE path, options;
  int width = 0, height = 0;

  rb_scan_args(arg_count, args, "1:", &path, &options);
  if (arg_count > 1) {
    VALUE rb_width  = rb_hash_aref(options, ID2SYM(rb_intern("width")));
    VALUE rb_height = rb_hash_aref(options, ID2SYM(rb_intern("height")));
    
    if (!(NIL_P(rb_width))) {
      if (FIXNUM_P(rb_width)) { width = FIX2INT(rb_width); } 
      else { rb_raise(rb_eArgError, ":width must be a integer"); }
    }
    if (!(NIL_P(rb_height))) {
      if (FIXNUM_P(rb_height)) { height = FIX2INT(rb_height); } 
      else { rb_raise(rb_eArgError, ":height must be a integer"); }
    }
  }

  FREE_IMAGE_FORMAT format = FreeImage_GetFIFFromFilename(StringValuePtr(path));
  if (format == FIF_UNKNOWN) { rb_raise(rb_eArgError, "can't save to unrecognized image format"); }
  
  Page* page;
  Data_Get_Struct(self, Page, page);
  page_load_data(self);
  VALUE output = (page->render(StringValuePtr(path), width, height) ? Qtrue : Qfalse);
  page_unload_data(self);
  return output;
}
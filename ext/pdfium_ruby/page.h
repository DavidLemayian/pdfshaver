#ifndef __PAGE_H__
#define __PAGE_H__

// forward declaration since Page/Document classes are interdependent
class Document;
#include "pdfium_ruby.h"
#include "document.h"
#include "fpdf_text.h"

class Page {
  public:
    // C++ constructor & destructor.
    Page();
    ~Page();
    
    // Ruby Data Initializer
    void initialize(Document* document, int page_number);
    
    // PDFium data initializer & cleanup
    bool load();
    void unload();
    
    // Data access methods
    double width();
    double height();
    double aspect();
    int text_length();
    
    bool render(char* path, int width, int height);
    
  private:
    int           page_index;
    bool          opened;
    Document      *document;
    FPDF_PAGE     fpdf_page;
    FPDF_TEXTPAGE text_page;
};

void Define_Page();
VALUE initialize_page_internals(int arg_count, VALUE* args, VALUE self);
VALUE page_render(int arg_count, VALUE* args, VALUE self);
VALUE page_allocate(VALUE rb_PDFShaver_Page);
VALUE page_load_data(VALUE rb_PDFShaver_Page);
VALUE page_unload_data(VALUE rb_PDFShaver_Page);
VALUE page_text_length(VALUE rb_PDFShaver_Page);
//static void destroy_page(Page* page);

#endif
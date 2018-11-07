#include "..\src\xml\repr.h"
#include "..\src\inkscape.h"
#include "..\src\document.h"

//Requires glibmm library to test, which requires GNOME
//Gnome requires a Linux or Unix based OS
//There's a site that explains how to compile it for Windows
//http://anadoxin.org/blog/compiling-glibmm-on-windows.html
//Personally couldn't get it working
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    g_type_init();
    Inkscape::GC::init();
    if ( !Inkscape::Application::exists() )
        Inkscape::Application::create("", false);
    //void* a= sp_repr_read_mem((const char*)data, size, 0);
    SPDocument *doc = SPDocument::createNewDocFromMem( (const char*)data, size, 0);
    if(doc)
        doc->doUnref();
    return 0;
}

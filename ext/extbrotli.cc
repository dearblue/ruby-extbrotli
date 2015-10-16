#include "extbrotli.h"

VALUE mBrotli;
VALUE mConst;
VALUE eError;
VALUE eNeedsMoreInput;
VALUE eNeedsMoreOutput;

EXTBROTLI_CEXTERN
void
Init_extbrotli(void)
{
    mBrotli = rb_define_module("Brotli");

    mConst = rb_define_module_under(mBrotli, "Constants");
    rb_include_module(mBrotli, mConst);

    eError = rb_define_class_under(mBrotli, "Error", rb_eRuntimeError);
    eNeedsMoreInput = rb_define_class_under(mBrotli, "NeedsMoreInput", eError);
    eNeedsMoreOutput = rb_define_class_under(mBrotli, "NeedsMoreOutput", eError);

    extbrotli_init_lowlevelencoder();
    extbrotli_init_lowleveldecoder();
}

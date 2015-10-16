#ifndef EXTBROTLI_H
#define EXTBROTLI_H 1

#include <ruby.h>

#ifdef __cplusplus
#   define EXTBROTLI_CEXTERN        extern "C"
#   define EXTBROTLI_BEGIN_CEXTERN  EXTBROTLI_CEXTERN {
#   define EXTBROTLI_END_CEXTERN    }
#else
#   define EXTBROTLI_CEXTERN
#   define EXTBROTLI_BEGIN_CEXTERN
#   define EXTBROTLI_END_CEXTERN
#endif

#define RDOCFAKE(...)

EXTBROTLI_BEGIN_CEXTERN

extern VALUE mBrotli;
RDOCFAKE(mBrotli = rb_define_module("Brotli"));

extern VALUE mConst;
RDOCFAKE(mConst = rb_define_module_under(mBrotli, "Constants"));

extern VALUE eError;
RDOCFAKE(eError = rb_define_class_under(mBrotli, "Error", rb_eRuntimeError));

extern VALUE eNeedsMoreInput;
RDOCFAKE(eNeedsMoreInput = rb_define_class_under(mBrotli, "NeedsMoreInput", eError));

extern VALUE eNeedsMoreOutput;
RDOCFAKE(eNeedsMoreOutput = rb_define_class_under(mBrotli, "NeedsMoreOutput", eError));

void extbrotli_init_lowlevelencoder(void);
void extbrotli_init_lowleveldecoder(void);

static inline void
referr(VALUE obj)
{
    rb_raise(rb_eTypeError,
            "invalid reference - #<%s:%p>",
            rb_obj_classname(obj), (void *)obj);
}

static inline void
reiniterror(VALUE obj)
{
    rb_raise(rb_eTypeError,
            "initialized object already - #<%s:%p>",
            rb_obj_classname(obj), (void *)obj);
}

static inline void *
getrefp(VALUE obj, const rb_data_type_t *type)
{
    void *p;
    TypedData_Get_Struct(obj, void, type, p);
    return p;
}

static inline void *
getref(VALUE obj, const rb_data_type_t *type)
{
    void *p = getrefp(obj, type);
    if (!p) { referr(obj); }
    return p;
}


EXTBROTLI_END_CEXTERN

#endif /* EXTBROTLI_H */

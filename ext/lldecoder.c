#include "extbrotli.h"
#include <brotli/dec/decode.h>

static VALUE cLLDecoder; /* class Brotli::LowLevelDecoder */

static void
lldec_free(void *pp)
{
    if (pp) {
        BrotliState *p = (BrotliState *)pp;
        BrotliStateCleanup(p);
    }
}

static const rb_data_type_t lldecoder_type = {
    .wrap_struct_name = "extbrotli.lowleveldecoder",
    .function.dmark = NULL,
    .function.dfree = lldec_free,
    .function.dsize = NULL,
};

static inline BrotliState *
getlldecoderp(VALUE obj)
{
    return (BrotliState *)getrefp(obj, &lldecoder_type);
}

static inline BrotliState *
getlldecoder(VALUE obj)
{
    return (BrotliState *)getref(obj, &lldecoder_type);
}

static VALUE
lldec_alloc(VALUE mod)
{
    return TypedData_Wrap_Struct(mod, &lldecoder_type, NULL);
}

static VALUE
lldec_init(VALUE dec)
{
    BrotliState *p = getlldecoderp(dec);
    if (p) { reiniterror(dec); }
    DATA_PTR(dec) = p = ALLOC(BrotliState);
    BrotliStateInit(p);
    return dec;
}

/*
 * call-seq:
 *  decode(src, dest, maxdest, finish) -> [result_code, src, dest]
 */
static VALUE
lldec_decode(VALUE dec, VALUE src, VALUE dest, VALUE maxdest, VALUE finish)
{
    BrotliState *p = getlldecoder(dec);

    char *srcp, *destp;
    size_t srcsize, destsize;
    if (NIL_P(src)) {
        srcp = NULL;
        srcsize = 0;
    } else {
        rb_obj_infect(dec, src);
        rb_check_type(src, RUBY_T_STRING);
        RSTRING_GETMEM(src, srcp, srcsize);
    }
    if (NIL_P(dest)) {
        destp = NULL;
        destsize = 0;
    } else {
        rb_check_type(dest, RUBY_T_STRING);
        destsize = NUM2SIZET(maxdest);
        rb_str_modify(dest);
        rb_str_set_len(dest, 0);
        rb_str_modify_expand(dest, destsize);
        destp = RSTRING_PTR(dest);
    }
    char *srcpp = srcp, *destpp = destp;
    size_t total;
    BrotliResult s = BrotliDecompressBufferStreaming(
            &srcsize, (const uint8_t **)&srcp, NUM2INT(finish),
            &destsize, (uint8_t **)&destp, &total, p);
    switch (s) {
    case BROTLI_RESULT_ERROR:
        rb_raise(eError, "failed BrotliDecompressBufferStreaming() - #<%s:%p>", rb_obj_classname(dec), (void *)dec);
    case BROTLI_RESULT_SUCCESS:
    case BROTLI_RESULT_NEEDS_MORE_INPUT:
    case BROTLI_RESULT_NEEDS_MORE_OUTPUT:
        if (destp > destpp) {
            rb_obj_infect(dest, dec);
            rb_str_set_len(dest, destp - destpp);
        }
        if (srcsize > 0) {
            src = rb_str_substr(src, srcp - srcpp, srcsize);
        } else {
            src = Qnil;
        }
        break;
    default:
        rb_raise(eError,
                "failed BrotliDecompressBufferStreaming() - unknown result code (%d) - #<%s:%p>",
                s, rb_obj_classname(dec), (void *)dec);
    }

    const VALUE vec[3] = { INT2NUM(s), src, dest };
    return rb_ary_new4(3, vec);
}

static void
lldec_s_decode_args(int argc, VALUE argv[], VALUE *src, VALUE *dest, size_t *srcsize, size_t *destsize, int *partial)
{
    if (argc > 0) {
        *src = argv[0];
        rb_check_type(*src, RUBY_T_STRING);
        *srcsize = RSTRING_LEN(*src);

        int s;
        switch (argc) {
        case 1:
            s = BrotliDecompressedSize(*srcsize, (const uint8_t *)RSTRING_PTR(*src), destsize);
            if (s == 0) { rb_raise(eError, "failed BrotliDecompressedSize()"); }
            *dest = rb_str_buf_new(*destsize);
            *partial = 0;
            return;
        case 2:
            *dest = argv[1];
            if (rb_type_p(*dest, RUBY_T_STRING)) {
                s = BrotliDecompressedSize(*srcsize, (const uint8_t *)RSTRING_PTR(*src), destsize);
                if (s == 0) { rb_raise(eError, "failed BrotliDecompressedSize()"); }
                rb_check_type(*dest, RUBY_T_STRING);
                rb_str_modify(*dest);
                rb_str_set_len(*dest, 0);
                rb_str_modify_expand(*dest, *destsize);
            } else {
                *destsize = NUM2SIZET(*dest);
                *dest = rb_str_buf_new(*destsize);
            }
            *partial = 0;
            return;
        case 3:
            *destsize = NUM2SIZET(argv[1]);
            *dest = argv[2];
            if (*dest == Qtrue || *dest == Qfalse) {
                *partial = RTEST(*dest) ? 1 : 0;
                *dest = rb_str_buf_new(*destsize);
            } else {
                rb_check_type(*dest, RUBY_T_STRING);
                rb_str_modify(*dest);
                rb_str_set_len(*dest, 0);
                rb_str_modify_expand(*dest, *destsize);
                *partial = 0;
            }
            return;
        case 4:
            *destsize = NUM2SIZET(argv[1]);
            *dest = argv[2];
            rb_check_type(*dest, RUBY_T_STRING);
            rb_str_modify(*dest);
            rb_str_set_len(*dest, 0);
            rb_str_modify_expand(*dest, *destsize);
            *partial = RTEST(argv[3]) ? 1 : 0;
            return;
        }
    }

    rb_error_arity(argc, 1, 4);
}

/*
 * call-seq:
 *  decode(src, dest = "") -> decoded binary string
 *  decode(src, destsize, partial = false) -> decoded binary string
 *  decode(src, destsize, dest = "", partial = false) -> decoded binary string
 */
static VALUE
lldec_s_decode(int argc, VALUE argv[], VALUE mod)
{
    VALUE src, dest;
    size_t srcsize, destsize;
    int partial;
    lldec_s_decode_args(argc, argv, &src, &dest, &srcsize, &destsize, &partial);

    int s = BrotliDecompressBuffer(srcsize, (const uint8_t *)RSTRING_PTR(src),
            &destsize, (uint8_t *)RSTRING_PTR(dest));

    if (s == BROTLI_RESULT_NEEDS_MORE_INPUT) {
        if (!partial) {
            rb_raise(eNeedsMoreInput, "failed BrotliDecompressBuffer()");
        }
    } else if (s == BROTLI_RESULT_NEEDS_MORE_OUTPUT) {
        if (!partial) {
            rb_raise(eNeedsMoreOutput, "failed BrotliDecompressBuffer()");
        }
    } else if (s != BROTLI_RESULT_SUCCESS) {
        rb_raise(eError, "failed BrotliDecompressBuffer() - status=%d", s);
    }

    rb_obj_infect(dest, src);
    rb_str_set_len(dest, destsize);
    return dest;
}

void
extbrotli_init_lowleveldecoder(void)
{
    cLLDecoder = rb_define_class_under(mBrotli, "LowLevelDecoder", rb_cObject);
    rb_define_singleton_method(cLLDecoder, "decode", RUBY_METHOD_FUNC(lldec_s_decode), -1);

    rb_define_alloc_func(cLLDecoder, lldec_alloc);
    rb_define_method(cLLDecoder, "initialize", RUBY_METHOD_FUNC(lldec_init), 0);
    rb_define_method(cLLDecoder, "decode", RUBY_METHOD_FUNC(lldec_decode), 4);

    rb_include_module(cLLDecoder, mConst);
    rb_define_const(mConst, "RESULT_ERROR",             INT2FIX(BROTLI_RESULT_ERROR));
    rb_define_const(mConst, "RESULT_SUCCESS",           INT2FIX(BROTLI_RESULT_SUCCESS));
    rb_define_const(mConst, "RESULT_NEEDS_MORE_INPUT",  INT2FIX(BROTLI_RESULT_NEEDS_MORE_INPUT));
    rb_define_const(mConst, "RESULT_NEEDS_MORE_OUTPUT", INT2FIX(BROTLI_RESULT_NEEDS_MORE_OUTPUT));
}

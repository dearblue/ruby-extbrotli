#include "extbrotli.h"
#include <functional>
#include <brotli/enc/encode.h>

static inline size_t
aux_brotli_compress_bound(size_t srcsize)
{
    return srcsize * 1.2 + 10240;
}

static inline size_t
rb_num2sizet(VALUE n)
{
#if SIZEOF_SIZE_T > SIZEOF_LONG && defined(SIZEOF_LONG_LONG)
    return (size_t)NUM2ULL(n);
#else
    return (size_t)NUM2ULONG(n);
#endif
}

static inline size_t
rb_num2ssizet(VALUE n)
{
#if SIZEOF_SIZE_T > SIZEOF_LONG && defined(SIZEOF_LONG_LONG)
    return (ssize_t)NUM2LL(n);
#else
    return (ssize_t)NUM2LONG(n);
#endif
}

struct aux_ruby_jump_tag
{
    int tag;

    aux_ruby_jump_tag(int tag) : tag(tag) { }

    static void check_and_throw(int tag)
    {
        if (tag) {
            throw aux_ruby_jump_tag(tag);
        }
    }
};

#if 0 && __cplusplus >= 201103L
static VALUE
aux_throw_ruby_exception_protected(VALUE pp)
{
    const std::function<void ()> &func = *reinterpret_cast<const std::function<void (void)> *>(pp);
    func();
    return 0;
}

template <typename T>
static inline T
aux_throw_ruby_exception(const std::function<T (void)> &func)
{
    int tag;
    VALUE s;
    const std::function<void (void)> lambda = [&] { s = func(); };
    rb_protect(aux_throw_ruby_exception_protected, (VALUE)&lambda, &tag);
    aux_ruby_jump_tag::check_and_throw(tag);
    return s;
}

static inline void
aux_throw_ruby_exception(const std::function<void (void)> &func)
{
    int tag;
    const std::function<void (void)> lambda = [&] { func(); };
    rb_protect(aux_throw_ruby_exception_protected, (VALUE)&lambda, &tag);
    aux_ruby_jump_tag::check_and_throw(tag);
}

static inline void
aux_raise_stub(VALUE exc, const char *fmt, ...)
{
    va_list args;
    VALUE mesg;

    va_start(args, fmt);
    mesg = rb_vsprintf(fmt, args);
    va_end(args);
    throw rb_exc_new3(exc, mesg);
}

#define rb_check_type(...) \
    aux_throw_ruby_exception([&] { rb_check_type(__VA_ARGS__); }) \

#define rb_str_buf_new(...) \
    aux_throw_ruby_exception<VALUE>([&] { return rb_str_buf_new(__VA_ARGS__); }) \

#define rb_num2sizet(...) \
    aux_throw_ruby_exception<size_t>([&] { return rb_num2sizet(__VA_ARGS__); }) \

#endif

static VALUE cLLEncoder;

using namespace std;
using namespace brotli;

static int
getoption_int(VALUE opts, ID key, int min, int max, int default_value)
{
    VALUE v = rb_hash_lookup(opts, ID2SYM(key));
    if (NIL_P(v)) { return default_value; }
    int n = NUM2INT(v);
    if (n >= min && (n <= max || max <= min)) { return n; }
    if (max > min) {
        rb_raise(rb_eArgError,
                "option ``%s'' is out of range (%d for %d .. %d)",
                rb_id2name(key), n, min, max);
    } else {
        rb_raise(rb_eArgError,
                "option ``%s'' is out of range (%d for %d+)",
                rb_id2name(key), n, min);
    }
}

static void
setup_encode_params(VALUE opts, BrotliParams &params, VALUE &predict)
{
    static const ID id_predict = rb_intern("predict");
    static const ID id_mode = rb_intern("mode");
    static const ID id_quality = rb_intern("quality");
    static const ID id_lgwin = rb_intern("lgwin");
    static const ID id_lgblock = rb_intern("lgblock");

    if (NIL_P(opts)) {
        predict = Qnil;
        params.quality = 9; // FIXME: changed from 11 as constructor.
        return;
    }
    rb_check_type(opts, RUBY_T_HASH);

    predict = rb_hash_lookup(opts, ID2SYM(id_predict));
    if (!NIL_P(predict)) { rb_check_type(predict, RUBY_T_STRING); }

    params.mode = (BrotliParams::Mode)getoption_int(opts, id_mode, BrotliParams::MODE_GENERIC, BrotliParams::MODE_FONT, BrotliParams::MODE_GENERIC);
    params.quality = getoption_int(opts, id_quality, 0, 11, 9);
    params.lgwin = getoption_int(opts, id_lgwin, kMinWindowBits, kMaxWindowBits, 22);
    params.lgblock = getoption_int(opts, id_lgblock, kMinInputBlockBits, kMaxInputBlockBits, 0);
}

static void
llenc_free(void *pp)
{
    if (pp) {
        BrotliCompressor *p = (BrotliCompressor *)pp;
        delete p;
    }
}

static const rb_data_type_t llencoder_type = {
    /* .wrap_struct_name = */ "extbrotli.lowlevelencoder",
    /* .function.dmark = */ NULL,
    /* .function.dfree = */ llenc_free,
    /* .function.dsize = */ NULL,
};

static inline BrotliCompressor *
getencoderp(VALUE obj)
{
    return (BrotliCompressor *)getrefp(obj, &llencoder_type);
}

static inline BrotliCompressor *
getencoder(VALUE obj)
{
    return (BrotliCompressor *)getref(obj, &llencoder_type);
}

static VALUE
llenc_alloc(VALUE mod)
{
    return TypedData_Wrap_Struct(mod, &llencoder_type, NULL);
}

static void
llenc_init_args(int argc, VALUE argv[], BrotliParams &params, VALUE &predict)
{
    VALUE opts;
    rb_scan_args(argc, argv, "0:", &opts);
    setup_encode_params(opts, params, predict);
}

/*
 * call-seq:
 *  initialize(opts = {})
 *
 * [opts predict: nil]  preset dictionary. string or nil
 * [opts mode: 0]       Brotli::MODE_GENERIC, Brotli::MODE_TEXT or Brotli::MODE_FONT
 * [opts quality: 9]    given 0 .. 11. lower is high speed, higher is high compression.
 * [opts lgwin: 22]     log window size. given 10 .. 24.
 * [opts lgblock: nil]  log block size. given 16 .. 24.
 *
 */
static VALUE
llenc_init(int argc, VALUE argv[], VALUE enc)
{
    BrotliCompressor *p = getencoderp(enc);
    if (p) { reiniterror(enc); }
    try {
        VALUE predict;
        BrotliParams params;
        llenc_init_args(argc, argv, params, predict);
        rb_obj_infect(enc, predict);
        DATA_PTR(enc) = p = new BrotliCompressor(params);
        if (!NIL_P(predict)) {
            p->BrotliSetCustomDictionary(RSTRING_LEN(predict), (const uint8_t *)RSTRING_PTR(predict));
        }
    } catch(...) {
        rb_raise(eError, "C++ runtime error - #<%s:%p>", rb_obj_classname(enc), (void *)enc);
    }
    return enc;
}

/*
 * call-seq:
 *  encode_metablock(src, dest, destsize, islast) -> dest
 */
static VALUE
llenc_encode_metablock(VALUE enc, VALUE src, VALUE dest, VALUE destsize, VALUE islast)
{
    BrotliCompressor *p = getencoder(enc);
    rb_obj_infect(enc, src);
    rb_check_type(src, RUBY_T_STRING);
    rb_check_type(dest, RUBY_T_STRING);
    rb_str_modify(dest);
    rb_str_set_len(dest, 0);
    size_t destsize1 = NUM2SIZET(destsize);
    rb_str_modify_expand(dest, destsize1);
    bool islast1 = RTEST(islast) ? true : false;
    bool s = p->WriteMetaBlock(RSTRING_LEN(src), (const uint8_t *)RSTRING_PTR(src),
            islast1, &destsize1, (uint8_t *)RSTRING_PTR(dest));
    if (!s) {
        rb_raise(eError,
                "failed BrotliCompressor::WriteMetaBlock() - #<%s:%p>",
                rb_obj_classname(enc), (void *)enc);
    }
    rb_obj_infect(dest, enc);
    rb_str_set_len(dest, destsize1);
    return dest;
}

/*
 * call-seq:
 *  encode_metadata(src, dest, destsize, islast) -> dest
 */
static VALUE
llenc_encode_metadata(VALUE enc, VALUE src, VALUE dest, VALUE destsize, VALUE islast)
{
    BrotliCompressor *p = getencoder(enc);
    rb_obj_infect(enc, src);
    rb_check_type(src, RUBY_T_STRING);
    rb_check_type(dest, RUBY_T_STRING);
    rb_str_modify(dest);
    rb_str_set_len(dest, 0);
    size_t destsize1 = NUM2SIZET(destsize);
    rb_str_modify_expand(dest, destsize1);
    bool islast1 = RTEST(islast) ? true : false;
    bool s = p->WriteMetadata(RSTRING_LEN(src), (const uint8_t *)RSTRING_PTR(src),
            islast1, &destsize1, (uint8_t *)RSTRING_PTR(dest));
    if (!s) {
        rb_raise(eError,
                "failed BrotliCompressor::WriteMetadata - #<%s:%p>",
                rb_obj_classname(enc), (void *)enc);
    }
    rb_obj_infect(dest, enc);
    rb_str_set_len(dest, destsize1);
    return dest;
}

/*
 * call-seq:
 *  finish(dest, destsize) -> dest
 */
static VALUE
llenc_finish(VALUE enc, VALUE dest, VALUE destsize)
{
    BrotliCompressor *p = getencoder(enc);
    rb_check_type(dest, RUBY_T_STRING);
    rb_str_modify(dest);
    rb_str_set_len(dest, 0);
    size_t destsize1 = NUM2SIZET(destsize);
    rb_str_modify_expand(dest, destsize1);
    bool s = p->FinishStream(&destsize1, (uint8_t *)RSTRING_PTR(dest));
    if (!s) { rb_raise(eError, "failed BrotliCompressor::FinishStream - #<%s:%p>", rb_obj_classname(enc), (void *)enc); }
    rb_obj_infect(dest, enc);
    rb_str_set_len(dest, destsize1);
    return dest;
}

/*
 * call-seq:
 *  copy_to_ringbuffer(src) -> self
 */
static VALUE
llenc_copy_to_ringbuffer(VALUE enc, VALUE src)
{
    BrotliCompressor *p = getencoder(enc);
    rb_obj_infect(enc, src);
    rb_check_type(src, RUBY_T_STRING);
    p->CopyInputToRingBuffer(RSTRING_LEN(src), (uint8_t *)RSTRING_PTR(src));
    return enc;
}

/*
 * call-seq:
 *  encode_brotlidata(dest, islast, forceflush) -> dest
 */
static VALUE
llenc_encode_brotlidata(VALUE enc, VALUE dest, VALUE islast, VALUE forceflush)
{
    BrotliCompressor *p = getencoder(enc);
    rb_check_type(dest, RUBY_T_STRING);
    rb_str_modify(dest);
    rb_str_set_len(dest, 0);
    size_t destsize = 0;
    char *destp = NULL;
    bool s = p->WriteBrotliData(RTEST(islast), RTEST(forceflush), &destsize, (uint8_t **)&destp);
    if (!s) { rb_raise(eError, "failed BrotliCompressor::WriteBrotliData - #<%s:%p>", rb_obj_classname(enc), (void *)enc); }
    rb_str_modify_expand(dest, destsize);
    memcpy(RSTRING_PTR(dest), destp, destsize);
    rb_obj_infect(dest, enc);
    rb_str_set_len(dest, destsize);
    return dest;
}

static VALUE
llenc_blocksize(VALUE enc)
{
    return SIZET2NUM(getencoder(enc)->input_block_size());
}

static inline void
llenc_s_encode_args(int argc, VALUE argv[], VALUE &src, size_t &srcsize, VALUE &dest, size_t &destsize, VALUE &predict, BrotliParams &params)
{
    VALUE opts, u, v;
    switch (rb_scan_args(argc, argv, "12:", &src, &u, &v, &opts)) {
    case 1:
        rb_check_type(src, RUBY_T_STRING);
        srcsize = RSTRING_LEN(src);
        destsize = aux_brotli_compress_bound(srcsize);
        dest = rb_str_buf_new(destsize);
        break;
    case 2:
        rb_check_type(src, RUBY_T_STRING);
        srcsize = RSTRING_LEN(src);
        if (rb_type_p(u, RUBY_T_STRING)) {
            destsize = aux_brotli_compress_bound(srcsize);
            rb_check_type(dest = u, RUBY_T_STRING);
            rb_str_modify(dest);
            rb_str_set_len(dest, 0);
            rb_str_modify_expand(dest, destsize);
        } else {
            destsize = rb_num2sizet(u);
            dest = rb_str_buf_new(destsize);
        }
        break;
    case 3:
        rb_check_type(src, RUBY_T_STRING);
        srcsize = RSTRING_LEN(src);
        destsize = rb_num2sizet(u);
        rb_check_type(dest = v, RUBY_T_STRING);
        rb_str_modify(dest);
        rb_str_set_len(dest, 0);
        rb_str_modify_expand(dest, destsize);
        break;
    default:
        rb_error_arity(argc, 1, 3);
    }

    setup_encode_params(opts, params, predict);
}

/*
 * call-seq:
 *  encode(src, dest = "", predict: nil, mode: 0, quality: 9, lgwin: 22, lgblock: nil) -> encoded binary string
 *  encode(src, destsize, dest = "", predict: nil, mode: 0, quality: 9, lgwin: 22, lgblock: nil) -> encoded binary string
 */
static VALUE
llenc_s_encode(int argc, VALUE argv[], VALUE br)
{
    try {
        VALUE src, dest, predict;
        size_t srcsize, destsize;
        BrotliParams params;
        llenc_s_encode_args(argc, argv, src, srcsize, dest, destsize, predict, params);

        const uint8_t *srcp = reinterpret_cast<const uint8_t *>(RSTRING_PTR(src));
        uint8_t *destp = reinterpret_cast<uint8_t *>(RSTRING_PTR(dest));
        int st = BrotliCompressBuffer(params, srcsize, srcp, &destsize, destp);
        if (!st) {
            rb_raise(eError, "failed BrotliCompressBuffer() (status=%d)", st);
        }
        rb_obj_infect(dest, src);
        rb_str_set_len(dest, destsize);
        return dest;
    } catch(VALUE e) {
        rb_exc_raise(e);
    } catch(aux_ruby_jump_tag e) {
        rb_jump_tag(e.tag);
    } catch(...) {
        rb_raise(eError, "C++ runtime error");
    }
}

static VALUE
llenc_s_compress_bound(VALUE mod, VALUE size)
{
    return SIZET2NUM(aux_brotli_compress_bound(NUM2SIZET(size)));
}

void
extbrotli_init_lowlevelencoder(void)
{
    cLLEncoder = rb_define_class_under(mBrotli, "LowLevelEncoder", rb_cObject);
    rb_define_singleton_method(cLLEncoder, "encode", RUBY_METHOD_FUNC(llenc_s_encode), -1);
    rb_define_singleton_method(cLLEncoder, "compress_bound", RUBY_METHOD_FUNC(llenc_s_compress_bound), 1);

    rb_define_alloc_func(cLLEncoder, llenc_alloc);
    rb_define_method(cLLEncoder, "initialize", RUBY_METHOD_FUNC(llenc_init), -1);
    rb_define_method(cLLEncoder, "encode_metablock", RUBY_METHOD_FUNC(llenc_encode_metablock), 4);
    rb_define_method(cLLEncoder, "encode_metadata", RUBY_METHOD_FUNC(llenc_encode_metadata), 4);
    rb_define_method(cLLEncoder, "finish", RUBY_METHOD_FUNC(llenc_finish), 2);
    rb_define_method(cLLEncoder, "copy_to_ringbuffer", RUBY_METHOD_FUNC(llenc_copy_to_ringbuffer), 1);
    rb_define_method(cLLEncoder, "encode_brotlidata", RUBY_METHOD_FUNC(llenc_encode_brotlidata), 3);
    rb_define_method(cLLEncoder, "blocksize", RUBY_METHOD_FUNC(llenc_blocksize), 0);

    rb_include_module(cLLEncoder, mConst);
}

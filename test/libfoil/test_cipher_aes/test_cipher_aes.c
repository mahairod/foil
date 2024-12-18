/*
 * Copyright (C) 2016-2023 Slava Monich <slava@monich.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer
 *     in the documentation and/or other materials provided with the
 *     distribution.
 *  3. Neither the names of the copyright holders nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING
 * IN ANY WAY OUT OF THE USE OR INABILITY TO USE THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * any official policies, either expressed or implied.
 */

#include "test_common.h"

#include "foil_key.h"
#include "foil_cipher.h"
#include "foil_output.h"

#include <gutil_misc.h>

#define DATA_DIR "data/"
#define TEST_TIMEOUT (10) /* seconds */

typedef struct test_cipher_aes {
    const char* name;
    GTestDataFunc fn;
    const char* key_file;
    GType (*key_type)(void);
    GType (*enc_type)(void);
    GType (*dec_type)(void);
    GUtilData in;
} TestCipherAes;

typedef struct test_cipher_aes_out {
    TestCipherAes aes;
    GUtilData out;
} TestCipherAesOut;

typedef struct test_cipher_aes_vector {
    const char* name;
    const char* key_iv;
    const char* in;
    const char* out;
    GType (*key_type)(void);
    GType (*cipher_type)(void);
} TestCipherAesVector;

static
void
test_padding(
    guint8* block,
    gsize data_size,
    gsize block_size)
{
    gsize i, n = block_size - data_size;

    for (i = 0; i < n; i++) {
        block[data_size + i] = (guint8)i;
    }
}

static
void
test_cipher_aes_basic(
    gconstpointer param)
{
    const TestCipherAes* test = param;
    char* key_path = g_strconcat(DATA_DIR, test->key_file, NULL);
    FoilKey* key = foil_key_new_from_file(test->key_type(), key_path);
    FoilCipher* enc = foil_cipher_new(test->enc_type(), key);
    FoilCipher* dec = foil_cipher_new(test->dec_type(), key);

    g_assert(!foil_cipher_type_supports_key(test->enc_type(), 0));
    g_assert(!foil_cipher_type_supports_key(test->dec_type(), 0));
    g_assert(!foil_cipher_type_supports_key(FOIL_KEY_AES128, 0));
    g_assert(foil_cipher_symmetric(enc));
    g_assert(foil_cipher_symmetric(dec));
    g_assert(foil_cipher_set_padding_func(enc, test_padding));
    g_assert(foil_cipher_step(dec, key_path, NULL) < 0);

    foil_key_unref(key);
    foil_cipher_unref(enc);
    foil_cipher_unref(dec);
    g_free(key_path);
}

static
void
test_cipher_aes_padding(
    gconstpointer param)
{
    const TestCipherAesOut* test = param;
    char* key_path = g_strconcat(DATA_DIR, test->aes.key_file, NULL);
    FoilKey* key = foil_key_new_from_file(test->aes.key_type(), key_path);
    FoilCipher* enc = foil_cipher_new(test->aes.enc_type(), key);
    GByteArray* out = g_byte_array_new();
    const gsize blk = foil_cipher_input_block_size(enc);
    const guint8* in = test->aes.in.bytes;
    gsize k, off;

    g_assert(key);
    g_assert(enc);
    g_assert_cmpuint(foil_cipher_output_block_size(enc), == ,blk);
    g_assert(foil_cipher_set_padding_func(enc, test_padding));

    for (k = test->aes.in.size; k >= blk; k -= blk, in += blk) {
        off = out->len;
        g_byte_array_set_size(out, out->len + blk);
        g_assert_cmpint(foil_cipher_step(enc, in, out->data + off), == ,blk);
    }

    off = out->len;
    g_byte_array_set_size(out, out->len + blk);
    g_assert_cmpint(foil_cipher_finish(enc, in, k, out->data + off), == ,blk);

    g_assert_cmpuint(out->len, == ,test->out.size);
    g_assert(!memcmp(out->data, test->out.bytes, out->len));

    g_byte_array_free(out, TRUE);
    foil_cipher_unref(enc);
    foil_key_unref(key);
    g_free(key_path);
}

static
gboolean
test_timeout(
    gpointer param)
{
    GERR("TIMEOUT");
    g_assert(FALSE);
    g_main_loop_quit(param);
    return G_SOURCE_CONTINUE;
}

static
void
test_cipher_aes_async_done(
    FoilCipher* cipher,
    int result,
    void* arg)
{
    g_assert(result == 16);
    g_main_loop_quit(arg);
}

static
void
test_cipher_aes_cancel(
    gconstpointer param)
{
    GMainLoop* loop;
    guint timeout_id;
    const TestCipherAes* test = param;
    char* key_path = g_strconcat(DATA_DIR, test->key_file, NULL);
    FoilKey* key = foil_key_new_from_file(FOIL_KEY_AES128, key_path);
    GType type = FOIL_CIPHER_AES_CBC_ENCRYPT;
    FoilCipher* enc = foil_cipher_new(type, key);
    FoilOutput* memout;
    guint id;
    static const guint8 in[16] = {0};
    static const guint8 expected1[] = {
        0x07, 0xfe, 0xef, 0x74, 0xe1, 0xd5, 0x03, 0x6e,
        0x90, 0x0e, 0xee, 0x11, 0x8e, 0x94, 0x92, 0x93
    };
    static const guint8 expected2[] = {
        0x89, 0xcf, 0x84, 0x08, 0x25, 0x0b, 0xf8, 0xc4,
        0xac, 0x9a, 0x44, 0x86, 0x53, 0x64, 0xb8, 0x37
    };
    guint8 out[16];
    guint8 original[sizeof(out)];

    g_assert(foil_cipher_input_block_size(enc) == sizeof(in));
    g_assert(foil_cipher_output_block_size(enc) == sizeof(out));
    memset(out, 0, sizeof(out));
    memcpy(original, out, sizeof(out));
    g_assert(!foil_cipher_step_async(enc, in, NULL /* error */, NULL, NULL));
    id = foil_cipher_step_async(enc, in, out, NULL, NULL);
    g_assert(id);
    g_source_remove(id);
    foil_cipher_cancel_all(enc);
    g_assert(!memcmp(out, original, sizeof(out)));
    foil_cipher_unref(enc);

    enc = foil_cipher_new(type, key);
    g_assert(foil_cipher_finish_async(enc, in, 0, NULL, NULL, NULL));
    g_assert(foil_cipher_finish_async(enc, in, 0, NULL, NULL, NULL));
    foil_cipher_cancel_all(enc);
    foil_cipher_unref(enc);

    enc = foil_cipher_new(type, key);
    g_assert(foil_cipher_step_async(enc, in, out, NULL, NULL));
    id = foil_cipher_step_async(enc, in, out, NULL, NULL);
    g_assert(foil_cipher_step_async(enc, in, out, NULL, NULL));
    g_source_remove(id);
    foil_cipher_unref(enc);

    enc = foil_cipher_new(type, key);
    id = foil_cipher_step_async(enc, in, out, NULL, NULL);
    g_assert(foil_cipher_step_async(enc, in, out, NULL, NULL));
    g_source_remove(id);
    foil_cipher_unref(enc);

    /* Even if we don't cancel the requests, they get cancelled
     * automatically when the cipher gets destroyed */
    enc = foil_cipher_new(type, key);
    g_assert(foil_cipher_step_async(enc, in, out, NULL, NULL));
    g_assert(foil_cipher_finish_async(enc, in, 0, NULL, NULL, NULL));
    g_assert(!memcmp(out, original, sizeof(out)));
    foil_cipher_unref(enc);

    /* Actually encrypt something */
    enc = foil_cipher_new(type, key);
    loop = g_main_loop_new(NULL, TRUE);
    g_assert(foil_cipher_finish_async(enc, in, sizeof(in), out,
        test_cipher_aes_async_done, loop));
    timeout_id = g_timeout_add_seconds(TEST_TIMEOUT, test_timeout, loop);
    g_main_loop_run(loop);
    g_source_remove(timeout_id);
    g_main_loop_unref(loop);
    foil_cipher_unref(enc);
    GDEBUG("Result:");
    TEST_DEBUG_HEXDUMP(out, sizeof(out));
    g_assert(!memcmp(out, expected1, sizeof(out)));

    /* The same but with the additional foil_cipher_step_async */
    enc = foil_cipher_new(type, key);
    loop = g_main_loop_new(NULL, TRUE);
    g_assert(foil_cipher_step_async(enc, in, out, NULL, NULL));
    g_assert(foil_cipher_finish_async(enc, in, sizeof(in), out,
        test_cipher_aes_async_done, loop));
    timeout_id = g_timeout_add_seconds(TEST_TIMEOUT, test_timeout, loop);
    g_main_loop_run(loop);
    g_source_remove(timeout_id);
    g_main_loop_unref(loop);
    foil_cipher_unref(enc);
    GDEBUG("Result:");
    TEST_DEBUG_HEXDUMP(out, sizeof(out));
    g_assert(!memcmp(out, expected2, sizeof(out)));

    enc = foil_cipher_new(type, key);
    memout = foil_output_file_new_tmp();
    g_assert(foil_cipher_write_data_async(enc, in, sizeof(in), memout,
        NULL, NULL, NULL));
    g_assert(id);
    foil_cipher_cancel_all(enc);
    foil_cipher_unref(enc);
    foil_output_unref(memout);

    foil_key_unref(key);
    g_free(key_path);
}

static
GBytes*
test_cipher_bytes(
    FoilCipher* cipher,
    GBytes* bytes)
{
    gsize size = 0;
    const void* data = g_bytes_get_data(bytes, &size);
    const gsize in_size = foil_cipher_input_block_size(cipher);
    const gsize out_size = foil_cipher_output_block_size(cipher);
    const guint8* ptr = data;
    const gsize tail = size % in_size;
    const guint n = size / in_size;
    void* out_block = g_malloc(out_size);
    GByteArray* out = g_byte_array_new();
    int nout = 0;
    guint i;

    /* Full input blocks */
    for (i = 0; i < n; i++) {
        nout = foil_cipher_step(cipher, ptr, out_block);
        g_assert(nout >= 0);
        g_byte_array_append(out, out_block, nout);
        ptr += in_size;
    }

    /* Finish the process */
    nout = foil_cipher_finish(cipher, ptr, tail, out_block);
    g_assert(nout >= 0);
    g_byte_array_append(out, out_block, nout);
    g_free(out_block);
    return g_byte_array_free_to_bytes(out);
}

static
void
test_cipher_aes_clone(
    gconstpointer param)
{
    const TestCipherAes* test = param;
    char* key_path = g_strconcat(DATA_DIR, test->key_file, NULL);
    GBytes* in = g_bytes_new_static(test->in.bytes, test->in.size);
    FoilKey* key = foil_key_new_from_file(test->key_type(), key_path);
    FoilCipher* enc1 = foil_cipher_new(test->enc_type(), key);
    FoilCipher* enc2 = foil_cipher_clone(enc1);
    GBytes* out1 = test_cipher_bytes(enc1, in);
    GBytes* out2 = test_cipher_bytes(enc2, in);
    FoilCipher* dec1 = foil_cipher_new(test->dec_type(), key);
    FoilCipher* dec2 = foil_cipher_clone(dec1);
    GBytes* res1 = test_cipher_bytes(dec1, out1);
    GBytes* res2 = test_cipher_bytes(dec2, out2);
    GDEBUG("Plain text:");
    TEST_DEBUG_HEXDUMP_BYTES(in);
    GDEBUG("Encrypted (%u bytes):", (guint)g_bytes_get_size(out1));
    TEST_DEBUG_HEXDUMP_BYTES(out1);
    g_assert(g_bytes_equal(out1, out2));
    GDEBUG("Decrypted:");
    TEST_DEBUG_HEXDUMP_BYTES(res1);
    g_assert(g_bytes_equal(res1, res2));
    g_bytes_unref(in);
    g_bytes_unref(out1);
    g_bytes_unref(out2);
    g_bytes_unref(res1);
    g_bytes_unref(res2);
    foil_cipher_unref(enc1);
    foil_cipher_unref(enc2);
    foil_cipher_unref(dec1);
    foil_cipher_unref(dec2);
    foil_key_unref(key);
    g_free(key_path);
}

static
void
test_cipher_aes_sync(
    gconstpointer param)
{
    const TestCipherAes* test = param;
    char* key_path = g_strconcat(DATA_DIR, test->key_file, NULL);
    FoilKey* key = foil_key_new_from_file(test->key_type(), key_path);
    GBytes* in = g_bytes_new_static(test->in.bytes, test->in.size);
    GBytes* out = foil_cipher_bytes(test->enc_type(), key, in);
    GBytes* dec = foil_cipher_bytes(test->dec_type(), key, out);
    GBytes* dec2;
    g_assert(dec);
    dec2 = g_bytes_new_from_bytes(dec, 0, test->in.size);
    GDEBUG("Plain text:");
    TEST_DEBUG_HEXDUMP_BYTES(in);
    GDEBUG("Encrypted (%u bytes):", (guint)g_bytes_get_size(out));
    TEST_DEBUG_HEXDUMP_BYTES(out);
    GDEBUG("Decrypted:");
    TEST_DEBUG_HEXDUMP_BYTES(dec);
    g_assert(g_bytes_equal(in, dec2));
    g_bytes_unref(in);
    g_bytes_unref(out);
    g_bytes_unref(dec);
    g_bytes_unref(dec2);
    foil_key_unref(key);
    g_free(key_path);
}

static
void
test_cipher_aes_async_proc(
    FoilCipher* cipher,
    gboolean ok,
    void* arg)
{
    g_assert(ok);
    g_main_loop_quit(arg);
}

static
void
test_cipher_aes_async(
    gconstpointer param)
{
    const TestCipherAes* test = param;
    GMainLoop* loop = g_main_loop_new(NULL, TRUE);
    char* key_path = g_strconcat(DATA_DIR, test->key_file, NULL);
    FoilKey* key = foil_key_new_from_file(test->key_type(), key_path);
    FoilCipher* cipher = foil_cipher_new(test->enc_type(), key);
    FoilOutput* out = foil_output_mem_new(NULL);
    GBytes* enc;
    GBytes* dec;
    GBytes* dec2;
    guint timeout_id, id;

    /* This one must fail (no output) */
    g_assert(!foil_cipher_write_data_async(cipher, test->in.bytes,
        test->in.size, NULL, NULL, test_cipher_aes_async_proc, loop));

    /* Encrypt asynchronously */
    id = foil_cipher_write_data_async(cipher, test->in.bytes,
        test->in.size, out, NULL, test_cipher_aes_async_proc, loop);
    g_assert(id);
    timeout_id = g_timeout_add_seconds(TEST_TIMEOUT, test_timeout, loop);
    g_main_loop_run(loop);
    g_source_remove(timeout_id);
    foil_cipher_unref(cipher);
    enc = foil_output_free_to_bytes(out);
    g_assert(enc);

    /* Decrypt asynchronously */
    cipher = foil_cipher_new(test->dec_type(), key);
    out = foil_output_mem_new(NULL);
    id = foil_cipher_write_data_async(cipher, g_bytes_get_data(enc, NULL),
        g_bytes_get_size(enc), out, NULL, test_cipher_aes_async_proc, loop);
    timeout_id = g_timeout_add_seconds(TEST_TIMEOUT, test_timeout, loop);
    g_main_loop_run(loop);
    g_source_remove(timeout_id);
    foil_cipher_unref(cipher);
    dec = foil_output_free_to_bytes(out);
    g_assert(dec);
    dec2 = g_bytes_new_from_bytes(dec, 0, test->in.size);

    GDEBUG("Plain text:");
    TEST_DEBUG_HEXDUMP_DATA(&test->in);
    GDEBUG("Encrypted (%u bytes):", (guint)g_bytes_get_size(enc));
    TEST_DEBUG_HEXDUMP_BYTES(enc);
    GDEBUG("Decrypted:");
    TEST_DEBUG_HEXDUMP_BYTES(dec);
    g_assert(gutil_bytes_equal_data(dec2, &test->in));
    g_bytes_unref(enc);
    g_bytes_unref(dec);
    g_bytes_unref(dec2);
    foil_key_unref(key);
    g_main_loop_unref(loop);
    g_free(key_path);
}

static
void
test_cipher_aes_vector(
    gconstpointer param)
{
    const TestCipherAesVector* test = param;
    GBytes* key_bytes = gutil_hex2bytes(test->key_iv, -1);
    GBytes* in_bytes = gutil_hex2bytes(test->in, -1);
    FoilKey* key = foil_key_new_from_bytes(test->key_type(), key_bytes);
    GBytes* out = foil_cipher_bytes(test->cipher_type(), key, in_bytes);
    GBytes* out_expected = gutil_hex2bytes(test->out, -1);

    GDEBUG("Key+IV:");
    g_assert(key_bytes);
    TEST_DEBUG_HEXDUMP_BYTES(key_bytes);

    GDEBUG("In:");
    g_assert(in_bytes);
    TEST_DEBUG_HEXDUMP_BYTES(in_bytes);

    GDEBUG("Out:");
    g_assert(out);
    TEST_DEBUG_HEXDUMP_BYTES(out);

    g_assert(out_expected);
    g_assert(g_bytes_equal(out, out_expected));

    g_bytes_unref(key_bytes);
    g_bytes_unref(in_bytes);
    g_bytes_unref(out);
    g_bytes_unref(out_expected);
    foil_key_unref(key);
}

static const char input_short[] = "This is a secret.This is a secr";
static const char input_long[] =
    "When in the Course of human events, it becomes necessary for one "
    "people to dissolve the political bands which have connected them "
    "with another, and to assume among the powers of the earth, the "
    "separate and equal station to which the Laws of Nature and of "
    "Nature's God entitle them, a decent respect to the opinions of "
    "mankind requires that they should declare the causes which impel "
    "them to the separation.\n\n"
    "We hold these truths to be self-evident, that all men are created "
    "equal, that they are endowed by their Creator with certain "
    "unalienable Rights, that among these are Life, Liberty and the "
    "pursuit of Happiness.--That to secure these rights, Governments "
    "are instituted among Men, deriving their just powers from the "
    "consent of the governed, --That whenever any Form of Government "
    "becomes destructive of these ends, it is the Right of the People "
    "to alter or to abolish it, and to institute new Government, laying "
    "its foundation on such principles and organizing its powers in such "
    "form, as to them shall seem most likely to effect their Safety and "
    "Happiness. Prudence, indeed, will dictate that Governments long "
    "established should not be changed for light and transient causes; "
    "and accordingly all experience hath shewn, that mankind are more "
    "disposed to suffer, while evils are sufferable, than to right "
    "themselves by abolishing the forms to which they are accustomed. "
    "But when a long train of abuses and usurpations, pursuing "
    "invariably the same Object evinces a design to reduce them under "
    "absolute Despotism, it is their right, it is their duty, to throw "
    "off such Government, and to provide new Guards for their future "
    "security.--Such has been the patient sufferance of these Colonies; "
    "and such is now the necessity which constrains them to alter their "
    "former Systems of Government. The history of the present King of "
    "Great Britain is a history of repeated injuries and usurpations, "
    "all having in direct object the establishment of an absolute "
    "Tyranny over these States. To prove this, let Facts be submitted "
    "to a candid world.\n";

#define TEST_(name) "/cipher_aes/" name
#define TEST_BASIC(bits,mode) \
    { TEST_("basic" #bits "-" #mode), test_cipher_aes_basic, \
      "aes" #bits, foil_key_aes##bits##_get_type, \
      foil_impl_cipher_aes_##mode##_encrypt_get_type, \
      foil_impl_cipher_aes_##mode##_decrypt_get_type}
#define TEST_CLONE_(bits,mode,name) \
    { TEST_("clone" #bits "-" #mode "-" #name), \
      test_cipher_aes_clone, "aes" #bits, foil_key_aes##bits##_get_type, \
      foil_impl_cipher_aes_##mode##_encrypt_get_type, \
      foil_impl_cipher_aes_##mode##_decrypt_get_type, \
      { (const void*) input_##name, sizeof(input_##name) } }
#define TEST_CLONE(bits,name) \
    TEST_CLONE_(bits,cbc,name), \
    TEST_CLONE_(bits,cfb,name), \
    TEST_CLONE_(bits,ctr,name), \
    TEST_CLONE_(bits,ecb,name)
#define TEST_SYNC_(bits,mode,name) \
    { TEST_("sync" #bits "-" #mode "-" #name), \
      test_cipher_aes_sync, "aes" #bits, foil_key_aes##bits##_get_type, \
      foil_impl_cipher_aes_##mode##_encrypt_get_type, \
      foil_impl_cipher_aes_##mode##_decrypt_get_type, \
      { (const void*) input_##name, sizeof(input_##name) } }
#define TEST_SYNC(bits,name) \
    TEST_SYNC_(bits,cbc,name), \
    TEST_SYNC_(bits,cfb,name), \
    TEST_SYNC_(bits,ctr,name), \
    TEST_SYNC_(bits,ecb,name)
#define TEST_ASYNC_(bits,mode,name) \
    { TEST_("async" #bits "-" #mode "-" #name), \
      test_cipher_aes_async, "aes" #bits, foil_key_aes##bits##_get_type, \
      foil_impl_cipher_aes_##mode##_encrypt_get_type, \
      foil_impl_cipher_aes_##mode##_decrypt_get_type, \
      { (const void*) input_##name, sizeof(input_##name) } }
#define TEST_ASYNC(bits,name) \
    TEST_ASYNC_(bits,cbc,name), \
    TEST_ASYNC_(bits,cfb,name), \
    TEST_ASYNC_(bits,ctr,name), \
    TEST_ASYNC_(bits,ecb,name)

static const TestCipherAes tests[] = {
    { TEST_("cancel"), test_cipher_aes_cancel, "aes128" },
    TEST_BASIC(128,cbc),
    TEST_BASIC(128,cfb),
    TEST_BASIC(128,ctr),
    TEST_BASIC(192,cbc),
    TEST_BASIC(192,cfb),
    TEST_BASIC(192,ctr),
    TEST_BASIC(256,cbc),
    TEST_BASIC(256,cfb),
    TEST_BASIC(256,ctr),
    TEST_CLONE(128,short),
    TEST_CLONE(192,short),
    TEST_CLONE(256,short),
    TEST_CLONE(128,long),
    TEST_CLONE(192,long),
    TEST_CLONE(256,long),
    TEST_SYNC(128,short),
    TEST_SYNC(192,short),
    TEST_SYNC(256,short),
    TEST_SYNC(128,long),
    TEST_SYNC(192,long),
    TEST_SYNC(256,long),
    TEST_ASYNC(128,short),
    TEST_ASYNC(192,short),
    TEST_ASYNC(256,short),
    TEST_ASYNC(128,long),
    TEST_ASYNC(192,long),
    TEST_ASYNC(256,long),
};

static const char pad_input[] = "This is a secret.";
static const guint8 pad_output_128_cbc[] = {
    0x45, 0x94, 0x6d, 0x37, 0xf9, 0xe5, 0x94, 0x20,
    0xce, 0x15, 0xd9, 0xa0, 0xe2, 0x47, 0x98, 0xf8,
    0x10, 0x9f, 0x21, 0x27, 0x1b, 0x39, 0x1b, 0xcb,
    0xd1, 0xec, 0x20, 0x54, 0x3e, 0x26, 0xa0, 0xf4
};
static const guint8 pad_output_128_cfb[] = {
    0x53, 0x96, 0x86, 0x07, 0xc1, 0xbc, 0x70, 0x4e,
    0xf1, 0x2e, 0x9d, 0x74, 0xed, 0xe6, 0xf7, 0xe7,
    0x10, 0xb7, 0xb9, 0x40, 0x5a, 0x33, 0x57, 0x31,
    0x87, 0x86, 0x20, 0x92, 0x2b, 0x6d, 0x78, 0x21
};
static const guint8 pad_output_128_ctr[] = {
    0x53, 0x96, 0x86, 0x07, 0xc1, 0xbc, 0x70, 0x4e,
    0xf1, 0x2e, 0x9d, 0x74, 0xed, 0xe6, 0xf7, 0xe7,
    0xbc, 0xc2, 0xc5, 0x05, 0xda, 0x0f, 0x86, 0x07,
    0x40, 0x7e, 0x75, 0x6b, 0x81, 0xf2, 0xd2, 0x05
};
static const guint8 pad_output_192_cbc[] = {
    0x8c, 0x14, 0xf6, 0x8c, 0x4b, 0x96, 0x76, 0x65,
    0xa1, 0xc5, 0xa2, 0x60, 0x73, 0x95, 0xfd, 0x15,
    0x20, 0x5e, 0x78, 0x59, 0xc1, 0xf1, 0xb2, 0x71,
    0xaf, 0x46, 0x36, 0x74, 0x45, 0x92, 0x9c, 0xc6
};
static const guint8 pad_output_192_cfb[] = {
    0x54, 0x08, 0xd6, 0x8d, 0x66, 0xea, 0x38, 0x98,
    0xbb, 0x7c, 0x8a, 0xc3, 0x7c, 0x80, 0x45, 0xda,
    0xe4, 0x07, 0xce, 0x81, 0x29, 0x2a, 0x1d, 0x5b,
    0x0a, 0xb5, 0x4b, 0x48, 0xd6, 0xc1, 0xa9, 0x64
};
static const guint8 pad_output_192_ctr[] = {
    0x54, 0x08, 0xd6, 0x8d, 0x66, 0xea, 0x38, 0x98,
    0xbb, 0x7c, 0x8a, 0xc3, 0x7c, 0x80, 0x45, 0xda,
    0x07, 0xf7, 0x63, 0xe9, 0xa3, 0x76, 0xc2, 0xba,
    0xa3, 0x53, 0x9c, 0x80, 0x18, 0x8f, 0x49, 0xf8
};
static const guint8 pad_output_256_cbc[] = {
    0x7b, 0x13, 0xa2, 0x1d, 0xe0, 0x08, 0x4d, 0xa6,
    0x92, 0x88, 0x03, 0xd9, 0xfb, 0x44, 0x28, 0x85,
    0xa4, 0x3a, 0x33, 0x84, 0x4d, 0x19, 0x6d, 0x0f,
    0x82, 0xe2, 0x26, 0x9d, 0x42, 0xa4, 0x32, 0xc7
};
static const guint8 pad_output_256_cfb[] = {
    0x0e, 0x06, 0x6d, 0x24, 0x28, 0x92, 0x02, 0xb6,
    0x91, 0x0e, 0x26, 0x58, 0x61, 0xb1, 0xc3, 0xe6,
    0xd4, 0x29, 0xf8, 0x97, 0xf9, 0xcb, 0x94, 0xb9,
    0x8f, 0xe3, 0xd9, 0x07, 0xb7, 0xf4, 0x8a, 0x10
};
static const guint8 pad_output_256_ctr[] = {
    0x0e, 0x06, 0x6d, 0x24, 0x28, 0x92, 0x02, 0xb6,
    0x91, 0x0e, 0x26, 0x58, 0x61, 0xb1, 0xc3, 0xe6,
    0x4e, 0xf3, 0x10, 0xd4, 0xc1, 0x86, 0x5c, 0x58,
    0x53, 0x11, 0xf3, 0x58, 0x78, 0xee, 0x2c, 0xc2
};

#define TEST_PAD(bits,mode)           \
    { { TEST_("pad" #bits "-" #mode), \
    test_cipher_aes_padding, "aes" #bits, foil_key_aes##bits##_get_type, \
    foil_impl_cipher_aes_##mode##_encrypt_get_type, \
    foil_impl_cipher_aes_##mode##_decrypt_get_type, \
    { (const void*) pad_input, sizeof(pad_input) } }, \
    { TEST_ARRAY_AND_SIZE(pad_output_##bits##_##mode) } }

static const TestCipherAesOut out_tests[] = {
    TEST_PAD(128,cbc),
    TEST_PAD(128,cfb),
    TEST_PAD(128,ctr),
    TEST_PAD(192,cbc),
    TEST_PAD(192,cfb),
    TEST_PAD(192,ctr),
    TEST_PAD(256,cbc),
    TEST_PAD(256,cfb),
    TEST_PAD(256,ctr)
};

/* Examples from NIST Special Publication 800-38A */
static const TestCipherAesVector test_vectors[] = {
#define TEST_VECTOR_(x) TEST_("vector/" x)
#define TEST_VECTOR_INPUT \
    "6bc1bee22e409f96e93d7e117393172a" \
    "ae2d8a571e03ac9c9eb76fac45af8e51" \
    "30c81c46a35ce411e5fbc1191a0a52ef" \
    "f69f2445df4f9b17ad2b417be66c3710"
#define TEST_VECTOR_ENCRYPT_DECRYPT(MODE,mode,bits,key,out) { \
    TEST_VECTOR_(#MODE "-AES" #bits ".Encrypt"), #key, \
    TEST_VECTOR_INPUT, #out, foil_key_aes##bits##_get_type, \
    foil_impl_cipher_aes_##mode##_encrypt_get_type },{ \
    TEST_VECTOR_(#MODE "-AES" #bits ".Decrypt"), #key, \
    #out, TEST_VECTOR_INPUT, foil_key_aes##bits##_get_type, \
    foil_impl_cipher_aes_##mode##_decrypt_get_type }

    /* F.1.1 ECB-AES128.Encrypt */
    /* F.1.2 ECB-AES128.Decrypt */
    TEST_VECTOR_ENCRYPT_DECRYPT(ECB,ecb,128,
        2b7e151628aed2a6abf7158809cf4f3c\
00000000000000000000000000000000,
        3ad77bb40d7a3660a89ecaf32466ef97\
f5d3d58503b9699de785895a96fdbaaf\
43b1cd7f598ece23881b00e3ed030688\
7b0c785e27e8ad3f8223207104725dd4),

    /* F.1.3 ECB-AES192.Encrypt */
    /* F.1.4 ECB-AES192.Decrypt */
    TEST_VECTOR_ENCRYPT_DECRYPT(ECB,ecb,192,
        8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b\
00000000000000000000000000000000,
        bd334f1d6e45f25ff712a214571fa5cc\
974104846d0ad3ad7734ecb3ecee4eef\
ef7afd2270e2e60adce0ba2face6444e\
9a4b41ba738d6c72fb16691603c18e0e),

    /* F.1.5 ECB-AES256.Encrypt */
    /* F.1.6 ECB-AES256.Decrypt */
    TEST_VECTOR_ENCRYPT_DECRYPT(ECB,ecb,256,
        603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4\
00000000000000000000000000000000,
        f3eed1bdb5d2a03c064b5a7e3db181f8\
591ccb10d410ed26dc5ba74a31362870\
b6ed21b99ca6f4f9f153e7b1beafed1d\
23304b7a39f9f3ff067d8d8f9e24ecc7),

    /* F.2.1 CBC-AES128.Encrypt */
    /* F.2.2 CBC-AES128.Decrypt */
    TEST_VECTOR_ENCRYPT_DECRYPT(CBC,cbc,128,
        2b7e151628aed2a6abf7158809cf4f3c\
000102030405060708090a0b0c0d0e0f,
        7649abac8119b246cee98e9b12e9197d\
5086cb9b507219ee95db113a917678b2\
73bed6b8e3c1743b7116e69e22229516\
3ff1caa1681fac09120eca307586e1a7),

    /* F.2.3 CBC-AES192.Encrypt */
    /* F.2.4 CBC-AES192.Decrypt */
    TEST_VECTOR_ENCRYPT_DECRYPT(CBC,cbc,192,
        8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b\
000102030405060708090a0b0c0d0e0f,
        4f021db243bc633d7178183a9fa071e8\
b4d9ada9ad7dedf4e5e738763f69145a\
571b242012fb7ae07fa9baac3df102e0\
08b0e27988598881d920a9e64f5615cd),

    /* F.2.5 CBC-AES256.Encrypt */
    /* F.2.6 CBC-AES256.Decrypt */
    TEST_VECTOR_ENCRYPT_DECRYPT(CBC,cbc,256,
        603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4\
000102030405060708090a0b0c0d0e0f,
        f58c4c04d6e5f1ba779eabfb5f7bfbd6\
9cfc4e967edb808d679f777bc6702c7d\
39f23369a9d9bacfa530e26304231461\
b2eb05e2c39be9fcda6c19078c6a9d1b),

    /* F.3.13 CFB128-AES128.Encrypt */
    /* F.3.14 CFB128-AES128.Decrypt */
    TEST_VECTOR_ENCRYPT_DECRYPT(CFB,cfb,128,
        2b7e151628aed2a6abf7158809cf4f3c\
000102030405060708090a0b0c0d0e0f,
        3b3fd92eb72dad20333449f8e83cfb4a\
c8a64537a0b3a93fcde3cdad9f1ce58b\
26751f67a3cbb140b1808cf187a4f4df\
c04b05357c5d1c0eeac4c66f9ff7f2e6),

    /* F.3.15 CFB128-AES192.Encrypt */
    /* F.3.16 CFB128-AES192.Decrypt */
    TEST_VECTOR_ENCRYPT_DECRYPT(CFB,cfb,192,
        8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b\
000102030405060708090a0b0c0d0e0f,
        cdc80d6fddf18cab34c25909c99a4174\
67ce7f7f81173621961a2b70171d3d7a\
2e1e8a1dd59b88b1c8e60fed1efac4c9\
c05f9f9ca9834fa042ae8fba584b09ff),

    /* F.3.17 CFB128-AES256.Encrypt */
    /* F.3.18 CFB128-AES256.Decrypt */
    TEST_VECTOR_ENCRYPT_DECRYPT(CFB,cfb,256,
        603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4\
000102030405060708090a0b0c0d0e0f,
        dc7e84bfda79164b7ecd8486985d3860\
39ffed143b28b1c832113c6331e5407b\
df10132415e54b92a13ed0a8267ae2f9\
75a385741ab9cef82031623d55b1e471),

    /* F.5.1 CTR-AES128.Encrypt */
    /* F.5.2 CTR-AES128.Decrypt */
    TEST_VECTOR_ENCRYPT_DECRYPT(CTR,ctr,128,
    2b7e151628aed2a6abf7158809cf4f3c\
f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff,
    874d6191b620e3261bef6864990db6ce\
9806f66b7970fdff8617187bb9fffdff\
5ae4df3edbd5d35e5b4f09020db03eab\
1e031dda2fbe03d1792170a0f3009cee),

    /* F.5.3 CTR-AES192.Encrypt */
    /* F.5.4 CTR-AES192.Decrypt */
    TEST_VECTOR_ENCRYPT_DECRYPT(CTR,ctr,192,\
    8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b\
f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff,\
    1abc932417521ca24f2b0459fe7e6e0b\
090339ec0aa6faefd5ccc2c6f4ce8e94\
1e36b26bd1ebc670d1bd1d665620abf7\
4f78a7f6d29809585a97daec58c6b050),

    /* F.5.5 CTR-AES256.Encrypt */
    /* F.5.6 CTR-AES256.Decrypt */
    TEST_VECTOR_ENCRYPT_DECRYPT(CTR,ctr,256,\
    603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4\
f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff,\
    601ec313775789a5b7a7f504bbf3d228\
f443e3ca4d62b59aca84e990cacaf5c5\
2b0930daa23de94ce87017ba2d84988d\
dfc9c58db67aada613c2dd08457941a6),
};

int main(int argc, char* argv[])
{
    guint i;
    g_test_init(&argc, &argv, NULL);
    for (i = 0; i < G_N_ELEMENTS(tests); i++) {
        g_test_add_data_func(tests[i].name, tests + i, tests[i].fn);
    }
    for (i = 0; i < G_N_ELEMENTS(out_tests); i++) {
        g_test_add_data_func(out_tests[i].aes.name, out_tests + i,
            out_tests[i].aes.fn);
    }
    for (i = 0; i < G_N_ELEMENTS(test_vectors); i++) {
        g_test_add_data_func(test_vectors[i].name, test_vectors + i,
            test_cipher_aes_vector);
    }
    return test_run();
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

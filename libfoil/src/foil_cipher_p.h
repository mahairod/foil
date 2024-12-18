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

#ifndef FOIL_CIPHER_P_H
#define FOIL_CIPHER_P_H

#include "foil_types_p.h"
#include "foil_cipher.h"

typedef enum foil_cipher_flags {
    FOIL_CIPHER_DEFAULT   = 0x00,
    FOIL_CIPHER_SYMMETRIC = 0x01,
    FOIL_CIPHER_ENCRYPT   = 0x02,
    FOIL_CIPHER_DECRYPT   = 0x04
} FOIL_CIPHER_FLAGS;

typedef struct foil_cipher_class FoilCipherClass;
typedef struct foil_cipher_priv FoilCipherPriv;
struct foil_cipher_class {
    GObjectClass object;
    const char* name;
    FOIL_CIPHER_FLAGS flags;
    FoilCipherPaddingFunc fn_pad; /* Default padding */
    gboolean (*fn_supports_key)(FoilCipherClass* klass, GType key_type);
    void (*fn_init_with_key)(FoilCipher* cipher, FoilKey* key);
    void (*fn_copy)(FoilCipher* dest, FoilCipher* src);
    int (*fn_step)(FoilCipher* cipher, const void* in, void* out);
    int (*fn_finish)(FoilCipher* cipher, const void* in, int n, void* out);
};

struct foil_cipher {
    GObject object;
    FoilCipherPriv* priv;
    FoilKey* key;
    FoilCipherPaddingFunc fn_pad;
    int input_block_size;
    int output_block_size;
};

GType foil_cipher_get_type(void) FOIL_INTERNAL;
#define FOIL_TYPE_CIPHER (foil_cipher_get_type())
#define FOIL_CIPHER(obj) (G_TYPE_CHECK_INSTANCE_CAST(obj, \
        FOIL_TYPE_CIPHER, FoilCipher))
#define FOIL_IS_CIPHER(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, \
        FOIL_TYPE_CIPHER)
#define FOIL_CIPHER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
        FOIL_TYPE_CIPHER, FoilCipherClass))
#define FOIL_CIPHER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
        FOIL_TYPE_CIPHER, FoilCipherClass))

typedef struct foil_cipher_run {
    const FoilBytes* blocks;
    guint nblocks;
    guint current_block;
    gsize current_offset;
    gsize bytes_total;
    gsize bytes_left;
    const guint8* in_ptr;
    guint8* in_buf;
    gsize in_len;
    guint in_block_size;
} FoilCipherRun;

void
foil_cipher_run_init(
    FoilCipher* self,
    FoilCipherRun* run,
    const FoilBytes* blocks,
    guint nblocks)
    FOIL_INTERNAL;

void
foil_cipher_run_next(
    FoilCipherRun* run)
    FOIL_INTERNAL;

void
foil_cipher_run_deinit(
    FoilCipherRun* run)
    FOIL_INTERNAL;

void
foil_cipher_priv_add(
    FoilCipherClass* klass)
    FOIL_INTERNAL;

FoilCipherPriv*
foil_cipher_priv_get(
    FoilCipher* cipher)
    FOIL_INTERNAL;

void
foil_cipher_priv_finalize(
    FoilCipherPriv* priv)
    FOIL_INTERNAL;

void
foil_cipher_priv_cancel_all(
    FoilCipherPriv* priv)
    FOIL_INTERNAL;

int
foil_cipher_symmetric_finish(
    FoilCipher* cipher,
    const void* from,
    int flen,
    void* to)
    FOIL_INTERNAL;

#endif /* FOIL_CIPHER_P_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

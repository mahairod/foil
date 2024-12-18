/*
 * Copyright (C) 2016-2022 by Slava Monich <slava@monich.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *   3. Neither the names of the copyright holders nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * any official policies, either expressed or implied.
 */

#ifndef FOIL_DIGEST_H
#define FOIL_DIGEST_H

#include "foil_types.h"

#include <glib-object.h>

G_BEGIN_DECLS

/* Digest algorithm */
gsize
foil_digest_type_size(
    GType type);

const char*
foil_digest_type_name(
    GType type);

GBytes*
foil_digest_data(
    GType type,
    const void* data,
    gsize size);

gboolean
foil_digest_data_buf(
    GType type,
    const void* data,
    gsize size,
    void* digest); /* Since 1.0.27 */

GBytes*
foil_digest_bytes(
    GType type,
    GBytes* bytes);

/* Single digest */

FoilDigest*
foil_digest_new(
    GType type);

FoilDigest*
foil_digest_ref(
    FoilDigest* digest);

void
foil_digest_unref(
    FoilDigest* digest);

gsize
foil_digest_size(
    FoilDigest* digest);

const char*
foil_digest_name(
    FoilDigest* digest);

FoilDigest*
foil_digest_clone(
    FoilDigest* digest); /* Since 1.0.8 */

gboolean
foil_digest_copy(
    FoilDigest* digest,
    FoilDigest* source); /* Since 1.0.8 */

gboolean
foil_digest_reset(
    FoilDigest* digest); /* Since 1.0.27 */

gboolean
foil_digest_update(
    FoilDigest* digest,
    const void* data,
    gsize size); /* Has return value since 1.0.26 */

gboolean
foil_digest_update_bytes(
    FoilDigest* self,
    GBytes* bytes); /* Has return value since 1.0.26 */

GBytes*
foil_digest_finish(
    FoilDigest* digest);

GBytes*
foil_digest_free_to_bytes(
    FoilDigest* digest);

/* Implementation types */
GType foil_impl_digest_md5_get_type(void);
GType foil_impl_digest_sha1_get_type(void);
GType foil_impl_digest_sha256_get_type(void);
GType foil_impl_digest_sha512_get_type(void); /* Since 1.0.22 */
#define FOIL_DIGEST_MD5 (foil_impl_digest_md5_get_type())
#define FOIL_DIGEST_SHA1 (foil_impl_digest_sha1_get_type())
#define FOIL_DIGEST_SHA256 (foil_impl_digest_sha256_get_type())
#define FOIL_DIGEST_SHA512 (foil_impl_digest_sha512_get_type())

#define foil_digest_new_md5() foil_digest_new(FOIL_DIGEST_MD5)
#define foil_digest_new_sha1() foil_digest_new(FOIL_DIGEST_SHA1)
#define foil_digest_new_sha256() foil_digest_new(FOIL_DIGEST_SHA256)
#define foil_digest_new_sha512() foil_digest_new(FOIL_DIGEST_SHA512)

G_END_DECLS

#endif /* FOIL_DIGEST_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

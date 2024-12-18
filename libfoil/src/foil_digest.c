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

#include "foil_digest_p.h"
#include "foil_util_p.h"

/* Logging */
#define GLOG_MODULE_NAME foil_log_digest
#include "foil_log_p.h"
GLOG_MODULE_DEFINE2("foil-digest", FOIL_LOG_MODULE);

G_DEFINE_ABSTRACT_TYPE(FoilDigest, foil_digest, G_TYPE_OBJECT);
#define FOIL_DIGEST(obj) (G_TYPE_CHECK_INSTANCE_CAST(obj, \
        FOIL_TYPE_DIGEST, FoilDigest))
#define FOIL_IS_DIGEST(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, \
        FOIL_TYPE_DIGEST)
#define FOIL_DIGEST_GET_CLASS(obj) G_TYPE_INSTANCE_GET_CLASS((obj),\
        FOIL_TYPE_DIGEST, FoilDigestClass)
#define foil_digest_class_ref(type) ((FoilDigestClass*)foil_class_ref(type, \
        FOIL_TYPE_DIGEST))

gsize
foil_digest_type_size(
    GType type)
{
    gsize size = 0;
    FoilDigestClass* klass = foil_digest_class_ref(type);

    if (G_LIKELY(klass)) {
        size = klass->size;
        g_type_class_unref(klass);
    }
    return size;
}

gsize
foil_digest_type_block_size(
    GType type)
{
    gsize size = 0;
    FoilDigestClass* klass = foil_digest_class_ref(type);

    if (G_LIKELY(klass)) {
        size = klass->block_size;
        g_type_class_unref(klass);
    }
    return size;
}

const char*
foil_digest_type_name(
    GType type)
{
    const char* name = NULL;
    FoilDigestClass* klass = foil_digest_class_ref(type);

    if (G_LIKELY(klass)) {
        name = klass->name;
        g_type_class_unref(klass);
    }
    return name;
}

GBytes*
foil_digest_data(
    GType type,
    const void* data,
    gsize size)
{
    GBytes* result = NULL;

    if (G_LIKELY(data || !size)) {
        FoilDigestClass* klass = foil_digest_class_ref(type);

        if (G_LIKELY(klass)) {
            void* digest = klass->fn_digest_alloc();

            klass->fn_digest(data, size, digest);
            result = g_bytes_new_with_free_func(digest, klass->size,
                klass->fn_digest_free, digest);
            g_type_class_unref(klass);
        }
    }
    return result;
}

gboolean
foil_digest_data_buf(
    GType type,
    const void* data,
    gsize size,
    void* digest) /* Since 1.0.27 */
{
    /* The output buffer is supposed to be large enough */
    if (G_LIKELY(data || !size) && G_LIKELY(digest)) {
        FoilDigestClass* klass = foil_digest_class_ref(type);

        if (G_LIKELY(klass)) {
            klass->fn_digest(data, size, digest);
            g_type_class_unref(klass);
            return TRUE;
        }
    }
    return FALSE;
}

GBytes*
foil_digest_bytes(
    GType type,
    GBytes* bytes)
{
    if (G_LIKELY(bytes)) {
        gsize size = 0;
        const void* data = g_bytes_get_data(bytes, &size);

        return foil_digest_data(type, data, size);
    } else {
        return NULL;
    }
}

gsize
foil_digest_size(
    FoilDigest* self)
{
    return G_LIKELY(self) ? FOIL_DIGEST_GET_CLASS(self)->size : 0;
}

gsize
foil_digest_block_size(
    FoilDigest* self)
{
    return G_LIKELY(self) ? FOIL_DIGEST_GET_CLASS(self)->block_size : 0;
}

const char*
foil_digest_name(
    FoilDigest* self)
{
    return G_LIKELY(self) ? FOIL_DIGEST_GET_CLASS(self)->name : NULL;
}

FoilDigest*
foil_digest_new(
    GType type)
{
    FoilDigest* digest = NULL;
    FoilDigestClass* klass = foil_digest_class_ref(type);

    if (G_LIKELY(klass)) {
        digest = g_object_new(type, NULL);
        g_type_class_unref(klass);
    }
    return digest;
}

FoilDigest*
foil_digest_ref(
     FoilDigest* self)
{
    if (G_LIKELY(self)) {
        GASSERT(FOIL_IS_DIGEST(self));
        g_object_ref(self);
    }
    return self;
}

void
foil_digest_unref(
     FoilDigest* self)
{
    if (G_LIKELY(self)) {
        GASSERT(FOIL_IS_DIGEST(self));
        g_object_unref(self);
    }
}

FoilDigest*
foil_digest_clone(
    FoilDigest* self) /* Since 1.0.8 */
{
    if (G_LIKELY(self)) {
        GType type = G_TYPE_FROM_INSTANCE(self);
        FoilDigest* clone = foil_digest_new(type);

        if (foil_digest_copy(clone, self)) {
            return clone;
        }
        foil_digest_unref(clone);
    }
    return NULL;
}

gboolean
foil_digest_copy(
    FoilDigest* self,
    FoilDigest* source) /* Since 1.0.8 */
{
    if (G_LIKELY(self) && G_LIKELY(source)) {
        if (self == source) {
            /* Same object, nothing to copy */
            return TRUE;
        } else {
            /* Both must be of the same class */
            FoilDigestClass* klass = FOIL_DIGEST_GET_CLASS(self);

            if (klass == FOIL_DIGEST_GET_CLASS(source) && klass->fn_copy) {
                klass->fn_copy(self, source);
                if (self->result) {
                    g_bytes_unref(self->result);
                    self->result = NULL;
                }
                if (source->result) {
                    self->result = g_bytes_ref(source->result);
                }
                return TRUE;
            }
        }
    }
    return FALSE;
}

gboolean
foil_digest_reset(
    FoilDigest* self) /* Since 1.0.27 */
{
    if (G_LIKELY(self)) {
        FoilDigestClass* klass = FOIL_DIGEST_GET_CLASS(self);

        if (klass->fn_reset) {
            klass->fn_reset(self);
            if (self->result) {
                g_bytes_unref(self->result);
                self->result = NULL;
            }
            return TRUE;
        }
    }
    return FALSE;
}

gboolean
foil_digest_update(
    FoilDigest* self,
    const void* data,
    gsize size) /* Has return value since 1.0.26 */
{
    if (G_LIKELY(self) && G_LIKELY(!self->result)) {
        FOIL_DIGEST_GET_CLASS(self)->fn_update(self, data, size);
        return TRUE;
    } else {
        return FALSE;
    }
}

gboolean
foil_digest_update_bytes(
    FoilDigest* self,
    GBytes* bytes) /* Has return value since 1.0.26 */
{
    if (G_LIKELY(self) && G_LIKELY(bytes) && G_LIKELY(!self->result)) {
        gsize size = 0;
        const void* data = g_bytes_get_data(bytes, &size);

        FOIL_DIGEST_GET_CLASS(self)->fn_update(self, data, size);
        return TRUE;
    } else {
        return FALSE;
    }
}

GBytes*
foil_digest_finish(
    FoilDigest* self)
{
    if (G_LIKELY(self)) {
        if (!self->result) {
            FoilDigestClass* klass = FOIL_DIGEST_GET_CLASS(self);
            void* data = klass->fn_digest_alloc();

            FOIL_DIGEST_GET_CLASS(self)->fn_finish(self, data);
            self->result = g_bytes_new_with_free_func(data, klass->size,
                klass->fn_digest_free, data);
        }
        return self->result;
    }
    return NULL;
}

GBytes*
foil_digest_free_to_bytes(
    FoilDigest* self)
{
    if (G_LIKELY(self)) {
        GBytes* bytes = foil_digest_finish(self);

        g_bytes_ref(bytes);
        foil_digest_unref(self);
        return bytes;
    }
    return NULL;
}

static
void
foil_digest_finalize(
    GObject* object)
{
    FoilDigest* self = FOIL_DIGEST(object);

    if (self->result) {
        g_bytes_unref(self->result);
    } else {
        /* This clears the internal buffers */
        FOIL_DIGEST_GET_CLASS(self)->fn_finish(self, NULL);
    }
    G_OBJECT_CLASS(foil_digest_parent_class)->finalize(object);
}

static
void
foil_digest_init(
    FoilDigest* self)
{
}

static
void
foil_digest_class_init(
    FoilDigestClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = foil_digest_finalize;
}

/* Callbacks for generic FoilDigest/FoilHmac actions */

void
foil_digest_update_digest(
    void* digest,
    const void* data,
    gsize size)
{
    foil_digest_update(digest, data, size);
}

void
foil_digest_unref_digest(
    void* digest)
{
    foil_digest_unref(digest);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

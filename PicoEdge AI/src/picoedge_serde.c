#include "picoedge.h"
#include <errno.h>

uint16_t pe_crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

ssize_t pe_export_knowledge(const struct pe_context *ctx, uint8_t *out_buf, size_t max_len) {
    if (!ctx || !out_buf || max_len < sizeof(pe_knowledge_blob_t)) {
        return -EINVAL;
    }

    pe_knowledge_blob_t *blob = (pe_knowledge_blob_t *)out_buf;
    memset(blob, 0, sizeof(*blob));
    blob->magic = PE_KNOWLEDGE_MAGIC;
    blob->version = 0x0001;
    blob->payload_len = sizeof(pe_ad_engine_t) + sizeof(pe_clf_engine_t);
    memcpy(&blob->ad, &ctx->ad, sizeof(pe_ad_engine_t));
    memcpy(&blob->clf, &ctx->clf, sizeof(pe_clf_engine_t));

    blob->crc16 = pe_crc16_ccitt((const uint8_t *)blob, offsetof(pe_knowledge_blob_t, crc16));

    return sizeof(pe_knowledge_blob_t);
}

int pe_import_knowledge(struct pe_context *ctx, const uint8_t *in_buf, size_t len) {
    if (!ctx || !in_buf || len != sizeof(pe_knowledge_blob_t)) {
        return -EINVAL;
    }

    const pe_knowledge_blob_t *blob = (const pe_knowledge_blob_t *)in_buf;

    if (blob->magic != PE_KNOWLEDGE_MAGIC || blob->version != 0x0001) {
        return -EINVAL;
    }

    uint16_t crc = pe_crc16_ccitt(in_buf, offsetof(pe_knowledge_blob_t, crc16));
    if (crc != blob->crc16) {
        return -EINVAL;
    }

    memcpy(&ctx->ad, &blob->ad, sizeof(pe_ad_engine_t));
    memcpy(&ctx->clf, &blob->clf, sizeof(pe_clf_engine_t));

    return 0;
}

ssize_t pe_export_centroid(const struct pe_context *ctx, bool is_fault_proto, uint8_t slot_id, uint8_t *out_buf, size_t max_len) {
    if (!ctx || !out_buf || max_len < sizeof(pe_centroid_blob_t)) {
        return -EINVAL;
    }

    pe_centroid_blob_t *blob = (pe_centroid_blob_t *)out_buf;
    memset(blob, 0, sizeof(*blob));
    blob->magic = PE_CENTROID_MAGIC;
    blob->is_fault_prototype = is_fault_proto ? 1 : 0;
    blob->slot_id = slot_id;

    if (is_fault_proto) {
        if (slot_id >= CONFIG_PE_MAX_CLASSES || !ctx->clf.class_active[slot_id]) return -EINVAL;
        blob->count = ctx->clf.class_counts[slot_id];
        memcpy(blob->vector, ctx->clf.prototypes[slot_id], sizeof(blob->vector));
        memcpy(blob->variance, ctx->clf.feat_var, sizeof(blob->variance));
    } else {
        if (slot_id >= ctx->ad.active_regimes) return -EINVAL;
        blob->count = ctx->ad.regime_counts[slot_id];
        memcpy(blob->vector, ctx->ad.centroids[slot_id], sizeof(blob->vector));
        // Ad engine uses global variance
        memcpy(blob->variance, ctx->ad.global_m2, sizeof(blob->variance));
    }

    blob->crc16 = pe_crc16_ccitt((const uint8_t *)blob, offsetof(pe_centroid_blob_t, crc16));

    return sizeof(pe_centroid_blob_t);
}

int pe_import_centroid(struct pe_context *ctx, const uint8_t *in_buf, size_t len) {
    if (!ctx || !in_buf || len != sizeof(pe_centroid_blob_t)) {
        return -EINVAL;
    }

    const pe_centroid_blob_t *blob = (const pe_centroid_blob_t *)in_buf;

    if (blob->magic != PE_CENTROID_MAGIC) {
        return -EINVAL;
    }

    uint16_t crc = pe_crc16_ccitt(in_buf, offsetof(pe_centroid_blob_t, crc16));
    if (crc != blob->crc16) {
        return -EINVAL;
    }

    if (blob->is_fault_prototype) {
        if (blob->slot_id >= CONFIG_PE_MAX_CLASSES) return -EINVAL;
        ctx->clf.class_active[blob->slot_id] = 1;
        ctx->clf.class_counts[blob->slot_id] = blob->count;
        memcpy(ctx->clf.prototypes[blob->slot_id], blob->vector, sizeof(blob->vector));
        memcpy(ctx->clf.feat_var, blob->variance, sizeof(blob->variance));
    } else {
        if (blob->slot_id >= CONFIG_PE_MAX_REGIMES) return -EINVAL;
        if (blob->slot_id >= ctx->ad.active_regimes) ctx->ad.active_regimes = blob->slot_id + 1;
        ctx->ad.regime_counts[blob->slot_id] = blob->count;
        memcpy(ctx->ad.centroids[blob->slot_id], blob->vector, sizeof(blob->vector));
        memcpy(ctx->ad.global_m2, blob->variance, sizeof(blob->variance));
    }

    return 0;
}

int pe_hotload_weights(struct pe_context *ctx, const uint8_t *pe_pkg_blob, size_t len) {
    if (!ctx || !pe_pkg_blob || len != sizeof(pe_pkg_blob_t)) {
        return -EINVAL;
    }

    const pe_pkg_blob_t *blob = (const pe_pkg_blob_t *)pe_pkg_blob;

    if (blob->magic != PE_PKG_MAGIC || blob->num_classes > CONFIG_PE_MAX_CLASSES) {
        return -EINVAL;
    }

    uint16_t crc = pe_crc16_ccitt(pe_pkg_blob, offsetof(pe_pkg_blob_t, crc16));
    if (crc != blob->crc16) {
        return -EINVAL;
    }

    memcpy(ctx->clf.feat_mean, blob->feat_mean, sizeof(blob->feat_mean));
    memcpy(ctx->clf.feat_var, blob->feat_std, sizeof(blob->feat_std));
    memcpy(ctx->clf.prototypes, blob->weights, sizeof(blob->weights));
    memcpy(ctx->clf.bias, blob->bias, sizeof(blob->bias));

    ctx->clf.weight_mode = PE_CLF_MODE_HOTLOADED_LINEAR;
    ctx->clf.num_hot_classes = blob->num_classes;
    ctx->active_feats_mask = blob->active_feats_mask;

    return 0;
}

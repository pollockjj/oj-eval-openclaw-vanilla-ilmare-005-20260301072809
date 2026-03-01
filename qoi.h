#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG  = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG  = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG   = 0xc0; 
constexpr uint8_t QOI_OP_RGB_TAG   = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG  = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

/**
 * @brief encode the raw pixel data of an image to qoi format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);

/**
 * @brief decode the qoi format of an image to raw pixel data
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);


bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {
    if ((channels != 3u && channels != 4u) || colorspace > 1u || width == 0u || height == 0u) {
        return false;
    }

    const uint64_t px_num = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);

    // qoi-header part

    // write magic bytes "qoif"
    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    // write image width
    QoiWriteU32(width);
    // write image height
    QoiWriteU32(height);
    // write channel number
    QoiWriteU8(channels);
    // write color space specifier
    QoiWriteU8(colorspace);

    /* qoi-data part */
    int run = 0;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r = 0u, g = 0u, b = 0u, a = 255u;
    uint8_t pre_r = 0u, pre_g = 0u, pre_b = 0u, pre_a = 255u;

    for (uint64_t i = 0; i < px_num; ++i) {
        r = QoiReadU8();
        g = QoiReadU8();
        b = QoiReadU8();
        if (channels == 4u) a = QoiReadU8();
        if (!std::cin.good()) return false;

        if (r == pre_r && g == pre_g && b == pre_b && a == pre_a) {
            ++run;
            if (run == 62 || i + 1 == px_num) {
                QoiWriteU8(static_cast<uint8_t>(QOI_OP_RUN_TAG | (run - 1)));
                run = 0;
            }
            continue;
        }

        if (run > 0) {
            QoiWriteU8(static_cast<uint8_t>(QOI_OP_RUN_TAG | (run - 1)));
            run = 0;
        }

        const int hash_idx = QoiColorHash(r, g, b, a);
        if (history[hash_idx][0] == r && history[hash_idx][1] == g && history[hash_idx][2] == b && history[hash_idx][3] == a) {
            QoiWriteU8(static_cast<uint8_t>(QOI_OP_INDEX_TAG | hash_idx));
        } else {
            history[hash_idx][0] = r;
            history[hash_idx][1] = g;
            history[hash_idx][2] = b;
            history[hash_idx][3] = a;

            if (a == pre_a) {
                const int dr = static_cast<int>(static_cast<int8_t>(r - pre_r));
                const int dg = static_cast<int>(static_cast<int8_t>(g - pre_g));
                const int db = static_cast<int>(static_cast<int8_t>(b - pre_b));

                if (dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1) {
                    QoiWriteU8(static_cast<uint8_t>(QOI_OP_DIFF_TAG | ((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2)));
                } else {
                    const int dr_dg = dr - dg;
                    const int db_dg = db - dg;
                    if (dg >= -32 && dg <= 31 && dr_dg >= -8 && dr_dg <= 7 && db_dg >= -8 && db_dg <= 7) {
                        QoiWriteU8(static_cast<uint8_t>(QOI_OP_LUMA_TAG | (dg + 32)));
                        QoiWriteU8(static_cast<uint8_t>(((dr_dg + 8) << 4) | (db_dg + 8)));
                    } else {
                        QoiWriteU8(QOI_OP_RGB_TAG);
                        QoiWriteU8(r);
                        QoiWriteU8(g);
                        QoiWriteU8(b);
                    }
                }
            } else {
                QoiWriteU8(QOI_OP_RGBA_TAG);
                QoiWriteU8(r);
                QoiWriteU8(g);
                QoiWriteU8(b);
                QoiWriteU8(a);
            }
        }

        pre_r = r;
        pre_g = g;
        pre_b = b;
        pre_a = a;
    }

    // qoi-padding part
    for (size_t i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        QoiWriteU8(QOI_PADDING[i]);
    }

    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {

    char c1 = QoiReadChar();
    char c2 = QoiReadChar();
    char c3 = QoiReadChar();
    char c4 = QoiReadChar();
    if (c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') {
        return false;
    }

    // read image width
    width = QoiReadU32();
    // read image height
    height = QoiReadU32();
    // read channel number
    channels = QoiReadU8();
    // read color space specifier
    colorspace = QoiReadU8();
    if (!std::cin.good()) return false;

    if ((channels != 3u && channels != 4u) || colorspace > 1u || width == 0u || height == 0u) {
        return false;
    }

    const uint64_t px_num = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);

    int run = 0;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r = 0u, g = 0u, b = 0u, a = 255u;

    for (uint64_t i = 0; i < px_num; ++i) {
        if (run > 0) {
            --run;
        } else {
            const uint8_t data = QoiReadU8();
            if (!std::cin.good()) return false;

            if (data == QOI_OP_RGB_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
                if (!std::cin.good()) return false;
            } else if (data == QOI_OP_RGBA_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
                a = QoiReadU8();
                if (!std::cin.good()) return false;
            } else {
                const uint8_t tag = data & QOI_MASK_2;
                if (tag == QOI_OP_INDEX_TAG) {
                    const uint8_t idx = data & 0x3f;
                    r = history[idx][0];
                    g = history[idx][1];
                    b = history[idx][2];
                    a = history[idx][3];
                } else if (tag == QOI_OP_DIFF_TAG) {
                    r = static_cast<uint8_t>(r + (((data >> 4) & 0x03) - 2));
                    g = static_cast<uint8_t>(g + (((data >> 2) & 0x03) - 2));
                    b = static_cast<uint8_t>(b + ((data & 0x03) - 2));
                } else if (tag == QOI_OP_LUMA_TAG) {
                    const uint8_t data2 = QoiReadU8();
                    if (!std::cin.good()) return false;
                    const int dg = (data & 0x3f) - 32;
                    const int dr_dg = ((data2 >> 4) & 0x0f) - 8;
                    const int db_dg = (data2 & 0x0f) - 8;
                    r = static_cast<uint8_t>(r + dg + dr_dg);
                    g = static_cast<uint8_t>(g + dg);
                    b = static_cast<uint8_t>(b + dg + db_dg);
                } else if (tag == QOI_OP_RUN_TAG) {
                    run = data & 0x3f;
                    if (run == 62 || run == 63) return false;
                } else {
                    return false;
                }
            }
        }

        const int hash_idx = QoiColorHash(r, g, b, a);
        history[hash_idx][0] = r;
        history[hash_idx][1] = g;
        history[hash_idx][2] = b;
        history[hash_idx][3] = a;

        QoiWriteU8(r);
        QoiWriteU8(g);
        QoiWriteU8(b);
        if (channels == 4u) QoiWriteU8(a);
    }

    bool valid = true;
    for (size_t i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        if (QoiReadU8() != QOI_PADDING[i]) valid = false;
    }

    return valid;
}

#endif // QOI_FORMAT_CODEC_QOI_H_

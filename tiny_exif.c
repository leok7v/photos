/*
  TinyEXIF.cpp -- A simple ISO C++ library to parse basic EXIF and XMP
                  information from a JPEG file.
  https://github.com/cdcseacave/TinyEXIF
  Copyright (c) 2015-2017 Seacave
  cdc.seacave@gmail.com
  All rights reserved.

  Based on the easyexif library (2013 version)
    https://github.com/mayanklahiri/easyexif
  of Mayank Lahiri (mlahiri@gmail.com).

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
  NO EVENT SHALL THE FREEBSD PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "tiny_exif.h"
#include <stdio.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <ctype.h>
#include "yxml.h"
#include "crt.h"

#ifndef null
#define null ((void*)0)
#endif

#ifdef _MSC_VER
static int strcasecmp(const char* a, const char* b) { return _stricmp(a, b); }
#endif

#ifndef countof
#define countof(a) ((int)(sizeof(a) / sizeof((a)[0])))
#endif

#ifndef traceln
#define traceln(fmt, ...) fprintf(stderr, fmt "\n", __VA_ARGS__)
#endif

// https://www.awaresystems.be/imaging/tiff/tifftags/privateifd/exif.html
/* tags https://github.com/php/php-src/blob/master/ext/exif/exif.c */
#define TAG_GPS_VERSION_ID              0x0000
#define TAG_GPS_LATITUDE_REF            0x0001
#define TAG_GPS_LATITUDE                0x0002
#define TAG_GPS_LONGITUDE_REF           0x0003
#define TAG_GPS_LONGITUDE               0x0004
#define TAG_GPS_ALTITUDE_REF            0x0005
#define TAG_GPS_ALTITUDE                0x0006
#define TAG_GPS_TIME_STAMP              0x0007
#define TAG_GPS_SATELLITES              0x0008
#define TAG_GPS_STATUS                  0x0009
#define TAG_GPS_MEASURE_MODE            0x000A
#define TAG_GPS_DOP                     0x000B
#define TAG_GPS_SPEED_REF               0x000C
#define TAG_GPS_SPEED                   0x000D
#define TAG_GPS_TRACK_REF               0x000E
#define TAG_GPS_TRACK                   0x000F
#define TAG_GPS_IMG_DIRECTION_REF       0x0010
#define TAG_GPS_IMG_DIRECTION           0x0011
#define TAG_GPS_MAP_DATUM               0x0012
#define TAG_GPS_DEST_LATITUDE_REF       0x0013
#define TAG_GPS_DEST_LATITUDE           0x0014
#define TAG_GPS_DEST_LONGITUDE_REF      0x0015
#define TAG_GPS_DEST_LONGITUDE          0x0016
#define TAG_GPS_DEST_BEARING_REF        0x0017
#define TAG_GPS_DEST_BEARING            0x0018
#define TAG_GPS_DEST_DISTANCE_REF       0x0019
#define TAG_GPS_DEST_DISTANCE           0x001A
#define TAG_GPS_PROCESSING_METHOD       0x001B
#define TAG_GPS_AREA_INFORMATION        0x001C
#define TAG_GPS_DATE_STAMP              0x001D
#define TAG_GPS_DIFFERENTIAL            0x001E
#define TAG_TIFF_COMMENT                0x00FE /* SHOULDN'T HAPPEN */
#define TAG_NEW_SUBFILE                 0x00FE /* New version of subfile tag */
#define TAG_SUBFILE_TYPE                0x00FF /* Old version of subfile tag */
#define TAG_IMAGEWIDTH                  0x0100
#define TAG_IMAGEHEIGHT                 0x0101
#define TAG_BITS_PER_SAMPLE             0x0102
#define TAG_COMPRESSION                 0x0103
#define TAG_PHOTOMETRIC_INTERPRETATION  0x0106
#define TAG_TRESHHOLDING                0x0107
#define TAG_CELL_WIDTH                  0x0108
#define TAG_CELL_HEIGHT                 0x0109
#define TAG_FILL_ORDER                  0x010A
#define TAG_DOCUMENT_NAME               0x010D
#define TAG_IMAGE_DESCRIPTION           0x010E
#define TAG_MAKE                        0x010F
#define TAG_MODEL                       0x0110
#define TAG_STRIP_OFFSETS               0x0111
#define TAG_ORIENTATION                 0x0112
#define TAG_SAMPLES_PER_PIXEL           0x0115
#define TAG_ROWS_PER_STRIP              0x0116
#define TAG_STRIP_BYTE_COUNTS           0x0117
#define TAG_MIN_SAMPPLE_VALUE           0x0118
#define TAG_MAX_SAMPLE_VALUE            0x0119
#define TAG_X_RESOLUTION                0x011A
#define TAG_Y_RESOLUTION                0x011B
#define TAG_PLANAR_CONFIGURATION        0x011C
#define TAG_PAGE_NAME                   0x011D
#define TAG_X_POSITION                  0x011E
#define TAG_Y_POSITION                  0x011F
#define TAG_FREE_OFFSETS                0x0120
#define TAG_FREE_BYTE_COUNTS            0x0121
#define TAG_GRAY_RESPONSE_UNIT          0x0122
#define TAG_GRAY_RESPONSE_CURVE         0x0123
#define TAG_RESOLUTION_UNIT             0x0128
#define TAG_PAGE_NUMBER                 0x0129
#define TAG_TRANSFER_FUNCTION           0x012D
#define TAG_SOFTWARE                    0x0131
#define TAG_DATETIME                    0x0132
#define TAG_ARTIST                      0x013B
#define TAG_HOST_COMPUTER               0x013C
#define TAG_PREDICTOR                   0x013D
#define TAG_WHITE_POINT                 0x013E
#define TAG_PRIMARY_CHROMATICITIES      0x013F
#define TAG_COLOR_MAP                   0x0140
#define TAG_HALFTONE_HINTS              0x0141
#define TAG_TILE_WIDTH                  0x0142
#define TAG_TILE_LENGTH                 0x0143
#define TAG_TILE_OFFSETS                0x0144
#define TAG_TILE_BYTE_COUNTS            0x0145
#define TAG_SUB_IFD                     0x014A
#define TAG_INK_SETMPUTER               0x014C
#define TAG_INK_NAMES                   0x014D
#define TAG_NUMBER_OF_INKS              0x014E
#define TAG_DOT_RANGE                   0x0150
#define TAG_TARGET_PRINTER              0x0151
#define TAG_EXTRA_SAMPLE                0x0152
#define TAG_SAMPLE_FORMAT               0x0153
#define TAG_S_MIN_SAMPLE_VALUE          0x0154
#define TAG_S_MAX_SAMPLE_VALUE          0x0155
#define TAG_TRANSFER_RANGE              0x0156
#define TAG_JPEG_TABLES                 0x015B
#define TAG_JPEG_PROC                   0x0200
#define TAG_JPEG_INTERCHANGE_FORMAT     0x0201
#define TAG_JPEG_INTERCHANGE_FORMAT_LEN 0x0202
#define TAG_JPEG_RESTART_INTERVAL       0x0203
#define TAG_JPEG_LOSSLESS_PREDICTOR     0x0205
#define TAG_JPEG_POINT_TRANSFORMS       0x0206
#define TAG_JPEG_Q_TABLES               0x0207
#define TAG_JPEG_DC_TABLES              0x0208
#define TAG_JPEG_AC_TABLES              0x0209
#define TAG_YCC_COEFFICIENTS            0x0211
#define TAG_YCC_SUB_SAMPLING            0x0212
#define TAG_YCC_POSITIONING             0x0213
#define TAG_REFERENCE_BLACK_WHITE       0x0214
/* 0x0301 - 0x0302 */
/* 0x0320 */
/* 0x0343 */
/* 0x5001 - 0x501B */
/* 0x5021 - 0x503B */
/* 0x5090 - 0x5091 */
/* 0x5100 - 0x5101 */
/* 0x5110 - 0x5113 */
/* 0x80E3 - 0x80E6 */
/* 0x828d - 0x828F */
#define TAG_COPYRIGHT                   0x8298
#define TAG_EXPOSURETIME                0x829A
#define TAG_FNUMBER                     0x829D
#define TAG_EXIF_IFD_POINTER            0x8769
#define TAG_ICC_PROFILE                 0x8773
#define TAG_EXPOSURE_PROGRAM            0x8822
#define TAG_SPECTRAL_SENSITY            0x8824
#define TAG_GPS_IFD_POINTER             0x8825
#define TAG_ISOSPEED                    0x8827
#define TAG_OPTOELECTRIC_CONVERSION_F   0x8828
/* 0x8829 - 0x882b */
#define TAG_EXIFVERSION                 0x9000
#define TAG_DATE_TIME_ORIGINAL          0x9003
#define TAG_DATE_TIME_DIGITIZED         0x9004
#define TAG_OFFSET_TIME_ORIGINAL        0x9011
#define TAG_COMPONENT_CONFIG            0x9101
#define TAG_COMPRESSED_BITS_PER_PIXEL   0x9102
#define TAG_SHUTTERSPEED                0x9201
#define TAG_APERTURE                    0x9202
#define TAG_BRIGHTNESS_VALUE            0x9203
#define TAG_EXPOSURE_BIAS_VALUE         0x9204
#define TAG_MAX_APERTURE                0x9205
#define TAG_SUBJECT_DISTANCE            0x9206
#define TAG_METRIC_MODULE               0x9207
#define TAG_LIGHT_SOURCE                0x9208
#define TAG_FLASH                       0x9209
#define TAG_FOCAL_LENGTH                0x920A
/* 0x920B - 0x920D */
/* 0x9211 - 0x9216 */
#define TAG_SUBJECT_AREA                0x9214
#define TAG_MAKER_NOTE                  0x927C
#define TAG_USERCOMMENT                 0x9286
#define TAG_SUB_SEC_TIME                0x9290
#define TAG_SUB_SEC_TIME_ORIGINAL       0x9291
#define TAG_SUB_SEC_TIME_DIGITIZED      0x9292
/* 0x923F */
/* 0x935C */
#define TAG_XP_TITLE                    0x9C9B
#define TAG_XP_COMMENTS                 0x9C9C
#define TAG_XP_AUTHOR                   0x9C9D
#define TAG_XP_KEYWORDS                 0x9C9E
#define TAG_XP_SUBJECT                  0x9C9F
#define TAG_FLASH_PIX_VERSION           0xA000
#define TAG_COLOR_SPACE                 0xA001
#define TAG_COMP_IMAGE_WIDTH            0xA002 /* compressed images only */
#define TAG_COMP_IMAGE_HEIGHT           0xA003
#define TAG_RELATED_SOUND_FILE          0xA004
#define TAG_INTEROP_IFD_POINTER         0xA005 /* IFD pointer */
#define TAG_FLASH_ENERGY                0xA20B
#define TAG_SPATIAL_FREQUENCY_RESPONSE  0xA20C
#define TAG_FOCALPLANE_X_RES            0xA20E
#define TAG_FOCALPLANE_Y_RES            0xA20F
#define TAG_FOCALPLANE_RESOLUTION_UNIT  0xA210
#define TAG_SUBJECT_LOCATION            0xA214
#define TAG_EXPOSURE_INDEX              0xA215
#define TAG_SENSING_METHOD              0xA217
#define TAG_FILE_SOURCE                 0xA300
#define TAG_SCENE_TYPE                  0xA301
#define TAG_CFA_PATTERN                 0xA302
#define TAG_CUSTOM_RENDERED             0xA401
#define TAG_EXPOSURE_MODE               0xA402
#define TAG_WHITE_BALANCE               0xA403
#define TAG_DIGITAL_ZOOM_RATIO          0xA404
#define TAG_FOCAL_LENGTH_IN_35_MM_FILM  0xA405
#define TAG_SCENE_CAPTURE_TYPE          0xA406
#define TAG_GAIN_CONTROL                0xA407
#define TAG_CONTRAST                    0xA408
#define TAG_SATURATION                  0xA409
#define TAG_SHARPNESS                   0xA40A
#define TAG_DEVICE_SETTING_DESCRIPTION  0xA40B
#define TAG_SUBJECT_DISTANCE_RANGE      0xA40C
#define TAG_IMAGE_UNIQUE_ID             0xA420


enum JPEG_MARKERS {
    JM_START = 0xFF,
    JM_SOF0  = 0xC0,
    JM_SOF1  = 0xC1,
    JM_SOF2  = 0xC2,
    JM_SOF3  = 0xC3,
    JM_DHT   = 0xC4,
    JM_SOF5  = 0xC5,
    JM_SOF6  = 0xC6,
    JM_SOF7  = 0xC7,
    JM_JPG   = 0xC8,
    JM_SOF9  = 0xC9,
    JM_SOF10 = 0xCA,
    JM_SOF11 = 0xCB,
    JM_DAC   = 0xCC,
    JM_SOF13 = 0xCD,
    JM_SOF14 = 0xCE,
    JM_SOF15 = 0xCF,
    JM_RST0  = 0xD0,
    JM_RST1  = 0xD1,
    JM_RST2  = 0xD2,
    JM_RST3  = 0xD3,
    JM_RST4  = 0xD4,
    JM_RST5  = 0xD5,
    JM_RST6  = 0xD6,
    JM_RST7  = 0xD7,
    JM_SOI   = 0xD8,
    JM_EOI   = 0xD9,
    JM_SOS   = 0xDA,
    JM_DQT	 = 0xDB,
    JM_DNL	 = 0xDC,
    JM_DRI	 = 0xDD,
    JM_DHP	 = 0xDE,
    JM_EXP	 = 0xDF,
    JM_APP0	 = 0xE0,
    JM_APP1  = 0xE1, // EXIF and XMP
    JM_APP2  = 0xE2,
    JM_APP3  = 0xE3,
    JM_APP4  = 0xE4,
    JM_APP5  = 0xE5,
    JM_APP6  = 0xE6,
    JM_APP7  = 0xE7,
    JM_APP8  = 0xE8,
    JM_APP9  = 0xE9,
    JM_APP10 = 0xEA,
    JM_APP11 = 0xEB,
    JM_APP12 = 0xEC,
    JM_APP13 = 0xED, // IPTC
    JM_APP14 = 0xEE,
    JM_APP15 = 0xEF,
    JM_JPG0	 = 0xF0,
    JM_JPG1	 = 0xF1,
    JM_JPG2	 = 0xF2,
    JM_JPG3	 = 0xF3,
    JM_JPG4	 = 0xF4,
    JM_JPG5	 = 0xF5,
    JM_JPG6	 = 0xF6,
    JM_JPG7	 = 0xF7,
    JM_JPG8	 = 0xF8,
    JM_JPG9	 = 0xF9,
    JM_JPG10 = 0xFA,
    JM_JPG11 = 0xFB,
    JM_JPG12 = 0xFC,
    JM_JPG13 = 0xFD,
    JM_COM   = 0xFE
};

// Parser helper
typedef struct entry_parser_s {
    const uint8_t* data;
    uint32_t bytes;
    uint32_t tiff_header_start;
    bool     alignIntel; // byte alignment (defined in EXIF header)
    uint32_t offs; // current offset into buffer
    uint16_t tag;
    uint16_t format;
    uint32_t length;
    exif_info_t* info;
} entry_parser_t;

static void parser_init(entry_parser_t* p, uint32_t _offs) {
    p->offs = _offs - 12;
}

static uint32_t parse32(const uint8_t* buf, bool intel);

static uint32_t get_data(entry_parser_t* p) {
    return parse32(p->data + p->offs + 8, p->alignIntel);
}

static uint32_t get_sub_ifd(entry_parser_t* p) {
    return p->tiff_header_start + get_data(p);
}

static bool is_short(entry_parser_t* p) { return p->format == 3; }
static bool is_long(entry_parser_t* p) { return p->format == 4; }
static bool is_rational(entry_parser_t* p) { return p->format == 5 || p->format == 10; }
static bool is_signed_rational(entry_parser_t* p) { return p->format == 10; }
static bool is_float(entry_parser_t* p) { return p->format == 11; }
static bool is_undefined(entry_parser_t* p) { return p->format == 7; }

static uint8_t parse8(const uint8_t* buf) { return buf[0]; }

static uint16_t parse16(const uint8_t* buf, bool intel) {
    if (intel)
        return ((uint16_t)buf[1]<<8) | buf[0];
    return ((uint16_t)buf[0]<<8) | buf[1];
}

static uint32_t parse32(const uint8_t* buf, bool intel) {
    if (intel)
        return ((uint32_t)buf[3]<<24) |
            ((uint32_t)buf[2]<<16) |
            ((uint32_t)buf[1]<<8)  |
            buf[0];
    return ((uint32_t)buf[0]<<24) |
        ((uint32_t)buf[1]<<16) |
        ((uint32_t)buf[2]<<8)  |
        buf[3];
}

static float parse_float(const uint8_t* buf, bool intel) {
    union {
        uint32_t i;
        float f;
    } i2f;
    i2f.i = parse32(buf, intel);
    return i2f.f;
}

static double parse_rational(const uint8_t* buf, bool intel, bool isSigned) {
    const uint32_t denominator = parse32(buf+4, intel);
    if (denominator == 0)
        return 0.0;
    const uint32_t numerator = parse32(buf, intel);
    return isSigned ?
        (double)(int32_t)numerator/(double)(int32_t)denominator :
        (double)numerator/(double)denominator;
}

static exif_str_t parse_string(entry_parser_t* p, const uint8_t* buf,
    uint32_t num_components,
    uint32_t value,
    uint32_t base,
    uint32_t count,
    bool intel) {
    char* res = p->info->next;
    if (num_components <= 4) {
        if ((size_t)(p->info->next - p->info->strings + sizeof(p->info->strings)) <= num_components + 1) {
            return "";
        }
        p->info->next += num_components + 1;
        char j = intel ? 0 : 24;
        char j_m = intel ? -8 : 8;
        for (uint32_t i = 0; i<num_components; i++, j -= j_m) {
            res[i] = (value >> j) & 0xff;
        }
        if (num_components > 0 && res[num_components - 1] == 0) {
            res[num_components - 1] = 0;
        }
    } else if (base + value + num_components <= count) {
        const char* const s = (const char*)buf + base + value;
        uint32_t num = 0;
        while (num < num_components && s[num] != 0) { ++num; }
        while (num && s[num-1] == ' ') { --num; }
        if ((size_t)(p->info->next - p->info->strings + sizeof(p->info->strings)) <= num + 1) {
            return "";
        }
        p->info->next += num + 1;
        memcpy(res, s, num);
        res[num] = 0;
    }
    return res;
}

static void parse_tag(entry_parser_t* p) {
    p->offs  += 12;
    p->tag    = parse16(p->data + p->offs, p->alignIntel);
    p->format = parse16(p->data + p->offs + 2, p->alignIntel);
    p->length = parse32(p->data + p->offs + 4, p->alignIntel);
}

static bool parser_fetch_str(entry_parser_t* p, exif_str_t* val) {
    if (p->format != 2 || p->length == 0)
        return false;
    *val = parse_string(p, p->data, p->length, get_data(p), p->tiff_header_start,
        p->bytes, p->alignIntel);
    return true;
}

static bool parser_fetch8(entry_parser_t* p, uint8_t* val) {
    if ((p->format != 1 && p->format != 2 && p->format != 6) || p->length == 0)
        return false;
    *val = parse8(p->data + p->offs + 8);
    return true;
}

static bool parser_fetch16(entry_parser_t* p, uint16_t* val) {
    if (!is_short(p) || p->length == 0)
        return false;
    *val = parse16(p->data + p->offs + 8, p->alignIntel);
    return true;
}

static bool parser_fetch16_idx(entry_parser_t* p, uint16_t* val, uint32_t idx) {
    if (!is_short(p) || p->length <= idx)
        return false;
    *val = parse16(p->data + get_sub_ifd(p) + idx * 2, p->alignIntel);
    return true;
}

static bool parser_fetch32(entry_parser_t* p, uint32_t* val) {
    if (!is_long(p) || p->length == 0)
        return false;
    *val = parse32(p->data + p->offs + 8, p->alignIntel);
    return true;
}

static bool parser_fetch_float(entry_parser_t* p, float* val) {
    if (!is_float(p) || p->length == 0)
        return false;
    *val = parse_float(p->data + p->offs + 8, p->alignIntel);
    return true;
}

static bool parser_fetch_double(entry_parser_t* p, double* val) {
    if (!is_rational(p) || p->length == 0)
        return false;
    *val = parse_rational(p->data + get_sub_ifd(p), p->alignIntel, is_signed_rational(p));
    return true;
}

static bool parser_fetch_double_idx(entry_parser_t* p, double* val, uint32_t idx) {
    if (!is_rational(p) || p->length <= idx)
        return false;
    *val = parse_rational(p->data + get_sub_ifd(p) + idx * 8, p->alignIntel, is_signed_rational(p));
    return true;
}

static bool parser_fetch_float_as_doble(entry_parser_t* p, double* val) {
    float _val;
    if (!parser_fetch_float(p, &_val))
        return false;
    *val = _val;
    return true;
}

static int parse_xmp_xml(exif_info_t* ei, const char* xml, uint32_t len);

static int parse_xmp(exif_info_t* ei, const char* buf, unsigned len) {
	unsigned offs = 29; // current offset into buffer
	if (!buf || len < offs) {
		return EXIF_PARSE_ABSENT_DATA;
    } else if (strncmp(buf, "http://ns.adobe.com/xap/1.0/\0", offs) != 0) {
		return EXIF_PARSE_ABSENT_DATA;
    } else if (offs >= len) {
		return EXIF_PARSE_CORRUPT_DATA;
    } else {
    	return parse_xmp_xml(ei, buf + offs, len - offs);
    }
}

// Parse tag as MakerNote IFD
static void exif_parse_ifd_maker_note(entry_parser_t* p) {
    const uint32_t startOff = p->offs;
    const uint32_t off = get_sub_ifd(p);
    if (0 != strcasecmp(p->info->Make, "DJI"))
        return;
    int num_entries = parse16(p->data + p->offs, p->alignIntel);
    if ((uint32_t)(2 + 12 * num_entries) > p->length)
        return;
    parser_init(p, off+2);
    parse_tag(p);
    --num_entries;
    exif_str_t maker = null;
    if (p->tag == 1 && parser_fetch_str(p, &maker)) {
        if (0 == strcasecmp(maker, "DJI")) {
            while (--num_entries >= 0) {
                parse_tag(p);
                switch (p->tag) {
                    case 3:
                        // SpeedX
                        parser_fetch_float_as_doble(p, &p->info->GeoLocation.SpeedX);
                        break;
                    case 4:
                        // SpeedY
                        parser_fetch_float_as_doble(p, &p->info->GeoLocation.SpeedY);
                        break;
                    case 5:
                        // SpeedZ
                        parser_fetch_float_as_doble(p, &p->info->GeoLocation.SpeedZ);
                        break;
                    case 9:
                        // Camera Pitch
                        parser_fetch_float_as_doble(p, &p->info->GeoLocation.PitchDegree);
                        break;
                    case 10:
                        // Camera Yaw
                        parser_fetch_float_as_doble(p, &p->info->GeoLocation.YawDegree);
                        break;
                    case 11:
                        // Camera Roll
                        parser_fetch_float_as_doble(p, &p->info->GeoLocation.RollDegree);
                        break;
                }
            }
        }
    }
    parser_init(p, startOff+12);
}

// Parse tag as GPS IFD
static void exif_parse_ifd_gps(entry_parser_t* p) {
    switch (p->tag) {
        case 1:
            // GPS north or south
            parser_fetch8(p, &p->info->GeoLocation.LatComponents.direction);
            break;
        case 2:
            // GPS latitude
            if (is_rational(p) && p->length == 3) {
                parser_fetch_double_idx(p, &p->info->GeoLocation.LatComponents.degrees, 0);
                parser_fetch_double_idx(p, &p->info->GeoLocation.LatComponents.minutes, 1);
                parser_fetch_double_idx(p, &p->info->GeoLocation.LatComponents.seconds, 2);
            }
            break;
        case 3:
            // GPS east or west
            parser_fetch8(p, &p->info->GeoLocation.LonComponents.direction);
            break;
        case 4:
            // GPS longitude
            if (is_rational(p) && p->length == 3) {
                parser_fetch_double_idx(p, &p->info->GeoLocation.LonComponents.degrees, 0);
                parser_fetch_double_idx(p, &p->info->GeoLocation.LonComponents.minutes, 1);
                parser_fetch_double_idx(p, &p->info->GeoLocation.LonComponents.seconds, 2);
            }
            break;
        case 5: {
            // GPS altitude reference (below or above sea level)
            uint8_t altitude = 0;
            parser_fetch8(p, &altitude);
            p->info->GeoLocation.AltitudeRef = altitude;
            break;
        }
        case 6:
            // GPS altitude
            parser_fetch_double(p, &p->info->GeoLocation.Altitude);
            break;
        case 7:
            // GPS timestamp
            if (is_rational(p) && p->length == 3) {
                double h = 0, m = 0, s = 0;
                parser_fetch_double_idx(p, &h, 0);
                parser_fetch_double_idx(p, &m, 1);
                parser_fetch_double_idx(p, &s, 2);
                char buffer[256];
                snprintf(buffer, 256, "%g %g %g", h, m, s);
                p->info->GeoLocation.GPSTimeStamp = buffer;
            }
            break;
        case 11:
            // Indicates the GPS DOP (data degree of precision)
            parser_fetch_double(p, &p->info->GeoLocation.GPSDOP);
            break;
        case 18:
            // GPS geodetic survey data
            parser_fetch_str(p, &p->info->GeoLocation.GPSMapDatum);
            break;
        case 29:
            // GPS date-stamp
            parser_fetch_str(p, &p->info->GeoLocation.GPSDateStamp);
            break;
        case 30:
            // GPS differential indicates whether differential correction is applied to the GPS receiver
            parser_fetch16(p, &p->info->GeoLocation.GPSDifferential);
            break;
    }
}

// Parse tag as Exif IFD
static void exif_parse_ifd(entry_parser_t* p) {
    switch (p->tag) {
        case 0x02bc:
            p->info->has_xmp = true;
    		// XMP Metadata (Adobe technote 9-14-02)
		    if (is_undefined(p)) {
			    exif_str_t xml = null;
                parser_fetch_str(p, &xml);
			    parse_xmp_xml(p->info, xml, (uint32_t)strlen(xml));
		    }
            break;
        case TAG_EXPOSURETIME: // Exposure time in seconds
            parser_fetch_double(p, &p->info->ExposureTime);
            break;
        case TAG_FNUMBER:
            parser_fetch_double(p, &p->info->FNumber);
            break;
        case TAG_EXPOSURE_PROGRAM:
            parser_fetch16(p, &p->info->ExposureProgram);
            break;
        case TAG_ISOSPEED:
            parser_fetch16(p, &p->info->ISOSpeedRatings);
            break;
        case TAG_DATE_TIME_ORIGINAL:
            parser_fetch_str(p, &p->info->DateTimeOriginal);
            break;
        case TAG_DATE_TIME_DIGITIZED:
            parser_fetch_str(p, &p->info->DateTimeDigitized);
            break;
        case TAG_SHUTTERSPEED:
            parser_fetch_double(p, &p->info->ShutterSpeedValue);
            p->info->ShutterSpeedValue = 1.0/exp(p->info->ShutterSpeedValue * log(2));
            break;
        case TAG_APERTURE:
            parser_fetch_double(p, &p->info->ApertureValue);
            p->info->ApertureValue = exp(p->info->ApertureValue * log(2) * 0.5);
            break;
        case TAG_BRIGHTNESS_VALUE:
            parser_fetch_double(p, &p->info->BrightnessValue);
            break;
        case TAG_EXPOSURE_BIAS_VALUE:
            parser_fetch_double(p, &p->info->ExposureBiasValue);
            break;
        case TAG_SUBJECT_DISTANCE:
            parser_fetch_double(p, &p->info->SubjectDistance);
            break;
        case TAG_METRIC_MODULE:
            parser_fetch16(p, &p->info->MeteringMode);
            break;
        case TAG_LIGHT_SOURCE:
            parser_fetch16(p, &p->info->LightSource);
            break;
        case TAG_FLASH:
            parser_fetch16(p, &p->info->Flash);
            break;
        case TAG_FOCAL_LENGTH:
            parser_fetch_double(p, &p->info->FocalLength);
            break;
        case TAG_SUBJECT_AREA:
            if (is_short(p) && p->length > 1) {
                p->info->SubjectAreas = (uint16_t)p->length;
                for (uint32_t i = 0; i < p->info->SubjectAreas; i++)
                    parser_fetch16_idx(p, &p->info->SubjectArea[i], i);
            }
            break;
        case TAG_MAKER_NOTE:
            exif_parse_ifd_maker_note(p);
            break;
        case TAG_USERCOMMENT:
            parser_fetch_str(p, &p->info->ImageDescription);
            break;
        case TAG_SUB_SEC_TIME_ORIGINAL:
            // Fractions of seconds for DateTimeOriginal
            parser_fetch_str(p, &p->info->SubSecTimeOriginal);
            break;
        case TAG_COMP_IMAGE_WIDTH: // only for compressed images
            if (!parser_fetch32(p, &p->info->ImageWidth)) {
                uint16_t _ImageWidth;
                if (parser_fetch16(p, &_ImageWidth))
                    p->info->ImageWidth = _ImageWidth;
            }
            break;
        case TAG_COMP_IMAGE_HEIGHT:
            if (!parser_fetch32(p, &p->info->ImageHeight)) {
                uint16_t _ImageHeight;
                if (parser_fetch16(p, &_ImageHeight))
                    p->info->ImageHeight = _ImageHeight;
            }
            break;
        case TAG_FOCALPLANE_X_RES:
            parser_fetch_double(p, &p->info->LensInfo.FocalPlaneXResolution);
            break;
        case TAG_FOCALPLANE_Y_RES:
            parser_fetch_double(p, &p->info->LensInfo.FocalPlaneYResolution);
            break;
        case TAG_FOCALPLANE_RESOLUTION_UNIT:
            parser_fetch16(p, &p->info->LensInfo.FocalPlaneResolutionUnit);
            break;
        case TAG_EXPOSURE_INDEX:
            // Exposure Index and ISO Speed Rating are often used interchangeably
            if (p->info->ISOSpeedRatings == 0) {
                double ExposureIndex;
                if (parser_fetch_double(p, &ExposureIndex))
                    p->info->ISOSpeedRatings = (uint16_t)ExposureIndex;
            }
            break;
        case TAG_DIGITAL_ZOOM_RATIO:
            // Digital Zoom Ratio
            parser_fetch_double(p, &p->info->LensInfo.DigitalZoomRatio);
            break;
        case TAG_FOCAL_LENGTH_IN_35_MM_FILM:
            if (!parser_fetch_double(p, &p->info->LensInfo.FocalLengthIn35mm)) {
                uint16_t _FocalLengthIn35mm;
                if (parser_fetch16(p, &_FocalLengthIn35mm))
                    p->info->LensInfo.FocalLengthIn35mm = (double)_FocalLengthIn35mm;
            }
            break;
        case 0xa431:
            // Serial number of the camera
            parser_fetch_str(p, &p->info->SerialNumber);
            break;
        case 0xa432:
            // Focal length and FStop.
            if (parser_fetch_double_idx(p, &p->info->LensInfo.FocalLengthMin, 0))
                if (parser_fetch_double_idx(p, &p->info->LensInfo.FocalLengthMax, 1))
                    if (parser_fetch_double_idx(p, &p->info->LensInfo.FStopMin, 2))
                        parser_fetch_double_idx(p, &p->info->LensInfo.FStopMax, 3);
            break;
        case 0xa433:
            // Lens make.
            parser_fetch_str(p, &p->info->LensInfo.Make);
            break;
        case 0xa434:
            // Lens model.
            parser_fetch_str(p, &p->info->LensInfo.Model);
            break;
        case TAG_ARTIST:
            parser_fetch_str(p, &p->info->Artist);
            break;
        case TAG_EXIFVERSION:
            parser_fetch32(p, &p->info->ExifVersion);
            break;
        case TAG_MAX_APERTURE:
            parser_fetch_double(p, &p->info->MaxAperture);
            break;
        case TAG_COLOR_SPACE:
            parser_fetch16(p, &p->info->ColorSpace);
            break;
        case TAG_SENSING_METHOD: // short
            parser_fetch16(p, &p->info->SensingMethod);
            break;
        case TAG_SCENE_TYPE:
            parser_fetch16(p, &p->info->SceneType);
            break;
        case TAG_EXPOSURE_MODE:
            parser_fetch16(p, &p->info->ExposureMode);
            break;
        case TAG_WHITE_BALANCE:
            parser_fetch16(p, &p->info->WhiteBalance);
            break;
        case TAG_SCENE_CAPTURE_TYPE:
            parser_fetch16(p, &p->info->CaptureType);
            break;
        case TAG_YCC_POSITIONING:
            parser_fetch16(p, &p->info->YCCPositioning);
            break;
        case TAG_OFFSET_TIME_ORIGINAL:
            parser_fetch_str(p, &p->info->OffsetTimeOriginal);
            break;
        case TAG_COMPONENT_CONFIG:
            parser_fetch16(p, &p->info->ComponentConfig);
            break;
        case TAG_FLASH_PIX_VERSION:
            parser_fetch32(p, &p->info->FlashPixVersion);
            break;
        default:
            traceln("skip: 0x%04X", p->tag);
    }
}

// Parse tag as Image IFD
static void exif_parse_ifd_image(entry_parser_t* p,
        uint32_t* exif_sub_ifd_offset, uint32_t* gps_sub_ifd_offset) {
    switch (p->tag) {
        case TAG_BITS_PER_SAMPLE:
            parser_fetch16(p, &p->info->BitsPerSample);
            break;
        case TAG_IMAGE_DESCRIPTION:
            parser_fetch_str(p, &p->info->ImageDescription);
            break;
        case TAG_MAKE: // Camera maker
            parser_fetch_str(p, &p->info->Make);
            break;
        case TAG_MODEL: // Camera model
            parser_fetch_str(p, &p->info->Model);
            break;
        case TAG_ORIENTATION: // Orientation of image
            parser_fetch16(p, &p->info->Orientation);
            break;
        case TAG_X_RESOLUTION:
            parser_fetch_double(p, &p->info->XResolution);
            break;
        case TAG_Y_RESOLUTION:
            parser_fetch_double(p, &p->info->YResolution);
            break;
        case TAG_RESOLUTION_UNIT:
            parser_fetch16(p, &p->info->ResolutionUnit);
            break;
        case TAG_SOFTWARE:
            parser_fetch_str(p, &p->info->Software);
            break;
        case TAG_DATETIME: // EXIF/TIFF date/time of image modification
            // "YYYY:MM:DD HH:MM:SS" 20 (0x14) bytes
            parser_fetch_str(p, &p->info->DateTime);
            break;
        case 0x1001:
            // Original Image width
            if (!parser_fetch32(p, &p->info->RelatedImageWidth)) {
                uint16_t _RelatedImageWidth;
                if (parser_fetch16(p, &_RelatedImageWidth))
                    p->info->RelatedImageWidth = _RelatedImageWidth;
            }
            break;
        case 0x1002:
            // Original Image height
            if (!parser_fetch32(p, &p->info->RelatedImageHeight)) {
                uint16_t _RelatedImageHeight;
                if (parser_fetch16(p, &_RelatedImageHeight))
                    p->info->RelatedImageHeight = _RelatedImageHeight;
            }
            break;
        case TAG_COPYRIGHT:
            parser_fetch_str(p, &p->info->Copyright);
            break;
        case TAG_EXIF_IFD_POINTER: // EXIF SubIFD offset
            *exif_sub_ifd_offset = get_sub_ifd(p);
            break;
        case TAG_GPS_IFD_POINTER: // GPS IFS offset
            *gps_sub_ifd_offset = get_sub_ifd(p);
            break;
        default: // Try to parse as EXIF IFD tag, as some images store them in here
            exif_parse_ifd(p);
            break;
    }
}

static void geolocation_parse_coords(exif_info_t* ei) {
    // Convert GPS latitude
    ei->GeoLocation.LatComponents;
    ei->GeoLocation.LonComponents;
    if (ei->GeoLocation.LatComponents.degrees != DBL_MAX ||
        ei->GeoLocation.LatComponents.minutes != 0 ||
        ei->GeoLocation.LatComponents.seconds != 0) {
        ei->GeoLocation.Latitude =
            ei->GeoLocation.LatComponents.degrees +
            ei->GeoLocation.LatComponents.minutes / 60 +
            ei->GeoLocation.LatComponents.seconds / 3600;
        if ('S' == ei->GeoLocation.LatComponents.direction)
            ei->GeoLocation.Latitude = -ei->GeoLocation.Latitude;
    }
    // Convert GPS longitude
    if (ei->GeoLocation.LonComponents.degrees != DBL_MAX ||
        ei->GeoLocation.LonComponents.minutes != 0 ||
        ei->GeoLocation.LonComponents.seconds != 0) {
        ei->GeoLocation.Longitude =
            ei->GeoLocation.LonComponents.degrees +
            ei->GeoLocation.LonComponents.minutes / 60 +
            ei->GeoLocation.LonComponents.seconds / 3600;
        if ('W' == ei->GeoLocation.LonComponents.direction)
            ei->GeoLocation.Longitude = -ei->GeoLocation.Longitude;
    }
    // Convert GPS altitude
    if (ei->GeoLocation.Altitude != DBL_MAX && ei->GeoLocation.AltitudeRef == 1) {
        ei->GeoLocation.Altitude = -ei->GeoLocation.Altitude;
    }
}


// Main parsing function for an EXIF segment.
// Do a sanity check by looking for bytes "Exif\0\0".
// The marker has to contain at least the TIFF header, otherwise the
// JM_APP1 data is corrupt. So the minimum length specified here has to be:
//   6 bytes: "Exif\0\0" string
//   2 bytes: TIFF header (either "II" or "MM" string)
//   2 bytes: TIFF magic (short 0x2a00 in Motorola byte order)
//   4 bytes: Offset to first IFD
// =========
//  14 bytes
//
// PARAM: 'data' start of the EXIF TIFF, which must be the bytes "Exif\0\0".
// PARAM: 'bytes' bytes of buffer

static int exif_parse_from_segment(exif_info_t* ei, const uint8_t* data, uint32_t bytes) {
    uint32_t offs = 6; // current offset into buffer
    if (data == null || bytes < offs)
        return EXIF_PARSE_ABSENT_DATA;
    if (memcmp(data, "Exif\0\0", offs) != 0)
        return EXIF_PARSE_ABSENT_DATA;
    // Now parsing the TIFF header. The first two bytes are either "II" or
    // "MM" for Intel or Motorola byte alignment. Sanity check by parsing
    // the uint16_t that follows, making sure it equals 0x2a. The
    // last 4 bytes are an offset into the first IFD, which are added to
    // the global offset counter. For this block, we expect the following
    // minimum size:
    //  2 bytes: 'II' or 'MM'
    //  2 bytes: 0x002a
    //  4 bytes: offset to first IDF
    // -----------------------------
    //  8 bytes
    if (offs + 8 > bytes)
        return EXIF_PARSE_CORRUPT_DATA;
    bool alignIntel;
    if (data[offs] == 'I' && data[offs+1] == 'I')
        alignIntel = true; // 1: Intel byte alignment
    else
    if (data[offs] == 'M' && data[offs+1] == 'M')
        alignIntel = false; // 0: Motorola byte alignment
    else
        return EXIF_PARSE_UNKNOWN_BYTEALIGN;
    entry_parser_t p = {0};
    p.info = ei;
    p.data = data;
    p.bytes = bytes;
    p.tiff_header_start = offs;
    p.alignIntel = alignIntel;
    offs += 2;
    if (0x2a != parse16(data + offs, alignIntel))
        return EXIF_PARSE_CORRUPT_DATA;
    offs += 2;
    const uint32_t first_ifd_offset = parse32(data + offs, alignIntel);
    offs += first_ifd_offset - 4;
    if (offs >= bytes)
        return EXIF_PARSE_CORRUPT_DATA;
    // Now parsing the first Image File Directory (IFD0, for the main image).
    // An IFD consists of a variable number of 12-byte directory entries. The
    // first two bytes of the IFD section contain the number of directory
    // entries in the section. The last 4 bytes of the IFD contain an offset
    // to the next IFD, which means this IFD must contain exactly 6 + 12 * num
    // bytes of data.
    if (offs + 2 > bytes)
        return EXIF_PARSE_CORRUPT_DATA;
    int num_entries = parse16(data + offs, alignIntel);
    if (offs + 6 + 12 * num_entries > bytes)
        return EXIF_PARSE_CORRUPT_DATA;
    uint32_t exif_sub_ifd_offset = bytes;
    uint32_t gps_sub_ifd_offset  = bytes;
    parser_init(&p, offs + 2);
    while (--num_entries >= 0) {
        parse_tag(&p);
        exif_parse_ifd_image(&p, &exif_sub_ifd_offset, &gps_sub_ifd_offset);
    }
    // Jump to the EXIF SubIFD if it exists and parse all the information
    // there. Note that it's possible that the EXIF SubIFD doesn't exist.
    // The EXIF SubIFD contains most of the interesting information that a
    // typical user might want.
    if (exif_sub_ifd_offset + 4 <= bytes) {
        offs = exif_sub_ifd_offset;
        num_entries = parse16(data + offs, alignIntel);
        if (offs + 6 + 12 * num_entries > bytes)
            return EXIF_PARSE_CORRUPT_DATA;
        parser_init(&p, offs + 2);
        while (--num_entries >= 0) {
            parse_tag(&p);
            exif_parse_ifd(&p);
        }
    }
    // Jump to the GPS SubIFD if it exists and parse all the information
    // there. Note that it's possible that the GPS SubIFD doesn't exist.
    if (gps_sub_ifd_offset + 4 <= bytes) {
        offs = gps_sub_ifd_offset;
        num_entries = parse16(data + offs, alignIntel);
        if (offs + 6 + 12 * num_entries > bytes)
            return EXIF_PARSE_CORRUPT_DATA;
        parser_init(&p, offs + 2);
        while (--num_entries >= 0) {
            parse_tag(&p);
            exif_parse_ifd_gps(&p);
        }
        geolocation_parse_coords(ei);
    }
    return EXIF_PARSE_SUCCESS;
}

// Set all data members to default values.
// Should be called before parsing a new stream.

void exif_clear(exif_info_t* ei) {
    ei->Fields = EXIF_FIELD_NA;
    ei->next = ei->strings;
    // Strings
    ei->ImageDescription  = "";
    ei->Make              = "";
    ei->Model             = "";
    ei->SerialNumber      = "";
    ei->Software          = "";
    ei->DateTime          = "";
    ei->DateTimeOriginal  = "";
    ei->DateTimeDigitized = "";
    ei->SubSecTimeOriginal= "";
    ei->Copyright         = "";
    // Shorts / unsigned / double
    ei->ImageWidth        = 0;
    ei->ImageHeight       = 0;
    ei->RelatedImageWidth = 0;
    ei->RelatedImageHeight= 0;
    ei->Orientation       = 0;
    ei->XResolution       = 0;
    ei->YResolution       = 0;
    ei->ResolutionUnit    = 0;
    ei->BitsPerSample     = 0;
    ei->ExposureTime      = 0;
    ei->FNumber           = 0;
    ei->ExposureProgram   = 0;
    ei->ISOSpeedRatings   = 0;
    ei->ShutterSpeedValue = 0;
    ei->ApertureValue     = 0;
    ei->BrightnessValue   = 0;
    ei->ExposureBiasValue = 0;
    ei->SubjectDistance   = 0;
    ei->FocalLength       = 0;
    ei->Flash             = 0;
    ei->MeteringMode      = 0;
    ei->LightSource       = 0;
    ei->ProjectionType    = 0;
    ei->SubjectAreas      = 0;
    memset(ei->SubjectArea, 0, sizeof(ei->SubjectArea));
    // Calibration
    ei->Calibration.FocalLength = 0;
    ei->Calibration.OpticalCenterX = 0;
    ei->Calibration.OpticalCenterY = 0;
    // LensInfo
    ei->LensInfo.FocalLengthMax = 0;
    ei->LensInfo.FocalLengthMin = 0;
    ei->LensInfo.FStopMax = 0;
    ei->LensInfo.FStopMin = 0;
    ei->LensInfo.DigitalZoomRatio = 0;
    ei->LensInfo.FocalLengthIn35mm = 0;
    ei->LensInfo.FocalPlaneXResolution = 0;
    ei->LensInfo.FocalPlaneYResolution = 0;
    ei->LensInfo.FocalPlaneResolutionUnit = 0;
    ei->LensInfo.Make = "";
    ei->LensInfo.Model = "";
    // Geolocation
    ei->GeoLocation.Latitude                = DBL_MAX;
    ei->GeoLocation.Longitude               = DBL_MAX;
    ei->GeoLocation.Altitude                = DBL_MAX;
    ei->GeoLocation.AltitudeRef             = 0;
    ei->GeoLocation.RelativeAltitude        = DBL_MAX;
    ei->GeoLocation.RollDegree              = DBL_MAX;
    ei->GeoLocation.PitchDegree             = DBL_MAX;
    ei->GeoLocation.YawDegree               = DBL_MAX;
    ei->GeoLocation.SpeedX                  = DBL_MAX;
    ei->GeoLocation.SpeedY                  = DBL_MAX;
    ei->GeoLocation.SpeedZ                  = DBL_MAX;
    ei->GeoLocation.AccuracyXY              = 0;
    ei->GeoLocation.AccuracyZ               = 0;
    ei->GeoLocation.GPSDOP                  = 0;
    ei->GeoLocation.GPSDifferential         = 0;
    ei->GeoLocation.GPSMapDatum             = "";
    ei->GeoLocation.GPSTimeStamp            = "";
    ei->GeoLocation.GPSDateStamp            = "";
    ei->GeoLocation.LatComponents.degrees   = DBL_MAX;
    ei->GeoLocation.LatComponents.minutes   = 0;
    ei->GeoLocation.LatComponents.seconds   = 0;
    ei->GeoLocation.LatComponents.direction = 0;
    ei->GeoLocation.LonComponents.degrees   = DBL_MAX;
    ei->GeoLocation.LonComponents.minutes   = 0;
    ei->GeoLocation.LonComponents.seconds   = 0;
    ei->GeoLocation.LonComponents.direction = 0;
    // GPano
    ei->GPano.PosePitchDegrees = DBL_MAX;
    ei->GPano.PoseRollDegrees = DBL_MAX;
    // Video metadata
    ei->MicroVideo.HasMicroVideo = 0;
    ei->MicroVideo.MicroVideoVersion = 0;
    ei->MicroVideo.MicroVideoOffset = 0;
}

// 'stream' an interface to fetch JPEG image stream
// 'data'   a pointer to a JPEG image
// 'bytes'  number of bytes in the JPEG image
//  returns EXIF_PARSE_SUCCESS (0) on success with 'result' filled out
//          error code otherwise, as defined by the EXIF_PARSE_* macros

int exif_parse_from_stream(exif_info_t* ei, exif_stream_t* stream) {
    exif_clear(ei);
    // Sanity check: all JPEG files start with 0xFFD8 and end with 0xFFD9
    // This check also ensures that the user has supplied a correct value for bytes.
    const uint8_t* buf = stream->get(stream, 2);
    if (buf == null || buf[0] != JM_START || buf[1] != JM_SOI)
        return EXIF_PARSE_INVALID_JPEG;
    // Scan for JM_APP1 header (bytes 0xFF 0xE1) and parse its length.
    // Exit if both EXIF and XMP sections were parsed.
    uint32_t apps1 = ei->Fields;
    for (;;) {
        buf = stream->get(stream, 2);
        if (buf == null) { break; }
        // find next marker;
        // in cases of markers appended after the compressed data,
        // optional JM_START fill bytes may precede the marker
        if (*buf++ != JM_START) {
            break;
        }
        uint8_t marker;
        while ((marker = buf[0]) == JM_START && (buf = stream->get(stream, 1)) != null);
        // select marker
        uint16_t section_bytes = 0;
        bool ignored = false;
        switch (marker) {
            case 0x00:
            case 0x01:
            case JM_START:
            case JM_RST0:
            case JM_RST1:
            case JM_RST2:
            case JM_RST3:
            case JM_RST4:
            case JM_RST5:
            case JM_RST6:
            case JM_RST7:
            case JM_SOI:
                break;
            case JM_SOS: // start of stream: and we're done
            case JM_EOI: // no data? not good
                return apps1 & EXIF_FIELD_ALL ? (int)EXIF_PARSE_SUCCESS : EXIF_PARSE_ABSENT_DATA;
            case JM_APP1:
                buf = stream->get(stream, 2);
                if (buf == null) {
                    return apps1 & EXIF_FIELD_ALL ? (int)EXIF_PARSE_SUCCESS : EXIF_PARSE_INVALID_JPEG;
                }
                section_bytes = parse16(buf, false);
                if (section_bytes <= 2 || (buf=stream->get(stream, section_bytes-=2)) == null)
                    return apps1 & EXIF_FIELD_ALL ? (int)EXIF_PARSE_SUCCESS : EXIF_PARSE_INVALID_JPEG;
                ei->has_app1 = true;
                int ret = exif_parse_from_segment(ei, buf, section_bytes);
                switch (ret) {
                    case EXIF_PARSE_ABSENT_DATA:
                        ret =parse_xmp(ei, (const char*)buf, section_bytes);
				        switch (ret) {
				            case EXIF_PARSE_ABSENT_DATA:
					            break;
				            case EXIF_PARSE_SUCCESS:
                                apps1 |= EXIF_FIELD_XMP;
					            if (apps1 == EXIF_FIELD_ALL) {
						            return EXIF_PARSE_SUCCESS;
                                }
					            break;
				            default:
					            return apps1 & EXIF_FIELD_ALL ? (int)EXIF_PARSE_SUCCESS : ret;
				        }
                        break;
                    case EXIF_PARSE_SUCCESS:
                        if ((apps1 |= EXIF_FIELD_EXIF) == EXIF_FIELD_ALL) {
                            return EXIF_PARSE_SUCCESS;
                        }
                        break;
                    default:
                        return apps1 & EXIF_FIELD_ALL ? (int)EXIF_PARSE_SUCCESS : ret;
                }
                break;
            case JM_APP14:
            case JM_APP13: // IPCT
            case JM_SOF0:
            case JM_DHT:
            case JM_DQT:
            case JM_DRI:
                ignored = true; // and fall down to default:
            default:
                if (!ignored) { traceln("unhandled: 0x%02X", marker); }
                // skip the section
                buf = stream->get(stream, 2);
                section_bytes = buf != null ? parse16(buf, false) : 0;
//              traceln("marker: 0x%02X section length: %d", marker, section_bytes);
                if (buf == null || section_bytes <= 2 || !stream->skip(stream, section_bytes - 2)) {
                    return apps1 & EXIF_FIELD_ALL ? (int)EXIF_PARSE_SUCCESS : EXIF_PARSE_INVALID_JPEG;
                }
        }
    }
    return apps1 & EXIF_FIELD_ALL ? (int)EXIF_PARSE_SUCCESS : EXIF_PARSE_ABSENT_DATA;
}

// TODO: this is vast and not very useful in generic case:
// see:
// https://github.com/leok7v/photos/blob/main/metadata_test_file_IIM_XMP_EXIF.xml
// https://github.com/leok7v/photos/blob/main/IPTC-PhotometadataRef-Std2022.1.xml
// for details.
// <MicrosoftPhoto:Rating>0</MicrosoftPhoto:Rating>
// <aux:Lens>Samsung Galaxy S7 Rear Camera</aux:Lens>
// <xmpMM:History>
// <xmpRights:Marked>True</xmpRights:Marked>
// <xmpRights:UsageTerms>
// <crs:AlreadyApplied>True</crs:AlreadyApplied>  Adobe Camera Raw Settings
// <photomechanic:ColorClass>0</photomechanic:ColorClass>
// <photomechanic:EditStatus>edit status</photomechanic:EditStatus>
// <photomechanic:PMVersion>PM5</photomechanic:PMVersion>
// <photomechanic:Prefs>0:0:0:-00001</photomechanic:Prefs>
// <photomechanic:Tagged>False</photomechanic:Tagged>
// <photoshop:AuthorsPosition>stf</photoshop:AuthorsPosition>
// <photoshop:CaptionWriter>jp</photoshop:CaptionWriter>
// <photoshop:Category>Category</photoshop:Category>
// <photoshop:City>Anytown</photoshop:City>
// <photoshop:Country>United States</photoshop:Country>
// <photoshop:Credit>credit here</photoshop:Credit>
// <photoshop:DateCreated>2017-05-29T17:19:21-04:00</photoshop:DateCreated>
// <plus:CopyrightOwnerID>Default</plus:CopyrightOwnerID>
// <plus:CopyrightOwnerName>Joe Photographer</plus:CopyrightOwnerName>
// <photoshop:History>

static int parse_xmp_legacy(exif_info_t* ei, const char* xml, uint32_t len) {
    (void)ei; (void)xml; (void)len; // TODO: remove when xml is implemented
    // TODO: find test files with all those tags and implement them
	// Skip xpacket end section so that tinyxml2 lib parses the section correctly.
#ifdef TINYEXIF_LEGACY_XMP_SUPPORT // no way to test it in absence of test materials
	const char* szEnd = Tools::strrnstr(xml, "<?xpacket end=", len);
	if (szEnd != NULL)
		len = (uint32_t)(szEnd - xml);

	// Try parsing the XML packet.
	tinyxml2::XMLDocument doc;
	const tinyxml2::XMLElement* document;
	if (doc.Parse(xml, len) != tinyxml2::XML_SUCCESS ||
		((document=doc.FirstChildElement("x:xmpmeta")) == NULL && (document=doc.FirstChildElement("xmp:xmpmeta")) == NULL) ||
		(document=document->FirstChildElement("rdf:RDF")) == NULL ||
		(document=document->FirstChildElement("rdf:Description")) == NULL)
		return PARSE_ABSENT_DATA;

	// Try parsing the XMP content for tiff details.
	if (Orientation == 0) {
		uint32_t _Orientation(0);
		document->QueryUnsignedAttribute("tiff:Orientation", &_Orientation);
		Orientation = (uint16_t)_Orientation;
	}
	if (ImageWidth == 0 && ImageHeight == 0) {
		document->QueryUnsignedAttribute("tiff:ImageWidth", &ImageWidth);
		if (document->QueryUnsignedAttribute("tiff:ImageHeight", &ImageHeight) != tinyxml2::XML_SUCCESS)
			document->QueryUnsignedAttribute("tiff:ImageLength", &ImageHeight) ;
	}
	if (XResolution == 0 && YResolution == 0 && ResolutionUnit == 0) {
		document->QueryDoubleAttribute("tiff:XResolution", &XResolution);
		document->QueryDoubleAttribute("tiff:YResolution", &YResolution);
		uint32_t _ResolutionUnit(0);
		document->QueryUnsignedAttribute("tiff:ResolutionUnit", &_ResolutionUnit);
		ResolutionUnit = (uint16_t)_ResolutionUnit;
	}
	// Try parsing the XMP content for projection type.
	{
	    const tinyxml2::XMLElement* const element(document->FirstChildElement("GPano:ProjectionType"));
	    if (element != NULL) {
		    const char* const szProjectionType(element->GetText());
		    if (szProjectionType != NULL) {
			    if (0 == strcasecmp(szProjectionType, "perspective"))
				    ProjectionType = 1;
			    else
			    if (0 == strcasecmp(szProjectionType, "equirectangular") ||
				    0 == strcasecmp(szProjectionType, "spherical"))
				    ProjectionType = 2;
		    }
	    }
	}
	// Try parsing the XMP content for supported maker's info.
	struct ParseXMP	{
		// try yo fetch the value both from the attribute and child element
		// and parse if needed rational numbers stored as string fraction
		static bool Value(const tinyxml2::XMLElement* document, const char* name, double& value) {
			const char* szAttribute = document->Attribute(name);
			if (szAttribute == NULL) {
				const tinyxml2::XMLElement* const element(document->FirstChildElement(name));
				if (element == NULL || (szAttribute=element->GetText()) == NULL)
					return false;
			}
			std::vector<std::string> values;
			Tools::strSplit(szAttribute, '/', values);
			switch (values.size()) {
			case 1: value = strtod(values.front().c_str(), NULL); return true;
			case 2: value = strtod(values.front().c_str(), NULL)/strtod(values.back().c_str(), NULL); return true;
			}
			return false;
		}
		// same as previous function but with unsigned int results
		static bool Value(const tinyxml2::XMLElement* document, const char* name, uint32_t& value) {
			const char* szAttribute = document->Attribute(name);
			if (szAttribute == NULL) {
				const tinyxml2::XMLElement* const element(document->FirstChildElement(name));
				if (element == NULL || (szAttribute = element->GetText()) == NULL)
					return false;
			}
			value = strtoul(szAttribute, NULL, 0); return true;
			return false;
		}
	};
	const char* szAbout(document->Attribute("rdf:about"));
	if (0 == strcasecmp(Make.c_str(), "DJI") || (szAbout != NULL && 0 == strcasecmp(szAbout, "DJI Meta Data"))) {
		ParseXMP::Value(document, "drone-dji:AbsoluteAltitude", GeoLocation.Altitude);
		ParseXMP::Value(document, "drone-dji:RelativeAltitude", GeoLocation.RelativeAltitude);
		ParseXMP::Value(document, "drone-dji:GimbalRollDegree", GeoLocation.RollDegree);
		ParseXMP::Value(document, "drone-dji:GimbalPitchDegree", GeoLocation.PitchDegree);
		ParseXMP::Value(document, "drone-dji:GimbalYawDegree", GeoLocation.YawDegree);
		ParseXMP::Value(document, "drone-dji:CalibratedFocalLength", Calibration.FocalLength);
		ParseXMP::Value(document, "drone-dji:CalibratedOpticalCenterX", Calibration.OpticalCenterX);
		ParseXMP::Value(document, "drone-dji:CalibratedOpticalCenterY", Calibration.OpticalCenterY);
	} else if (0 == strcasecmp(Make.c_str(), "senseFly") || 0 == strcasecmp(Make.c_str(), "Sentera")) {
		ParseXMP::Value(document, "Camera:Roll", GeoLocation.RollDegree);
		if (ParseXMP::Value(document, "Camera:Pitch", GeoLocation.PitchDegree)) {
			// convert to DJI format: senseFly uses pitch 0 as NADIR, whereas DJI -90
			GeoLocation.PitchDegree = Tools::NormD180(GeoLocation.PitchDegree-90.0);
		}
		ParseXMP::Value(document, "Camera:Yaw", GeoLocation.YawDegree);
		ParseXMP::Value(document, "Camera:GPSXYAccuracy", GeoLocation.AccuracyXY);
		ParseXMP::Value(document, "Camera:GPSZAccuracy", GeoLocation.AccuracyZ);
	} else if (0 == strcasecmp(Make.c_str(), "PARROT")) {
		ParseXMP::Value(document, "Camera:Roll", GeoLocation.RollDegree) ||
		ParseXMP::Value(document, "drone-parrot:CameraRollDegree", GeoLocation.RollDegree);
		if (ParseXMP::Value(document, "Camera:Pitch", GeoLocation.PitchDegree) ||
			ParseXMP::Value(document, "drone-parrot:CameraPitchDegree", GeoLocation.PitchDegree)) {
			// convert to DJI format: senseFly uses pitch 0 as NADIR, whereas DJI -90
			GeoLocation.PitchDegree = Tools::NormD180(GeoLocation.PitchDegree-90.0);
		}
		ParseXMP::Value(document, "Camera:Yaw", GeoLocation.YawDegree) ||
		ParseXMP::Value(document, "drone-parrot:CameraYawDegree", GeoLocation.YawDegree);
		ParseXMP::Value(document, "Camera:AboveGroundAltitude", GeoLocation.RelativeAltitude);
	}
	ParseXMP::Value(document, "GPano:PosePitchDegrees", GPano.PosePitchDegrees);
	ParseXMP::Value(document, "GPano:PoseRollDegrees", GPano.PoseRollDegrees);
	// parse GCamera:MicroVideo
	if (document->Attribute("GCamera:MicroVideo")) {
		ParseXMP::Value(document, "GCamera:MicroVideo", MicroVideo.HasMicroVideo);
		ParseXMP::Value(document, "GCamera:MicroVideoVersion", MicroVideo.MicroVideoVersion);
		ParseXMP::Value(document, "GCamera:MicroVideoOffset", MicroVideo.MicroVideoOffset);
	}
#endif
	return EXIF_PARSE_SUCCESS;
}

typedef struct xml_npv_s {
    const char* n;
    const char**v;
    int32_t up; // for nested tags e.g.:
    // <dc:creator><rdf:Seq><rdf:li>name</rdf:li></rdf:Seq></dc:creator>
    // <rdf:Alt><rdf:li>...</rdf:li></rdf:Alt>
    // <rdf:Bag><rdf:li>...</rdf:li></rdf:Bag>
    int32_t append; // for appending lists like:
    // <dc:subject><rdf:Bag>
    //    <rdf:li>keyword1</rdf:li><rdf:li>keyword2</rdf:li>...
    // </rdf:Bag></dc:subject>
    const char* p; // parent for disambiguation
} xml_npv_t;

// https://stackoverflow.com/questions/29001433/how-rdfbag-rdfseq-and-rdfalt-is-different-while-using-them

// xml_rdf_t represents rdf:Bag rdf:Seq and rdf:Alt
// interpretation is left to the client.
// UTF-8 "\x00\x00" terminated list of "\x00" terminated strings

typedef const char* xml_rdf_t;
typedef const char* xml_str_t;

typedef struct xml_context_s {
    yxml_t yxml;
    exif_info_t* ei;
    char* stack[64]; // element names stack
    int32_t top;
    xml_npv_t* npv;
    int32_t npv_count;
    int32_t index;
} xml_context_t;

static const char* yxml_parent(xml_context_t* ctx, int up) {
    int sp = ctx->top - 1 - up;
    return sp >= 0 ? ctx->stack[sp] : "";
}

static inline bool yxml_strequ(const char* s0, const char* s1) {
    uint32_t l0 = (uint32_t)strlen(s0);
    uint32_t l1 = (uint32_t)strlen(s1);
    return l0 == l1 && memcmp(s0, s1, l0) == 0;
}

static void yxml_element_start(yxml_t* x) {
    xml_context_t* ctx = (xml_context_t*)x;
	assert(yxml_symlen(x, x->elem) == strlen(x->elem));
    if (ctx->ei->not_enough_memory || ctx->top >= countof(ctx->stack)) {
        ctx->ei->not_enough_memory = true;
    } else {
        ctx->stack[ctx->top++] = x->elem;
        for (int i = 0; i < ctx->npv_count && ctx->index < 0; i++) {
            // if .up == 0 both up and top are the same and are top
            //             element of the stack
            // if .up > 0  up could be empty "" or parent/grandparent
            //             of element
            // In XMP spec the elements may have rdf:containers as content
            //             but in the field broken writters may write single
            //             item container as direct content.
            const char* up  = yxml_parent(ctx, ctx->npv[i].up);
            const char* top = yxml_parent(ctx, 0);
            const char* name = ctx->npv[i].n;
            if (yxml_strequ(up, name) || yxml_strequ(top, name)) {
                const char* parent = ctx->npv[i].p;
                bool has_parent = parent == null; // parent was not required
                for (int p = ctx->top - 2; p >= 0 && !has_parent; p--) {
                    has_parent = yxml_strequ(ctx->stack[p], parent);
                }
                if (has_parent) {
                    assert(ctx->index < 0); // no content nesting!
                    if (!ctx->npv[i].append || *ctx->npv[i].v == null || *ctx->npv[i].v[0] == 0) {
                        ctx->ei->next++; // extra zero byte after zero separated appended rdf:li
                        *ctx->npv[i].v = ctx->ei->next;
                    } else { // appended values "\x00" separated:
                        if (ctx->ei->next + 3 >= ctx->ei->strings + countof(ctx->ei->strings)) {
                            ctx->ei->not_enough_memory = true;
                            break;
                        }
                        memcpy(ctx->ei->next, "\x00\x00", 2); // including terminating double zero
                        ctx->ei->next++; // only advance first "\x00" byte
                    }
                    ctx->index = i;
                }
            }
        }
    }
}

static void yxml_content(yxml_t* x) {
    xml_context_t* ctx = (xml_context_t*)x;
    if (!ctx->ei->not_enough_memory) {
        if (ctx->index >= 0) {
            char* p = ctx->yxml.data;
            if (ctx->ei->next[0] == 0) { // skip starting white space bytes
                while (isspace(*(uint8_t*)p)) { p++; }
            }
            const int64_t k = strlen((char*)p);
            if (ctx->ei->next + k + 1 >= ctx->ei->strings + countof(ctx->ei->strings)) {
                ctx->ei->not_enough_memory = true;
            }
            memcpy(ctx->ei->next, p, k + 1); // including terminating zero byte
            ctx->ei->next += k;
        }
    }
}

static void yxml_element_end(yxml_t* x) {
    xml_context_t* ctx = (xml_context_t*)x;
    if (!ctx->ei->not_enough_memory) {
        assert(ctx->top > 0);
        const int i = ctx->index;
        if (i >= 0) {
            const char* top = yxml_parent(ctx, 0);
            const char* name = ctx->npv[i].n;
            if (yxml_strequ(top, name)) {
                ctx->index = -1;
            }
            ctx->ei->next++; // double zero byte termination
        }
        ctx->top--;
    }
}

static void yxml_xmp(yxml_t* x, yxml_ret_t r) {
	switch(r) {
	    case YXML_ELEMSTART: yxml_element_start(x); break;
	    case YXML_CONTENT  : yxml_content(x);       break;
	    case YXML_ELEMEND  : yxml_element_end(x);   break;
        // ignored:
	    case YXML_ATTRSTART:
	    case YXML_ATTREND:
	    case YXML_PICONTENT:
	    case YXML_ATTRVAL:
	    case YXML_PISTART:
	    case YXML_PIEND:
	    case YXML_OK:
		    break;
	    default:
		    assert(r != YXML_OK);
	}
}

static const char* dump_exif_xmp_rdf2str(const char* rdf) {
    static char append[1024];
    char* p = append;
    *p = 0;
    while (*rdf != 0) {
        char* s = (char*)rdf;
        const int64_t k = strlen(s);
        if (p == append) {
            p += snprintf(p, append + sizeof(append) - p, "%s", s);
        } else {
            p += snprintf(p, append + sizeof(append) - p, "<|>%s", s);
        }
        rdf += k + 1;
    }
    return append;
}

static void dump_exif_xmp(exif_info_t* ei) {
    #define dump(field) do { \
        /* assert(ei->xmp.field != null && ei->xmp.field[0] != 0); */ \
        traceln("%-55s: %s", #field, dump_exif_xmp_rdf2str(ei->xmp.field)); \
    } while (0)
    dump(Iptc4xmpCore.CountryCode);
    dump(Iptc4xmpCore.CreatorContactInfo.CiAdrCity );
    dump(Iptc4xmpCore.CreatorContactInfo.CiAdrCtry );
    dump(Iptc4xmpCore.CreatorContactInfo.CiAdrExtadr);
    dump(Iptc4xmpCore.CreatorContactInfo.CiAdrPcode);
    dump(Iptc4xmpCore.CreatorContactInfo.CiAdrRegion);
    dump(Iptc4xmpCore.CreatorContactInfo.CiEmailWork);
    dump(Iptc4xmpCore.CreatorContactInfo.CiTelWork );
    dump(Iptc4xmpCore.CreatorContactInfo.CiUrlWork );
    dump(Iptc4xmpCore.IntellectualGenre);
    dump(Iptc4xmpCore.Location);
    dump(Iptc4xmpCore.Scene);
    dump(Iptc4xmpCore.SubjectCode);
    dump(Iptc4xmpExt.AOCopyrightNotice);
    dump(Iptc4xmpExt.AOCreator);
    dump(Iptc4xmpExt.AODateCreated);
    dump(Iptc4xmpExt.AddlModelInfo);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCopyrightNotice);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCreator);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCircaDateCreated);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCircaDateCreated);
    dump(Iptc4xmpExt.ArtworkOrObject.AOContentDescription);
    dump(Iptc4xmpExt.ArtworkOrObject.AOContributionDescription);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCreatorId);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCurrentCopyrightOwnerId);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCurrentCopyrightOwnerName);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCurrentLicensorId);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCurrentLicensorName);
    dump(Iptc4xmpExt.ArtworkOrObject.AOPhysicalDescription);
    dump(Iptc4xmpExt.ArtworkOrObject.AOSource);
    dump(Iptc4xmpExt.ArtworkOrObject.AOSourceInvNo);
    dump(Iptc4xmpExt.ArtworkOrObject.AOSourceInvURL);
    dump(Iptc4xmpExt.ArtworkOrObject.AOStylePeriod);
    dump(Iptc4xmpExt.ArtworkOrObject.AOTitle);
    dump(Iptc4xmpExt.DigImageGUID);
    dump(Iptc4xmpExt.EmbdEncRightsExpr);
    dump(Iptc4xmpExt.DigitalSourceType);
    dump(Iptc4xmpExt.Event);
    dump(Iptc4xmpExt.EventId);
    dump(Iptc4xmpExt.LinkedEncRightsExpr.LinkedRightsExpr);
    dump(Iptc4xmpExt.LinkedEncRightsExpr.RightsExprEncType);
    dump(Iptc4xmpExt.LinkedEncRightsExpr.RightsExprLangId);
    dump(Iptc4xmpExt.LocationCreated.City);
    dump(Iptc4xmpExt.LocationCreated.CountryCode);
    dump(Iptc4xmpExt.LocationCreated.CountryName);
    dump(Iptc4xmpExt.LocationCreated.ProvinceState);
    dump(Iptc4xmpExt.LocationCreated.Sublocation);
    dump(Iptc4xmpExt.LocationCreated.WorldRegion);
    dump(Iptc4xmpExt.LocationShown.City);
    dump(Iptc4xmpExt.LocationShown.CountryCode);
    dump(Iptc4xmpExt.LocationShown.CountryName);
    dump(Iptc4xmpExt.LocationShown.ProvinceState);
    dump(Iptc4xmpExt.LocationShown.Sublocation);
    dump(Iptc4xmpExt.LocationShown.WorldRegion);
    dump(Iptc4xmpExt.MaxAvailHeight);
    dump(Iptc4xmpExt.MaxAvailWidth);
    dump(Iptc4xmpExt.ModelAge);
    dump(Iptc4xmpExt.OrganisationInImageCode);
    dump(Iptc4xmpExt.OrganisationInImageName);
    dump(Iptc4xmpExt.PersonInImage);
    dump(Iptc4xmpExt.PersonCharacteristic);
    dump(Iptc4xmpExt.PersonDescription);
    dump(Iptc4xmpExt.PersonInImageWDetails.PersonCharacteristic);
    dump(Iptc4xmpExt.PersonInImageWDetails.PersonDescription);
    dump(Iptc4xmpExt.PersonInImageWDetails.PersonId);
    dump(Iptc4xmpExt.PersonInImageWDetails.PersonName);
    dump(Iptc4xmpExt.ProductInImage.ProductDescription);
    dump(Iptc4xmpExt.ProductInImage.ProductGTIN);
    dump(Iptc4xmpExt.ProductInImage.ProductId);
    dump(Iptc4xmpExt.ProductInImage.ProductName);
    dump(Iptc4xmpExt.RegistryId.RegItemId);
    dump(Iptc4xmpExt.RegistryId.RegOrgId);
    dump(Iptc4xmpExt.AboutCvTerm.CvId);
    dump(Iptc4xmpExt.AboutCvTerm.CvTermId);
    dump(Iptc4xmpExt.AboutCvTerm.CvTermName);
    dump(Iptc4xmpExt.AboutCvTerm.CvTermRefinedAbout);
    dump(dc.creator);
    dump(dc.date);
    dump(dc.format);
    dump(dc.description);
    dump(dc.rights);
    dump(dc.subject);
    dump(dc.title);
    dump(Lens);
    dump(exif.GPSAltitude);
    dump(exif.GPSAltitudeRef);
    dump(exif.GPSLatitude);
    dump(exif.GPSLongitude);
    dump(CreateDate);
    dump(CreatorTool);
    dump(MetadataDate);
    dump(ModifyDate);
    dump(Rating);
}

static int parse_xmp_xml(exif_info_t* ei, const char* xml, uint32_t bytes) {
    parse_xmp_legacy(ei, xml, bytes);
    xml_context_t context = {0};
    context.ei = ei;
    char xml_stack[8 * 1024]; // the xml_stack size is determined by the longest tag in depth...
	yxml_init(&context.yxml, xml_stack, sizeof(xml_stack));
    // nvp O(n^2) not a performance champion. Can be optimized using hashmaps
    xml_npv_t npv[] = {
        { "Iptc4xmpCore:CiAdrCity"  , &ei->xmp.Iptc4xmpCore.CreatorContactInfo.CiAdrCity  },
        { "Iptc4xmpCore:CiAdrCtry"  , &ei->xmp.Iptc4xmpCore.CreatorContactInfo.CiAdrCtry  },
        { "Iptc4xmpCore:CiAdrExtadr", &ei->xmp.Iptc4xmpCore.CreatorContactInfo.CiAdrExtadr},
        { "Iptc4xmpCore:CiAdrPcode" , &ei->xmp.Iptc4xmpCore.CreatorContactInfo.CiAdrPcode },
        { "Iptc4xmpCore:CiAdrRegion", &ei->xmp.Iptc4xmpCore.CreatorContactInfo.CiAdrRegion},
        { "Iptc4xmpCore:CiEmailWork", &ei->xmp.Iptc4xmpCore.CreatorContactInfo.CiEmailWork},
        { "Iptc4xmpCore:CiTelWork"  , &ei->xmp.Iptc4xmpCore.CreatorContactInfo.CiTelWork  },
        { "Iptc4xmpCore:CiUrlWork"  , &ei->xmp.Iptc4xmpCore.CreatorContactInfo.CiUrlWork  },

        { "Iptc4xmpCore:IntellectualGenre" , &ei->xmp.Iptc4xmpCore.IntellectualGenre },
        { "Iptc4xmpCore:Location"          , &ei->xmp.Iptc4xmpCore.Location },
        { "Iptc4xmpCore:Scene"             , &ei->xmp.Iptc4xmpCore.Scene },
        { "Iptc4xmpCore:SubjectCode"       , &ei->xmp.Iptc4xmpCore.SubjectCode },

        { "Iptc4xmpExt:AOCopyrightNotice"           , &ei->xmp.Iptc4xmpExt.ArtworkOrObject.AOCopyrightNotice, 0, false, "Iptc4xmpExt:ArtworkOrObject" },
        { "Iptc4xmpExt:AOCreator"                   , &ei->xmp.Iptc4xmpExt.ArtworkOrObject.AOCreator,         0, false, "Iptc4xmpExt:ArtworkOrObject" },
        { "Iptc4xmpExt:AOCircaDateCreated"          , &ei->xmp.Iptc4xmpExt.ArtworkOrObject.AOCircaDateCreated },
        { "Iptc4xmpExt:AOContentDescription"        , &ei->xmp.Iptc4xmpExt.ArtworkOrObject.AOContentDescription, 2 },
        { "Iptc4xmpExt:AOContributionDescription"   , &ei->xmp.Iptc4xmpExt.ArtworkOrObject.AOContributionDescription, 2 },
        { "Iptc4xmpExt:AOCreatorId"                 , &ei->xmp.Iptc4xmpExt.ArtworkOrObject.AOCreatorId, 2 },

        { "Iptc4xmpExt:AOCurrentCopyrightOwnerId"   , &ei->xmp.Iptc4xmpExt.ArtworkOrObject.AOCurrentCopyrightOwnerId },
        { "Iptc4xmpExt:AOCurrentCopyrightOwnerName" , &ei->xmp.Iptc4xmpExt.ArtworkOrObject.AOCurrentCopyrightOwnerName },
        { "Iptc4xmpExt:AOCurrentLicensorId"         , &ei->xmp.Iptc4xmpExt.ArtworkOrObject.AOCurrentLicensorId},
        { "Iptc4xmpExt:AOCurrentLicensorName"       , &ei->xmp.Iptc4xmpExt.ArtworkOrObject.AOCurrentLicensorName},

        { "Iptc4xmpExt:AOPhysicalDescription"       , &ei->xmp.Iptc4xmpExt.ArtworkOrObject.AOPhysicalDescription, 2 },
        { "Iptc4xmpExt:AOSource"                    , &ei->xmp.Iptc4xmpExt.ArtworkOrObject.AOSource},
        { "Iptc4xmpExt:AOSourceInvNo"               , &ei->xmp.Iptc4xmpExt.ArtworkOrObject.AOSourceInvNo},
        { "Iptc4xmpExt:AOSourceInvURL"              , &ei->xmp.Iptc4xmpExt.ArtworkOrObject.AOSourceInvURL},
        { "Iptc4xmpExt:AOStylePeriod"               , &ei->xmp.Iptc4xmpExt.ArtworkOrObject.AOStylePeriod, 2},
        { "Iptc4xmpExt:AOTitle"                     , &ei->xmp.Iptc4xmpExt.ArtworkOrObject.AOTitle, 2},

        { "Iptc4xmpExt:DigImageGUID"                , &ei->xmp.Iptc4xmpExt.DigImageGUID},
        { "Iptc4xmpExt:EmbdEncRightsExpr"           , &ei->xmp.Iptc4xmpExt.EmbdEncRightsExpr, 2},
        { "Iptc4xmpExt:DigitalSourceType"           , &ei->xmp.Iptc4xmpExt.DigitalSourceType},
        { "Iptc4xmpExt:Event"                       , &ei->xmp.Iptc4xmpExt.Event, 2},
        { "Iptc4xmpExt:EventId"                     , &ei->xmp.Iptc4xmpExt.EventId, 2},

        { "Iptc4xmpExt:LinkedRightsExpr"        , &ei->xmp.Iptc4xmpExt.LinkedEncRightsExpr.LinkedRightsExpr,  0, false, "Iptc4xmpExt:LinkedEncRightsExpr"},
        { "Iptc4xmpExt:RightsExprEncType"       , &ei->xmp.Iptc4xmpExt.LinkedEncRightsExpr.RightsExprEncType, 0, false, "Iptc4xmpExt:LinkedEncRightsExpr"},
        { "Iptc4xmpExt:RightsExprLangId"        , &ei->xmp.Iptc4xmpExt.LinkedEncRightsExpr.RightsExprLangId,  0, false, "Iptc4xmpExt:LinkedEncRightsExpr"},

        { "Iptc4xmpExt:City"                    , &ei->xmp.Iptc4xmpExt.LocationCreated.City,          0, false, "Iptc4xmpExt:LocationCreated"},
        { "Iptc4xmpExt:CountryCode"             , &ei->xmp.Iptc4xmpExt.LocationCreated.CountryCode,   0, false, "Iptc4xmpExt:LocationCreated"},
        { "Iptc4xmpExt:CountryName"             , &ei->xmp.Iptc4xmpExt.LocationCreated.CountryName,   0, false, "Iptc4xmpExt:LocationCreated"},
        { "Iptc4xmpExt:ProvinceState"           , &ei->xmp.Iptc4xmpExt.LocationCreated.ProvinceState, 0, false, "Iptc4xmpExt:LocationCreated"},
        { "Iptc4xmpExt:Sublocation"             , &ei->xmp.Iptc4xmpExt.LocationCreated.Sublocation,   0, false, "Iptc4xmpExt:LocationCreated"},
        { "Iptc4xmpExt:WorldRegion"             , &ei->xmp.Iptc4xmpExt.LocationCreated.WorldRegion,   0, false, "Iptc4xmpExt:LocationCreated"},

        { "Iptc4xmpExt:City"                    , &ei->xmp.Iptc4xmpExt.LocationShown.City,          0, false, "Iptc4xmpExt:LocationShown"},
        { "Iptc4xmpExt:CountryCode"             , &ei->xmp.Iptc4xmpExt.LocationShown.CountryCode,   0, false, "Iptc4xmpExt:LocationShown"},
        { "Iptc4xmpExt:CountryName"             , &ei->xmp.Iptc4xmpExt.LocationShown.CountryName,   0, false, "Iptc4xmpExt:LocationShown"},
        { "Iptc4xmpExt:ProvinceState"           , &ei->xmp.Iptc4xmpExt.LocationShown.ProvinceState, 0, false, "Iptc4xmpExt:LocationShown"},
        { "Iptc4xmpExt:Sublocation"             , &ei->xmp.Iptc4xmpExt.LocationShown.Sublocation,   0, false, "Iptc4xmpExt:LocationShown"},
        { "Iptc4xmpExt:WorldRegion"             , &ei->xmp.Iptc4xmpExt.LocationShown.WorldRegion,   0, false, "Iptc4xmpExt:LocationShown"},

        { "Iptc4xmpExt:MaxAvailHeight"          , &ei->xmp.Iptc4xmpExt.MaxAvailHeight},
        { "Iptc4xmpExt:MaxAvailWidth"           , &ei->xmp.Iptc4xmpExt.MaxAvailWidth},
        { "Iptc4xmpExt:ModelAge"                , &ei->xmp.Iptc4xmpExt.ModelAge},

        { "Iptc4xmpExt:PersonCharacteristic" , &ei->xmp.Iptc4xmpExt.PersonInImageWDetails.PersonCharacteristic, 7, true, "Iptc4xmpExt:PersonInImageWDetails"},
        { "Iptc4xmpExt:PersonDescription"    , &ei->xmp.Iptc4xmpExt.PersonInImageWDetails.PersonDescription,    2, true, "Iptc4xmpExt:PersonInImageWDetails"},
        { "Iptc4xmpExt:PersonId"             , &ei->xmp.Iptc4xmpExt.PersonInImageWDetails.PersonId,             2, true, "Iptc4xmpExt:PersonInImageWDetails"},
        { "Iptc4xmpExt:PersonName"           , &ei->xmp.Iptc4xmpExt.PersonInImageWDetails.PersonName,           2, true, "Iptc4xmpExt:PersonInImageWDetails"},

        { "Iptc4xmpExt:ProductDescription"   , &ei->xmp.Iptc4xmpExt.ProductInImage.ProductDescription, 2, true,  "Iptc4xmpExt:ProductInImage"},
        { "Iptc4xmpExt:ProductGTIN"          , &ei->xmp.Iptc4xmpExt.ProductInImage.ProductGTIN,        0, false, "Iptc4xmpExt:ProductInImage"},
        { "Iptc4xmpExt:ProductId"            , &ei->xmp.Iptc4xmpExt.ProductInImage.ProductId,          0, false, "Iptc4xmpExt:ProductInImage"},
        { "Iptc4xmpExt:ProductName"          , &ei->xmp.Iptc4xmpExt.ProductInImage.ProductName,        2, true,  "Iptc4xmpExt:ProductInImage"},

        { "Iptc4xmpExt:OrganisationInImageCode" , &ei->xmp.Iptc4xmpExt.OrganisationInImageCode, 2},
        { "Iptc4xmpExt:OrganisationInImageName" , &ei->xmp.Iptc4xmpExt.OrganisationInImageName, 2},
        { "Iptc4xmpExt:PersonInImage"           , &ei->xmp.Iptc4xmpExt.PersonInImage,           2},
        { "Iptc4xmpExt:PersonCharacteristic"    , &ei->xmp.Iptc4xmpExt.PersonCharacteristic,    2},
        { "Iptc4xmpExt:PersonDescription"       , &ei->xmp.Iptc4xmpExt.PersonDescription,       2},

        { "Iptc4xmpExt:RegItemId"            , &ei->xmp.Iptc4xmpExt.RegistryId.RegItemId},
        { "Iptc4xmpExt:RegOrgId"             , &ei->xmp.Iptc4xmpExt.RegistryId.RegOrgId},

        { "Iptc4xmpExt:CvId"                 , &ei->xmp.Iptc4xmpExt.AboutCvTerm.CvId,               0, false, "Iptc4xmpExt:AboutCvTerm"},
        { "Iptc4xmpExt:CvTermId"             , &ei->xmp.Iptc4xmpExt.AboutCvTerm.CvTermId,           0, false, "Iptc4xmpExt:AboutCvTerm"},
        { "Iptc4xmpExt:CvTermName"           , &ei->xmp.Iptc4xmpExt.AboutCvTerm.CvTermName,         2, true, "Iptc4xmpExt:AboutCvTerm"},
        { "Iptc4xmpExt:CvTermRefinedAbout"   , &ei->xmp.Iptc4xmpExt.AboutCvTerm.CvTermRefinedAbout, 0, false, "Iptc4xmpExt:AboutCvTerm"},

        { "dc:creator"              , &ei->xmp.dc.creator,      2, true },
        { "dc:date"                 , &ei->xmp.dc.date,         2, true },
        { "dc:format"               , &ei->xmp.dc.format},
        { "dc:description"          , &ei->xmp.dc.description,  2, true },
        { "dc:rights"               , &ei->xmp.dc.rights,       2, true },
        { "dc:subject"              , &ei->xmp.dc.subject,      2, true },
        { "dc:title"                , &ei->xmp.dc.title,        2, true },

        { "aux:Lens"                , &ei->xmp.Lens },

        { "exif:GPSAltitude"        , &ei->xmp.exif.GPSAltitude },
        { "exif:GPSAltitudeRef"     , &ei->xmp.exif.GPSAltitudeRef },
        { "exif:GPSLatitude"        , &ei->xmp.exif.GPSLatitude },
        { "exif:GPSLongitude"       , &ei->xmp.exif.GPSLongitude },

        { "xmp:CreateDate"          , &ei->xmp.CreateDate },
        { "xmp:CreatorTool"         , &ei->xmp.CreatorTool },
        { "xmp:MetadataDate"        , &ei->xmp.MetadataDate },
        { "xmp:ModifyDate"          , &ei->xmp.ModifyDate },
        { "xmp:Rating"              , &ei->xmp.Rating },

        // for disambiguation non-parented items must apprear last in the list
        { "Iptc4xmpExt:AOCopyrightNotice"           , &ei->xmp.Iptc4xmpExt.AOCopyrightNotice },
        { "Iptc4xmpExt:AOCreator"                   , &ei->xmp.Iptc4xmpExt.AOCreator         },
        { "Iptc4xmpExt:AODateCreated"               , &ei->xmp.Iptc4xmpExt.AODateCreated },
        { "Iptc4xmpExt:AddlModelInfo"               , &ei->xmp.Iptc4xmpExt.AddlModelInfo },

        { "Iptc4xmpCore:CountryCode", &ei->xmp.Iptc4xmpCore.CountryCode }
    };
    context.npv = npv;
    context.npv_count = countof(npv);
    context.index = -1;
    yxml_ret_t r = YXML_OK;
    context.ei->not_enough_memory = false;
    for (uint32_t i = 0; i < bytes && !context.ei->not_enough_memory; i++) {
		r = yxml_parse(&context.yxml, xml[i]);
        if (r >= 0) {
		    yxml_xmp(&context.yxml, r);
        } else {
            break;
        }
    }
    r = yxml_eof(&context.yxml);
    for (int32_t i = 0; i < context.npv_count; i++) {
        if (*context.npv[i].v == null) {
            *context.npv[i].v = "\x00\x00";
            // much easier for the clients to handle
        }
    }
    if (ei->dump) {
        dump_exif_xmp(ei);
    }
    assert(r == YXML_OK);
    return r == YXML_OK ? EXIF_PARSE_SUCCESS : EXIF_PARSE_CORRUPT_DATA;
}

typedef struct exif_stream_buffer_s {
    exif_stream_t stream;
    const uint8_t* it;
    const uint8_t* end;
} exif_stream_buffer_t;

static const uint8_t* get(exif_stream_t* stream, uint32_t bytes) {
    exif_stream_buffer_t* sb = (exif_stream_buffer_t*)stream;
    const uint8_t* next = sb->it + bytes;
    if (next > sb->end) return null;
    const uint8_t* begin = sb->it;
    sb->it = next;
    return begin;
}

static bool skip(exif_stream_t* stream, uint32_t bytes) {
    return stream->get(stream, bytes) != null;
}

int exif_parse_from_memory(exif_info_t* ei, const uint8_t* buf, uint32_t bytes) {
    exif_stream_buffer_t exif_stream_buffer = {0};
    exif_stream_buffer.it = buf;
    exif_stream_buffer.end = buf + bytes;
    exif_stream_buffer.stream.get = get;
    exif_stream_buffer.stream.skip = skip;
    return exif_parse_from_stream(ei, &exif_stream_buffer.stream);
}

int exif_from_stream(exif_info_t* ei, exif_stream_t* stream) {
    return exif_parse_from_stream(ei, stream);
}

int exif_from_memory(exif_info_t* ei, const uint8_t* data, uint32_t length) {
    return exif_parse_from_memory(ei, data, length);
}



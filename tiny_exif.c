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

#define null ((void*)0)

#ifdef _MSC_VER
static int strcasecmp(const char* a, const char* b) {
    return _stricmp(a, b);
}
#endif

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
    unsigned bytes;
    unsigned tiff_header_start;
    bool     alignIntel; // byte alignment (defined in EXIF header)
    unsigned offs; // current offset into buffer
    uint16_t tag; 
    uint16_t format;
    uint32_t length;
    exif_info_t* info;
} entry_parser_t;

//  EntryParser(exif_info_t &_info, 
//      const uint8_t* _buf, unsigned _len, unsigned _tiff_header_start, bool _alignIntel)
//      : info(_info), data(_buf), bytes(_len), tiff_header_start(_tiff_header_start), 
//        alignIntel(_alignIntel), offs(0) {}

static void parser_init(entry_parser_t* p, unsigned _offs) {
    p->offs = _offs - 12;
}

static uint32_t parse32(const uint8_t* buf, bool intel);

static uint32_t get_data(entry_parser_t* p) { 
    return parse32(p->data + p->offs + 8, p->alignIntel); 
}

static uint32_t get_sub_ifd(entry_parser_t* p) { 
    return p->tiff_header_start + get_data(p); 
}

static bool IsShort(entry_parser_t* p) { return p->format == 3; }
static bool IsLong(entry_parser_t* p) { return p->format == 4; }
static bool IsRational(entry_parser_t* p) { return p->format == 5 || p->format == 10; }
static bool IsSRational(entry_parser_t* p) { return p->format == 10; }
static bool IsFloat(entry_parser_t* p) { return p->format == 11; }
// static bool IsUndefined(entry_parser_t* p) { return p->format == 7; }

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
        for (unsigned i = 0; i<num_components; i++, j -= j_m)
            res[i] = (value >> j) & 0xff;
        if (num_components > 0 && res[num_components - 1] == 0)
            res[num_components - 1] = 0;
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
    if (!IsShort(p) || p->length == 0)
        return false;
    *val = parse16(p->data + p->offs + 8, p->alignIntel);
    return true;
}

static bool parser_fetch16_idx(entry_parser_t* p, uint16_t* val, uint32_t idx) {
    if (!IsShort(p) || p->length <= idx)
        return false;
    *val = parse16(p->data + get_sub_ifd(p) + idx * 2, p->alignIntel);
    return true;
}

static bool parser_fetch32(entry_parser_t* p, uint32_t* val) {
    if (!IsLong(p) || p->length == 0)
        return false;
    *val = parse32(p->data + p->offs + 8, p->alignIntel);
    return true;
}

static bool parser_fetch_float(entry_parser_t* p, float* val) {
    if (!IsFloat(p) || p->length == 0)
        return false;
    *val = parse_float(p->data + p->offs + 8, p->alignIntel);
    return true;
}

static bool parser_fetch_double(entry_parser_t* p, double* val) {
    if (!IsRational(p) || p->length == 0)
        return false;
    *val = parse_rational(p->data + get_sub_ifd(p), p->alignIntel, IsSRational(p));
    return true;
}

static bool parser_fetch_double_idx(entry_parser_t* p, double* val, uint32_t idx) {
    if (!IsRational(p) || p->length <= idx)
        return false;
    *val = parse_rational(p->data + get_sub_ifd(p) + idx * 8, p->alignIntel, IsSRational(p));
    return true;
}

static bool parser_fetch_float_as_doble(entry_parser_t* p, double* val) {
    float _val;
    if (!parser_fetch_float(p, &_val))
        return false;
    *val = _val;
    return true;
}

// 'stream' an interface to fetch JPEG image stream
// 'data'   a pointer to a JPEG image
// 'bytes'  number of bytes in the JPEG image
//  returns PARSE_SUCCESS (0) on success with 'result' filled out
//          error code otherwise, as defined by the PARSE_* macros
int exif_parse_from_stream(exif_info_t* ei, exif_stream_t* stream);
int exif_parse_from_memory(exif_info_t* ei, const uint8_t* data, uint32_t bytes);

// Set all data members to default values.
// Should be called before parsing a new stream.
void exif_clear(exif_info_t* ei);

int exif_from_stream(exif_info_t* ei, exif_stream_t* stream) {
    return exif_parse_from_stream(ei, stream);
}

int exif_from_memory(exif_info_t* ei, const uint8_t* data, uint32_t length) {
    return exif_parse_from_memory(ei, data, length);
}

static int exif_parse_from_segment(exif_info_t* ei, const uint8_t* data, uint32_t bytes);
// Parse tag as Image IFD.
static void exif_parse_ifd_image(entry_parser_t* p, uint32_t* sub_ifd, uint32_t* gps_offset);
// Parse tag as Exif IFD.
static void exif_parse_ifd(entry_parser_t* p);
// Parse tag as GPS IFD.
static void exif_parse_ifd_gps(entry_parser_t* p);
// Parse tag as MakerNote IFD.
static void exif_parse_ifd_maker_note(entry_parser_t* p);

// Parse tag as Image IFD
static void exif_parse_ifd_image(entry_parser_t* p, uint32_t* exif_sub_ifd_offset, uint32_t* gps_sub_ifd_offset) {
    switch (p->tag) {
        case 0x0102:
            // Bits per sample
            parser_fetch16(p, &p->info->BitsPerSample);
            break;
        case 0x010e:
            // Image description
            parser_fetch_str(p, &p->info->ImageDescription);
            break;
        case 0x010f:
            // Camera maker
            parser_fetch_str(p, &p->info->Make);
            break;
        case 0x0110:
            // Camera model
            parser_fetch_str(p, &p->info->Model);
            break;
        case 0x0112:
            // Orientation of image
            parser_fetch16(p, &p->info->Orientation);
            break;

        case 0x011a:
            // XResolution 
            parser_fetch_double(p, &p->info->XResolution);
            break;
        case 0x011b:
            // YResolution 
            parser_fetch_double(p, &p->info->YResolution);
            break;
        case 0x0128:
            // Resolution Unit
            parser_fetch16(p, &p->info->ResolutionUnit);
            break;
        case 0x0131:
            // Software used for image
            parser_fetch_str(p, &p->info->Software);
            break;
        case 0x0132:
            // EXIF/TIFF date/time of image modification
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
        case 0x8298:
            // Copyright information
            parser_fetch_str(p, &p->info->Copyright);
            break;
        case 0x8769:
            // EXIF SubIFD offset
            *exif_sub_ifd_offset = get_sub_ifd(p);
            break;
        case 0x8825:
            // GPS IFS offset
            *gps_sub_ifd_offset = get_sub_ifd(p);
            break;
        default:
            // Try to parse as EXIF tag, as some images store them in here
            exif_parse_ifd(p);
            break;
    }
}

// Parse tag as Exif IFD
static void exif_parse_ifd(entry_parser_t* p) {
    switch (p->tag) {
        case 0x02bc:
            // TODO: XMP?
            break;
        case 0x829a:
            // Exposure time in seconds
            parser_fetch_double(p, &p->info->ExposureTime);
            break;
        case 0x829d:
            // FNumber
            parser_fetch_double(p, &p->info->FNumber);
            break;
        case 0x8822:
            // Exposure Program
            parser_fetch16(p, &p->info->ExposureProgram);
            break;
        case 0x8827:
            // ISO Speed Rating
            parser_fetch16(p, &p->info->ISOSpeedRatings);
            break;
        case 0x9003:
            // Original date and time
            parser_fetch_str(p, &p->info->DateTimeOriginal);
            break;
        case 0x9004:
            // Digitization date and time
            parser_fetch_str(p, &p->info->DateTimeDigitized);
            break;
        case 0x9201:
            // Shutter speed value
            parser_fetch_double(p, &p->info->ShutterSpeedValue);
            p->info->ShutterSpeedValue = 1.0/exp(p->info->ShutterSpeedValue * log(2));
            break;
        case 0x9202:
            // Aperture value
            parser_fetch_double(p, &p->info->ApertureValue);
            p->info->ApertureValue = exp(p->info->ApertureValue * log(2) * 0.5);
            break;
        case 0x9203:
            // Brightness value
            parser_fetch_double(p, &p->info->BrightnessValue);
            break;
        case 0x9204:
            // Exposure bias value 
            parser_fetch_double(p, &p->info->ExposureBiasValue);
            break;
        case 0x9206:
            // Subject distance
            parser_fetch_double(p, &p->info->SubjectDistance);
            break;
        case 0x9207:
            // Metering mode
            parser_fetch16(p, &p->info->MeteringMode);
            break;
        case 0x9208:
            // Light source
            parser_fetch16(p, &p->info->LightSource);
            break;
        case 0x9209:
            // Flash info
            parser_fetch16(p, &p->info->Flash);
            break;
        case 0x920a:
            // Focal length
            parser_fetch_double(p, &p->info->FocalLength);
            break;
        case 0x9214:
            // Subject area
            if (IsShort(p) && p->length > 1) {
                p->info->SubjectAreas = (uint16_t)p->length;
                for (uint32_t i = 0; i < p->info->SubjectAreas; i++)
                    parser_fetch16_idx(p, &p->info->SubjectArea[i], i);
            }
            break;
        case 0x927c:
            // MakerNote
            exif_parse_ifd_maker_note(p);
            break;
        case 0x9291:
            // Fractions of seconds for DateTimeOriginal
            parser_fetch_str(p, &p->info->SubSecTimeOriginal);
            break;
        case 0xa002:
            // EXIF Image width
            if (!parser_fetch32(p, &p->info->ImageWidth)) {
                uint16_t _ImageWidth;
                if (parser_fetch16(p, &_ImageWidth))
                    p->info->ImageWidth = _ImageWidth;
            }
            break;
        case 0xa003:
            // EXIF Image height
            if (!parser_fetch32(p, &p->info->ImageHeight)) {
                uint16_t _ImageHeight;
                if (parser_fetch16(p, &_ImageHeight))
                    p->info->ImageHeight = _ImageHeight;
            }
            break;
        case 0xa20e:
            // Focal plane X resolution
            parser_fetch_double(p, &p->info->LensInfo.FocalPlaneXResolution);
            break;
        case 0xa20f:
            // Focal plane Y resolution
            parser_fetch_double(p, &p->info->LensInfo.FocalPlaneYResolution);
            break;
        case 0xa210:
            // Focal plane resolution unit
            parser_fetch16(p, &p->info->LensInfo.FocalPlaneResolutionUnit);
            break;
        case 0xa215:
            // Exposure Index and ISO Speed Rating are often used interchangeably
            if (p->info->ISOSpeedRatings == 0) {
                double ExposureIndex;
                if (parser_fetch_double(p, &ExposureIndex))
                    p->info->ISOSpeedRatings = (uint16_t)ExposureIndex;
            }
            break;
        case 0xa404:
            // Digital Zoom Ratio
            parser_fetch_double(p, &p->info->LensInfo.DigitalZoomRatio);
            break;
        case 0xa405:
            // Focal length in 35mm film
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
    }
}

// Parse tag as MakerNote IFD
static void exif_parse_ifd_maker_note(entry_parser_t* p) {
    const unsigned startOff = p->offs;
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
            if (IsRational(p) && p->length == 3) {
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
            if (IsRational(p) && p->length == 3) {
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
            if (IsRational(p) && p->length == 3) {
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

// Locates the JM_APP1 segment and parses it using
// parseFromEXIFSegment() or parseFromXMPSegment()
#include "crt.h"

int exif_parse_from_stream(exif_info_t* ei, exif_stream_t* stream) {
    exif_clear(ei);
    // Sanity check: all JPEG files start with 0xFFD8 and end with 0xFFD9
    // This check also ensures that the user has supplied a correct value for bytes.
    const uint8_t* buf = stream->get(stream, 2);
    if (buf == null || buf[0] != JM_START || buf[1] != JM_SOI)
        return PARSE_INVALID_JPEG;
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
//      traceln("marker: 0x%02X", marker);
        // select marker
        uint16_t sectionLength = 0;
//      traceln("marker: 0x%02X", marker);
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
                return apps1 & FIELD_ALL ? (int)PARSE_SUCCESS : PARSE_ABSENT_DATA;
            case JM_APP1:
                buf = stream->get(stream, 2);
                if (buf == null)
                    return apps1 & FIELD_ALL ? (int)PARSE_SUCCESS : PARSE_INVALID_JPEG;
                sectionLength = parse16(buf, false);
                if (sectionLength <= 2 || (buf=stream->get(stream, sectionLength-=2)) == null)
                    return apps1 & FIELD_ALL ? (int)PARSE_SUCCESS : PARSE_INVALID_JPEG;
                int ret = exif_parse_from_segment(ei, buf, sectionLength);
                switch (ret) {
                    case PARSE_ABSENT_DATA:
                        // TODO: XMP
                        break;
                    case PARSE_SUCCESS:
                        if ((apps1 |= FIELD_EXIF) == FIELD_ALL)
                            return PARSE_SUCCESS;
                        break;
                    default:
                        return apps1 & FIELD_ALL ? (int)PARSE_SUCCESS : ret;
                }
                break;
            default:
                // skip the section
                buf = stream->get(stream, 2);
                if (buf != null) {
                    sectionLength = parse16(buf, false);
                }
//              traceln("marker: 0x%02X section length: %d", marker, sectionLength);
                if (buf == null || sectionLength <= 2 || !stream->skip(stream, sectionLength - 2)) {
                    return apps1 & FIELD_ALL ? (int)PARSE_SUCCESS : PARSE_INVALID_JPEG;
                }
        }
    }
    return apps1 & FIELD_ALL ? (int)PARSE_SUCCESS : PARSE_ABSENT_DATA;
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
    unsigned offs = 6; // current offset into buffer
    if (data == null || bytes < offs)
        return PARSE_ABSENT_DATA;
    if (memcmp(data, "Exif\0\0", offs) != 0)
        return PARSE_ABSENT_DATA;
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
        return PARSE_CORRUPT_DATA;
    bool alignIntel;
    if (data[offs] == 'I' && data[offs+1] == 'I')
        alignIntel = true; // 1: Intel byte alignment
    else
    if (data[offs] == 'M' && data[offs+1] == 'M')
        alignIntel = false; // 0: Motorola byte alignment
    else
        return PARSE_UNKNOWN_BYTEALIGN;
    entry_parser_t p = {0};
    p.info = ei;
    p.data = data;
    p.bytes = bytes;
    p.tiff_header_start = offs;
    p.alignIntel = alignIntel;
    offs += 2;
    if (0x2a != parse16(data + offs, alignIntel))
        return PARSE_CORRUPT_DATA;
    offs += 2;
    const unsigned first_ifd_offset = parse32(data + offs, alignIntel);
    offs += first_ifd_offset - 4;
    if (offs >= bytes)
        return PARSE_CORRUPT_DATA;
    // Now parsing the first Image File Directory (IFD0, for the main image).
    // An IFD consists of a variable number of 12-byte directory entries. The
    // first two bytes of the IFD section contain the number of directory
    // entries in the section. The last 4 bytes of the IFD contain an offset
    // to the next IFD, which means this IFD must contain exactly 6 + 12 * num
    // bytes of data.
    if (offs + 2 > bytes)
        return PARSE_CORRUPT_DATA;
    int num_entries = parse16(data + offs, alignIntel);
    if (offs + 6 + 12 * num_entries > bytes)
        return PARSE_CORRUPT_DATA;
    unsigned exif_sub_ifd_offset = bytes;
    unsigned gps_sub_ifd_offset  = bytes;
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
            return PARSE_CORRUPT_DATA;
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
            return PARSE_CORRUPT_DATA;
        parser_init(&p, offs + 2);
        while (--num_entries >= 0) {
            parse_tag(&p);
            exif_parse_ifd_gps(&p);
        }
        geolocation_parse_coords(ei);
    }
    return PARSE_SUCCESS;
}

void exif_clear(exif_info_t* ei) {
    ei->Fields = FIELD_NA;
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


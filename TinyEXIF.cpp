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

#include "TinyEXIF.h"
#include "crt.h"
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cfloat>
#include <vector>
#include <algorithm>
#include <iostream>

#ifndef TINYEXIF_NO_XMP_SUPPORT
#include <tinyxml2.h>
#endif // TINYEXIF_NO_XMP_SUPPORT

#ifdef _MSC_VER
int strcasecmp(const char* a, const char* b) {
    return _stricmp(a, b);
}
#else
#include <string.h>
#endif

#ifndef TINYEXIF_NO_XMP_SUPPORT
// search string inside a string, case sensitive
static const char* strrnstr(const char* haystack, const char* needle, size_t bytes) {
    const size_t needle_len(strlen(needle));
    if (0 == needle_len)
        return haystack;
    if (bytes <= needle_len)
        return nullptr;
    for (size_t i=bytes-needle_len; i-- > 0; ) {
        if (haystack[0] == needle[0] &&
            0 == strncmp(haystack, needle, needle_len))
            return haystack;
        haystack++;
    }
    return nullptr;
}

// split an input string with a delimiter and fill a string vector
static void strSplit(const std::string& str, char delim, std::vector<std::string>& values) {
    values.clear();
    std::string::size_type start(0), end(0);
    while (end != std::string::npos) {
        end = str.find(delim, start);
        values.emplace_back(str.substr(start, end-start));
        start = end + 1;
    }
}

// make sure the given degrees value is between -180 and 180
static double NormD180(double d) {
    return (d = fmod(d+180.0, 360.0)) < 0 ? d+180.0 : d-180.0;
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
class EntryParser {
private:
    const uint8_t* buf = nullptr;
    const unsigned bytes = 0;
    const unsigned tiff_header_start = 0;
    const bool alignIntel = false; // byte alignment (defined in EXIF header)
    unsigned offs = 0; // current offset into buffer
    uint16_t tag = 0, format = 0;
    uint32_t length = 0;
    EXIFInfo &info;

public:
    EntryParser(EXIFInfo &_info, 
        const uint8_t* _buf, unsigned _len, unsigned _tiff_header_start, bool _alignIntel)
        : info(_info), buf(_buf), bytes(_len), tiff_header_start(_tiff_header_start), 
          alignIntel(_alignIntel), offs(0) {}

    void Init(unsigned _offs) {
        offs = _offs - 12;
    }

    void ParseTag() {
        offs  += 12;
        tag    = parse16(buf + offs, alignIntel);
        format = parse16(buf + offs + 2, alignIntel);
        length = parse32(buf + offs + 4, alignIntel);
    }

    const uint8_t* get() const { return buf; }
    unsigned GetOffset() const { return offs; }
    bool IsIntelAligned() const { return alignIntel; }

    uint16_t GetTag() const { return tag; }
    uint32_t GetLength() const { return length; }
    uint32_t GetData() const { return parse32(buf + offs + 8, alignIntel); }
    uint32_t GetSubIFD() const { return tiff_header_start + GetData(); }

    bool IsShort() const { return format == 3; }
    bool IsLong() const { return format == 4; }
    bool IsRational() const { return format == 5 || format == 10; }
    bool IsSRational() const { return format == 10; }
    bool IsFloat() const { return format == 11; }
    bool IsUndefined() const { return format == 7; }

    std::string FetchString() const {
        return parseString(buf, length, GetData(), tiff_header_start, bytes, alignIntel);
    }

    bool fetch_str(exif_str_t& val) const {
        if (format != 2 || length == 0)
            return false;
        if (info.next == nullptr) { info.next = info.strings; }
        std::string s = FetchString();
        fatal_if( (size_t)(info.next - info.strings) <= (s.size() + 1));
        memcpy(info.next, s.c_str(), s.size() + 1);
        val = info.next;
        info.next += s.size() + 1;
        return true;
    }

    bool fetch8(uint8_t& val) const {
        if ((format != 1 && format != 2 && format != 6) || length == 0)
            return false;
        val = parse8(buf + offs + 8);
        return true;
    }

    bool fetch16(uint16_t& val) const {
        if (!IsShort() || length == 0)
            return false;
        val = parse16(buf + offs + 8, alignIntel);
        return true;
    }

    bool fetch16_idx(uint16_t& val, uint32_t idx) const {
        if (!IsShort() || length <= idx)
            return false;
        val = parse16(buf + GetSubIFD() + idx*2, alignIntel);
        return true;
    }

    bool fetch32(uint32_t& val) const {
        if (!IsLong() || length == 0)
            return false;
        val = parse32(buf + offs + 8, alignIntel);
        return true;
    }

    bool fetch_float(float& val) const {
        if (!IsFloat() || length == 0)
            return false;
        val = parseFloat(buf + offs + 8, alignIntel);
        return true;
    }

    bool fetch_double(double& val) const {
        if (!IsRational() || length == 0)
            return false;
        val = parseRational(buf + GetSubIFD(), alignIntel, IsSRational());
        return true;
    }

    bool fetch_double_idx(double& val, uint32_t idx) const {
        if (!IsRational() || length <= idx)
            return false;
        val = parseRational(buf + GetSubIFD() + idx*8, alignIntel, IsSRational());
        return true;
    }

    bool fetch_float_as_doble(double& val) const {
        float _val;
        if (!fetch_float(_val))
            return false;
        val = _val;
        return true;
    }

public:

    static uint8_t parse8(const uint8_t* buf) {
        return buf[0];
    }

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

    static float parseFloat(const uint8_t* buf, bool intel) {
        union {
            uint32_t i;
            float f;
        } i2f;
        i2f.i = parse32(buf, intel);
        return i2f.f;
    }

    static double parseRational(const uint8_t* buf, bool intel, bool isSigned) {
        const uint32_t denominator = parse32(buf+4, intel);
        if (denominator == 0)
            return 0.0;
        const uint32_t numerator = parse32(buf, intel);
        return isSigned ?
            (double)(int32_t)numerator/(double)(int32_t)denominator :
            (double)numerator/(double)denominator;
    }

    static std::string parseString(const uint8_t* buf,
        unsigned num_components,
        unsigned data,
        unsigned base,
        unsigned bytes,
        bool intel)
    {
        std::string value;
        if (num_components <= 4) {
            value.resize(num_components);
            char j = intel ? 0 : 24;
            char j_m = intel ? -8 : 8;
            for (unsigned i=0; i<num_components; ++i, j -= j_m)
                value[i] = (data >> j) & 0xff;
            if (value[num_components-1] == '\0')
                value.resize(num_components-1);
        } else
        if (base+data+num_components <= bytes) {
            const char* const sz((const char*)buf+base+data);
            unsigned num(0);
            while (num < num_components && sz[num] != '\0')
                ++num;
            while (num && sz[num-1] == ' ')
                --num;
            value.assign(sz, num);
        }
        return value;
    }
};

EXIFInfo::EXIFInfo(exif_stream_t* stream) {
    parseFrom(stream);
}

EXIFInfo::EXIFInfo(const uint8_t* data, unsigned length) {
    parseFrom(data, length);
}

// Parse tag as Image IFD
void EXIFInfo::parseIFDImage(EntryParser& parser, unsigned& exif_sub_ifd_offset, unsigned& gps_sub_ifd_offset) {
    switch (parser.GetTag()) {
        case 0x0102:
            // Bits per sample
            parser.fetch16(BitsPerSample);
            break;
        case 0x010e:
            // Image description
            parser.fetch_str(ImageDescription);
            break;
        case 0x010f:
            // Camera maker
            parser.fetch_str(Make);
            break;
        case 0x0110:
            // Camera model
            parser.fetch_str(Model);
            break;
        case 0x0112:
            // Orientation of image
            parser.fetch16(Orientation);
            break;

        case 0x011a:
            // XResolution 
            parser.fetch_double(XResolution);
            break;
        case 0x011b:
            // YResolution 
            parser.fetch_double(YResolution);
            break;
        case 0x0128:
            // Resolution Unit
            parser.fetch16(ResolutionUnit);
            break;
        case 0x0131:
            // Software used for image
            parser.fetch_str(Software);
            break;
        case 0x0132:
            // EXIF/TIFF date/time of image modification
            parser.fetch_str(DateTime);
            break;
        case 0x1001:
            // Original Image width
            if (!parser.fetch32(RelatedImageWidth)) {
                uint16_t _RelatedImageWidth;
                if (parser.fetch16(_RelatedImageWidth))
                    RelatedImageWidth = _RelatedImageWidth;
            }
            break;
        case 0x1002:
            // Original Image height
            if (!parser.fetch32(RelatedImageHeight)) {
                uint16_t _RelatedImageHeight;
                if (parser.fetch16(_RelatedImageHeight))
                    RelatedImageHeight = _RelatedImageHeight;
            }
            break;
        case 0x8298:
            // Copyright information
            parser.fetch_str(Copyright);
            break;
        case 0x8769:
            // EXIF SubIFD offset
            exif_sub_ifd_offset = parser.GetSubIFD();
            break;
        case 0x8825:
            // GPS IFS offset
            gps_sub_ifd_offset = parser.GetSubIFD();
            break;
        default:
            // Try to parse as EXIF tag, as some images store them in here
            parseIFDExif(parser);
            break;
    }
}

// Parse tag as Exif IFD
void EXIFInfo::parseIFDExif(EntryParser& parser) {
    switch (parser.GetTag()) {
        case 0x02bc:
#ifndef TINYEXIF_NO_XMP_SUPPORT
            // XMP Metadata (Adobe technote 9-14-02)
            if (parser.IsUndefined()) {
                const std::string strXML(parser.FetchString());
                parseFromXMPSegmentXML(strXML.c_str(), (unsigned)strXML.length());
            }
#endif
            break;
        case 0x829a:
            // Exposure time in seconds
            parser.fetch_double(ExposureTime);
            break;
        case 0x829d:
            // FNumber
            parser.fetch_double(FNumber);
            break;
        case 0x8822:
            // Exposure Program
            parser.fetch16(ExposureProgram);
            break;
        case 0x8827:
            // ISO Speed Rating
            parser.fetch16(ISOSpeedRatings);
            break;
        case 0x9003:
            // Original date and time
            parser.fetch_str(DateTimeOriginal);
            break;
        case 0x9004:
            // Digitization date and time
            parser.fetch_str(DateTimeDigitized);
            break;
        case 0x9201:
            // Shutter speed value
            parser.fetch_double(ShutterSpeedValue);
            ShutterSpeedValue = 1.0/exp(ShutterSpeedValue*log(2));
            break;
        case 0x9202:
            // Aperture value
            parser.fetch_double(ApertureValue);
            ApertureValue = exp(ApertureValue*log(2)*0.5);
            break;
        case 0x9203:
            // Brightness value
            parser.fetch_double(BrightnessValue);
            break;
        case 0x9204:
            // Exposure bias value 
            parser.fetch_double(ExposureBiasValue);
            break;
        case 0x9206:
            // Subject distance
            parser.fetch_double(SubjectDistance);
            break;
        case 0x9207:
            // Metering mode
            parser.fetch16(MeteringMode);
            break;
        case 0x9208:
            // Light source
            parser.fetch16(LightSource);
            break;
        case 0x9209:
            // Flash info
            parser.fetch16(Flash);
            break;
        case 0x920a:
            // Focal length
            parser.fetch_double(FocalLength);
            break;
        case 0x9214:
            // Subject area
            if (parser.IsShort() && parser.GetLength() > 1) {
                SubjectArea.resize(parser.GetLength());
                for (uint32_t i=0; i<parser.GetLength(); ++i)
                    parser.fetch16_idx(SubjectArea[i], i);
            }
            break;
        case 0x927c:
            // MakerNote
            parseIFDMakerNote(parser);
            break;
        case 0x9291:
            // Fractions of seconds for DateTimeOriginal
            parser.fetch_str(SubSecTimeOriginal);
            break;
        case 0xa002:
            // EXIF Image width
            if (!parser.fetch32(ImageWidth)) {
                uint16_t _ImageWidth;
                if (parser.fetch16(_ImageWidth))
                    ImageWidth = _ImageWidth;
            }
            break;
        case 0xa003:
            // EXIF Image height
            if (!parser.fetch32(ImageHeight)) {
                uint16_t _ImageHeight;
                if (parser.fetch16(_ImageHeight))
                    ImageHeight = _ImageHeight;
            }
            break;
        case 0xa20e:
            // Focal plane X resolution
            parser.fetch_double(LensInfo.FocalPlaneXResolution);
            break;
        case 0xa20f:
            // Focal plane Y resolution
            parser.fetch_double(LensInfo.FocalPlaneYResolution);
            break;
        case 0xa210:
            // Focal plane resolution unit
            parser.fetch16(LensInfo.FocalPlaneResolutionUnit);
            break;
        case 0xa215:
            // Exposure Index and ISO Speed Rating are often used interchangeably
            if (ISOSpeedRatings == 0) {
                double ExposureIndex;
                if (parser.fetch_double(ExposureIndex))
                    ISOSpeedRatings = (uint16_t)ExposureIndex;
            }
            break;
        case 0xa404:
            // Digital Zoom Ratio
            parser.fetch_double(LensInfo.DigitalZoomRatio);
            break;
        case 0xa405:
            // Focal length in 35mm film
            if (!parser.fetch_double(LensInfo.FocalLengthIn35mm)) {
                uint16_t _FocalLengthIn35mm;
                if (parser.fetch16(_FocalLengthIn35mm))
                    LensInfo.FocalLengthIn35mm = (double)_FocalLengthIn35mm;
            }
            break;
        case 0xa431:
            // Serial number of the camera
            parser.fetch_str(SerialNumber);
            break;
        case 0xa432:
            // Focal length and FStop.
            if (parser.fetch_double_idx(LensInfo.FocalLengthMin, 0))
                if (parser.fetch_double_idx(LensInfo.FocalLengthMax, 1))
                    if (parser.fetch_double_idx(LensInfo.FStopMin, 2))
                        parser.fetch_double_idx(LensInfo.FStopMax, 3);
            break;
        case 0xa433:
            // Lens make.
            parser.fetch_str(LensInfo.Make);
            break;
        case 0xa434:
            // Lens model.
            parser.fetch_str(LensInfo.Model);
            break;
    }
}

// Parse tag as MakerNote IFD
void EXIFInfo::parseIFDMakerNote(EntryParser& parser) {
    const unsigned startOff = parser.GetOffset();
    const uint32_t off = parser.GetSubIFD();
    if (0 != strcasecmp(Make, "DJI"))
        return;
    int num_entries = EntryParser::parse16(parser.get()+off, parser.IsIntelAligned());
    if (uint32_t(2 + 12 * num_entries) > parser.GetLength())
        return;
    parser.Init(off+2);
    parser.ParseTag();
    --num_entries;
    exif_str_t maker = nullptr;
    if (parser.GetTag() == 1 && parser.fetch_str(maker)) {
        if (0 == strcasecmp(maker, "DJI")) {
            while (--num_entries >= 0) {
                parser.ParseTag();
                switch (parser.GetTag()) {
                    case 3:
                        // SpeedX
                        parser.fetch_float_as_doble(GeoLocation.SpeedX);
                        break;
                    case 4:
                        // SpeedY
                        parser.fetch_float_as_doble(GeoLocation.SpeedY);
                        break;
                    case 5:
                        // SpeedZ
                        parser.fetch_float_as_doble(GeoLocation.SpeedZ);
                        break;
                    case 9:
                        // Camera Pitch
                        parser.fetch_float_as_doble(GeoLocation.PitchDegree);
                        break;
                    case 10:
                        // Camera Yaw
                        parser.fetch_float_as_doble(GeoLocation.YawDegree);
                        break;
                    case 11:
                        // Camera Roll
                        parser.fetch_float_as_doble(GeoLocation.RollDegree);
                        break;
                }
            }
        }
    }
    parser.Init(startOff+12);
}

// Parse tag as GPS IFD
void EXIFInfo::parseIFDGPS(EntryParser& parser) {
    switch (parser.GetTag()) {
        case 1:
            // GPS north or south
            parser.fetch8(GeoLocation.LatComponents.direction);
            break;
        case 2:
            // GPS latitude
            if (parser.IsRational() && parser.GetLength() == 3) {
                parser.fetch_double_idx(GeoLocation.LatComponents.degrees, 0);
                parser.fetch_double_idx(GeoLocation.LatComponents.minutes, 1);
                parser.fetch_double_idx(GeoLocation.LatComponents.seconds, 2);
            }
            break;
        case 3:
            // GPS east or west
            parser.fetch8(GeoLocation.LonComponents.direction);
            break;
        case 4:
            // GPS longitude
            if (parser.IsRational() && parser.GetLength() == 3) {
                parser.fetch_double_idx(GeoLocation.LonComponents.degrees, 0);
                parser.fetch_double_idx(GeoLocation.LonComponents.minutes, 1);
                parser.fetch_double_idx(GeoLocation.LonComponents.seconds, 2);
            }
            break;
        case 5:
            // GPS altitude reference (below or above sea level)
            parser.fetch8((uint8_t&)GeoLocation.AltitudeRef);
            break;
        case 6:
            // GPS altitude
            parser.fetch_double(GeoLocation.Altitude);
            break;
        case 7:
            // GPS timestamp
            if (parser.IsRational() && parser.GetLength() == 3) {
                double h = 0, m = 0, s = 0;
                parser.fetch_double_idx(h, 0);
                parser.fetch_double_idx(m, 1);
                parser.fetch_double_idx(s, 2);
                char buffer[256];
                snprintf(buffer, 256, "%g %g %g", h, m, s);
                GeoLocation.GPSTimeStamp = buffer;
            }
            break;
        case 11:
            // Indicates the GPS DOP (data degree of precision)
            parser.fetch_double(GeoLocation.GPSDOP);
            break;
        case 18:
            // GPS geodetic survey data
            parser.fetch_str(GeoLocation.GPSMapDatum);
            break;
        case 29:
            // GPS date-stamp
            parser.fetch_str(GeoLocation.GPSDateStamp);
            break;
        case 30:
            // GPS differential indicates whether differential correction is applied to the GPS receiver
            parser.fetch16(GeoLocation.GPSDifferential);
            break;
    }
}

// Locates the JM_APP1 segment and parses it using
// parseFromEXIFSegment() or parseFromXMPSegment()

int EXIFInfo::parseFrom(exif_stream_t* stream) {
    clear();
    // Sanity check: all JPEG files start with 0xFFD8 and end with 0xFFD9
    // This check also ensures that the user has supplied a correct value for bytes.
    const uint8_t* buf(stream->get(stream, 2));
    if (buf == nullptr || buf[0] != JM_START || buf[1] != JM_SOI)
        return PARSE_INVALID_JPEG;
    // Scan for JM_APP1 header (bytes 0xFF 0xE1) and parse its length.
    // Exit if both EXIF and XMP sections were parsed.
    struct APP1S {
        uint32_t& val;
        inline APP1S(uint32_t& v) : val(v) {}
        inline operator uint32_t () const { return val; }
        inline operator uint32_t& () { return val; }
        inline int operator () (int code=PARSE_ABSENT_DATA) const { return val&FIELD_ALL ? (int)PARSE_SUCCESS : code; }
    } app1s(Fields);
    while ((buf = stream->get(stream, 2)) != nullptr) {
        // find next marker;
        // in cases of markers appended after the compressed data,
        // optional JM_START fill bytes may precede the marker
        if (*buf++ != JM_START)
            break;
        uint8_t marker;
        while ((marker=buf[0]) == JM_START && (buf = stream->get(stream, 1)) != nullptr);
        // select marker
        uint16_t sectionLength;
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
                return app1s();
            case JM_APP1:
                if ((buf=stream->get(stream, 2)) == nullptr)
                    return app1s(PARSE_INVALID_JPEG);
                sectionLength = EntryParser::parse16(buf, false);
                if (sectionLength <= 2 || (buf=stream->get(stream, sectionLength-=2)) == nullptr)
                    return app1s(PARSE_INVALID_JPEG);
                switch (int ret=parseFromEXIFSegment(buf, sectionLength)) {
                    case PARSE_ABSENT_DATA:
#ifndef TINYEXIF_NO_XMP_SUPPORT
                        switch (ret=parseFromXMPSegment(buf, sectionLength)) {
                            case PARSE_ABSENT_DATA:
                                break;
                            case PARSE_SUCCESS:
                                if ((app1s|=FIELD_XMP) == FIELD_ALL)
                                    return PARSE_SUCCESS;
                                break;
                            default:
                                return app1s(ret); // some error
                        }
#endif
                        break;
                    case PARSE_SUCCESS:
                        if ((app1s|=FIELD_EXIF) == FIELD_ALL)
                            return PARSE_SUCCESS;
                        break;
                    default:
                        return app1s(ret); // some error
                }
                break;
            default:
                // skip the section
                if ((buf=stream->get(stream, 2)) == nullptr ||
                    (sectionLength=EntryParser::parse16(buf, false)) <= 2 ||
                    !stream->skip(stream, sectionLength-2))
                    return app1s(PARSE_INVALID_JPEG);
        }
    }
    return app1s();
}


typedef struct exif_stream_buffer_s {
    exif_stream_t stream;
    const uint8_t* it;
    const uint8_t* end;
} exif_stream_buffer_t;

static const uint8_t* get(exif_stream_t* stream, uint32_t bytes) {
    exif_stream_buffer_t* sb = (exif_stream_buffer_t*)stream;
    const uint8_t* next = sb->it + bytes;
    if (next > sb->end) return nullptr;
    const uint8_t* begin = sb->it;
    sb->it = next;
    return begin;
}

static bool skip(exif_stream_t* stream, uint32_t bytes) {
    return stream->get(stream, bytes) != nullptr;
}

int EXIFInfo::parseFrom(const uint8_t* buf, uint32_t bytes) {
    exif_stream_buffer_t exif_stream_buffer = {0};
    exif_stream_buffer.it = buf;
    exif_stream_buffer.end = buf + bytes;
    exif_stream_buffer.stream.get = get;
    exif_stream_buffer.stream.skip = skip;
    return parseFrom(&exif_stream_buffer.stream);
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

int EXIFInfo::parseFromEXIFSegment(const uint8_t* data, unsigned bytes) {
    unsigned offs = 6; // current offset into buffer
    if (data == null || bytes < offs)
        return PARSE_ABSENT_DATA;
    if (!std::equal(data, data + offs, "Exif\0\0"))
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
    EntryParser parser(*this, data, bytes, offs, alignIntel);
    offs += 2;
    if (0x2a != EntryParser::parse16(data + offs, alignIntel))
        return PARSE_CORRUPT_DATA;
    offs += 2;
    const unsigned first_ifd_offset = EntryParser::parse32(data + offs, alignIntel);
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
    int num_entries = EntryParser::parse16(data + offs, alignIntel);
    if (offs + 6 + 12 * num_entries > bytes)
        return PARSE_CORRUPT_DATA;
    unsigned exif_sub_ifd_offset = bytes;
    unsigned gps_sub_ifd_offset  = bytes;
    parser.Init(offs+2);
    while (--num_entries >= 0) {
        parser.ParseTag();
        parseIFDImage(parser, exif_sub_ifd_offset, gps_sub_ifd_offset);
    }
    // Jump to the EXIF SubIFD if it exists and parse all the information
    // there. Note that it's possible that the EXIF SubIFD doesn't exist.
    // The EXIF SubIFD contains most of the interesting information that a
    // typical user might want.
    if (exif_sub_ifd_offset + 4 <= bytes) {
        offs = exif_sub_ifd_offset;
        num_entries = EntryParser::parse16(data + offs, alignIntel);
        if (offs + 6 + 12 * num_entries > bytes)
            return PARSE_CORRUPT_DATA;
        parser.Init(offs+2);
        while (--num_entries >= 0) {
            parser.ParseTag();
            parseIFDExif(parser);
        }
    }
    // Jump to the GPS SubIFD if it exists and parse all the information
    // there. Note that it's possible that the GPS SubIFD doesn't exist.
    if (gps_sub_ifd_offset + 4 <= bytes) {
        offs = gps_sub_ifd_offset;
        num_entries = EntryParser::parse16(data + offs, alignIntel);
        if (offs + 6 + 12 * num_entries > bytes)
            return PARSE_CORRUPT_DATA;
        parser.Init(offs+2);
        while (--num_entries >= 0) {
            parser.ParseTag();
            parseIFDGPS(parser);
        }
        GeoLocation.parseCoords();
    }
    return PARSE_SUCCESS;
}

#ifndef TINYEXIF_NO_XMP_SUPPORT

// Main parsing function for a XMP segment.
// Do a sanity check by looking for bytes "http://ns.adobe.com/xap/1.0/\0".
// So the minimum length specified here has to be:
//  29 bytes: "http://ns.adobe.com/xap/1.0/\0" string
//
// PARAM: 'buf' start of the XMP header, which must be the bytes "http://ns.adobe.com/xap/1.0/\0".
// PARAM: 'bytes' length of buffer
//
int EXIFInfo::parseFromXMPSegment(const uint8_t* buf, unsigned bytes) {
    unsigned offs = 29; // current offset into buffer
    if (!buf || bytes < offs)
        return PARSE_ABSENT_DATA;
    if (!std::equal(buf, buf+offs, "http://ns.adobe.com/xap/1.0/\0"))
        return PARSE_ABSENT_DATA;
    if (offs >= bytes)
        return PARSE_CORRUPT_DATA;
    return parseFromXMPSegmentXML((const char*)(buf + offs), bytes - offs);
}

int EXIFInfo::parseFromXMPSegmentXML(const char* szXML, unsigned bytes) {
    // Skip xpacket end section so that tinyxml2 lib parses the section correctly.
    const char* szEnd(Tools::strrnstr(szXML, "<?xpacket end=", bytes));
    if (szEnd != nullptr)
        bytes = (unsigned)(szEnd - szXML);

    // Try parsing the XML packet.
    tinyxml2::XMLDocument doc;
    const tinyxml2::XMLElement* document;
    if (doc.Parse(szXML, bytes) != tinyxml2::XML_SUCCESS ||
        ((document=doc.FirstChildElement("x:xmpmeta")) == nullptr && (document=doc.FirstChildElement("xmp:xmpmeta")) == nullptr) ||
        (document=document->FirstChildElement("rdf:RDF")) == nullptr ||
        (document=document->FirstChildElement("rdf:Description")) == nullptr)
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
    if (element != nullptr) {
        const char* const szProjectionType(element->GetText());
        if (szProjectionType != nullptr) {
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
            if (szAttribute == nullptr) {
                const tinyxml2::XMLElement* const element(document->FirstChildElement(name));
                if (element == nullptr || (szAttribute=element->GetText()) == nullptr)
                    return false;
            }
            std::vector<std::string> values;
            Tools::strSplit(szAttribute, '/', values);
            switch (values.size()) {
            case 1: value = strtod(values.front().c_str(), nullptr); return true;
            case 2: value = strtod(values.front().c_str(), nullptr)/strtod(values.back().c_str(), nullptr); return true;
            }
            return false;
        }
        // same as previous function but with unsigned int results
        static bool Value(const tinyxml2::XMLElement* document, const char* name, uint32_t& value) {
            const char* szAttribute = document->Attribute(name);
            if (szAttribute == nullptr) {
                const tinyxml2::XMLElement* const element(document->FirstChildElement(name));
                if (element == nullptr || (szAttribute = element->GetText()) == nullptr)
                    return false;
            }
            value = strtoul(szAttribute, nullptr, 0); return true;
            return false;
        }
    };
    const char* szAbout(document->Attribute("rdf:about"));
    if (0 == strcasecmp(Make.c_str(), "DJI") || (szAbout != nullptr && 0 == strcasecmp(szAbout, "DJI Meta Data"))) {
        ParseXMP::Value(document, "drone-dji:AbsoluteAltitude", GeoLocation.Altitude);
        ParseXMP::Value(document, "drone-dji:RelativeAltitude", GeoLocation.RelativeAltitude);
        ParseXMP::Value(document, "drone-dji:GimbalRollDegree", GeoLocation.RollDegree);
        ParseXMP::Value(document, "drone-dji:GimbalPitchDegree", GeoLocation.PitchDegree);
        ParseXMP::Value(document, "drone-dji:GimbalYawDegree", GeoLocation.YawDegree);
        ParseXMP::Value(document, "drone-dji:CalibratedFocalLength", Calibration.FocalLength);
        ParseXMP::Value(document, "drone-dji:CalibratedOpticalCenterX", Calibration.OpticalCenterX);
        ParseXMP::Value(document, "drone-dji:CalibratedOpticalCenterY", Calibration.OpticalCenterY);
    } else
    if (0 == strcasecmp(Make.c_str(), "senseFly") || 0 == strcasecmp(Make.c_str(), "Sentera")) {
        ParseXMP::Value(document, "Camera:Roll", GeoLocation.RollDegree);
        if (ParseXMP::Value(document, "Camera:Pitch", GeoLocation.PitchDegree)) {
            // convert to DJI format: senseFly uses pitch 0 as NADIR, whereas DJI -90
            GeoLocation.PitchDegree = Tools::NormD180(GeoLocation.PitchDegree-90.0);
        }
        ParseXMP::Value(document, "Camera:Yaw", GeoLocation.YawDegree);
        ParseXMP::Value(document, "Camera:GPSXYAccuracy", GeoLocation.AccuracyXY);
        ParseXMP::Value(document, "Camera:GPSZAccuracy", GeoLocation.AccuracyZ);
    } else
    if (0 == strcasecmp(Make.c_str(), "PARROT")) {
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
    return PARSE_SUCCESS;
}
#endif

void EXIFInfo::Geolocation_t::parseCoords() {
    // Convert GPS latitude
    if (LatComponents.degrees != DBL_MAX ||
        LatComponents.minutes != 0 ||
        LatComponents.seconds != 0) {
        Latitude =
            LatComponents.degrees +
            LatComponents.minutes / 60 +
            LatComponents.seconds / 3600;
        if ('S' == LatComponents.direction)
            Latitude = -Latitude;
    }
    // Convert GPS longitude
    if (LonComponents.degrees != DBL_MAX ||
        LonComponents.minutes != 0 ||
        LonComponents.seconds != 0) {
        Longitude =
            LonComponents.degrees +
            LonComponents.minutes / 60 +
            LonComponents.seconds / 3600;
        if ('W' == LonComponents.direction)
            Longitude = -Longitude;
    }
    // Convert GPS altitude
    if (hasAltitude() &&
        AltitudeRef == 1) {
        Altitude = -Altitude;
    }
}

bool EXIFInfo::Geolocation_t::hasLatLon() const {
    return Latitude != DBL_MAX && Longitude != DBL_MAX;
}

bool EXIFInfo::Geolocation_t::hasAltitude() const {
    return Altitude != DBL_MAX;
}

bool EXIFInfo::Geolocation_t::hasRelativeAltitude() const {
    return RelativeAltitude != DBL_MAX;
}

bool EXIFInfo::Geolocation_t::hasOrientation() const {
    return RollDegree != DBL_MAX && PitchDegree != DBL_MAX && YawDegree != DBL_MAX;
}

bool EXIFInfo::Geolocation_t::hasSpeed() const {
    return SpeedX != DBL_MAX && SpeedY != DBL_MAX && SpeedZ != DBL_MAX;
}

bool EXIFInfo::GPano_t::hasPosePitchDegrees() const {
    return PosePitchDegrees != DBL_MAX;
}

bool EXIFInfo::GPano_t::hasPoseRollDegrees() const {
    return PoseRollDegrees != DBL_MAX;
}

void EXIFInfo::clear() {
    Fields = FIELD_NA;
    // Strings
    ImageDescription  = "";
    Make              = "";
    Model             = "";
    SerialNumber      = "";
    Software          = "";
    DateTime          = "";
    DateTimeOriginal  = "";
    DateTimeDigitized = "";
    SubSecTimeOriginal= "";
    Copyright         = "";
    // Shorts / unsigned / double
    ImageWidth        = 0;
    ImageHeight       = 0;
    RelatedImageWidth = 0;
    RelatedImageHeight= 0;
    Orientation       = 0;
    XResolution       = 0;
    YResolution       = 0;
    ResolutionUnit    = 0;
    BitsPerSample     = 0;
    ExposureTime      = 0;
    FNumber           = 0;
    ExposureProgram   = 0;
    ISOSpeedRatings   = 0;
    ShutterSpeedValue = 0;
    ApertureValue     = 0;
    BrightnessValue   = 0;
    ExposureBiasValue = 0;
    SubjectDistance   = 0;
    FocalLength       = 0;
    Flash             = 0;
    MeteringMode      = 0;
    LightSource       = 0;
    ProjectionType    = 0;
    SubjectArea.clear();
    // Calibration
    Calibration.FocalLength = 0;
    Calibration.OpticalCenterX = 0;
    Calibration.OpticalCenterY = 0;
    // LensInfo
    LensInfo.FocalLengthMax = 0;
    LensInfo.FocalLengthMin = 0;
    LensInfo.FStopMax = 0;
    LensInfo.FStopMin = 0;
    LensInfo.DigitalZoomRatio = 0;
    LensInfo.FocalLengthIn35mm = 0;
    LensInfo.FocalPlaneXResolution = 0;
    LensInfo.FocalPlaneYResolution = 0;
    LensInfo.FocalPlaneResolutionUnit = 0;
    LensInfo.Make = "";
    LensInfo.Model = "";
    // Geolocation
    GeoLocation.Latitude                = DBL_MAX;
    GeoLocation.Longitude               = DBL_MAX;
    GeoLocation.Altitude                = DBL_MAX;
    GeoLocation.AltitudeRef             = 0;
    GeoLocation.RelativeAltitude        = DBL_MAX;
    GeoLocation.RollDegree              = DBL_MAX;
    GeoLocation.PitchDegree             = DBL_MAX;
    GeoLocation.YawDegree               = DBL_MAX;
    GeoLocation.SpeedX                  = DBL_MAX;
    GeoLocation.SpeedY                  = DBL_MAX;
    GeoLocation.SpeedZ                  = DBL_MAX;
    GeoLocation.AccuracyXY              = 0;
    GeoLocation.AccuracyZ               = 0;
    GeoLocation.GPSDOP                  = 0;
    GeoLocation.GPSDifferential         = 0;
    GeoLocation.GPSMapDatum             = "";
    GeoLocation.GPSTimeStamp            = "";
    GeoLocation.GPSDateStamp            = "";
    GeoLocation.LatComponents.degrees   = DBL_MAX;
    GeoLocation.LatComponents.minutes   = 0;
    GeoLocation.LatComponents.seconds   = 0;
    GeoLocation.LatComponents.direction = 0;
    GeoLocation.LonComponents.degrees   = DBL_MAX;
    GeoLocation.LonComponents.minutes   = 0;
    GeoLocation.LonComponents.seconds   = 0;
    GeoLocation.LonComponents.direction = 0;
    // GPano
    GPano.PosePitchDegrees = DBL_MAX;
    GPano.PoseRollDegrees = DBL_MAX;
    // Video metadata
    MicroVideo.HasMicroVideo = 0;
    MicroVideo.MicroVideoVersion = 0;
    MicroVideo.MicroVideoOffset = 0;
}


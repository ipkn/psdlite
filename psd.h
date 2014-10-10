#pragma once

#include <cstdint>
#include <string>
#include <iostream>

namespace psd
{
    template <typename T> void BEtoLE(T& x);
    template <> inline void BEtoLE<uint8_t>(uint8_t& x)
    {
        // do nothing;
    }

    template <> inline void BEtoLE<uint32_t>(uint32_t& x)
    {
        uint8_t t;
        uint8_t* p = reinterpret_cast<uint8_t*>(&x);
        t = p[0]; p[0] = p[3]; p[3] = t;
        t = p[1]; p[1] = p[2]; p[2] = t;
    }
    template <> inline void BEtoLE<uint16_t>(uint16_t& x)
    {
        uint8_t t;
        uint8_t* p = reinterpret_cast<uint8_t*>(&x);
        t = p[0]; p[0] = p[1]; p[1] = t;
    }

    enum class ColorMode : uint16_t
    {
        Bitmap = 0,
        Grayscale = 1,
        Indexed = 2,
        RGB = 3,
        CMYK = 4,
        Multichannel = 7,
        Duotone = 8,
        Lab = 9,
    };

#pragma pack(push, 1)
    struct Header
    {
        uint32_t signature;
        uint16_t version;
        uint16_t dummy1{};
        uint32_t dummy2{};
        uint16_t num_channels;
        uint32_t height;
        uint32_t width;
        uint16_t bit_depth;
        uint16_t color_mode;

        void convert()
        {
            BEtoLE(version);
            BEtoLE(num_channels);
            BEtoLE(height);
            BEtoLE(width);
            BEtoLE(bit_depth);
            BEtoLE(color_mode);
        }
    };
#pragma pack(pop)

    class psd
    {
        public:
            psd(std::istream&& stream);

            bool save(std::ostream&& f);

            Header header;

            operator bool();
        private:
            bool read_header(std::istream&& f);
            bool read_color_mode(std::istream&& f);

            bool write_header(std::ostream&& f);
            bool write_color_mode(std::ostream&& f);

            bool valid_{false};
    };

}

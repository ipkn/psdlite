#pragma once

#include <cstdint>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>

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
    template <> inline void BEtoLE<int32_t>(int32_t& x)
    {
        BEtoLE(*(uint32_t*)&x);
    }
    template <> inline void BEtoLE<int16_t>(int16_t& x)
    {
        BEtoLE(*(uint16_t*)&x);
    }


    template <typename T>
    struct be
    {
        be()
            : x(0)
        {
        }

        be(T x)
            : x(x)
        {
            BEtoLE(x);
        }

        be(const be& y)
            : x(y.x)
        {
        }

        operator T()
        {
            T y = x;
            BEtoLE(y);
            return y;
        }

        be& operator = (be y)
        {
            x = y;
            return *this;
        }

        be& operator = (T y)
        {
            x = y;
            BEtoLE(y);
            return *this;
        }

        T x;
    };

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
        be<uint16_t> version;
        uint16_t dummy1{};
        uint32_t dummy2{};
        be<uint16_t> num_channels;
        be<uint32_t> height;
        be<uint32_t> width;
        be<uint16_t> bit_depth;
        be<uint16_t> color_mode;
    };

    struct ImageResourceBlock
    {
        uint32_t signature;
        be<uint16_t> image_resource_id;
        std::string name; // encoded as pascal string; 1 byte length header

        std::vector<char> buffer;

        uint32_t size() const;
        bool read(std::istream& stream);
        bool write(std::ostream& stream);
    };

    struct Layer
    {
        be<uint32_t> top, left, bottom, right;
        be<uint16_t> num_channels;
        std::vector<std::pair<be<uint16_t>, be<uint32_t>>> channel_infos; // ID, length
        uint32_t blend_signature;
        be<uint32_t> blend_key;
        uint8_t opacity; // 0 for transparent
        uint8_t clipping; // 0 base, 1 non-base
        uint8_t bit_flags; 
        uint8_t dummy1;
        be<uint32_t> extra_data_length;
        std::vector<char> additional_extra_data;

        struct LayerMask
        {
            be<uint32_t> size;
            be<uint32_t> top, left, bottom, right;
            uint8_t default_color;
            uint8_t flags;
            std::vector<char> additional_data;

            bool read(std::istream& f);
            bool write(std::ostream& f)
            {
                f.write((char*)&size, 4);
                if (size)
                {
                    f.write((char*)&top, 4*4+2);
                    uint32_t remaining = size - (4*4+2);
                    additional_data.resize(remaining);
                    f.write(&additional_data[0], remaining);
                }
                return true;
            }

        } mask;

        struct LayerBlendingRanges
        {
            std::vector<char> data;
            bool read(std::istream& f);

            bool write(std::ostream& f)
            {
                be<uint32_t> size = data.size();
                f.write((char*)&size, 4);
                f.write(&data[0], size);
                return true;
            }
        } blending_ranges;
        std::string name;
        std::wstring wname;
        std::string utf8name;
        bool has_text{};

        bool read(std::istream& f);
        bool write(std::ostream& f);
    };

    struct ImageData
    {
    };

    struct LayerInfo
    {
        be<int16_t> num_layers{};
        bool has_merged_alpha_channel{};
        std::vector<Layer> layers;
        std::vector<ImageData> channel_image_datas;

        bool read(std::istream& stream);
        bool write(std::ostream& stream);
    };

    struct GlobalLayerMaskInfo
    {
        be<uint32_t> length;
        be<uint16_t> overlay_colorspace;
        be<uint16_t> color_component[4];
        be<uint16_t> opacity; // 0 = transparent 100 = opaque
        uint8_t kind;

        bool read(std::istream& stream);
        bool write(std::ostream& stream);
    };

#pragma pack(pop)

    class psd
    {
        public:
            psd();
            template <typename Stream>
            psd(Stream&& stream)
            {
                load(stream);
            }

            bool load(std::istream& stream);
            bool save(std::ostream& f);

            Header header;
            std::vector<ImageResourceBlock> image_resources;
            LayerInfo layer_info;
            GlobalLayerMaskInfo global_layer_mask_info;
            std::vector<Layer>& layers{layer_info.layers};
            std::vector<ImageData>& channel_image_datas{layer_info.channel_image_datas};

            operator bool();
        private:
            bool read_header(std::istream& f);
            bool read_color_mode(std::istream& f);
            bool read_image_resources(std::istream& f);
            bool read_layers_and_masks(std::istream& f);

            bool read_layer_info(std::istream& f);

            bool write_header(std::ostream& f);
            bool write_color_mode(std::ostream& f);
            bool write_image_resources(std::ostream& f);

            bool valid_{false};

    };

}

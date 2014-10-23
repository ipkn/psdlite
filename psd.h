#pragma once

#include <cstdint>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <cassert>

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

        operator T() const
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

        T operator += (T y)
        {
            x += y;
            return x;
        }

        T operator -= (T y)
        {
            x -= y;
            return x;
        }
    };

    struct Signature
    {
        Signature()
            : sig(0)
        {
        }

        Signature(uint32_t sig)
            : sig(sig)
        {
        }

        Signature(const std::string& str)
        {
            assert(str.size() == 4);
            sig = *(uint32_t*)str.data();
        }

        uint32_t sig;

        operator std::string()
        {
            return std::string((char*)&sig, (char*)&sig+4);
        }
    };

    inline bool operator == (const Signature& sig, const std::string& str)
    {
        return sig.sig == Signature(str).sig;
    }

    inline bool operator == (const std::string& str, const Signature& sig)
    {
        return sig.sig == Signature(str).sig;
    }

    inline bool operator != (const Signature& sig, const std::string& str)
    {
        return sig.sig != Signature(str).sig;
    }

    inline bool operator != (const std::string& str, const Signature& sig)
    {
        return sig.sig != Signature(str).sig;
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
        Header()
            : dummy1(0), dummy2(0)
        {}
        Signature signature;
        be<uint16_t> version;
        uint16_t dummy1;
        uint32_t dummy2;
        be<uint16_t> num_channels;
        be<uint32_t> height;
        be<uint32_t> width;
        be<uint16_t> bit_depth;
        be<uint16_t> color_mode;
    };

    struct ImageResourceBlock
    {
        Signature signature;
        be<uint16_t> image_resource_id;
        std::string name; // encoded as pascal string; 1 byte length header

        std::vector<char> buffer;

        uint32_t size() const;
        bool read(std::istream& stream);
        bool write(std::ostream& stream);
    };
    
    struct ExtraData
    {
        Signature signature;
        Signature key;
        be<uint32_t> length;
        std::vector<char> data;

        uint32_t size() const { return data.size() + (data.size()%2); }
        bool read(std::istream& stream);
        bool write(std::ostream& stream);

        void luni_read_name(std::wstring& wname, std::string& utf8name);
    };

    struct ImageData
    {
        uint32_t w;
        uint32_t h;
        be<uint16_t> compression_method;
        std::vector<std::vector<char>> data;
        bool read(std::istream& f, uint32_t w, uint32_t h);
        bool write(std::ostream& f);

        bool read_with_method(std::istream& f, uint32_t w, uint32_t h, uint16_t compression_method);
        bool write_with_method(std::ostream& f, uint16_t compression_method);
    };

    struct MultipleImageData
    {
        uint32_t w;
        uint32_t h;
        uint32_t count;
        be<uint16_t> compression_method;
        std::vector<std::vector<std::vector<char>>> datas;
        bool read(std::istream& f, uint32_t w, uint32_t h, uint32_t count, uint16_t bit_depth);
        bool write(std::ostream& f);
    };

    struct Layer
    {
        Layer() : has_text(false) {}
        be<uint32_t> top, left, bottom, right;
        be<uint16_t> num_channels;
        std::vector<std::pair<be<int16_t>, be<uint32_t>>> channel_infos; // ID, length
        std::vector<ImageData> channel_info_data;
        ImageData* get_channel_info_by_id(int16_t id)
        {
            for(uint16_t i = 0; i < channel_infos.size(); i ++)
                if (channel_infos[i].first == id)
                    return &channel_info_data[i];
            return nullptr;
        }

        Signature blend_signature;
        be<uint32_t> blend_key;
        uint8_t opacity; // 0 for transparent
        uint8_t clipping; // 0 base, 1 non-base
        uint8_t bit_flags; 
        uint8_t dummy1;
        be<uint32_t> extra_data_length;
        std::vector<ExtraData> additional_extra_data;

        struct LayerMask
        {
            uint32_t size() const { return 4 + (uint32_t)length; }
            be<uint32_t> length;
            be<uint32_t> top, left, bottom, right;
            uint8_t default_color;
            uint8_t flags;
            std::vector<char> additional_data;

            bool read(std::istream& f);
            bool write(std::ostream& f);

        } mask;

        struct LayerBlendingRanges
        {
            uint32_t size() const { return data.size() + 4; }
            std::vector<char> data;
            bool read(std::istream& f);
            bool write(std::ostream& f);
        } blending_ranges;
        std::string name;
        std::wstring wname;
        std::string utf8name;
        bool has_text;

        bool read(std::istream& f);
        bool write(std::ostream& f);
        bool read_images(std::istream& f);
        bool write_images(std::ostream& f);
    };

    struct LayerInfo
    {
        LayerInfo()
            : num_layers(0), has_merged_alpha_channel(false)
        {}
        be<int16_t> num_layers;
        bool has_merged_alpha_channel;
        std::vector<Layer> layers;

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
        std::vector<char> data;

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
            std::vector<Layer>& layers() { return layer_info.layers; }
            MultipleImageData merged_image;

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

            bool valid_;

    };

}

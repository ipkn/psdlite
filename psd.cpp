#include "psd.h"
#include <cassert>
#include <sstream>

#define PSD_DEBUG

namespace psd
{
    template <uint32_t padding>
    uint32_t padded_size(uint32_t size)
    {
        return (size + padding-1)/padding*padding;
    }

    uint32_t ImageResourceBlock::size() const
    {
        return 
            4 + 2 + 
            padded_size<2>(1+name.size()) + 
            4 +
            padded_size<2>(buffer.size());
    }

    bool ImageResourceBlock::read(std::istream& stream)
    {
#ifdef PSD_DEBUG
        auto start_pos = stream.tellg();
#endif
        stream.read((char*)&signature, 4);
        if (signature != "8BIM")
        {
#ifdef PSD_DEBUG
            std::cout << "Invalid image resource block signature: " << std::string((char*)&signature, (char*)&signature+4) << std::endl;;
#endif
            return false;
        }

        stream.read((char*)&image_resource_id, 2);

        uint8_t length;
        stream.read((char*)&length, 1);
        name.resize(length);
        stream.read(&name[0], length);
        if (length % 2 == 0)
            stream.seekg(1, std::ios::cur);

        be<uint32_t> buffer_length;
        stream.read((char*)&buffer_length, 4);
        buffer.resize(buffer_length);
        stream.read(&buffer[0], buffer_length);
        if (buffer_length % 2 == 1)
            stream.seekg(1, std::ios::cur);
#ifdef PSD_DEBUG
        std::cout << "Block "<<image_resource_id<<" name: (" << (int)length << ")" << name << ' ' << buffer_length << ' ' << buffer.size() << ' ' << stream.tellg()-start_pos << ' ' << size() << std::endl;

        if (stream.tellg() - start_pos != size())
        {
            std::cout << "size wrong" << std::endl;
            return false;
        }
#endif

        return true;
    }

    bool ImageResourceBlock::write(std::ostream& stream)
    {
#ifdef PSD_DEBUG
        auto start_pos = stream.tellp();
#endif
        stream.write("8BIM", 4);
        stream.write((char*)&image_resource_id, 2);

        char padding = 0;

        uint8_t length = name.size();
        stream.write((char*)&length, 1);
        stream.write(name.data(), name.size());
        if (length % 2 == 0)
            stream.write(&padding, 1);

        be<uint32_t> buffer_length = buffer.size();
        stream.write((char*)&buffer_length, 4);
        stream.write(buffer.data(), buffer.size());
        if (buffer.size() % 2 == 1)
            stream.write(&padding, 1);

#ifdef PSD_DEBUG
        if (stream.tellp() - start_pos != size())
        {
            std::cerr << "if (stream.tellp() - start_pos != size())" << std::endl;
            return false;
        }
#endif

        return true;
    }

    psd::psd()
        : valid_(false)
    {
    }

    bool psd::load(std::istream& stream)
    {
        valid_ = false;
        if (!read_header(stream))
            return false;
        if (!read_color_mode(stream))
            return false;
        if (!read_image_resources(stream))
            return false;
        if (!read_layers_and_masks(stream))
            return false;
        if (!merged_image.read(stream, header.width, header.height, header.num_channels, header.bit_depth))
            return false;

        valid_ = true;
        return true;
    }

    bool psd::read_header(std::istream& f)
    {
        f.seekg(0);
        f.read((char*)&header, sizeof(header));

        if (header.signature != *(uint32_t*)"8BPS")
        {
            std::cerr << "signature error" << std::endl;
            return false;
        }

        if (header.version != 1)
        {
            std::cerr << "header version error" << std::endl;
            return false;
        }

        if (header.bit_depth != 8)
        {
            std::cerr << "Not supported bit depth: " << header.bit_depth << std::endl;
            return false;
        }

#ifdef PSD_DEBUG
        std::cout << "Header:" << std::endl;
        std::cout << "\tsignature: " << std::string((char*)&header.signature, (char*)&header.signature + 4) << std::endl;
        std::cout << "\tversion: " << header.version << std::endl;
        std::cout << "\tnum_channels: " << header.num_channels << std::endl;
        std::cout << "\twidth: " << header.width << std::endl;
        std::cout << "\theight: " << header.height << std::endl;
        std::cout << "\tbit_depth: " << header.bit_depth << std::endl;
        std::cout << "\tcolor_mode: " << header.color_mode << std::endl;
#endif

        return true;
    }

    bool psd::read_color_mode(std::istream& f)
    {
        uint32_t count;
        f.read((char*)&count, sizeof(count));
        if (count != 0)
        {
            std::cerr << "Not implemented color mode: " << header.color_mode;
            return false;
        }
        return true;
    }

    uint16_t Layer::name_size()
    {
        return padded_size<4>(1 + name.size());
    }

    bool Layer::LayerBlendingRanges::read(std::istream& f)
    {
        be<uint32_t> size;
        f.read((char*)&size, 4);
        data.resize(size);
        f.read(&data[0], size);
        return true;
    }

    bool Layer::LayerBlendingRanges::write(std::ostream& f)
    {
        be<uint32_t> size = data.size();
#ifdef PSD_DEBUG
        std::cout << "Writing blending ranges (size: " << size << ")" << std::endl;
#endif
        f.write((char*)&size, 4);
        f.write(&data[0], size);
        return true;
    }
    
    bool Layer::LayerMask::write(std::ostream& f)
    {
        f.write((char*)&length, 4);
        if (length)
        {
            f.write((char*)&top, 4*4+2);
            uint32_t remaining = length - (4*4+2);
            additional_data.resize(remaining);
            f.write(&additional_data[0], remaining);
        }
        return true;
    }

    bool Layer::LayerMask::read(std::istream& f)
    {
        f.read((char*)&length, 4);
#ifdef PSD_DEBUG
        std::cout << "Reading mask (size: " << length << ")" << std::endl;
#endif
        if (length)
        {
            f.read((char*)&top, 4*4+2);
            uint32_t remaining = length - (4*4+2);
            additional_data.resize(remaining);
            f.read(&additional_data[0], remaining);
        }
        return true;
    }

    bool Layer::read(std::istream& f)
    {
        f.read((char*)&top, 4*4+2);
#ifdef PSD_DEBUG
        std::cout << '\t' << top << ' ' << left <<' ' <<bottom << ' ' << right << std::endl;
        std::cout << "Number of channels: " << num_channels << std::endl;
#endif
        for(uint16_t i = 0; i < num_channels; i ++)
        {
            unsigned char buffer[6];
            f.read((char*)buffer, 6);
            channel_infos.emplace_back(
                (int16_t)((uint16_t)buffer[0]*256+(uint16_t)buffer[1]),
                (uint32_t)(
                    ((uint32_t)buffer[2]<<24)+
                    ((uint32_t)buffer[3]<<16)+
                    ((uint32_t)buffer[4]<<8)+
                    ((uint32_t)buffer[5]<<0))
                );
        }
        f.read((char*)&blend_signature, 4*3+4);
#ifdef PSD_DEBUG
        std::cout << "Blend Signature: " << std::string((char*)&blend_signature, (char*)&blend_signature+4) << std::endl;
#endif
        if ((*(uint32_t*)"8BIM") != blend_signature)
            return false;

        auto extra_start_pos = f.tellg();

        if (!mask.read(f))
        {
            std::cerr << "mask read fail" << std::endl;
            return false;
        }
 
        if (!blending_ranges.read(f))
        {
            std::cerr << "blending ranges read fail" << std::endl;
            return false;
        }

        uint8_t name_size;
        f.read((char*)&name_size, 1);
        name.resize(name_size);
        f.read(&name[0], name_size);
        switch(name_size%4)
        {
            case 0:
                f.seekg(3, std::ios::cur);
                break;
            case 1:
                f.seekg(2, std::ios::cur);
                break;
            case 2:
                f.seekg(1, std::ios::cur);
                break;
            case 3:
                break;
        }
        for(char c:name)
            wname += (wchar_t)c;
        utf8name = name;
#ifdef PSD_DEBUG
            std::cout << "ED size" << mask.size() << " + " << blending_ranges.size();
#endif
        while(f.tellg() - extra_start_pos < extra_data_length)
        {
            ExtraData ed;
            if (!ed.read(f))
            {
                std::cerr << "fail to read ExtraData" << std::endl;
                return false;
            }
#ifdef PSD_DEBUG
            std::cout << " + " << ed.size();
#endif
            additional_extra_data.push_back(std::move(ed));
        }
#ifdef PSD_DEBUG
            std::cout << std::endl;
#endif

        for(auto& ed:additional_extra_data)
        {
#ifdef PSD_DEBUG
            std::cout << '\t' << (std::string)ed.key;
#endif
            if (ed.key == "luni")
            {
                ed.luni_read_name(wname, utf8name);
            }
            else if (ed.key == "TySh")
            {
                has_text = true;
            }
        }

#ifdef PSD_DEBUG
        std::cout << std::endl;
        std::cout << "Layer " << utf8name << std::endl;
#endif

        return true;
    }

    bool ExtraData::read(std::istream& f)
    {
        f.read((char*)&signature, 4);
        if (signature != "8BIM" && 
            signature != "8B64")
        {
#ifdef PSD_DEBUG
            std::cout << "Extra data signature error at: " << f.tellg() << ' ' << (std::string)signature <<std::endl;
#endif
            return false;
        }

        f.read((char*)&key, 4);
        f.read((char*)&length, 4);
        data.resize(length);
        f.read(&data[0], length);
        return true;
    }

    void ExtraData::luni_read_name(std::wstring& wname, std::string& utf8name)
    {
        char* p = &data[0];
        be<uint32_t> uni_length = *(be<uint32_t>*)p;
        wname.clear();
        for(uint32_t i = 0; i < uni_length; i ++)
        {
            wname += (wchar_t)(uint16_t)*(be<uint16_t>*)(p+4+i*2);
        }
        utf8name.clear();
        for(auto wc : wname)
        {
            if (wc < 0x80)
                utf8name += (char)wc;
            else if (wc < 0x800) //110xxxxx 10xxxxxx
            {
                utf8name += (char)(0xC0 + ((wc>>6)&0x1F));
                utf8name += (char)(0x80 + (wc & 0x3F));
            }
            else  // 1110xxxx 10xxxxxx 10xxxxxx 6+6+4
            {
                utf8name += (char)(0xE0 + ((wc>>12)&0x0F));
                utf8name += (char)(0x80 + ((wc>>6) & 0x3F));
                utf8name += (char)(0x80 + (wc & 0x3F));
            }
        }
    }

    bool ExtraData::write(std::ostream& f)
    {
        f.write((char*)&signature, 4);
        f.write((char*)&key, 4);
        if (data.size() % 2 == 1)
            data.push_back(0);
        f.write((char*)&length, 4);
        data.resize(length);
        f.write(&data[0], length);
        return true;
    }

    bool Layer::write(std::ostream& f)
    {
#ifdef PSD_DEBUG
        if (num_channels != channel_infos.size())
            std::cout << "Image channel count: " << num_channels << " -> " << channel_infos.size() << std::endl;
#endif
        num_channels = channel_infos.size();
        f.write((char*)&top, 4*4+2);

        int idx = 0;
        for(auto& ci:channel_infos)
        {
            std::ostringstream image_buffer;
            channel_info_data[idx++].write(image_buffer);
#ifdef PSD_DEBUG
            //std::cout << "Image channel size change: " << ci.second << " -> " << image_buffer.str().size() << std::endl;
#endif
            ci.second = image_buffer.str().size();

            f.write((char*)&ci.first, 2);
            f.write((char*)&ci.second, 4);
        }
        uint32_t old_size = extra_data_length;
        extra_data_length = mask.size() + blending_ranges.size() + name_size();
#ifdef PSD_DEBUG
        std::cout << "Writing Layer " << old_size << " -> " << mask.size() << " + " << blending_ranges.size() << " + " << name_size();
#endif
        for(auto& ed:additional_extra_data)
        {
            extra_data_length += ed.size();
#ifdef PSD_DEBUG
            std::cout << " + " << ed.size();
#endif
        }
#ifdef PSD_DEBUG
        std::cout << " : " << extra_data_length << std::endl;
#endif
        f.write((char*)&blend_signature, 4*3+4);

        if (!mask.write(f))
            return false;
        if (!blending_ranges.write(f))
            return false;

        uint8_t name_size = name.size();
        f.write((char*)&name_size, 1);
        f.write(&name[0], name_size);
        switch(name_size%4)
        {
            case 0:
                f.write("\0\0\0", 3);
                break;
            case 1:
                f.write("\0\0", 2);
                break;
            case 2:
                f.write("\0", 1);
                break;
            case 3:
                break;
        }
        for(auto& ed:additional_extra_data)
        {
            ed.write(f);
        }

        return true;
    }

    bool Layer::read_images(std::istream& f)
    {
        for(auto& ci:channel_infos)
        {
            ImageData id;
            auto pos = f.tellg();
            id.read(f, right-left, bottom-top);
            auto read_size = f.tellg() - pos;

            if (read_size != ci.second)
            {
                std::cerr << "Layer read image fail" << ' ' << read_size << ' ' << ci.second << std::endl;
                return false;
            }
            channel_info_data.push_back(std::move(id));
        }

        return true;
    }

    bool Layer::write_images(std::ostream& f)
    {
        for(auto& id:channel_info_data)
        {
            if (!id.write(f))
                return false;
        }
        return true;
    }

    bool LayerInfo::read(std::istream& f)
    {
        be<uint32_t> length;
        f.read((char*)&length, 4);
        auto start_pos = f.tellg();

        f.read((char*)&num_layers, 2);
        
        if (num_layers < 0)
        {
            num_layers = -num_layers;
            has_merged_alpha_channel = true;
        }

#ifdef PSD_DEBUG
        std::cout  << "Number of layers: " << num_layers << std::endl;
#endif

        for(int32_t i = 0; i < num_layers; i ++)
        {
#ifdef PSD_DEBUG
            std::cout << "Layer " << i << ": (at " << f.tellg() << ")" << std::endl;
#endif
            Layer l;
            if (!l.read(f))
            {
                std::cerr << "Layer read fail" << std::endl;
                return false;
            }
            layers.push_back(std::move(l));
        }

        for(auto& l:layers)
        {
            if (!l.read_images(f))
            {
                std::cerr << "Layer read images fail" << std::endl;
                return false;
            }
        }

        auto diff = f.tellg() - start_pos;
        if (diff != length && diff + 1 != length)
        {
            std::cerr << "Layer diff fail" << diff << ' ' << length << std::endl;
            return false;
        }
        /*
        if (f.tellg()%2 == 1)
        {
#ifdef PSD_DEBUG
            std::cout << "Seek +1" << std::endl;
#endif
            f.seekg(1, f.cur);
        }
        */

        return true;
    }

    bool LayerInfo::write(std::ostream& f)
    {
        std::ostringstream os;
        be<int16_t> adjusted_num_layers;
        adjusted_num_layers = num_layers;
        if (has_merged_alpha_channel)
            adjusted_num_layers = -(int16_t)num_layers;
#ifdef PSD_DEBUG
        std::cout << "Writing number of layers: " << num_layers <<' ' << adjusted_num_layers << std::endl;
#endif
        os.write((char*)&adjusted_num_layers, 2);
        for(auto& l:layers)
        {
            if (!l.write(os))
                return false;
        }
        for(auto& l:layers)
        {
            if (!l.write_images(os))
                return false;
        }

        std::string output = os.str();
        if (output.size() % 2 == 1)
            output += '\x00';

        be<uint32_t> length = output.size();
        f.write((char*)&length, 4);
        f.write(output.data(), output.size());
        
        return true;
    }

    bool GlobalLayerMaskInfo::read(std::istream& f)
    {
        f.read((char*)&length, 4);
        if (length >= 2+2*4+2+1)
        {
            f.read((char*)&overlay_colorspace, 2+2*4+2+1);
            uint32_t remaining = length - (2+2*4+2+1);
            data.resize(remaining);
            f.read(&data[0], remaining);
        }
        else if (length != 0)
        {
#ifdef PSD_DEBUG
            std::cout << "Invalid GlobalLayerMaskInfo size: " << length << std::endl;
#endif
            return false;
        }
        return true;
    }

    bool GlobalLayerMaskInfo::write(std::ostream& f)
    {
        f.write((char*)&length, 4);
        if (length)
        {
            f.write((char*)&overlay_colorspace, 2+2*4+2+1);
            f.write((char*)&data[0], data.size());
        }
        return true;
    }

    bool ImageData::read_with_method(std::istream& f, uint32_t w, uint32_t h, uint16_t compression_method)
    {
        this->w = w;
        this->h = h;
        this->compression_method = compression_method;
        switch(compression_method)
        {
            case 0: // RAW
                {
                    data.resize(h);
                    for(uint32_t y = 0; y < h; y ++)
                    {
                        data[y].resize(w);
                        f.read(&data[y][0], w);
                    }
                }
                break;
            case 1: // PackBits by line
                {
                    std::vector<be<uint16_t>> lengths;
                    lengths.resize(h);
                    f.read((char*)&lengths[0], 2*h);
                    data.resize(h);
                    for(uint32_t y = 0; y < h; y++)
                    {
                        data[y].resize(lengths[y]);
                        f.read(&data[y][0], lengths[y]);
                        std::vector<char> uncompressed;

                        for(uint32_t i = 0; i < data[y].size(); i ++)
                        {
                            int c = data[y][i];
                            if (c >= 128) c -= 256;
                            if (c == -128)
                            {
                                continue;
                            }
                            else if (c < 0)
                            {
                                i++;
                                for(int j = 0; j < 1-c; j++)
                                    uncompressed.push_back(data[y][i]);
                            }
                            else
                            {
                                if (i+1 + c+1 > data[y].size())
                                {
#ifdef PSD_DEBUG
                                    std::cout << "PackBit source length invalid" << std::endl;
#endif
                                    return false;
                                }
                                uncompressed.insert(uncompressed.end(), data[y].begin()+i+1, data[y].begin()+i+1+c+1);
                                i += c+1;
                            }
                        }
                        if (uncompressed.size()*8%w != 0 || uncompressed.size() == 0)
                        {
#ifdef PSD_DEBUG
                            std::cout << "PackBit line " << y << " uncompressed length invalid " << uncompressed.size() << ' ' << w << std::endl;
#endif
                            return false;
                        }
                        data[y].swap(uncompressed);
                    }
                }
                break;
            default:
                if (compression_method >= 2)
                {
#ifdef PSD_DEBUG
                    std::cout << "Not supported compression method (ImageData): " << compression_method << std::endl;
#endif
                    return false;
                }
        }

        return true;
    }

    bool ImageData::read(std::istream& f, uint32_t w, uint32_t h)
    {
        this->w = w;
        this->h = h;
        f.read((char*)&compression_method, 2);
        return read_with_method(f, w, h, compression_method);
    }

    size_t PackBitCompress(const std::vector<char>& input, std::vector<char>& output)
    {
        auto it = input.begin();
        auto output_size_at_start = output.size();
        std::vector<char> sequence;
        while(it < input.end())
        {
            if (std::distance(it, input.end()) >= 3 && *it == *(it+1) && *it == *(it+2))
            {
                if (!sequence.empty())
                {
                    output.push_back((char)sequence.size()-1);
                    output.insert(output.end(), sequence.begin(), sequence.end());
                    sequence.clear();
                }
                int count = 0;
                char c = *it;
                while(it < input.end() && *it == c) ++it, ++count;
                while(count > 128)
                {
                    count -= 128;
                    output.push_back((char)-127);
                    output.push_back(c);
                }
                output.push_back((char)(1-count));
                output.push_back(c);
            }
            else
            {
                sequence.push_back(*it);
                if (sequence.size() == 128)
                {
                    output.push_back(127);
                    output.insert(output.end(), sequence.begin(), sequence.end());
                    sequence.clear();
                }
                ++it;
            }
        }
        if (!sequence.empty())
        {
            output.push_back((char)sequence.size()-1);
            output.insert(output.end(), sequence.begin(), sequence.end());
            sequence.clear();
        }
#ifdef PSD_DEBUG
        {
            std::vector<char> uncompressed;
            for(uint32_t i = output_size_at_start; i < output.size(); i ++)
            {
                int c = output[i];
                if (c >= 128) c -= 256;
                if (c == -128)
                {
                    continue;
                }
                else if (c < 0)
                {
                    i++;
                    for(int j = 0; j < 1-c; j++)
                        uncompressed.push_back(output[i]);
                }
                else
                {
                    if (i+1 + c+1 > output.size())
                    {
#ifdef PSD_DEBUG
                        std::cout << "PackBit source length invalid" << std::endl;
#endif
                        assert(false);
                    }
                    uncompressed.insert(uncompressed.end(), output.begin()+i+1, output.begin()+i+1+c+1);
                    i += c+1;
                }
            }
            if (input.size() != uncompressed.size())
            {
                for(uint32_t i = 0; i < input.size(); i ++)
                    std::cout << (int)input[i] << ' ';
                std::cout << std::endl;
                for(uint32_t i = output_size_at_start; i < output.size(); i ++)
                    std::cout << (int)output[i] << ' ';
                std::cout << std::endl;
                for(uint32_t i = 0; i < uncompressed.size(); i ++)
                    std::cout << (int)uncompressed[i] << ' ';
                std::cout << std::endl;

            }
            assert(input.size() == uncompressed.size());
            for(uint32_t i = 0; i < uncompressed.size(); i ++)
            {
                assert(input[i] == uncompressed[i]);
            }
        }
#endif
        return output.size() - output_size_at_start;
    }

    bool ImageData::write(std::ostream& f)
    {
        uint64_t raw_size = w*h;
        std::vector<be<uint16_t>> sizes;
        std::vector<char> merged;
        uint64_t packed_size = 0;

        for(auto& line:data)
        {
            sizes.push_back(PackBitCompress(line, merged));
            packed_size += (uint16_t)sizes.back();
        }
        
        if (raw_size > packed_size + 2 * sizes.size())
        {
            // using PackBits
            compression_method = 1;
            f.write((char*)&compression_method, 2);
            f.write((char*)&sizes[0], sizes.size() * 2);
            f.write(merged.data(), merged.size());
        }
        else
        {
            // using raw
            compression_method = 0;
            f.write((char*)&compression_method, 2);
            for(auto& line:data)
            {
                f.write(line.data(), line.size());
            }
        }

        return true;
    }

    bool MultipleImageData::read(std::istream& f, uint32_t w, uint32_t h, uint32_t count, uint16_t bit_depth)
    {
        this->w = w;
        this->h = h;
        this->count = count;
        f.read((char*)&compression_method, 2);
        ImageData imageData;
        if (!imageData.read_with_method(f, w, h*count, compression_method))
        {
            std::cerr << "MultipleImageData::read error" << std::endl;
            return false;
        }
        datas.resize(count);
        uint32_t row = 0;
        for(uint32_t ch = 0; ch < count; ch ++)
        {
            datas[ch].resize(h);
            for(uint32_t y = 0; y < h; y ++)
            {
                datas[ch][y].swap(imageData.data[row++]);
                if (datas[ch][y].size() != w*bit_depth/8)
                {
#ifdef PSD_DEBUG
                    std::cout << datas[ch][y].size() << ' ' << w << ' ' <<bit_depth << std::endl;
#endif
                    return false;
                }
            }
        }
        return true;
    }

    bool MultipleImageData::write(std::ostream& f)
    {
        ImageData imageData;
        imageData.w = w;
        imageData.h = h * datas.size();
        for(auto& data:datas)
        {
            for(auto& line:data)
                imageData.data.push_back(line);
        }
        if (!imageData.write(f))
            return false;
        return true;
    }

    bool psd::read_layers_and_masks(std::istream& f)
    {

        be<uint32_t> length;
        f.read((char*)&length, 4);
        auto start_pos = f.tellg();
        
        if (length == 0)
            return true;

        if (!layer_info.read(f))
            return false;

        if (!global_layer_mask_info.read(f))
            return false;

        if (f.tellg()-start_pos < length)
        {
            auto remaining = length - (f.tellg()-start_pos);
#ifdef PSD_DEBUG
            std::cout << "Layer remaining: " << remaining << " at " << f.tellg() << std::endl;
#endif
            additional_layer_data.resize(remaining);
            f.read(&additional_layer_data[0], remaining);
        }

        return true;
    }

    bool psd::write_layers_and_masks(std::ostream& f)
    {
        std::ostringstream os;

        if (!layer_info.write(os))
            return false;
        if (!global_layer_mask_info.write(os))
            return false;

        os.write(additional_layer_data.data(), additional_layer_data.size());

        std::string output = os.str();
        be<uint32_t> length(output.size());
        f.write((char*)&length, 4);
        f.write(output.data(), output.size());

        return true;
    }

    bool psd::save(std::ostream& f)
    {
        if (!write_header(f))
            return false;
        if (!write_color_mode(f))
            return false;
        if (!write_image_resources(f))
            return false;
        if (!write_layers_and_masks(f))
            return false;
        if (!merged_image.write(f))
            return false;

        return true;
    }

    bool psd::write_header(std::ostream& f)
    {
        f.write((char*)&header, sizeof(header));
        return true;
    }

    bool psd::write_color_mode(std::ostream& f)
    {
        uint32_t t = 0;
        f.write((char*)&t, 4);
        return true;
    }

    bool psd::read_image_resources(std::istream& f)
    {
        be<uint32_t> length;
        f.read((char*)&length, 4);
#ifdef PSD_DEBUG
        std::cout << "Image Resource Block length: " << length << std::endl;
#endif
        auto start_pos = f.tellg();

        image_resources.clear();

        while(f.tellg() - start_pos < length)
        {
            ImageResourceBlock b;
            if (!b.read(f))
            {
                std::cerr << "Cannot read ImageResourceBlock" << std::endl;
                return false;
            }
            image_resources.push_back(std::move(b));
        }
        return true;
    }

    bool psd::write_image_resources(std::ostream& f)
    {
        be<uint32_t> length = 0;
        for(auto& r:image_resources)
        {
            length += r.size();
        }
        f.write((char*)&length, 4);
        for(auto& r:image_resources)
        {
            if (!r.write(f))
                return false;
        }
        return true;
    }

    psd::operator bool()
    {
        return valid_;
    }

}

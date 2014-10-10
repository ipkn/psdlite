#include "psd.h"

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

    bool ImageResourceBlock::read(std::istream&& stream)
    {
#ifdef PSD_DEBUG
        auto start_pos = stream.tellg();
#endif
        stream.read((char*)&signature, 4);
        if (signature != *(uint32_t*)"8BIM")
        {
#ifdef PSD_DEBUG
            std::cout << "Invalid image resource block signature: " << std::string((char*)&signature, (char*)&signature+4) << std::endl;;
#endif
            return false;
        }

        stream.read((char*)&image_resource_id, 2);
        BEtoLE(image_resource_id);

        uint8_t length;
        stream.read((char*)&length, 1);
        name.resize(length);
        stream.read(&name[0], length);
        if (length % 2 == 0)
            stream.seekg(1, std::ios::cur);

        uint32_t buffer_length{};
        stream.read((char*)&buffer_length, 4);
        BEtoLE(buffer_length);
        buffer.resize(buffer_length);
        stream.read(&buffer[0], buffer_length);
        if (buffer_length % 2 == 1)
            stream.seekg(1, std::ios::cur);
#ifdef PSD_DEBUG
        std::cout << "Block name: (" << (int)length << ")" << name << ' ' << buffer_length << std::endl;

        if (stream.tellg() - start_pos != size())
            return false;
#endif

        return true;
    }

    bool ImageResourceBlock::write(std::ostream&& stream)
    {
#ifdef PSD_DEBUG
        auto start_pos = stream.tellp();
#endif
        stream.write("8BIM", 4);
        BEtoLE(image_resource_id);
        stream.write((char*)&image_resource_id, 2);
        BEtoLE(image_resource_id);

        char padding = 0;

        uint8_t length = name.size();
        stream.write((char*)&length, 1);
        stream.write(name.data(), name.size());
        if (length % 2 == 0)
            stream.write(&padding, 1);

        uint32_t buffer_length = buffer.size();
        BEtoLE(buffer_length);
        stream.write((char*)&buffer_length, 4);
        stream.write(buffer.data(), buffer.size());
        if (buffer.size() % 2 == 1)
            stream.write(&padding, 1);

#ifdef PSD_DEBUG
        if (stream.tellp() - start_pos != size())
            return false;
#endif

        return true;
    }

    psd::psd(std::istream&& stream)
    {
        if (!read_header(std::move(stream)))
            return;
        if (!read_color_mode(std::move(stream)))
            return;
        if (!read_image_resources(std::move(stream)))
            return;

        valid_ = true;
    }

    bool psd::read_header(std::istream&& f)
    {
        f.seekg(0);
        f.read((char*)&header, sizeof(header));
        header.convert();

        if (header.signature != *(uint32_t*)"8BPS")
            return false;

        if (header.version != 1)
            return false;

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

    bool psd::read_color_mode(std::istream&& f)
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

    bool psd::read_image_resources(std::istream&& f)
    {
        uint32_t length;
        f.read((char*)&length, 4);
        BEtoLE(length);
#ifdef PSD_DEBUG
        std::cout << "Image Resource Block length: " << length << std::endl;
#endif
        auto start_pos = f.tellg();
        while(f.tellg() - start_pos < length)
        {
            ImageResourceBlock b;
            if (!b.read(std::move(f)))
                return false;
            image_resources.push_back(std::move(b));
        }
        return true;
    }

    bool psd::save(std::ostream&& f)
    {
        if (!write_header(std::move(f)))
            return false;
        if (!write_color_mode(std::move(f)))
            return false;
        return true;
    }

    bool psd::write_header(std::ostream&& f)
    {
        header.convert();
        f.write((char*)&header, sizeof(header));
        header.convert();
        return true;
    }

    bool psd::write_color_mode(std::ostream&& f)
    {
        uint32_t t{};
        f.write((char*)&t, 4);
        return true;
    }

    bool psd::write_image_resources(std::ostream&& f)
    {
        uint32_t length{};
        for(auto& r:image_resources)
        {
            length += r.size();
        }
        f.write((char*)&length, 4);
        for(auto& r:image_resources)
        {
            if (!r.write(std::move(f)))
                return false;
        }
        return true;
    }

    psd::operator bool()
    {
        return valid_;
    }

}

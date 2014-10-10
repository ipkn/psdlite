#include "psd.h"

#define PSD_DEBUG

namespace psd
{
    psd::psd(std::istream&& stream)
    {
        if (!read_header(std::move(stream)))
            return;
        if (!read_color_mode(std::move(stream)))
            return;
        valid_ = true;
    }

    bool psd::read_header(std::istream&& f)
    {
        f.seekg(0);
        f.read((char*)&header, sizeof(header));
        header.convert();

        if (std::string((char*)&header.signature, (char*)&header.signature + 4) != "8BPS")
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

    psd::operator bool()
    {
        return valid_;
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

}

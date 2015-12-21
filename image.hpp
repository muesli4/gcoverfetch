#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <memory>

struct image
{
    typedef std::vector<char> image_data_type;
    typedef std::shared_ptr<image_data_type const> shared_image_type;

    image(char * ptr, std::size_t size, std::string const & format, std::string const & source);

    shared_image_type shared_image_data() const;

    std::string format() const;

    std::string source() const;

    void write_to(std::string path) const;

    std::string filename(std::string basename) const;

    private:

    std::shared_ptr<image_data_type> _shared_image_data;
    std::string _format;
    std::string _source;
};

#endif


#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <vector>
#include <string>
#include <algorithm>
#include <fstream>

struct image
{
    typedef std::vector<char> image_data_type;

    image(image_data_type const & image_data, std::string const & format, std::string const & source);

    image(char * ptr, std::size_t size, std::string const & format, std::string const & source);

    image_data_type data() const;

    std::string format() const;

    std::string source() const;

    void write_to(std::string path) const;

    std::string filename(std::string basename) const;

    private:

    image_data_type _image_data;
    std::string _format;
    std::string _source;
};

#endif


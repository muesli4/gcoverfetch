#include "image.hpp"

#include <iterator>

image::image(image_data_type const & image_data, std::string const & format, std::string const & source)
    : _image_data(image_data)
    , _format(format)
    , _source(source)
{
}

image::image(char * ptr, std::size_t size, std::string const & format, std::string const & source)
    : _image_data()
    , _format(format)
    , _source(source)
{
    _image_data.reserve(size);
    std::copy(ptr, ptr + size, std::back_inserter(_image_data));
}

image::image_data_type image::data() const
{
    return _image_data;
}

std::string image::format() const
{
    return _format;
}

std::string image::source() const
{
    return _source;
}

void image::write_to(std::string path) const
{
    std::ofstream ofs(path, std::ofstream::trunc);
    
    std::copy(_image_data.begin(), _image_data.end(), std::ostream_iterator<char>(ofs));

    ofs.close();
}

std::string image::filename(std::string basename) const
{
    return basename + '.' + _format;
}


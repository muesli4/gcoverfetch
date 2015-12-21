#include "image.hpp"

#include <iterator>

image::image(char * ptr, std::size_t size, std::string const & format, std::string const & source)
    : _shared_image_data(std::make_shared<image_data_type>())
    , _format(format)
    , _source(source)
{
    auto & image_data = *_shared_image_data;
    image_data.reserve(size);
    std::copy(ptr, ptr + size, std::back_inserter(image_data));
}

image::shared_image_type image::shared_image_data() const
{
    return _shared_image_data;
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
    auto const & image_data = *_shared_image_data;

    std::ofstream ofs(path, std::ofstream::trunc);
    
    std::copy(image_data.begin(), image_data.end(), std::ostream_iterator<char>(ofs));

    ofs.close();
}

std::string image::filename(std::string basename) const
{
    return basename + '.' + _format;
}


#include "fetch.hpp"

#include <sstream>

#include <glyr/glyr.h>

fetch fetch::_instance;

fetch::fetch()
{
    glyr_init();
}

fetch::~fetch()
{
    glyr_cleanup();
}

// TODO reuse GlyrMemCache in a buffer type?
std::vector<image> fetch_cover(std::string path, std::string artist, std::string album)
{
    GlyrQuery q;
    GLYR_ERROR e;
    int length = -1;

    glyr_query_init(&q);

    //glyr_opt_force_utf8(&q, true);
    //glyr_opt_parallel(&q, 0);
    glyr_opt_number(&q, 10);
    //glyr_opt_timeout(&q, 2);

    glyr_opt_artist(&q, artist.c_str());
    glyr_opt_album(&q, album.c_str());

    glyr_opt_type(&q, GLYR_GET_COVERART);
    //glyr_opt_dlcallback(&q, &cb, 0);
    glyr_opt_download(&q, true);
    //glyr_opt_from(&q, "discogs;coverartarchive;");

    //glyr_opt_img_maxsize(&q, 1200);
    //glyr_opt_img_minsize(&q, 400);

    // TODO remove
    glyr_opt_verbosity(&q, 2);

    GlyrMemCache * c = glyr_get(&q, &e, &length);


    std::string error_string;
    std::vector<image> result;

    if (e != GLYRE_OK)
    {
        std::stringstream ss;
        ss << "error with glyr_get: " << glyr_strerror(e);
        error_string = ss.str();
    }
    else
    {
        GlyrMemCache * current = c;

        while (current != 0)
        {
            // TODO create shared_ptr and finalize with glyr_cache_free !
            result.push_back(image(current->data, current->size, current->img_format, current->prov));

            current = current->next;
        }
    }

    // cleanup
    glyr_query_destroy(&q);
    glyr_free_list(c);

    if (!error_string.empty())
    {
        throw std::runtime_error(error_string);
    }

    return result;
}

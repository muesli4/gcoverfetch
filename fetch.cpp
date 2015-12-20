#include "fetch.hpp"

#include <sstream>

#include <glyr/glyr.h>

// TODO reuse GlyrMemCache in a buffer type?
std::vector<image> fetch_cover(std::string path, std::string artist, std::string album)
{
    // TODO refactor
    glyr_init();

    GlyrQuery q;
    GLYR_ERROR e;
    int length = -1;

    glyr_query_init(&q);

    glyr_opt_force_utf8(&q, true);
    glyr_opt_parallel(&q, true);
    glyr_opt_number(&q, 2);
    //glyr_opt_timeout(&q, 2);

    glyr_opt_musictree_path(&q, path.c_str());
    glyr_opt_artist(&q, artist.c_str());
    glyr_opt_album(&q, album.c_str());

    glyr_opt_type(&q, GLYR_GET_COVERART);
    //glyr_opt_dlcallback(&q, &cb, 0);
    glyr_opt_download(&q, true);
    glyr_opt_from(&q, "all");

    GlyrMemCache * c = glyr_get(&q, &e, &length);


    std::string error_string;
    std::vector<image> result;

    if (c == 0)
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
            result.push_back(image(current->data, current->size, current->img_format, current->prov));

            current = current->next;
        }
    }

    // cleanup
    glyr_query_destroy(&q);
    glyr_cache_free(c);

    glyr_cleanup();

    if (!error_string.empty())
    {
        throw std::runtime_error(error_string);
    }

    return result;
}

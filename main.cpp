//#include <stdio.h>

#include <glyr/glyr.h>

#include <unistd.h>

#include <cctype>

#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <array>
#include <condition_variable>

#include <boost/filesystem.hpp>

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/tstring.h>

#include "cover_dialog.hpp"
#include "image.hpp"

namespace fs = boost::filesystem;

std::vector<image> fetch_cover(std::string path, std::string artist, std::string album)
{
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

    if (!error_string.empty())
    {
        throw std::runtime_error(error_string);
    }

    return result;
}

std::string lowercase_extension(fs::path const & path)
{
    std::string ext = fs::extension(path);

    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    return ext.size() == 0 ? "" : ext.substr(1);
}

std::array<std::string const, 3> music_file_extensions = { "ogg", "flac", "mp3" };

bool has_music_extension(fs::path path)
{
    auto end = music_file_extensions.end();

    return find(music_file_extensions.begin(), end, lowercase_extension(path)) != end;
}

// a file with a audio extension
bool is_music_file(fs::path path)
{
    return !fs::is_directory(path) && has_music_extension(path);
}

bool has_discnumber_filename(fs::path path)
{
    std::string const s = path.filename().string();

    // starts with "CD " and contains at least one digit
    return s.size() >= 4
        && s.compare(0, 3, "CD ") == 0
        && std::all_of(std::next(s.begin(), 3), s.end(), [](char c) { return std::isdigit(c) != 0; } );
}

bool is_discnumber_directory(fs::path path)
{
    return fs::is_directory(path) && has_discnumber_filename(path);
}

bool is_album_directory(fs::path path)
{
    // directory contains music files or discnumber directories
    return std::any_of(fs::directory_iterator(path), fs::directory_iterator(), [](fs::path p)
    {
        return is_discnumber_directory(p) || is_music_file(p);
    });
}

// collect parent if predicate matches to any child and stops then
template <typename Pred>
bool collect_parent_recursive_if(fs::path path, Pred p, std::vector<fs::path> & paths)
{
    if (p(path))
    {
        return true;
    }
    else
    {
        if (fs::is_directory(path))
        {
            bool any_matches = false;
            fs::directory_iterator const end;
            for (fs::directory_iterator dir_it(path); dir_it != end; dir_it++)
            {
                any_matches = collect_parent_recursive_if(dir_it->path(), p, paths) || any_matches;
            }

            if (any_matches)
            {
                paths.push_back(path);
            }
        }

        return false;
    }
}

std::vector<fs::path> album_directories(fs::path music_directory)
{
    // TODO remove
    //return list_recursive_if(music_directory, ::is_album_directory);
    std::vector<fs::path> result;
    collect_parent_recursive_if(music_directory, [](fs::path p){ return is_music_file(p) || is_discnumber_directory(p); }, result);
    return result;
}

std::array<std::string const, 3> const cover_extensions = { "jpg", "jpeg", "png" };

int main(int argc, char * * argv)
{

    if (argc == 2)
    {
        glyr_init();

        cover_dialog dialog;


        try
        {

            bool quit = false;

            for (auto const & album_path : album_directories(argv[1]))
            {
                if (quit) break;

                std::cout << "found album directory '" << album_path.string() << "':" << std::endl;

                // already has a cover in the album directory?
                if (std::any_of(fs::directory_iterator(album_path), fs::directory_iterator(), [](fs::path const & fp)
                    {
                        auto end = cover_extensions.end();
                        return !is_directory(fp) && std::find(cover_extensions.begin(), end, lowercase_extension(fp)) != end;
                    })
                   )
                {
                    std::cout << "    > cover found" << std::endl;
                }
                else
                {

                    // fetch cover
                    fs::recursive_directory_iterator const end;
                    
                    std::string album;
                    std::string album_artist;

                    for (fs::recursive_directory_iterator dir_it(album_path); dir_it != end; dir_it++)
                    {
                        fs::path const & path = *dir_it;
                        if ((album.empty() || album_artist.empty()) && is_music_file(path))
                        {
                            TagLib::FileRef f(path.c_str());
                            TagLib::Tag & tag = *f.tag();

                            // read album
                            album = tag.album().to8Bit(true);

                            // read album artist
                            TagLib::StringList sl =
                                tag.properties()
                                [TagLib::String("albumartist", TagLib::String::UTF8)];

                            bool has_album_artist = false;
                            for (auto it = sl.begin(); it != sl.end(); it++)
                            {
                                album_artist = it->to8Bit(true);
                                has_album_artist = true;

                                std::cout << "FOUND ALBUM ARTIST" << std::endl;
                                break;
                            }

                            if (!has_album_artist)
                            {
                                album_artist = tag.artist().to8Bit(true);
                            }

                        }
                    }

                    if (album.empty() && album_artist.empty())
                    {
                        std::cout << "    > failed reading tag information" << std::endl;
                    }
                    else
                    {
                        std::cout << "    > trying to fetch cover for '" << album_artist << " - " << album << '\'' << std::endl;

                        auto res = fetch_cover(album_path.string(), album_artist, album);

                        if (res.size() >= 1)
                        {
                            std::cout << "    > found covers (" << res.size() << ")" << std::endl;

                            std::vector<std::pair<std::string, image::image_data_type>> image_infos;

                            for (auto const & img : res)
                            {
                                //std::string filename = img.filename((fs::temp_directory_path() / fs::unique_path()).string());

                                //img.write_to(filename);
                                //std::cout << "    > temporary image: " << filename << std::endl;

                                image_infos.push_back(std::make_pair(img.source(), img.data()));
                            }

                            // open dialog
                            {

                                dialog.set_title(album_artist + " - " + album);
                                dialog.load_covers(image_infos);
                                dialog.set_album_directory(album_path.string());

                                auto dialog_res = dialog.run();
                                cover_dialog::decision_type decision = dialog_res.first;

                                if (decision == cover_dialog::DECISION_ACCEPT)
                                {
                                    image const & chosen_image = res[dialog_res.second];
                                    std::string const filename = chosen_image.filename((album_path / "front").string());
                                    chosen_image.write_to(filename);

                                    std::cout << "    > wrote cover to: '" << filename << '\'' << std::endl;
                                }
                                else
                                {
                                    if (decision == cover_dialog::DECISION_QUIT)
                                        quit = true;
                                    
                                    std::cout << "    > declined" << std::endl;
                                }
                            }

                            //for (auto const & p : image_infos)
                            //{
                            //    fs::remove(p.first);
                            //}
                        }
                        else
                        {
                            std::cout << "    > couldn't find a matching cover" << std::endl;
                        }


                    }
                }
            }
        }
        catch (std::exception const & e)
        {
            std::cerr << e.what() << std::endl;
        }
        catch (Glib::Exception const & e)
        {
            std::cerr << e.what().raw() << std::endl;
        }

        glyr_cleanup();
    }
}

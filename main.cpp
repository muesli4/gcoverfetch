#include <cctype>

#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <array>

#include <boost/filesystem.hpp>

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/tstring.h>

#include "cover_dialog.hpp"
#include "image.hpp"
#include "fetch.hpp"

namespace fs = boost::filesystem;

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
    std::vector<fs::path> result;
    collect_parent_recursive_if(music_directory, [](fs::path p){ return is_music_file(p) || is_discnumber_directory(p); }, result);
    return result;
}

std::array<std::string const, 3> const cover_extensions = { "jpg", "jpeg", "png" };

// use TagLib to read album and artist from a music file
std::pair<std::string, std::string> read_artist_and_album(fs::path const & path)
{
    std::string album_artist;
    std::string album;

    TagLib::FileRef const f(path.c_str());
    TagLib::Tag const & tag = *f.tag();

    // TODO taglib does not read albumartist somehow
    // read album artist
    TagLib::StringList const & sl = tag.properties()["albumartist"];

    bool has_album_artist = false;
    for (auto it = sl.begin(); it != sl.end(); it++)
    {
        album_artist = it->to8Bit(true);
        has_album_artist = true;

        std::cout << "FOUND ALBUM ARTIST" << std::endl;
        break;
    }

    // if it doesn't have album artist, read the artist tag
    if (!has_album_artist)
    {
        album_artist = tag.artist().to8Bit(true);
    }

    // read album
    album = tag.album().to8Bit(true);

    return std::make_pair(album_artist, album);
}

int main(int argc, char * * argv)
{

    if (argc == 2)
    {
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
                // fetch cover
                else
                {
                    // extract tags from files
                    fs::directory_iterator const end;
                    
                    std::string album;
                    std::string album_artist;

                    // find all music files in the current directory
                    auto const scan_music_directory = [&](fs::path const & search_path)
                    {
                        for (fs::directory_iterator dir_it(search_path); dir_it != end; dir_it++)
                        {
                            fs::path const & sub_path = *dir_it;

                            if (is_music_file(sub_path))
                            {
                                if (album.empty() || album_artist.empty())
                                {
                                    auto res = read_artist_and_album(sub_path);
                                    album_artist = res.first;
                                    album = res.second;
                                }
                                else
                                {
                                    break;
                                }
                            }
                        }
                    };

                    // try the album directory
                    scan_music_directory(album_path);

                    // but if that fails, try whether some music files are
                    // within discnumber directories
                    if (album.empty() && album_artist.empty())
                    {
                        for (fs::directory_iterator dir_it(album_path); dir_it != end; dir_it++)
                        {
                            auto const & p = dir_it->path();

                            if (has_discnumber_filename(p))
                            {
                                scan_music_directory(p);
                                if (!album.empty() && !album_artist.empty())
                                    break;
                            }
                        }
                    }

                    if (album.empty() && album_artist.empty())
                    {
                        std::cout << "    > failed reading tag information" << std::endl;
                    }
                    else
                    {
                        std::cout << "    > trying to fetch cover for '"
                                  << album_artist
                                  << " - "
                                  << album
                                  << '\''
                                  << std::endl;

                        auto res = fetch_cover(album_path.string(), album_artist, album);

                        if (res.size() >= 1)
                        {
                            std::cout << "    > found covers ("
                                      << res.size()
                                      << ")"
                                      << std::endl;

                            std::vector<std::pair<std::string, image::shared_image_type>> image_infos;
                            for (auto const & img : res)
                            {
                                image_infos.push_back(std::make_pair(img.source(), img.shared_image_data()));
                            }

                            // open dialog
                            {

                                dialog.set_title(album_artist + " - " + album);
                                dialog.load_covers(image_infos);
                                dialog.set_album_directory(album_path.string());

                                auto const dialog_res = dialog.run();
                                cover_dialog::decision_type decision = dialog_res.first;

                                if (decision == cover_dialog::DECISION_ACCEPT)
                                {
                                    image const & chosen_image = res[dialog_res.second];
                                    std::string const filename = chosen_image.filename((album_path / "front").string());
                                    chosen_image.write_to(filename);

                                    std::cout << "    > wrote cover to: '"
                                              << filename
                                              << '\''
                                              << std::endl;
                                }
                                else
                                {
                                    if (decision == cover_dialog::DECISION_QUIT)
                                        quit = true;
                                    
                                    std::cout << "    > declined" << std::endl;
                                }
                            }
                        }
                        else
                        {
                            std::cout << "    > couldn't find a matching cover" << std::endl;
                        }
                    }
                }
                std::cout << std::endl;
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
    }
}

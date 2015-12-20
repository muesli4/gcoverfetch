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

#include <gtkmm/hvbox.h>
#include <gtkmm/button.h>
#include <gtkmm/stock.h>
#include <gtkmm/notebook.h>
#include <gtkmm/image.h>
#include <gtkmm/window.h>
#include <gtkmm/main.h>

namespace fs = boost::filesystem;

struct image
{
    typedef std::vector<char> image_data_type;

    image(image_data_type const & image_data, std::string const & format, std::string const & source)
        : _image_data(image_data)
        , _format(format)
        , _source(source)
    {
    }

    image(char * ptr, std::size_t size, std::string const & format, std::string const & source)
        : _image_data()
        , _format(format)
        , _source(source)
    {
        _image_data.reserve(size);
        std::copy(ptr, ptr + size, std::back_inserter(_image_data));

        for (std::size_t i = 0; i < size; i++)
        {
            if (ptr[i] != _image_data[i])
                std::cerr << "invalid byte at " << i << std::endl;
        }
    }

    image_data_type data() const { return _image_data; }

    std::string format() const { return _format; }

    std::string source() const { return _source; }

    void write_to(std::string path) const
    {
        std::ofstream ofs(path, std::ofstream::trunc);
        
        std::copy(_image_data.begin(), _image_data.end(), std::ostream_iterator<char>(ofs));

        ofs.close();
    }

    std::string filename(std::string basename) const
    {
        return basename + '.' + _format;
    }

    private:

    image_data_type _image_data;
    std::string _format;
    std::string _source;
};

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

// TODO remove
/*
template <typename Pred>
std::vector<fs::path> list_recursive_if(fs::path path, Pred p)
{
    std::vector<fs::path> result;

    if (is_directory(path))
    {
        if (p(path))
        {
            result.push_back(path);
        }
        else
        {
            // merge subresults
            fs::directory_iterator const end;
            for (fs::directory_iterator dir_it(path); dir_it != end; dir_it++)
            {
                auto res = list_recursive_if(dir_it->path(), p);
                std::copy(res.begin(), res.end(), std::back_inserter(result));
            }
        }
    }

    return result;
}
*/

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
    collect_parent_recursive_if(music_directory, [](fs::path p){ return is_music_file(p) || is_discnumber_directory(p); },  result);
    return result;
}

/*
bool collect_album_directories(fs::path const & path, std::vector<fs::path> & album_directories)
{
    if (fs::is_directory(path))
    {
    }
}
*/

std::array<std::string const, 3> const cover_extensions = { "jpg", "jpeg", "png" };

struct cover_dialog
{
    typedef enum
    {
        DECISION_ACCEPT,
        DECISION_DECLINE,
        DECISION_QUIT
    } decision_type;

    cover_dialog()
        : _application(Gtk::Application::create())
        , _window()
        , _vbox()
        , _hbox(true)
        , _add_button(Gtk::Stock::ADD)
        , _discard_button(Gtk::Stock::GO_FORWARD)
        , _open_folder_button(Gtk::Stock::OPEN)
        , _quit_button(Gtk::Stock::QUIT)
        , _image_notebook()
        , _images()
        , _album_directory("~/")
        , _decision()
    {

        _vbox.pack_start(_image_notebook);

        _add_button.signal_clicked().connect([&]()
        {
            _decision = DECISION_ACCEPT;
            _window.close();
        });

        _discard_button.signal_clicked().connect([&]()
        {
            _decision = DECISION_DECLINE;
            _window.close();
        });

        _open_folder_button.signal_clicked().connect([&]()
        {
            Gio::AppInfo::launch_default_for_uri("file://" + _album_directory);
        });

        _quit_button.signal_clicked().connect([&]()
        {
            _decision = DECISION_QUIT;
            _window.close();
        });

        _hbox.pack_start(_add_button);
        _hbox.pack_start(_discard_button);
        _hbox.pack_start(_open_folder_button);
        _hbox.pack_end(_quit_button);
        _vbox.pack_end(_hbox, false, false);

        _window.add(_vbox);
        _window.set_type_hint(Gdk::WINDOW_TYPE_HINT_DIALOG);

        _window.show_all_children();
    }

    void set_title(std::string title)
    {
        _window.set_title(title);
    }

    void load_covers(std::vector<std::pair<std::string, std::string>> const & image_infos)
    {
        while (_image_notebook.get_n_pages() > 0)
        {
            _image_notebook.remove_page();
        }

        _images.clear();
        _images.reserve(image_infos.size());

        for (auto const & p : image_infos)
        {

            _images.push_back(Gtk::Image(p.first));
            Gtk::Image & img = _images.back();
            _image_notebook.append_page(img, p.second);
            img.show();
        }
        _image_notebook.show();
    }

    void set_album_directory(std::string album_directory)
    {
        _album_directory = album_directory;
    }

    std::pair<decision_type, int> run()
    {
        _application->run(_window);
        _application = Gtk::Application::create();
        return std::make_pair(_decision, _image_notebook.get_current_page());
    }

    private:

    Glib::RefPtr<Gtk::Application> _application;

    Gtk::Window _window;
    Gtk::VBox _vbox;
    Gtk::HBox _hbox;
    Gtk::Button _add_button;
    Gtk::Button _discard_button;
    Gtk::Button _open_folder_button;
    Gtk::Button _quit_button;

    Gtk::Notebook _image_notebook;
    std::vector<Gtk::Image> _images;

    std::string _album_directory;

    decision_type _decision;
};

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

                            std::vector<std::pair<std::string, std::string>> image_infos;

                            for (auto const & img : res)
                            {
                                std::string filename = img.filename(fs::unique_path().native());

                                img.write_to(filename);

                                image_infos.push_back(std::make_pair(filename, img.source()));
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

                            for (auto const & p : image_infos)
                            {
                                fs::remove(p.first);
                            }
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

        glyr_cleanup();
    }
}

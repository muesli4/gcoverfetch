#include "cover_dialog.hpp"

#include <gdkmm/pixbufloader.h>

cover_dialog::cover_dialog()
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

void cover_dialog::set_title(std::string title)
{
    _window.set_title(title);
}

void cover_dialog::load_covers(std::vector<std::pair<std::string, image::image_data_type>> const & image_infos)
{
    while (_image_notebook.get_n_pages() > 0)
    {
        _image_notebook.remove_page();
    }

    _images.clear();
    _images.reserve(image_infos.size());

    for (auto const & p : image_infos)
    {
        image::image_data_type const & img_data = p.second;

        auto pbl_ptr = Gdk::PixbufLoader::create();
        pbl_ptr->write(reinterpret_cast<guint8 const *>(img_data.data()), img_data.size());
        pbl_ptr->close();

        _images.push_back(Gtk::Image(pbl_ptr->get_pixbuf()));
        Gtk::Image & img = _images.back();
        _image_notebook.append_page(img, p.first);
        img.show();
    }
    _image_notebook.show();
}

void cover_dialog::set_album_directory(std::string album_directory)
{
    _album_directory = album_directory;
}

std::pair<cover_dialog::decision_type, int> cover_dialog::run()
{
    _application->run(_window);
    _application = Gtk::Application::create();
    return std::make_pair(_decision, _image_notebook.get_current_page());
}


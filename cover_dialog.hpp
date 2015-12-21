#ifndef COVER_DIALOG_HPP
#define COVER_DIALOG_HPP

#include <vector>

#include <gtkmm/hvbox.h>
#include <gtkmm/button.h>
#include <gtkmm/stock.h>
#include <gtkmm/notebook.h>
#include <gtkmm/image.h>
#include <gtkmm/window.h>
#include <gtkmm/main.h>

#include "image.hpp"

struct cover_dialog
{
    typedef enum
    {
        DECISION_ACCEPT,
        DECISION_DECLINE,
        DECISION_QUIT
    } decision_type;

    cover_dialog();

    void set_title(std::string title);

    void load_covers(std::vector<std::pair<std::string, image::shared_image_type>> const & image_infos);

    void set_album_directory(std::string album_directory);

    std::pair<decision_type, int> run();

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
    std::vector<Gtk::Label> _labels;
    std::vector<image::shared_image_type> _image_buffers;

    std::string _album_directory;

    decision_type _decision;
};

#endif


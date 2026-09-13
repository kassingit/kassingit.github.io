#include "VueG.hpp"
#include <iostream>
#include "StreamClient.hpp"
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

static const char* CLASS_NAMES[] = {
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck",
    "boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench",
    "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra",
    "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
    "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove",
    "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup",
    "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange",
    "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
    "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse",
    "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink",
    "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
    "hair drier", "toothbrush"
};

void VueG::onNewFrame() {
    std::string frame = client.getLastFrame();

    if (frame.empty()) {
        img.set(blackImage);
        noDataLabel.set_visible(true);
        return;
    }
    noDataLabel.set_visible(false);

    try {
        auto loader = Gdk::PixbufLoader::create();
        loader->write(reinterpret_cast<const guint8*>(frame.data()), frame.size());
        loader->close();
        auto pixbuf = loader->get_pixbuf();

        if (!pixbuf) {
            img.set(blackImage);
            return;
        }

        if (detectionMode != DetectionMode::None && !cachedDetections.empty()) {
            int width = pixbuf->get_width();
            int height = pixbuf->get_height();

            auto surface = Cairo::ImageSurface::create(Cairo::Surface::Format::ARGB32, width, height);
            auto cr = Cairo::Context::create(surface);

            Gdk::Cairo::set_source_pixbuf(cr, pixbuf, 0, 0);
            cr->paint();

            int personCounter = 0;

            for (const auto& d : cachedDetections) {
                cr->set_source_rgb(0, 1, 0);
                cr->set_line_width(2.0);
                cr->rectangle(d.x1, d.y1, d.x2 - d.x1, d.y2 - d.y1);
                cr->stroke();

                std::string className = (d.classId >= 0 && d.classId < 80) ? CLASS_NAMES[d.classId] : "objet";
                std::ostringstream label;
                label << className;

                if (detectionMode == DetectionMode::WithCount && d.classId == 0) {
                    personCounter++;
                    label << " #" << personCounter;
                }

                cr->set_source_rgb(0, 1, 0);
                cr->set_font_size(14);
                cr->move_to(d.x1 + 4, d.y1 - 5);
                cr->show_text(label.str());
            }

            pixbuf = Gdk::Pixbuf::create(surface, 0, 0, width, height);
        }

        img.set(pixbuf);

    } catch (const Glib::Error& ex) {
        std::cerr << "PixbufLoader error: " << ex.what() << std::endl;
        img.set(blackImage);
    }
}

void VueG::onNewDetections() {
    cachedDetections = detectionWorker.getLatestDetections();
}

void VueG::onOpenAction() {
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Choisir une video a ouvrir");

    auto filter = Gtk::FileFilter::create();
    filter->set_name("Fichiers video");
    filter->add_pattern("*.mp4");
    filter->add_pattern("*.avi");
    filter->add_pattern("*.mkv");

    auto filterList = Gio::ListStore<Gtk::FileFilter>::create();
    filterList->append(filter);
    dialog->set_filters(filterList);

    dialog->open(*this, [this, dialog](const Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto file = dialog->open_finish(result);
            if (file) {
                std::string videoPath = file->get_path();
                onViewVideo(videoPath);
            }
        } catch (const Glib::Error& ex) {
            std::cerr << "[Open] Selection annulee ou erreur : " << ex.what() << std::endl;
        }
    });
}

void VueG::onViewVideo(const std::string& videoPath) {
    auto viewWindow = new Gtk::Window();
    viewWindow->set_title("Lecture video");
    viewWindow->set_default_size(800, 600);

    auto video = Gtk::make_managed<Gtk::Video>();
    auto mediaFile = Gtk::MediaFile::create_for_filename(videoPath);
    video->set_media_stream(mediaFile);
    video->set_autoplay(true);
    video->set_expand(true);

    viewWindow->set_child(*video);
    viewWindow->set_transient_for(*this);
    viewWindow->set_modal(false);
    viewWindow->show();
}

void VueG::onQuitAction() {
    close();
}

void VueG::onAboutAction() {
    auto aboutDialog = Gtk::make_managed<Gtk::AboutDialog>();
    aboutDialog->set_program_name("ESP32 Camera Monitor");
    aboutDialog->set_version("1.0.0");
    aboutDialog->set_copyright("© 2026 - Kelly");
    aboutDialog->set_license_type(Gtk::License::GPL_3_0);
    aboutDialog->set_comments(
        "ESP32 Camera Monitor est une application GTK qui permet de faire de la surveillance video\n"
        "et de capturer des images depuis une camera sans fil realisee a partir d'un ESP32 WROVER "
        "utilisant le reseau Wifi et un serveur HTTP.\n\n"
        "Fonctionnalites :\n"
        "  • Affichage en temps reel du flux video\n"
        "  • Capture d'images (JPEG)\n"
        "  • Gestion des dossiers de sauvegarde\n"
        "  • Interface moderne avec GTK4\n"
        "  • Detection d'objets et de personnes");

    std::vector<Glib::ustring> authors;
    authors.push_back("Kelly KASSIN kassinkelly87@gmail.com");
    aboutDialog->set_authors(authors);

    std::vector<Glib::ustring> artists;
    artists.push_back("Kelly KASSIN (Design & developpement)");
    aboutDialog->set_artists(artists);

    aboutDialog->set_website("https://github.com/kassingit/kassingit.github.io");
    aboutDialog->set_website_label("Github");

    aboutDialog->set_transient_for(*this);
    aboutDialog->set_modal(true);
    aboutDialog->show();
}

void VueG::onResetConnectionAction() {
    std::string currentUrl = client.getNewUrl();
    std::thread([this, currentUrl]() {
        client.reconnect(currentUrl);
    }).detach();
}

void VueG::onPauseClicked() {
    isPaused = !isPaused;

    if (isPaused) {
        pauseButton.set_label("▶ Reprendre");
        pauseSymbol.set_visible(true);
        pauseCircle.set_visible(true);
        noDataLabel.set_visible(false);
    } else {
        pauseButton.set_label("   ⏸ Pause   ");
        pauseSymbol.set_visible(false);
        pauseCircle.set_visible(false);
        noDataLabel.set_visible(false);
        onNewFrame();
    }
}

void VueG::addThumbnailToGallery(const std::string& jpegBytes, const std::string& labelText, const std::string& filePath) {
    try {
        auto loader = Gdk::PixbufLoader::create();
        loader->write(reinterpret_cast<const guint8*>(jpegBytes.data()), jpegBytes.size());
        loader->close();
        auto pixbuf = loader->get_pixbuf();

        if (!pixbuf) {
            std::cerr << "[GALLERY] Pixbuf null, image ignoree." << std::endl;
            return;
        }

        int thumbWidth = 270;
        int thumbHeight = (pixbuf->get_height() * thumbWidth) / pixbuf->get_width();
        auto thumbnail = pixbuf->scale_simple(thumbWidth, thumbHeight, Gdk::InterpType::BILINEAR);

        Gtk::Box* container = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
        container->set_spacing(4);
        container->set_margin(6);
        container->set_halign(Gtk::Align::CENTER);

        auto thumbImage = Gtk::make_managed<Gtk::Image>(thumbnail);
        thumbImage->set_size_request(thumbWidth, thumbHeight);
        container->append(*thumbImage);

        auto infoLabel = Gtk::make_managed<Gtk::Label>();
        infoLabel->set_markup("<span size='small' foreground='#d4cbcb'>" + labelText + "</span>");
        container->append(*infoLabel);

        Gtk::MenuButton* menuButton_captures = Gtk::make_managed<Gtk::MenuButton>();
        menuButton_captures->set_icon_name("view-more-symbolic");
        menuButton_captures->set_halign(Gtk::Align::END);
        menuButton_captures->set_valign(Gtk::Align::START);
        menuButton_captures->set_margin(8);
        menuButton_captures->set_hexpand(false);
        menuButton_captures->set_vexpand(false);

        Gtk::Box* popoverContent = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
        popoverContent->set_spacing(4);
        popoverContent->set_margin(6);

        Gtk::Button* viewButton = Gtk::make_managed<Gtk::Button>("Afficher");
        viewButton->set_halign(Gtk::Align::FILL);
        viewButton->signal_clicked().connect([this, jpegBytes]() { onViewThumbnail(jpegBytes); });
        popoverContent->append(*viewButton);

        popoverContent->append(*Gtk::make_managed<Gtk::Separator>());

        Gtk::Button* deleteButton = Gtk::make_managed<Gtk::Button>("Supprimer");
        deleteButton->set_halign(Gtk::Align::FILL);
        deleteButton->signal_clicked().connect([this, container, filePath]() {
            onDeleteThumbnail(container, filePath);
        });
        popoverContent->append(*deleteButton);

        Gtk::Popover* popover = Gtk::make_managed<Gtk::Popover>();
        popover->set_child(*popoverContent);
        menuButton_captures->set_popover(*popover);
        container->append(*menuButton_captures);

        if (capturesList.get_first_child() != nullptr) {
            auto separator = Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL);
            capturesList.prepend(*separator);
        }

        capturesList.prepend(*container);

    } catch (const Glib::Error& ex) {
        std::cerr << "[Capture] Erreur lors de la creation de la miniature : " << ex.what() << std::endl;
    }
}

void VueG::onDeleteThumbnail(Gtk::Widget* container, const std::string& filePath) {
    Gtk::Widget* next = container->get_next_sibling();
    Gtk::Widget* prev = container->get_prev_sibling();

    capturesList.remove(*container);

    if (next && dynamic_cast<Gtk::Separator*>(next)) {
        capturesList.remove(*next);
    } else if (prev && dynamic_cast<Gtk::Separator*>(prev)) {
        capturesList.remove(*prev);
    }

    try {
        if (fs::exists(filePath)) {
            fs::remove(filePath);
        }
    } catch (const fs::filesystem_error& ex) {
        std::cerr << "[Capture] Erreur suppression fichier : " << ex.what() << std::endl;
    }
}

void VueG::onViewThumbnail(const std::string& jpegBytes) {
    try {
        auto loader = Gdk::PixbufLoader::create();
        loader->write(reinterpret_cast<const guint8*>(jpegBytes.data()), jpegBytes.size());
        loader->close();

        auto pixbuf = loader->get_pixbuf();
        if (!pixbuf) return;

        auto viewWindow = new Gtk::Window();
        viewWindow->set_title("Image capturee");
        viewWindow->set_default_size(pixbuf->get_width(), pixbuf->get_height());

        auto fullImage = Gtk::make_managed<Gtk::Image>(pixbuf);
        viewWindow->set_child(*fullImage);

        viewWindow->set_transient_for(*this);
        viewWindow->set_modal(false);
        viewWindow->show();
    } catch (const Glib::Error& ex) {
        std::cerr << "[Afficher] Erreur : " << ex.what() << std::endl;
    }
}

void VueG::onCaptureClicked() {
    std::string frame = client.getLastFrame();

    if (frame.empty()) {
        std::cout << "[Capture]: No frame available for now !" << std::endl;
        return;
    }

    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time = *std::localtime(&now_time);

    std::ostringstream filename;
    filename << saveDirectory << "/capture_" << std::put_time(&local_time, "%d_%m_%Y_%H_%M_%S") << ".jpg";
    std::ofstream outFile(filename.str(), std::ios::binary);

    if (!outFile) {
        std::cerr << "[Capture] Impossible de creer le fichier : " << filename.str() << std::endl;
        return;
    }

    outFile.write(frame.data(), frame.size());
    outFile.close();

    std::ostringstream labelText;
    labelText << "Date : " << std::put_time(&local_time, "%d/%m/%Y") << "\n";
    labelText << "Heure : " << std::put_time(&local_time, "%H:%M:%S");

    addThumbnailToGallery(frame, labelText.str(), filename.str());
}

void VueG::loadExistingCaptures() {
    if (!fs::exists(saveDirectory) || !fs::is_directory(saveDirectory)) {
        return;
    }

    std::vector<fs::path> jpgFiles;
    for (const auto& entry : fs::directory_iterator(saveDirectory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".jpg") {
            jpgFiles.push_back(entry.path());
        }
    }

    std::sort(jpgFiles.begin(), jpgFiles.end(), [](const fs::path& a, const fs::path& b) {
        return fs::last_write_time(a) < fs::last_write_time(b);
    });

    for (const auto& path : jpgFiles) {
        std::ifstream inFile(path, std::ios::binary);
        if (!inFile) continue;

        std::ostringstream buffer;
        buffer << inFile.rdbuf();
        std::string jpegBytes = buffer.str();

        auto ftime = fs::last_write_time(path);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
        std::tm local_time = *std::localtime(&cftime);

        std::ostringstream labelText;
        labelText << "Date : " << std::put_time(&local_time, "%d/%m/%Y") << "\n";
        labelText << "Heure : " << std::put_time(&local_time, "%H:%M:%S");

        addThumbnailToGallery(jpegBytes, labelText.str(), path.string());
    }
}

void VueG::onRecordClicked() {
    if (!isRecording) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm local_time = *std::localtime(&now_time);

        std::ostringstream filename;
        filename << saveDirectory << "/video_"
                  << std::put_time(&local_time, "%d_%m_%Y_%H_%M_%S") << ".mp4";

        recorder.start(filename.str());
        isRecording = true;
        recordButton.set_label("⏹ Arreter");

        recordTimer = Glib::signal_timeout().connect(
            sigc::mem_fun(*this, &VueG::onRecordTick), 100);
    } else {
        recordTimer.disconnect();

        std::thread([this]() { recorder.stop(); }).detach();

        isRecording = false;
        recordButton.set_label("🎥 Enregistrer video ");
    }
}

bool VueG::onRecordTick() {
    std::string frame = client.getLastFrame();
    if (!frame.empty()) {
        recorder.pushFrame(frame);
    }
    return true;
}

std::string VueG::getConfigFilesPath() {
    std::string ConfigDir = Glib::get_home_dir() + "/.config/esp32_monitor/";
    if (!fs::exists(ConfigDir)) {
        fs::create_directories(ConfigDir);
    }
    return ConfigDir + "save_directory.conf";
}

void VueG::saveConfig() {
    std::string ConfigPath = getConfigFilesPath();
    std::ofstream ConfigFile(ConfigPath);

    if (ConfigFile.is_open()) {
        ConfigFile << saveDirectory << std::endl;
        ConfigFile.close();
    } else {
        std::cerr << "[CONFIG] Impossible de sauvegarder le fichier " << ConfigPath << std::endl;
    }
}

void VueG::loadConfig() {
    std::string configPath = getConfigFilesPath();
    std::ifstream configFile(configPath);

    if (configFile.is_open()) {
        std::string savedPath;
        std::getline(configFile, savedPath);
        configFile.close();

        if (!savedPath.empty() && fs::exists(savedPath) && fs::is_directory(savedPath)) {
            saveDirectory = savedPath;
            return;
        }
    }

    saveDirectory = Glib::get_home_dir() + "/Bureau/Personnal_Projects/ESP32_Monitor/Esp_monitor_gtk/Pictures";
    if (!fs::exists(saveDirectory)) {
        fs::create_directory(saveDirectory);
    }
}

void VueG::onChooseFolderClicked() {
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Choisir le dossier de sauvegarde");

    dialog->select_folder(*this, [this, dialog](const Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto folder = dialog->select_folder_finish(result);
            if (folder) {
                saveDirectory = folder->get_path();
            }
            saveConfig();
        } catch (const Glib::Error& ex) {
            std::cerr << "[Dossier] Selection annulee ou erreur : " << ex.what() << std::endl;
        }
    });
}
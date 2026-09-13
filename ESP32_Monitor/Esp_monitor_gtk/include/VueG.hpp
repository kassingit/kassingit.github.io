#pragma once
#include <gtkmm.h>
#include "StreamClient.hpp"
#include "DetectionWorker.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <Recorder.hpp>

class VueG : public Gtk::Window {
private:
    Gtk::Image img;
    Gtk::Overlay overlay;
    Gtk::MenuButton settingsButton;
    Gtk::MenuButton menuButton;

    Gtk::Box vbox{Gtk::Orientation::VERTICAL};
    Gtk::Box toolbar{Gtk::Orientation::HORIZONTAL};
    Gtk::Box PictureBox{Gtk::Orientation::VERTICAL};
    Gtk::Box mainBox{Gtk::Orientation::HORIZONTAL};
    Gtk::Frame* frame;

    Gtk::Button pauseButton;
    Gtk::Button captureButton;
    Gtk::Button recordButton;
    Gtk::Button folderPath;

    Glib::RefPtr<Gio::SimpleActionGroup> actionGroup;
    Glib::RefPtr<Gtk::PopoverMenu> menuPopover;

    StreamClient& client;

    void onNewFrame();
    void onPauseClicked();
    void onCaptureClicked();
    void onRecordClicked();

    void onOpenAction();
    void onQuitAction();
    void onAboutAction();
    void onResetConnectionAction();

    std::string saveDirectory = ".";
    void onChooseFolderClicked();
    void saveConfig();
    void loadConfig();
    std::string getConfigFilesPath();

    Gtk::ScrolledWindow capturesScroll;
    Gtk::Box capturesList{Gtk::Orientation::VERTICAL};
    void loadExistingCaptures();
    void addThumbnailToGallery(const std::string& jpegBytes, const std::string& labelText, const std::string& filePath);
    void onDeleteThumbnail(Gtk::Widget* container, const std::string& filePath);
    void onViewThumbnail(const std::string& jpegBytes);
    void onViewVideo(const std::string& videoPath);

    Glib::RefPtr<Gtk::Popover> settingsPopover;

    enum class DetectionMode { None, BoxesOnly, WithCount };
    DetectionMode detectionMode = DetectionMode::None;

    bool isPaused = false;
    Gtk::Overlay videoOverlay;
    Gtk::Label pauseSymbol;
    Gtk::Box pauseCircle{Gtk::Orientation::VERTICAL};

    Glib::RefPtr<Gdk::Pixbuf> blackImage;
    Gtk::Label noDataLabel;

    Recorder recorder;
    sigc::connection recordTimer;
    bool isRecording = false;
    bool onRecordTick();

    DetectionWorker detectionWorker{"models/yolo26l.onnx", client};
    std::vector<Detection> cachedDetections;
    void onNewDetections();

public:
    VueG(StreamClient& c) : client(c) {
        set_default_size(1400, 700);
        set_title("ESP32 Camera Monitor");

        set_name("main-window");
        folderPath.set_name("folder-button");

        auto cssProvider = Gtk::CssProvider::create();
        cssProvider->load_from_data(
            "#main-window { background-color: rgb(20, 2, 73); }"
        );

        Gtk::StyleContext::add_provider_for_display(
            Gdk::Display::get_default(),
            cssProvider,
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );

        img.set_expand(true);

        settingsButton.set_icon_name("preferences-system-symbolic");
        settingsButton.set_halign(Gtk::Align::START);
        settingsButton.set_valign(Gtk::Align::END);
        settingsButton.set_margin(12);
        settingsButton.set_hexpand(false);
        settingsButton.set_vexpand(false);

        menuButton.set_icon_name("open-menu-symbolic");
        menuButton.set_halign(Gtk::Align::START);
        menuButton.set_valign(Gtk::Align::START);
        menuButton.set_margin(12);
        menuButton.set_hexpand(false);
        menuButton.set_vexpand(false);

        actionGroup = Gio::SimpleActionGroup::create();
        actionGroup->add_action("open", sigc::mem_fun(*this, &VueG::onOpenAction));
        actionGroup->add_action("quit", sigc::mem_fun(*this, &VueG::onQuitAction));
        actionGroup->add_action("about", sigc::mem_fun(*this, &VueG::onAboutAction));
        actionGroup->add_action("reset", sigc::mem_fun(*this, &VueG::onResetConnectionAction));
        insert_action_group("win", actionGroup);

        auto menuModel = Gio::Menu::create();
        menuModel->append("Ouverture de video", "win.open");
        menuModel->append("Reinitialiser connexion", "win.reset");
        menuModel->append("A propos", "win.about");
        menuModel->append("Quitter", "win.quit");

        menuPopover = Glib::RefPtr<Gtk::PopoverMenu>(new Gtk::PopoverMenu(menuModel));
        menuButton.set_popover(*menuPopover);

        pauseButton.set_label("⏸ Pause");
        captureButton.set_label("📸 Capture");
        recordButton.set_label("🎥 Enregistrer video ");
        folderPath.set_label("📁 Dossier de sauvegarde");

        pauseButton.signal_clicked().connect(sigc::mem_fun(*this, &VueG::onPauseClicked));
        captureButton.signal_clicked().connect(sigc::mem_fun(*this, &VueG::onCaptureClicked));
        recordButton.signal_clicked().connect(sigc::mem_fun(*this, &VueG::onRecordClicked));
        folderPath.signal_clicked().connect(sigc::mem_fun(*this, &VueG::onChooseFolderClicked));

        toolbar.append(pauseButton);
        toolbar.append(captureButton);
        toolbar.append(recordButton);
        toolbar.append(folderPath);
        toolbar.set_halign(Gtk::Align::CENTER);
        toolbar.set_spacing(8);
        toolbar.set_margin(8);

        frame = Gtk::make_managed<Gtk::Frame>(" Captures ");
        frame->set_label_align(0.5);
        frame->set_expand(true);

        capturesList.set_spacing(6);
        capturesList.set_margin(4);

        capturesScroll.set_child(capturesList);
        capturesScroll.set_expand(true);
        capturesScroll.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
        frame->set_child(capturesScroll);

        // ---------------- Panneau de reglages ----------------
        Gtk::Box* settingOptions = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
        settingOptions->set_spacing(10);
        settingOptions->set_margin(10);

        Gtk::CheckButton* option1 = Gtk::make_managed<Gtk::CheckButton>("Sans detection");
        Gtk::CheckButton* option2 = Gtk::make_managed<Gtk::CheckButton>("Avec detection - Niveau 1 (boites uniquement)");
        Gtk::CheckButton* option3 = Gtk::make_managed<Gtk::CheckButton>("Avec detection - Niveau 2 (boites + comptage)");
        option2->set_group(*option1);
        option3->set_group(*option1);
        option1->set_active(true);

        settingOptions->append(*option1);
        settingOptions->append(*option2);
        settingOptions->append(*option3);

        Gtk::Box* apply_or_cancel_modification = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
        Gtk::Button* applyButton = Gtk::make_managed<Gtk::Button>("Appliquer");
        Gtk::Button* annulerButton = Gtk::make_managed<Gtk::Button>("annuler");

        applyButton->signal_clicked().connect([option1, option2, option3, this]() {
            if (option1->get_active()) {
                detectionMode = DetectionMode::None;
                detectionWorker.stop();
            } else if (option2->get_active()) {
                detectionMode = DetectionMode::BoxesOnly;
                detectionWorker.start();
            } else if (option3->get_active()) {
                detectionMode = DetectionMode::WithCount;
                detectionWorker.start();
            }
            std::cout << "[Settings] Mode de detection : " << static_cast<int>(detectionMode) << std::endl;
            settingsPopover->popdown();
        });

        annulerButton->signal_clicked().connect([this]() {
            settingsPopover->popdown();
        });

        apply_or_cancel_modification->append(*applyButton);
        apply_or_cancel_modification->append(*annulerButton);
        settingOptions->append(*apply_or_cancel_modification);

        settingsPopover = Glib::RefPtr<Gtk::Popover>(new Gtk::Popover());
        settingsPopover->set_child(*settingOptions);
        settingsButton.set_popover(*settingsPopover);

        // ---------------- Pause overlay ----------------
        pauseSymbol.set_markup("<span size='xx-large' weight='bold' foreground='white'>▶</span>");
        pauseSymbol.set_halign(Gtk::Align::CENTER);
        pauseSymbol.set_valign(Gtk::Align::CENTER);
        pauseSymbol.set_expand(true);
        pauseSymbol.set_opacity(0.9);
        pauseSymbol.set_visible(false);

        Gtk::Frame* circleFrame = Gtk::make_managed<Gtk::Frame>();
        circleFrame->set_halign(Gtk::Align::CENTER);
        circleFrame->set_valign(Gtk::Align::CENTER);
        circleFrame->set_size_request(150, 150);
        circleFrame->set_visible(true);

        auto circleCss = Gtk::CssProvider::create();
        circleCss->load_from_data(
            "frame {"
            "  border-radius: 75px;"
            "  background-color: rgba(137, 2, 2, 0.8);"
            "  min-width: 150px;"
            "  min-height: 150px;"
            "}"
        );
        circleFrame->get_style_context()->add_provider(circleCss, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        circleFrame->set_child(pauseSymbol);
        pauseCircle.append(*circleFrame);
        pauseCircle.set_visible(false);

        videoOverlay.set_child(img);
        videoOverlay.add_overlay(pauseCircle);
        videoOverlay.add_overlay(noDataLabel);
        videoOverlay.set_expand(true);

        vbox.append(videoOverlay);
        vbox.append(toolbar);

        PictureBox.append(*frame);
        PictureBox.set_margin(8);
        PictureBox.set_expand(false);
        PictureBox.set_size_request(280, -1);

        mainBox.append(vbox);
        mainBox.append(PictureBox);
        mainBox.set_spacing(8);

        overlay.set_child(mainBox);
        overlay.add_overlay(settingsButton);
        overlay.add_overlay(menuButton);
        overlay.set_expand(true);
        set_child(overlay);

        // ---------------- Image noire / NO DATA ----------------
        blackImage = Gdk::Pixbuf::create(Gdk::Colorspace::RGB, false, 8, 640, 480);
        blackImage->fill(0x000000FF);

        noDataLabel.set_markup("<span size='xx-large' weight='bold' foreground='red'>⚠ NO DATA</span>");
        noDataLabel.set_halign(Gtk::Align::CENTER);
        noDataLabel.set_valign(Gtk::Align::CENTER);
        noDataLabel.set_visible(false);

        client.newFrameSignal.connect(sigc::mem_fun(*this, &VueG::onNewFrame));
        detectionWorker.newDetectionsSignal.connect(sigc::mem_fun(*this, &VueG::onNewDetections));

        loadConfig();
        loadExistingCaptures();
    }

    virtual ~VueG() {}
};
#pragma once
#include <gtkmm.h>
#include "StreamClient.hpp"
#include <iostream>
#include "Recorder.hpp"

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

    //-------- for Dossier de sauvegarde
    std::string saveDirectory = "."; // we take by default the current directory
    void onChooseFolderClicked();
    void saveConfig();
    void loadConfig();
    std::string getConfigFilesPath();


    //----------- for capture list
    Gtk::ScrolledWindow capturesScroll;
    Gtk::Box capturesList{Gtk::Orientation::VERTICAL};
    void loadExistingCaptures();
    void addThumbnailToGallery(const std::string& jpegBytes, const std::string& labelText,const std::string& filePath);
    void onDeleteThumbnail(Gtk::Widget *container,const std::string& filePath);
    void onViewThumbnail(const std::string& jpegBytes);


    //--- for the setting options
    Glib::RefPtr<Gtk::Popover> settingsPopover;

    enum class DetectionMode{
        None,
        BoxOnly,
        WithCount
    };
    DetectionMode detectionMode = DetectionMode::None;

    //--- FOR PAUSE BUTTON
    bool isPaused = false;
    Gtk::Overlay videoOverlay;
    Gtk::Label pauseSymbol;
    Gtk::Box pauseCircle{Gtk::Orientation::VERTICAL};  // Circle to contain the replay symbol

    // FOR THE BLACK IMAGE 
    Glib::RefPtr<Gdk::Pixbuf> blackImage;


    // FOR THE VIDEO RECORDING
    Recorder recorder;
    sigc::connection recordTimer;

    bool isRecording = false;
    bool onRecordTick(); // it is called periodicly durant the recording

    // TO READ THE VIDEO
    void onViewVideo(const std::string& videoPath);


public:
    VueG(StreamClient& c) : client(c) {
        set_default_size(1400, 700);
        set_title("ESP32 Camera Monitor");

        // --------- needed to set the color 
        set_name("main-window");
        folderPath.set_name("folder-button"); 

        //------------------ windows color 
        auto cssProvider = Gtk::CssProvider::create();
        cssProvider->load_from_data(
            "#main-window { background-color: rgb(20, 2, 73); }"
        );

        Gtk::StyleContext::add_provider_for_display(
            Gdk::Display::get_default(),
            cssProvider,
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );

        // ------------- video 
        img.set_expand(true);

        // ---------------- setting button
        settingsButton.set_icon_name("preferences-system-symbolic");
        settingsButton.set_halign(Gtk::Align::START);  // collé à gauche
        settingsButton.set_valign(Gtk::Align::END);    // collé en bas
        settingsButton.set_margin(12);
        settingsButton.set_hexpand(false);  // ne pas s'étirer horizontalement
        settingsButton.set_vexpand(false);  // ne pas s'étirer verticalement

        // ---------------- Menu button
        menuButton.set_icon_name("open-menu-symbolic");
        menuButton.set_halign(Gtk::Align::START); 
        menuButton.set_valign(Gtk::Align::START); 
        menuButton.set_margin(12);
        menuButton.set_hexpand(false);  
        menuButton.set_vexpand(false);  

        actionGroup = Gio::SimpleActionGroup::create();

        actionGroup->add_action("open",sigc::mem_fun(*this, &VueG::onOpenAction));
        actionGroup->add_action("quit",sigc::mem_fun(*this, &VueG::onQuitAction));
        actionGroup->add_action("about",sigc::mem_fun(*this,&VueG::onAboutAction));
        actionGroup->add_action("reset",sigc::mem_fun(*this, &VueG::onResetConnectionAction));

        insert_action_group("win",actionGroup);

        auto menuModel = Gio::Menu::create();
        menuModel->append("Ouverture de video", "win.open");
        menuModel->append("Réinitialiser connexion", "win.reset");
        menuModel->append("À propos", "win.about");
        menuModel->append("Quitter", "win.quit");

        menuPopover = Glib::RefPtr<Gtk::PopoverMenu>(new Gtk::PopoverMenu(menuModel));
        menuButton.set_popover(*menuPopover);



        // ---------------- Barre d'outils (sous la vidéo, espace dédié) ----------------
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

        //---------- for Images Captured 
        frame = Gtk::make_managed<Gtk::Frame>(" Captures ");
        frame->set_label_align(0.5);  // 0.5 = centré
        frame->set_expand(true);


        capturesList.set_spacing(6);
        capturesList.set_margin(4);

        capturesScroll.set_child(capturesList);
        capturesScroll.set_expand(true);
        capturesScroll.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
        // NEVER = pas de défilement horizontal, AUTOMATIC = défilement vertical si besoin
        frame->set_child(capturesScroll);



        //---------- for the setting options ( S)
        Gtk::Box* settingOptions = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL); // the options will be display vertically
        settingOptions->set_spacing(10);
        settingOptions->set_margin(10);

                //---- Add the Options
        Gtk::CheckButton* option1 = Gtk::make_managed<Gtk::CheckButton>("Sans detection des objets");
        Gtk::CheckButton* option2 = Gtk::make_managed<Gtk::CheckButton>("Avec detection - Niveau 1 ");
        Gtk::CheckButton* option3 = Gtk::make_managed<Gtk::CheckButton>("Avec detection - Niveau 2 ");

        option2->set_group(*option1); // desactivate option1 when option2 is activated
        option3->set_group(*option1); // desactivate option1 when option2 is activated
        option1->set_active(true); // set option1 by default

                // ---- Add the options to setting Options box
        settingOptions->append(*option1);
        settingOptions->append(*option2);
        settingOptions->append(*option3);

                //------ Add a buttons to apply the settings and annuler
        Gtk::Box* apply_or_cancel_modification = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
        Gtk::Button* applyButton = Gtk::make_managed<Gtk::Button>("Appliquer");
        Gtk::Button* annulerButton = Gtk::make_managed<Gtk::Button>("annuler");

        applyButton->signal_clicked().connect([option1,option2,option3,this](){
                //---- Read the states
                if(option1->get_active()){
                        detectionMode = DetectionMode::None;
                }
                else if(option2->get_active()){
                        detectionMode = DetectionMode::BoxOnly;
                }

                else if(option3->get_active()){
                        detectionMode = DetectionMode::WithCount;
                }

                std::cout<<"[Settings] Detection actived :"<< static_cast<int>(detectionMode) <<std::endl;
                settingsPopover->popdown();
        });
                
        annulerButton->signal_clicked().connect([this](){
                settingsPopover->popdown();
                
        });

        apply_or_cancel_modification->append(*applyButton);
        apply_or_cancel_modification->append(*annulerButton);

        settingOptions->append(*apply_or_cancel_modification);

                // creation and attachement of the popover
        settingsPopover = Glib::RefPtr<Gtk::Popover>(new Gtk::Popover());
        settingsPopover->set_child(*settingOptions);
        settingsButton.set_popover(*settingsPopover);

        //--- FOR PAUSE SYMBOL 
        pauseSymbol.set_label("▶");
        pauseSymbol.set_markup("<span size='xx-large' weight='bold' foreground='white'>▶</span>");
        pauseSymbol.set_halign(Gtk::Align::CENTER);
        pauseSymbol.set_valign(Gtk::Align::CENTER);
        pauseSymbol.set_expand(true);
        pauseSymbol.set_opacity(0.9);
        pauseSymbol.set_visible(false); // not visible by default

        // CIRCLE
        Gtk::Frame* circleFrame = Gtk::make_managed<Gtk::Frame>();
        circleFrame->set_halign(Gtk::Align::CENTER);
        circleFrame->set_valign(Gtk::Align::CENTER);
        circleFrame->set_size_request(150, 150);  // Taille du cercle
        circleFrame->set_visible(true);

        // Appliquer le CSS pour le cercle rouge
        auto circleCss = Gtk::CssProvider::create();
        circleCss->load_from_data(
        "frame {"
        "  border-radius: 75px;"          // Cercle parfait
        "  background-color: rgba(137, 2, 2, 0.8);"  // Rouge semi-transparent
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
        videoOverlay.set_expand(true);

        // ------------ final assemblying 
        vbox.append(videoOverlay);
        vbox.append(toolbar);


        // box des images capturées à droite de la video
        PictureBox.append(*frame);
        PictureBox.set_margin(8); // the borders around the title
        PictureBox.set_expand(false);
        PictureBox.set_size_request(280, -1);  // largeur minimale 200px, hauteur libre (-1)
                                                // to allow the image in capture to be larger


        //
        mainBox.append(vbox);
        mainBox.append(PictureBox);
        mainBox.set_spacing(8); // space between the boxes


        overlay.set_child(mainBox);
        overlay.add_overlay(settingsButton);
        overlay.add_overlay(menuButton);
        overlay.set_expand(true);
        set_child(overlay);

        client.newFrameSignal.connect(sigc::mem_fun(*this, &VueG::onNewFrame));

        //========= FOR BLACKIMAGE WHEN CONNECTION FAILED
        blackImage = Gdk::Pixbuf::create(Gdk::Colorspace::RGB,false,8,640,480);
        blackImage->fill(0x000000FF);  // black (R=0, G=0, B=0, A=255);

        auto surface = Cairo::ImageSurface::create(Cairo::Surface::Format::RGB24, 640, 480);
        auto cr = Cairo::Context::create(surface);

                //== Write the text NO DATA in the black image 
        cr->set_source_rgb(0.0, 0.0, 0.0);
        cr->paint();

        cr->set_source_rgb(1.0, 0.0, 0.0); // Gris clair
        cr->set_font_size(20);
        cr->move_to(280, 240); // Ajusté pour bien centrer le texte
        cr->show_text("NO DATA !");

        // update the image
        blackImage = Gdk::Pixbuf::create(surface,0,0,640,480);
       
        img.set(blackImage);


        // Loading the configuration file before reload the existing captures
        loadConfig();
        
        // to refresh the images captures when the app will run after a period on shutdown
        loadExistingCaptures();

    }

    virtual ~VueG() {}
};
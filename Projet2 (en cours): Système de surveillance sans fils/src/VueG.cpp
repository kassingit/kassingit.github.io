#include "VueG.hpp"
#include <iostream>
#include "StreamClient.hpp"
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace fs = std::filesystem;

void VueG::onNewFrame() {
    std::string frame = client.getLastFrame();
    std::cout << "onNewFrame: got frame of size " << frame.size() << std::endl;

    // 
    if (frame.empty()) {
        std::cout << "[DEBUG] Frame vide, affichage de l'image noire" << std::endl;
        if (blackImage) {
            img.set(blackImage);
        }
        return;
    }
    
    std::cout << "onNewFrame: got frame of size " << frame.size() << std::endl;

    try {
        auto loader = Gdk::PixbufLoader::create();
        loader->write(reinterpret_cast<const guint8*>(frame.data()), frame.size());
        loader->close();
        auto pixbuf = loader->get_pixbuf();

        if (!pixbuf) {
            std::cerr << "[DEBUG] Pixbuf is null after decoding!" << std::endl;
            img.set(blackImage);
            return;
        }

        std::cout << "[DEBUG] Decoded pixbuf: " << pixbuf->get_width() << "x" << pixbuf->get_height() << std::endl;
        img.set(pixbuf);

    } catch (const Glib::Error& ex) {
        std::cerr << "PixbufLoader error: " << ex.what() << std::endl;
        img.set(blackImage);
    
    }
}

//================================================ FUNCTION: onViewVideo() =======================
// ROLE: ALLOW TO READ THE SELECTED VIDEO
void VueG::onViewVideo(const std::string& videoPath){
    auto viewWindow = new Gtk::Window();
    viewWindow->set_title("Lecture Vidéo");
    viewWindow->set_default_size(800,600);

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





//================================================ FUNCTION: onOpenAction() =======================
// ROLE: SELECT THE VIDEO WE WANT TO READ
void VueG::onOpenAction(){
    auto dialog = Gtk::FileDialog::create(); 
    dialog->set_title("Choisir une vidéo à ouvrir");
    
    auto filter = Gtk::FileFilter::create();
    filter->set_name("FIchiers vidéos");
    filter->add_pattern("*.mp4");
    filter->add_pattern("*.avi");
    filter->add_pattern("*.mkv"); 

    // PUT THE FILTER ( *.MP4, *.AVI) AND *.MKV) IN A CONTAINER
    auto filterList = Gio::ListStore<Gtk::FileFilter>::create();
    filterList ->append(filter);
    dialog->set_filters(filterList);

    dialog->open(*this,[this,dialog](const Glib::RefPtr<Gio::AsyncResult>&result)
    {
        try
        {
            auto file = dialog->open_finish(result);
            
            if(file)
            {
                std::string videoPath = file->get_path();
                std::cout<< "[INFO] IN onOpenAction() (VueG.cpp): video selectec " <<videoPath <<std::endl;
                onViewVideo(videoPath);
            }
        }

        catch(const Glib::Error& ex)
        {
            std::cerr <<"[DEBUG] IN onOpenAction() (VueG.cpp): video not selected or erro :" <<ex.what() <<std::endl;
        }
    
    });
}

//================================================
void VueG::onQuitAction(){
    std::cout << " [Quit function] : Closing the windows ! " <<std::endl;
    close();
};

//================================================
void VueG::onAboutAction(){
    std::cout << " [About function] : About  ! " <<std::endl;

    // creation of the windows "About"
    auto aboutDialog = Gtk::make_managed<Gtk::AboutDialog>();

    //Basics informations 
    aboutDialog->set_program_name("ESP32 Camera Monitor");
    aboutDialog->set_version("1.0.0");
    aboutDialog->set_copyright("© 2026 - Kelly");
    aboutDialog->set_license_type(Gtk::License::GPL_3_0);
    aboutDialog->set_comments(
        "ESP32 Camera Monitor est une application GTK qui permet de faire de faire de la surveillance vidéo\n"
        "et de capturer des images depuis une caméra  sans fils réalisée à partir d'un ESP32 WROVER utilisant le  réseau Wifi et un serveur HTTP.\n\n"
        "Fonctionnalités :\n"
        "  • Affichage en temps réel du flux vidéo\n"
        "  • Capture d'images (JPEG)\n"
        "  • Gestion des dossiers de sauvegarde\n"
        "  • Interface moderne avec GTK4\n"
        "  • Dectetion d'objets et de personnes");

    // Autors
    std::vector<Glib::ustring> authors;
    authors.push_back("Kelly KASSIN kassinkelly87@gmail.com");
    aboutDialog->set_authors(authors);

    // artists
    std::vector<Glib::ustring> artists;
    artists.push_back("Kelly KASSIN (Design & développement)");
    aboutDialog->set_artists(artists);

    //website : Github
    aboutDialog->set_website("https://github.com/kassingit/kassingit.github.io");
    aboutDialog->set_website_label("Github");

    // Display the Dialog box
    aboutDialog->set_transient_for(*this);
    aboutDialog->set_modal(true);
    aboutDialog->show();


};

//================================================
void VueG::onResetConnectionAction(){
    std::cout << " [Reset function] :  Restarting of the connection to:" << client.getNewUrl()  <<std::endl;
    std::string currentUrl = client.getNewUrl();

    std::thread([this,currentUrl](){
        client.reconnect(currentUrl);}
    ).detach();
};


//================================================

void VueG::onPauseClicked(){
    isPaused = !isPaused; // to change the state as soon as we click on PAUSE

    // isPaused is now TRUE
    client.setPaused(isPaused); // client is an element of StreamClient so it can modify paused to tell OnNewFrame to stop the flow
    if(isPaused){
        pauseButton.set_label("▶ Reprendre");
        pauseSymbol.set_visible(true); // we make the label visible
        pauseCircle.set_visible(true);

        std::cout << " [INFO] onPauseClicked :  streaming in pause ! " <<std::endl;
    }
    else{
        pauseButton.set_label("   ⏸ Pause   ");
        pauseSymbol.set_visible(false); // we make the label invisible
        pauseCircle.set_visible(false);
        std::cout << " [INFO] onPauseClicked :  streaming restarted ! " <<std::endl;
        onNewFrame();
    }

};


//=========================addThumbnailToGallery=======================

void VueG::addThumbnailToGallery(const std::string& jpegBytes, const std::string& labelText,const std::string& filePath){
    try {
        auto loader = Gdk::PixbufLoader::create();
        loader->write(reinterpret_cast<const guint8*>(jpegBytes.data()), jpegBytes.size());
        loader->close();
        auto pixbuf = loader->get_pixbuf();

        if (!pixbuf) { std::cerr <<"[GALLERY] Pixbuf null , image ignored." <<std::endl;
        }
        // Redimensionne en miniature (largeur 150px, hauteur proportionnelle)
        int thumbWidth = 270;
        int thumbHeight = (pixbuf->get_height() * thumbWidth) / pixbuf->get_width();
        auto thumbnail = pixbuf->scale_simple(thumbWidth, thumbHeight, Gdk::InterpType::BILINEAR);

        //---------------- Main container (HORIZONTAL) ---------------------
        Gtk::Box* container = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
        container->set_spacing(4);
        container->set_margin(6);
        container->set_halign(Gtk::Align::CENTER);


        //--------------  Thumbnail Image 
        auto thumbImage = Gtk::make_managed<Gtk::Image>(thumbnail);
        thumbImage->set_size_request(thumbWidth, thumbHeight);  // force la taille d'affichage réelle
        container->append(*thumbImage);

        auto infoLabel = Gtk::make_managed<Gtk::Label>();
        infoLabel->set_markup("<span size='small' foreground='#d4cbcb'>" + labelText + "</span>");
        
        //-------------- Image label  ( Date and Hour )
        container->append(*infoLabel);
        //capturesList.prepend(*container); // append() add the image below
        

        //---------- Menu of Thumbnail image
        Gtk::MenuButton* menuButton_captures = Gtk::make_managed<Gtk::MenuButton>();
        menuButton_captures->set_icon_name("view-more-symbolic");
        menuButton_captures->set_halign(Gtk::Align::END);
        menuButton_captures->set_valign(Gtk::Align::START);
        menuButton_captures->set_margin(8);
        menuButton_captures->set_hexpand(false);
        menuButton_captures->set_vexpand(false);

                        //---------- Make the popover with the options
        Gtk::Box* popoverContent = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
        popoverContent->set_spacing(4);
        popoverContent->set_margin(6);



                        //--------- OPTION : AFFICHER 
        Gtk::Button* viewButton = Gtk::make_managed<Gtk::Button>("Afficher");
        viewButton->set_halign(Gtk::Align::FILL);
        viewButton->signal_clicked().connect([this,jpegBytes](){onViewThumbnail(jpegBytes);});

                        // -------- ADD AFFICHER OPTION TO THE POPOVER
        popoverContent->append(*viewButton);

                        //-------- ADD A SEPARATOR BETWEEN THE OPTION 
        popoverContent->append(*Gtk::make_managed<Gtk::Separator>());


                        //--------- OPTION : SUPPRIMER 
        Gtk::Button* deleteButton = Gtk::make_managed<Gtk::Button>("Supprimer");
        deleteButton->set_halign(Gtk::Align::FILL);
        deleteButton->signal_clicked().connect([this,container,filePath](){ onDeleteThumbnail(container,filePath);});

                        // -------- ADD AFFICHER OPTION TO THE POPOVER
        popoverContent->append(*deleteButton);
        

                        //--------- CREATE THE POPOVER AND ATTACH IT
        Gtk::Popover* popover = Gtk::make_managed<Gtk::Popover>();
        popover->set_child(*popoverContent);
        menuButton_captures->set_popover(*popover);
        container->append(*menuButton_captures);
        

        // Ajoute un séparateur seulement s'il y a déjà des captures dans la liste
        if (capturesList.get_first_child() != nullptr) 
        {
            auto separator = Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL);
            capturesList.prepend(*separator);
        }

        capturesList.prepend(*container);

    } catch (const Glib::Error& ex) {
        std::cerr << "[Capture] Erreur lors de la création de la miniature : " << ex.what() << std::endl;
    }

}

void VueG::onDeleteThumbnail(Gtk::Widget *container,const std::string& filePath){
    Gtk::Widget* next = container->get_next_sibling();
    Gtk::Widget* prev = container->get_prev_sibling();

    capturesList.remove(*container);

    // On supprime le séparateur associé, qu'il soit avant ou après
    if (next && dynamic_cast<Gtk::Separator*>(next)) {
        capturesList.remove(*next);
    } else if (prev && dynamic_cast<Gtk::Separator*>(prev)) {
        capturesList.remove(*prev);
    }

    try{
        if(fs::exists(filePath)){
            fs::remove(filePath);
            std::cout <<"[DEBUG] In onDeleteThumbnail  : IMAGE DELETED " <<filePath <<std::endl;
        }
    }
    catch(const fs::filesystem_error& ex){
            std::cerr <<"[DEBUG] In onDeleteThumbnail  =>  ERRO WHILE DELETING IMAGE :" <<ex.what()<<std::endl;
    }
}


void VueG::onViewThumbnail(const std::string& jpegBytes){
    try{
        auto loader = Gdk::PixbufLoader::create();
        loader->write(reinterpret_cast<const guint8*>(jpegBytes.data()),jpegBytes.size());
        loader->close();

        auto pixbuf = loader->get_pixbuf(); // Loader contains the converted bytes into image, and we put this images into pixBuf
        if (!pixbuf){
            std::cerr <<"[DEBUG] onViewThumbnail : IMPOSSIBLE TO DECODE THE IMAGE " <<std::endl;
            return;
            }

        auto viewWindow = new Gtk::Window();
        viewWindow->set_title("IMAGE CAPTUREE");
        viewWindow->set_default_size(pixbuf->get_width(), pixbuf->get_height());

        auto fullImage = Gtk::make_managed<Gtk::Image>(pixbuf);
        viewWindow->set_child(*fullImage);

        viewWindow->set_transient_for(*this);
        viewWindow->set_modal(false);
        viewWindow->show();
    }
    catch (const Glib::Error& ex){
        std::cerr <<"[DEBUG] onViewThumbnail ERROR : "<< ex.what() <<std::endl;
    }
}

//================================================
void VueG::onCaptureClicked(){
    std::string frame = client.getLastFrame();

    if( frame.empty()){
        std::cout <<"[Capture]: No frame available for now !" <<std::endl;
        return;
    }

    //Generates a unique filename based on the current timestamp
    auto now = std::chrono::system_clock::now(); //retrieve the current time
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time = *std::localtime(&now_time); // put the date on the french form : day_month_year

    // --- Use saveDirectory ---
    std::ostringstream filename;
    filename << saveDirectory << "/capture_"  << std::put_time(&local_time, "%d_%m_%Y %H:%M:%S") << ".jpeg";
    std::ofstream outFile(filename.str(),std::ios::binary);
    
    if(!outFile){
        std::cerr <<" [Capture] Impossible to create the file :" <<filename.str() <<std::endl;
        return;
    }

    outFile.write(frame.data(), frame.size());
    outFile.close();

    std::cout <<"[Capture] Image saved :" <<filename.str()<<std::endl;

    std::ostringstream labelText;
    labelText << "Date :" <<std::put_time(&local_time, "%d/%m/%Y") << "\n";
    labelText << "Heure :" <<std::put_time(&local_time,"%H:%M:%S");


    //--------- miniaturisation of the images for "Captures" 
    addThumbnailToGallery(frame,labelText.str(), filename.str());
   
};

//------------- LOADEXISTINGCAPTURES ------------------
void VueG::loadExistingCaptures(){
    //Check if the directory is good and exist
    if (!fs::exists(saveDirectory) || !fs::is_directory(saveDirectory)){
    return ;
    }

    // we retrieve the path before sorting them by date
    std::vector<fs::path> jpgFiles;
    for (const auto& entry: fs::directory_iterator(saveDirectory)){
        if (entry.is_regular_file() && entry.path().extension() == ".jpg"){
        jpgFiles.push_back(entry.path());
        }
    }

    // sorting by modification date ( starting by the most old , so at the end of the sorting them will be down)
    std::sort(jpgFiles.begin(), jpgFiles.end(), [] (const fs::path& a, const fs::path& b){ 
        return fs::last_write_time(a) < fs::last_write_time(b);
    });

    for( const auto& path: jpgFiles){
        std::ifstream inFile(path, std::ios::binary);
        if (!inFile)continue;

        std::ostringstream buffer;
        buffer << inFile.rdbuf();

        std::string jpegBytes = buffer.str();

        // Modification of the date converted into readable date
        auto ftime = fs::last_write_time(path);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime -fs::file_time_type::clock::now() + std::chrono::system_clock::now());

        std::time_t cftime  = std::chrono::system_clock::to_time_t(sctp);
        std::tm local_time = *std::localtime(&cftime);

        std::ostringstream labelText;
        labelText <<"Date :"<< std::put_time(&local_time, "%d/%m/%Y") <<"\n";
        labelText <<"Heure :" <<std::put_time(&local_time, "%H:%M:%S");

        addThumbnailToGallery(jpegBytes, labelText.str(), path.string());
        }

    std::cout <<"[GALLERY] :" << jpgFiles.size() << " images reloaded from " << saveDirectory <<std::endl;

}


//===============================================

std::string VueG::getConfigFilesPath(){

    // Selecting Directory for the configuration file
    std::string ConfigDir = Glib::get_home_dir() + "/.config/esp32_monitor/";

    //If the Directory for the configuration file doesn't exists , we create it
    if(!fs::exists(ConfigDir)){
        fs::create_directories(ConfigDir);
    }

    return ConfigDir + "save_directory.conf";

}

// Save the directory chosen by the user to store recordings and captures
void VueG::saveConfig(){
    std::string ConfigPath = getConfigFilesPath();
    std::ofstream ConfigFile(ConfigPath);

    if(ConfigFile.is_open()){
        ConfigFile << saveDirectory <<std::endl;
        ConfigFile.close();
        std::cout <<"[CONFIG] PATH SAVED :" << saveDirectory << std::endl;
    }
    else{
        std::cerr << "[CONFIG] IMPOSSIBLE TO SAVE THE FILE "<< ConfigPath << std::endl;}

}


// Load the user path directory to restore the app and captured images
void VueG::loadConfig(){
    std::string configPath = getConfigFilesPath(); // take the path of the configuration path
    std::ifstream configFile(configPath); // export the content which is the configuration file ( it means export the configuration file as configFile)

    if( configFile.is_open()){ // verify if the configuration file is well opened
        std::string savedPath;  //  variable to store the saved path chosen by the user
        std::getline(configFile, savedPath); // write the configFile into savedFile
        configFile.close();

        if(!savedPath.empty() && fs::exists(savedPath) && fs::is_directory(savedPath)){
            saveDirectory = savedPath;
            std::cout <<"[CONFIG] PATH LOADED WITH SUCCESS !" <<saveDirectory <<std::endl;
            return;
        }
    }

    // if the file or the directory don't exist, use the current directory
    saveDirectory = Glib::get_home_dir() + "/Personal_Projects/esp32_monitor_gtk/Pictures";
    if(!fs::exists(saveDirectory)){
        fs::create_directory(saveDirectory);
        std::cout <<"[CONFIG] PATH NOT FILE. PATH USED :" << saveDirectory <<std::endl;
    }
}
//================================================

void VueG::onChooseFolderClicked(){
    auto dialog = Gtk::FileDialog::create();

    dialog->set_title("Choisir le dossier de sauvegarde ");
    dialog->select_folder(*this, [this, dialog](const Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto folder = dialog->select_folder_finish(result);
            if (folder) {
                saveDirectory = folder->get_path();
                std::cout << "[Dossier] Nouveau dossier de sauvegarde : " << saveDirectory << std::endl;
            }
            saveConfig();
        } catch (const Glib::Error& ex) {
            std::cerr << "[Dossier] Sélection annulée ou erreur : " << ex.what() << std::endl;
        }
    });

}

//=================================
bool VueG::onRecordTick(){
    std::string frame = client.getLastFrame();

    if(!frame.empty()){
        recorder.pushFrame(frame);
    }
    return true; // true allow to continue calling this function
}

void VueG::onRecordClicked(){
    if(!isRecording){
        // Start recording
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm local_time = *std::localtime(&now_time);

        std::ostringstream filename;
        filename <<saveDirectory <<"/video_" 
                 <<std::put_time(&local_time,"%d_%m_%Y_%H:%m:%S") <<".mp4";

        recorder.start(filename.str());
        isRecording = true;
        recordButton.set_label("⏹ Arrêter");

        // Send frame avery 100ms
        recordTimer = Glib::signal_timeout().connect(sigc::mem_fun(*this,&VueG::onRecordTick),100);
    }else{
        //Stop the recording
        recordTimer.disconnect(); //stop the timer
        std::thread([this](){recorder.stop();}).detach();

        isRecording = false;
        recordButton.set_label("🎥 Enregistrer video ");
    }
};
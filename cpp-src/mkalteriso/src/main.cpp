#include "main.hpp"
build_option bp;
int main(int argc,char* argv[]){
    uid_t euid;
    euid=geteuid();
    if(euid != 0){
        _msg_error("You're not root!");
        _msg_error("Please run this program on root!");
        return -810;
    }
    umask(S_IWGRP | S_IWOTH );
    cmdline::parser p;
    p.add<String>("application",'A',"Set an application name for the ISO",false,bp.app_name);
    p.add<String>("pacman_config",'C',"pacman configuration file.",false,bp.pacman_conf);
    p.add("help", 'h', "This message");
    p.add<String>("install_dir",'D',"Set an install_dir. All files will by located here.\n\t\t\t NOTE: Max 8 characters, use only [a-z0-9]",
    false,bp.install_dir);
    p.add<String>("iso_label",'L',"Set the ISO volume label",false,bp.iso_label);
    p.add<String>("iso_pub",'P',"Set the ISO publisher",false,bp.iso_publisher);
    p.add<String>("gpg-key",'g',"Set the GPG key to be used for signing the sqashfs image",false,"");
    p.add<String>("out_dir",'O',"Set the output directory",false,bp.out_dir);
    p.add<String>("packages",'p',"Package(s) to install, can be used multiple times",false,bp.aditional_packages);
    p.add("verbose",'v',"Enable verbose output");
    p.add("nodepends",0,"No depends check");
    p.add<String>("work",'w',"Set the working directory",false,bp.work_dir);
    p.add<String>("lang",'l',"Set Language",false,"en");
    p.add<String>("run_cmd",'r',"run command");
    if (!p.parse(argc, argv)||p.exist("help")){
        std::cout<<p.error_full()<<p.usage();
        return 0;
    }
    bp.app_name=p.get<String>("application");
    bp.pacman_conf=p.get<String>("pacman_config");
    bp.install_dir=p.get<String>("install_dir");
    bp.iso_label=p.get<String>("iso_label");
    bp.iso_publisher=p.get<String>("iso_pub");
    bp.gpg_key=p.get<String>("gpg-key");
    bp.out_dir=p.get<String>("out_dir");
    bp.aditional_packages=p.get<String>("packages");
    bp.work_dir=p.get<String>("work");
    bp.run_cmd=p.get<String>("run_cmd");
    String language_id=p.get<String>("lang");
    time_t timer;
    struct tm *date;
    char str[256];
    timer=time(NULL);
    bp.SOURCE_DATE_EPOCH=timer;
    
    Vector<String> cmd_ls=p.rest();
    if(cmd_ls.size()==0){
        _msg_error("No profile specified");
        return 0;
    }
    bp.profile=realpath("./channels/" + cmd_ls.at(0));
    bp.profile_name=cmd_ls.at(0);
    if(cmd_ls.at(0) == "releng"){
        bp.isreleng=true;
    }
    bp.airootfs_dir=bp.work_dir + "/airootfs";
    bp.isofs_dir=bp.work_dir+"/iso";
    bp.airootfs_image_tool_options.push_back("-comp");
    bp.airootfs_image_tool_options.push_back("xz");
    if(!p.exist("nodepends")){
        check_depends_class check_d;
        if(!check_d.check_depends()){
            return 810;
        }
    }

    parse_channel();
    set_lang(language_id);
    setup(bp);
    _build_profile();
    return 0;
}
lang_info gen_lang_list(String lang_name,String locale_gen,String timezonekun,String long_name){
    lang_info langi;
    langi.locale_gen=locale_gen;
    langi.name=lang_name;
    langi.timezone=timezonekun;
    langi.fullname=long_name;
    return langi;
}
void set_lang(String lang_id){
    Vector<lang_info> lang_lists;
    lang_lists.push_back(gen_lang_list("gl","en_US.UTF-8","UTC","global"));
    lang_lists.push_back(gen_lang_list("ja","ja_JP.UTF-8","Asia/Tokyo","japanese"));
    for(lang_info lkun:lang_lists){
        if(lkun.name == lang_id){
            bp.lang=lkun;
        }
    }
}
void parse_channel(){
    Vector<String> argskun;
    argskun.push_back(realpath("./archiso/get_profile_def.sh"));
    argskun.push_back("-s");
    argskun.push_back(realpath("./archiso/json_template.json"));
    argskun.push_back("-p");
    argskun.push_back(realpath(bp.profile + "/profiledef.sh"));
    argskun.push_back("-o");
    argskun.push_back(realpath("./.cache_channel_json.json"));
    int get_profile_result = FascodeUtil::custom_exec_v(argskun);
    if(get_profile_result != 0){
        _msg_error("NOT FOUND CHANNEL!");
        _exit(819);
        return;
    }
    std::ifstream json_stream(realpath("./.cache_channel_json.json"));
    remove(realpath("./.cache_channel_json.json").c_str());
    String json_data=String(std::istreambuf_iterator<char>(json_stream),
                            std::istreambuf_iterator<char>());
    nlohmann::json json_obj;
    try{
        json_obj=nlohmann::json::parse(json_data);
    }catch(nlohmann::json::parse_error msg){
        _msg_error(String(msg.what()));
        _exit(810);
        return;
    }
    bp.iso_name=json_obj["iso_name"].get<String>();
    bp.iso_label=json_obj["iso_label"].get<String>();
    bp.iso_publisher=json_obj["iso_publisher"].get<String>();
    bp.iso_application=json_obj["iso_application"].get<String>();
    bp.iso_version=json_obj["iso_version"].get<String>();
    bp.install_dir=json_obj["install_dir"].get<String>();
    bp.bootmodes=json_obj["bootmodes"].get<Vector<String>>();
    bp.arch=json_obj["arch"].get<String>();
    bp.img_name=bp.app_name + ".iso";
    char pathname[512];
    memset(pathname, '\0', 512); 
    getcwd(pathname,512);
    chdir(realpath(bp.profile).c_str());
    bp.packages_vector=parse_packages_folder("./packages." + bp.arch);
    if(dir_exist("./packages_aur." + bp.arch)) bp.aur_packages_vector=parse_packages_folder("./packages_aur." + bp.arch);
    
    bp.pacman_conf=realpath(json_obj["pacman_conf"].get<String>());
    chdir("../");
    if(!bp.isreleng){
        bp.packages_vector=parse_packages_folder(bp.packages_vector,"./share/packages." + bp.arch);
        bp.aur_packages_vector=parse_packages_folder(bp.aur_packages_vector,"./share/packages_aur." + bp.arch);
    }
    chdir(pathname);

}
Vector<String> parse_packages(String packages_file_path){
    Vector<String> return_collection;
    std::ifstream package_file_stream(packages_file_path);
    String line_buf;
    while(getline(package_file_stream,line_buf)){
        String replaced_space=std::regex_replace(line_buf,std::regex(" "),"");
        if(replaced_space.substr(0,1) != "#"){
            if(replaced_space != ""){
                return_collection.push_back(replaced_space);
            }
        }
    }
    return return_collection;
}
Vector<String> parse_packages(Vector<String> base_vector,String packages_file_path){
    std::ifstream package_file_stream(packages_file_path);
    String line_buf;
    while(getline(package_file_stream,line_buf)){
        String replaced_space=std::regex_replace(line_buf,std::regex(" "),"");
        if(replaced_space.substr(0,1) != "#"){
            if(replaced_space != ""){
                base_vector.push_back(replaced_space);
            }
        }
    }
    return base_vector;
}
bool ends_with(const std::string& str, const std::string& suffix) {
    size_t len1 = str.size();
    size_t len2 = suffix.size();
    return len1 >= len2 && str.compare(len1 - len2, len2, suffix) == 0;
}

Vector<String> parse_packages_folder(String packages_folder_path){
    Vector<String> return_object;
    std::filesystem::directory_iterator iter(packages_folder_path),end;
    std::error_code err;
    for(;iter != end && !err;iter.increment(err)){
        const std::filesystem::directory_entry entry=*iter;
        String fnamekun=entry.path().string();
        if(ends_with(fnamekun,bp.arch)){
            return_object=parse_packages(return_object,fnamekun);
        }
    }
    if(err){
        _msg_error(err.message());
    }
    return return_object;
}
Vector<String> parse_packages_folder(Vector<String> base_vect,String packages_folder_path){
    std::filesystem::directory_iterator iter(packages_folder_path),end;
    std::error_code err;
    for(;iter != end && !err;iter.increment(err)){
        const std::filesystem::directory_entry entry=*iter;
        String fnamekun=entry.path().string();
        if(ends_with(fnamekun,bp.arch)){
            base_vect=parse_packages(base_vect,fnamekun);
        }
    }
    if(err){
        _msg_error(err.message());
    }
    return base_vect;
}
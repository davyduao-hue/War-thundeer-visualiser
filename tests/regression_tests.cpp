#include "telemetry.hpp"
#include <chrono>
#include <iostream>
#include <sstream>
int main(){int n=0;auto check=[&](bool v){if(!v)throw std::runtime_error("Regression "+std::to_string(n+1));++n;};auto fails=[](auto fn){try{fn();return false;}catch(...){return true;}};
 auto dir=std::filesystem::temp_directory_path()/("wt-regression-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 try{std::filesystem::create_directory(dir);auto path=dir/"settings.json";
 auto write=[&](const std::string&s){std::ofstream f(path);f<<s;};
 wt::Settings settings;settings.fps=45;settings.overlay=false;
 write(R"({"overlay":true,"fps":"broken"})");check(fails([&]{settings.load(path);}));check(settings.fps==45&&!settings.overlay);
 write("[]");check(fails([&]{settings.load(path);}));check(settings.fps==45);
 write(std::string(66000,' '));check(fails([&]{settings.load(path);}));
 std::string name(62,'a');name+="\xe2\x82\xac";write(wt::json{{"name",name}}.dump());settings.load(path);check(std::string(settings.name)==std::string(62,'a'));
 settings.save(path);wt::Settings loaded;loaded.load(path);check(std::string(loaded.name)==std::string(settings.name));
 settings.fps=60;settings.save(path);loaded.load(path);check(loaded.fps==60);check(!std::filesystem::exists(path.string()+".tmp"));
 auto blocked=dir/"directory";std::filesystem::create_directory(blocked);{std::ofstream f(blocked/"keep");f<<"preserve";}
 check(fails([&]{settings.save(blocked);}));check(std::filesystem::exists(blocked/"keep"));check(!std::filesystem::exists(blocked.string()+".tmp"));
 std::string unicode="\xf0\x9f\x9a\x80";
 check(wt::utf8Prefix(unicode,3).empty());check(wt::utf8Prefix(unicode,4)==unicode);check(wt::utf8Prefix("abc",0).empty());
 auto e=wt::parseEndpoint(wt::json{{std::string(79,'k')+unicode,std::string(159,'v')+unicode}}.dump());
 check(e.fields[0].first.size()==79);check(e.fields[0].second.size()==159);check(!fails([&]{auto out=wt::json(e.fields).dump();}));
 wt::Shell shell{"test","AP",{{-.5,100},{100,90}},0,70,true};wt::Plate plate{"test",50,0,false,true};check(wt::analyze(shell,plate,0).classification==wt::Class::Uncertain);
 wt::Database db;db.version="preserved";check(fails([&]{db.loadJson(wt::json{{"schema",1},{"version","new"},{"plates",wt::json::array()},{"shells",wt::json::array()}});}));check(db.version=="preserved");
 settings.manualPenetration=381;std::snprintf(settings.manualShell,sizeof settings.manualShell,"Shot Mk 3");settings.save(path);loaded.load(path);check(loaded.manualPenetration==381);check(std::string(loaded.manualShell)=="Shot Mk 3");write(R"({"manual_penetration":-1})");check(fails([&]{loaded.load(path);}));check(loaded.manualPenetration==381);
 std::filesystem::remove_all(dir);std::cout<<n<<" regression checks passed\n";return 0;
 }catch(const std::exception&e){std::filesystem::remove_all(dir);std::cerr<<e.what()<<'\n';return 1;}}

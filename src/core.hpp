#pragma once
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
namespace wt {
using json=nlohmann::json;
struct Sample { double distance,penetration; };
struct Shell { std::string name,type; std::vector<Sample> table; double normalization=0,ricochet=70; bool verified=false; };
struct Plate { std::string name; double thickness=0,angle=0; bool complex=false,verified=false; };
enum class Class { Unlikely, Likely, Uncertain };
struct Result { Class classification=Class::Uncertain; double penetration=0,los=0; std::string reason; };
inline Result analyze(const Shell& s,const Plate& p,double distance) {
 Result r;
 if(!std::isfinite(distance)||distance<0||!std::isfinite(p.thickness)||p.thickness<=0||!std::isfinite(p.angle)||p.angle<0||p.angle>=90||s.table.empty()) {r.reason="Invalid or missing inputs";return r;}
 if(s.type!="AP"&&s.type!="APHE"&&s.type!="APFSDS"&&s.type!="HEAT") {r.reason="HE blast and unsupported shell models require additional data";return r;}
 double last=-1;
 for(auto a:s.table) {if(!std::isfinite(a.distance)||!std::isfinite(a.penetration)||a.distance<=last||a.penetration<=0){r.reason="Invalid penetration table";return r;}last=a.distance;}
 if(!std::isfinite(s.normalization)||s.normalization<0||s.normalization>20||!std::isfinite(s.ricochet)||s.ricochet<=0||s.ricochet>90){r.reason="Invalid angle model";return r;}
 if(distance<s.table.front().distance||distance>s.table.back().distance){r.reason="Distance outside documented table";return r;}
 r.penetration=s.table.back().penetration;
 for(size_t i=1;i<s.table.size();++i) if(distance<=s.table[i].distance){auto a=s.table[i-1],b=s.table[i]; r.penetration=a.penetration+(b.penetration-a.penetration)*(distance-a.distance)/(b.distance-a.distance);break;}
 // Only conventional kinetic shells use this simplified normalization model.
 double angle=std::max(0.0,p.angle-((s.type=="AP"||s.type=="APHE")?s.normalization:0));
 r.los=p.thickness/std::cos(angle*3.141592653589793/180);
 if(!s.verified||!p.verified){r.reason="SIMULATED / unverified profile; no gameplay prediction";return r;}
 if(p.complex){r.reason="Spaced/composite/reactive armour requires a validated material model";return r;}
 if(p.angle>=s.ricochet){r.reason="Ricochet region; probability unavailable";return r;}
 // Long rod performance needs obliquity data, not an AP normalization approximation.
 if(s.type=="APFSDS"&&p.angle>0){r.reason="Long-rod obliquity model unavailable";return r;}
 const double margin=r.penetration/r.los;
 r.classification=margin>1.15?Class::Likely:margin<0.85?Class::Unlikely:Class::Uncertain;
 r.reason="Simplified homogeneous plate estimate; uncertainty band +/-15%; damage unknown";
 return r;
}
struct Database {
 std::string version; std::vector<Plate> plates;std::vector<Shell> shells;
 void load(const std::filesystem::path& path){
  std::ifstream f(path);if(!f)throw std::runtime_error("Database not found");json j;f>>j;
  if(j.at("schema")!=1)throw std::runtime_error("Unsupported database schema");
  Database next;next.version=j.at("version");
  for(auto& v:j.at("plates")){next.plates.push_back({v.at("name"),v.at("thickness"),v.at("angle"),v.value("complex",false),false});}
  for(auto& v:j.at("shells")){Shell s;s.name=v.at("name");s.type=v.at("type");s.normalization=v.value("normalization",0.0);s.ricochet=v.value("ricochet",70.0);for(auto& t:v.at("table"))s.table.push_back({t.at(0),t.at(1)});next.shells.push_back(s);}
  if(next.plates.empty()||next.shells.empty())throw std::runtime_error("Empty database");
  // Imported records remain unverified until provenance and models are independently reviewed.
  *this=std::move(next);
 }
};
struct Settings {
 bool overlay=false,demo=true;int preset=1,fps=30;float opacity=.85f,scale=1;char name[64]="Commander";
 void load(const std::filesystem::path& p){std::ifstream f(p);if(!f)return;json j;f>>j;overlay=j.value("overlay",false);demo=j.value("demo",true);preset=std::clamp(j.value("preset",1),0,2);fps=std::clamp(j.value("fps",30),10,60);opacity=std::clamp(j.value("opacity",.85f),.1f,1.f);scale=std::clamp(j.value("scale",1.f),.5f,2.f);auto n=j.value("name",std::string("Commander"));std::snprintf(name,sizeof name,"%s",n.c_str());}
 void save(const std::filesystem::path& p)const{auto temp=p;temp+=".tmp";{std::ofstream f(temp);if(!f)throw std::runtime_error("Cannot save settings");f<<json{{"overlay",overlay},{"demo",demo},{"preset",preset},{"fps",fps},{"opacity",opacity},{"scale",scale},{"name",name}}.dump(2);f.flush();if(!f)throw std::runtime_error("Settings write failed");}
#ifdef _WIN32
 // Replacement handled by Windows application to preserve atomicity.
#endif
 std::error_code ec;std::filesystem::rename(temp,p,ec);if(ec){std::filesystem::copy_file(temp,p,std::filesystem::copy_options::overwrite_existing);std::filesystem::remove(temp);}}
};
}

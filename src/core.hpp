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
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace wt {
using json=nlohmann::json;
// Input is validated UTF-8 from JSON or ImGui. Never split a multibyte character.
inline std::string utf8Prefix(const std::string& text,size_t bytes){
 size_t end=std::min(bytes,text.size());
 if(end<text.size())while(end>0&&(static_cast<unsigned char>(text[end])&0xc0)==0x80)--end;
 return text.substr(0,end);
}
inline json readJson(std::istream& stream,size_t limit){
 std::string text;char chunk[4096];
 while(stream){stream.read(chunk,sizeof chunk);text.append(chunk,static_cast<size_t>(stream.gcount()));if(text.size()>limit)throw std::runtime_error("JSON file exceeds size limit");}
 if(stream.bad())throw std::runtime_error("JSON file read failed");
 return json::parse(text);
}

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
 for(auto a:s.table) {if(!std::isfinite(a.distance)||!std::isfinite(a.penetration)||a.distance<0||a.distance<=last||a.penetration<=0){r.reason="Invalid penetration table";return r;}last=a.distance;}
 if(!std::isfinite(s.normalization)||s.normalization<0||s.normalization>20||!std::isfinite(s.ricochet)||s.ricochet<=0||s.ricochet>90){r.reason="Invalid angle model";return r;}
 if(distance<s.table.front().distance||distance>s.table.back().distance){r.reason="Distance outside documented table";return r;}
 r.penetration=s.table.back().penetration;
 for(size_t i=1;i<s.table.size();++i) if(distance<=s.table[i].distance){auto a=s.table[i-1],b=s.table[i]; r.penetration=a.penetration+(b.penetration-a.penetration)*(distance-a.distance)/(b.distance-a.distance);break;}
 // Only conventional kinetic shells use this simplified normalization model.
 double angle=std::max(0.0,p.angle-((s.type=="AP"||s.type=="APHE")?s.normalization:0));
 r.los=p.thickness/std::cos(angle*3.141592653589793/180);
 if(!std::isfinite(r.penetration)||!std::isfinite(r.los)){r.penetration=0;r.los=0;r.reason="Calculation outside numeric range";return r;}
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
  std::ifstream f(path);if(!f)throw std::runtime_error("Database not found");loadJson(readJson(f,8*1024*1024));
 }
 void loadJson(const json& j){
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
 char manualShell[96]="Shot Mk 3";char manualShellType[24]="APDS";float manualPenetration=381;
 bool overlay=false,demo=true;int preset=1,fps=30;float opacity=.85f,scale=1;char name[64]="Commander";
 void load(const std::filesystem::path& p){
  std::ifstream f(p);if(!f){if(std::filesystem::exists(p))throw std::runtime_error("Cannot read settings");return;}
  auto j=readJson(f,65536);if(!j.is_object())throw std::runtime_error("Settings must be a JSON object");
  Settings next;
  auto shell=utf8Prefix(j.value("manual_shell",std::string("Shot Mk 3")),95);auto type=utf8Prefix(j.value("manual_shell_type",std::string("APDS")),23);
  std::snprintf(next.manualShell,sizeof next.manualShell,"%s",shell.c_str());std::snprintf(next.manualShellType,sizeof next.manualShellType,"%s",type.c_str());
  next.manualPenetration=j.value("manual_penetration",381.f);if(!std::isfinite(next.manualPenetration)||next.manualPenetration<0||next.manualPenetration>5000)throw std::runtime_error("Invalid manual penetration");
  next.overlay=j.value("overlay",false);next.demo=j.value("demo",true);
  next.preset=std::clamp(j.value("preset",1),0,2);next.fps=std::clamp(j.value("fps",30),10,60);
  next.opacity=std::clamp(j.value("opacity",.85f),.1f,1.f);next.scale=std::clamp(j.value("scale",1.f),.5f,2.f);
  auto n=utf8Prefix(j.value("name",std::string("Commander")),sizeof next.name-1);
  if(n.find('\0')!=std::string::npos)throw std::runtime_error("Display name contains a null character");
  std::snprintf(next.name,sizeof next.name,"%s",n.c_str());*this=next;
 }
 void save(const std::filesystem::path& p)const{
  // Serialize before touching disk so invalid input cannot damage a saved file.
  auto content=json{{"manual_shell",manualShell},{"manual_shell_type",manualShellType},{"manual_penetration",manualPenetration},{"overlay",overlay},{"demo",demo},{"preset",preset},{"fps",fps},{"opacity",opacity},{"scale",scale},{"name",name}}.dump(2);
  auto temp=p;temp+=".tmp";
  try{
   {std::ofstream f(temp,std::ios::binary|std::ios::trunc);if(!f)throw std::runtime_error("Cannot save settings");f<<content;f.close();if(!f)throw std::runtime_error("Settings write failed");}
#ifdef _WIN32
   if(!MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot replace settings (Windows error "+std::to_string(GetLastError())+")");
#else
   std::filesystem::rename(temp,p);
#endif
  }catch(...){std::error_code ignored;std::filesystem::remove(temp,ignored);throw;}
 }
};
}

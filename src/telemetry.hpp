#pragma once
#include "core.hpp"
namespace wt {
// Parsing is independent of HTTP and game-window discovery. Missing endpoints must
// not erase useful data from another endpoint. No fields imply shell identification.
struct Endpoint {
 bool received=false;
 std::optional<bool> valid;
 std::string status="Not polled";
 std::vector<std::pair<std::string,std::string>> fields;
};
inline Endpoint parseEndpoint(const std::string& body) {
 Endpoint e;
 try {
  auto j=json::parse(body);
  if(!j.is_object()){e.status="Expected a JSON object";return e;}
  e.received=true;
  if(j.contains("valid")){
   if(!j["valid"].is_boolean()){e.status="Invalid validity flag";return e;}
   e.valid=j["valid"].get<bool>();
  }
  e.status=e.valid ? (*e.valid?"Valid telemetry":"No active telemetry (valid=false)") : "JSON received; validity unspecified";
  if(e.valid && !*e.valid)return e;
  for(auto it=j.begin();it!=j.end()&&e.fields.size()<12;++it){
   if(it.key()=="valid")continue;
   if(it.value().is_number()||it.value().is_boolean()||it.value().is_string()){
    auto value=it.value().is_string()?it.value().get<std::string>():it.value().dump();
    // Bounds keep UI/report size predictable; labels and units stay as provided.
    e.fields.emplace_back(utf8Prefix(it.key(),80),utf8Prefix(value,160));
   }
  }
 }catch(const json::exception&){e.status="Malformed JSON";}
 return e;
}
struct TelemetrySnapshot {
 Endpoint state,indicators,map; bool simulated=false;
 bool connected()const{return state.received||indicators.received||map.received;}
 std::string summary()const{
  if(simulated)return "SIMULATED telemetry";
  if(!connected())return "Telemetry disconnected: "+state.status;
  return "State: "+state.status+" | Indicators: "+indicators.status;
 }
};
}

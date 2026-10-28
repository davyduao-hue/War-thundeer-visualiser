#pragma once
#include "core.hpp"
#include <chrono>
#include <mutex>
#include <cstdint>
namespace wt {
// Parsing is independent of HTTP and game-window discovery. Missing endpoints must
// not erase useful data from another endpoint. No fields imply shell identification.
struct Endpoint {
 std::chrono::steady_clock::time_point sampledAt=std::chrono::steady_clock::now();
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

// A generation identifies each mode change, including live -> demo -> live.
// Old worker results can never replace data from a newer request.
class TelemetryChannel {
 mutable std::mutex mutex;
 TelemetrySnapshot snapshot;
 bool mode=true;
 std::uint64_t generation=1;
public:
 struct Request {bool simulated;std::uint64_t generation;};
 TelemetryChannel(){snapshot.simulated=true;}
 Request request()const{std::lock_guard lock(mutex);return {mode,generation};}
 void setMode(bool value){std::lock_guard lock(mutex);if(mode==value)return;mode=value;++generation;snapshot={};snapshot.simulated=value;}
 bool publish(Request request,TelemetrySnapshot value){std::lock_guard lock(mutex);if(request.generation!=generation)return false;value.simulated=mode;snapshot=std::move(value);return true;}
 TelemetrySnapshot read(std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now())const{
  std::lock_guard lock(mutex);auto result=snapshot;
  for(auto endpoint:{&result.state,&result.indicators,&result.map})if(endpoint->received&&now-endpoint->sampledAt>std::chrono::seconds(4)){endpoint->received=false;endpoint->valid.reset();endpoint->fields.clear();endpoint->status="Stale data - waiting for update";}
  return result;
 }
};
}

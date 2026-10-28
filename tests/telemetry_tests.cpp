#include "telemetry.hpp"
#include <iostream>
int main(){int n=0;auto check=[&](bool v){if(!v)throw std::runtime_error("check "+std::to_string(n+1));++n;};try{
 auto e=wt::parseEndpoint(R"({"valid":true,"speed, km/h":42,"RPM":1800})");check(e.received&&e.valid==true);check(e.fields.size()==2);check(e.fields[1].first=="speed, km/h");check(e.fields[1].second=="42");
 e=wt::parseEndpoint(R"({"valid":false,"speed":999})");check(e.received&&e.valid==false);check(e.fields.empty());
 e=wt::parseEndpoint(R"({"valid":"true","speed":99})");check(!e.valid&&e.fields.empty());
 e=wt::parseEndpoint("<html>error</html>");check(!e.received);check(!wt::parseEndpoint("[]").received);
 e=wt::parseEndpoint(R"({"type":"test-fixture","speed":1.5})");check(e.received&&!e.valid);check(e.fields.size()==2);
 wt::TelemetrySnapshot s;s.state.status="HTTP 404";s.indicators=e;check(s.connected());check(s.summary().find("HTTP 404")!=std::string::npos);
 s.indicators={};check(!s.connected());s.map=wt::parseEndpoint(R"({"map_min":[0,0]})");check(s.connected());check(s.map.fields.empty());
 s.simulated=true;check(s.summary().find("SIMULATED")!=std::string::npos);
 wt::json large;for(int i=0;i<100;++i)large[std::to_string(i)]=std::string(1000,'x');e=wt::parseEndpoint(large.dump());check(e.fields.size()==12);check(e.fields[0].second.size()==160);
 wt::TelemetryChannel channel;auto demoRequest=channel.request();check(demoRequest.simulated);
 wt::TelemetrySnapshot sample;sample.state=wt::parseEndpoint(R"({"valid":true,"speed":42})");check(channel.publish(demoRequest,sample));check(channel.read().connected());
 channel.setMode(false);check(!channel.read().connected());check(!channel.publish(demoRequest,sample));auto liveRequest=channel.request();
 channel.setMode(true);channel.setMode(false);check(!channel.publish(liveRequest,sample));liveRequest=channel.request();check(channel.publish(liveRequest,sample));check(!channel.read().simulated);
 auto stale=channel.read(sample.state.sampledAt+std::chrono::seconds(5));check(!stale.connected());check(stale.state.fields.empty());check(!stale.state.valid);check(channel.read(sample.state.sampledAt+std::chrono::seconds(3)).connected());
 std::cout<<n<<" telemetry checks passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

#include "core.hpp"
#include <iostream>
int main(int argc,char**argv){int count=0;auto check=[&](bool ok){++count;if(!ok)throw std::runtime_error("Test failed: "+std::to_string(count));};try{
 wt::Shell s{"test","AP",{{0,120},{1000,80}},0,70,true};wt::Plate p{"test",80,0,false,true};
 check(wt::analyze(s,p,0).classification==wt::Class::Likely);
 check(wt::analyze(s,p,1000).classification==wt::Class::Uncertain);
 check(wt::analyze(s,p,500).penetration==100);
 p.thickness=200;check(wt::analyze(s,p,0).classification==wt::Class::Unlikely);
 p.thickness=80;p.angle=60;check(std::abs(wt::analyze(s,p,0).los-160)<.001);
 p.complex=true;check(wt::analyze(s,p,0).classification==wt::Class::Uncertain);
 p.complex=false;p.angle=90;check(wt::analyze(s,p,0).los==0);
 p.angle=0;check(wt::analyze(s,p,-1).classification==wt::Class::Uncertain);
 check(wt::analyze(s,p,1001).classification==wt::Class::Uncertain);
 s.type="HE";check(wt::analyze(s,p,0).classification==wt::Class::Uncertain);
 s.type="APFSDS";p.angle=40;check(wt::analyze(s,p,0).classification==wt::Class::Uncertain);
 s.type="HEAT";s.normalization=5;check(std::abs(wt::analyze(s,p,0).los-80/std::cos(40*3.141592653589793/180))<.001);
 s.verified=false;check(wt::analyze(s,p,0).classification==wt::Class::Uncertain);
 wt::Database db;if(argc!=2)throw std::runtime_error("database argument required");db.load(argv[1]);check(db.shells.size()==5);check(!db.shells[0].verified);
 auto path=std::filesystem::temp_directory_path()/"wt-settings-test.json";wt::Settings a;a.fps=45;a.save(path);wt::Settings b;b.load(path);check(b.fps==45);a.fps=20;a.save(path);b.load(path);check(b.fps==20);std::filesystem::remove(path);
 std::cout<<count<<" checks passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

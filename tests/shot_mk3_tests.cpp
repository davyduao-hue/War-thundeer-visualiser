#include "shot_mk3.hpp"
#include <iostream>
int main(){int n=0;auto check=[&](bool v){if(!v)throw std::runtime_error("Shot Mk.3 check "+std::to_string(n+1));++n;};try{
 for(size_t r=0;r<6;++r)for(size_t c=0;c<3;++c)check(wt::shotMk3Penetration(wt::shotDistances[r],wt::shotAngles[c])==wt::shotPenetration[r][c]);
 check(std::abs(*wt::shotMk3Penetration(300,0)-372.5)<1e-6);check(std::abs(*wt::shotMk3Penetration(10,15)-348)<1e-6);
 check(!wt::shotMk3Penetration(0,0));check(!wt::shotMk3Penetration(2001,0));check(!wt::shotMk3Penetration(10,61));check(!wt::shotMk3Penetration(10,-1));
 check(wt::analyzeShotMk3(70,10,60).classification==wt::Class::Likely); // would fail if angle applied twice
 check(wt::analyzeShotMk3(200,10,60).classification==wt::Class::Unlikely);check(wt::analyzeShotMk3(108,10,60).classification==wt::Class::Uncertain);
 check(wt::analyzeShotMk3(70,10,60,true).classification==wt::Class::Uncertain);check(wt::analyzeShotMk3(0,10,0).classification==wt::Class::Uncertain);
 std::cout<<n<<" Shot Mk.3 checks passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

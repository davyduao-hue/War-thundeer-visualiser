#pragma once
#include "core.hpp"
#include <array>
namespace wt {
// Transcribed from the stat-card screenshot supplied by the user. Values are
// penetrable nominal plate thickness at the listed angle, not LOS thickness.
inline constexpr std::array<double,6> shotDistances{10,100,500,1000,1500,2000};
inline constexpr std::array<double,3> shotAngles{0,30,60};
inline constexpr double shotPenetration[6][3]={{385,311,108},{381,308,107},{364,294,102},{344,277,96},{325,262,91},{307,247,86}};
inline std::optional<double> shotMk3Penetration(double distance,double angle){
 if(!std::isfinite(distance)||!std::isfinite(angle)||distance<10||distance>2000||angle<0||angle>60)return {};
 size_t r=0,c=0;while(r<4&&distance>shotDistances[r+1])++r;while(c<1&&angle>shotAngles[c+1])++c;
 double t=(distance-shotDistances[r])/(shotDistances[r+1]-shotDistances[r]);double u=(angle-shotAngles[c])/(shotAngles[c+1]-shotAngles[c]);
 double low=std::lerp(shotPenetration[r][c],shotPenetration[r+1][c],t),high=std::lerp(shotPenetration[r][c+1],shotPenetration[r+1][c+1],t);
 return std::lerp(low,high,u);
}
inline Result analyzeShotMk3(double thickness,double distance,double angle,bool complex=false){
 Result result;auto penetration=shotMk3Penetration(distance,angle);
 if(!penetration||!std::isfinite(thickness)||thickness<=0){result.reason="Outside supplied table or invalid thickness; no extrapolation";return result;}
 result.penetration=*penetration;result.los=thickness/std::cos(angle*3.141592653589793/180);
 if(complex){result.reason="Composite, spaced or reactive armour requires a separate validated model";return result;}
 // The angle effect is already in the table: compare with NOMINAL thickness.
 double margin=*penetration/thickness;
 result.classification=margin>1.15?Class::Likely:margin<.85?Class::Unlikely:Class::Uncertain;
 result.reason="Supplied stat-card interpolation; homogeneous plate only. +/-15% heuristic band, not a measured probability. Damage unknown.";
 return result;
}
}

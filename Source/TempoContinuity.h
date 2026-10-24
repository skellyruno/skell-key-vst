#pragma once
// Worker-only, evidence-gated temporary retention of an established tempo.
// Retained values have visibly limited confidence and expire; they are NOT new rhythmic evidence.
#include <algorithm>
#include <cmath>
namespace skell {
class TempoContinuity {
public:
 struct Estimate { float bpm=0,confidence=0; bool held=false; };
 void reset() noexcept { *this=TempoContinuity{}; }
 void observe(float bpm,float confidence,double time) noexcept {
  if(!std::isfinite(time)||!std::isfinite(bpm)||bpm<40||bpm>240||
     !std::isfinite(confidence)||confidence<=0) return;
  if(lastBpm_>0 && time>=lastTime_ && time-lastTime_<=9.0 &&
     std::abs(std::log2(bpm/lastBpm_))<0.055f) confirmations_=std::min(8,confirmations_+1);
  else confirmations_=1;
  lastBpm_=bpm;lastConfidence_=confidence;lastTime_=time;
 }
 Estimate recover(double time) const noexcept {
  if(confirmations_<2 || !std::isfinite(time)) return {};
  const double age=time-lastTime_;
  if(age<0 || age>9.0)return {};
  return {lastBpm_,std::min(0.12f,lastConfidence_*0.4f)*
          std::clamp(1.f-static_cast<float>(age/10.),0.f,1.f),true};
 }
private:
 float lastBpm_=0,lastConfidence_=0; double lastTime_=-1.e9; int confirmations_=0;
};
}

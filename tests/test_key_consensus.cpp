#include "KeyTemporalConsensus.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#ifdef SKELL_COMPARE_PRIOR
#include "PriorKeyTemporalConsensus.h"
#endif
static void check(bool ok,const char* m){if(!ok){std::fprintf(stderr,"FAIL %s\n",m);std::exit(1);}}
struct Phrases {std::array<float,12> bass{};int root=0;int completedPhrases()const{return 4;}int phraseRootAt(int)const{return root;}const auto& bassHistogram()const{return bass;}};
int main(){constexpr int hop=4096;const std::array<float,12> maj{8,1,4,1,6,4,1,7,1,4,1,3},min{8,1,4,6,1,4,1,7,4,1,3,2};
for(double fs:{44100.,48000.,96000.,192000.,384000.}){
    for(int root=0;root<12;++root)for(int mode=0;mode<2;++mode){skell::KeyTemporalConsensus e;e.prepare(fs,hop);Phrases p;p.root=root;p.bass[root]=30;std::array<float,12> c{};for(int i=0;i<12;++i)c[(root+i)%12]=(mode?min:maj)[i];const int raw=(root+6)%12;int first=-1;
#ifdef SKELL_COMPARE_PRIOR
        skell::PriorKeyTemporalConsensus old;old.prepare(fs,hop);int oldFirst=-1;
#endif
        for(int i=0;i<int(42*fs/hop);++i){e.observe(c);auto q=e.propose(raw,mode,p,maj.data(),min.data());if(q.root>=0&&first<0)first=i+1;
#ifdef SKELL_COMPARE_PRIOR
            old.observe(c);auto o=old.propose(raw,mode,p,maj.data(),min.data());if(o.root>=0&&oldFirst<0)oldFirst=i+1;if(fs<=48000)check(q.root==o.root&&q.mode==o.mode&&q.longFit==o.longFit&&q.phraseHits==o.phraseHits,"standard-rate exact decisions");
#endif
        }check(first>0&&std::abs(first*hop/fs-24.)<.25,"24s proposal at all rates");auto q=e.propose(raw,mode,p,maj.data(),min.data());check(q.root==root&&q.mode==mode,"correct transposed profile");Phrases wrong;wrong.root=raw;wrong.bass[raw]=30;check(e.propose(raw,mode,wrong,maj.data(),min.data()).root==-1,"abstain without agreement");
#ifdef SKELL_COMPARE_PRIOR
        if(fs>=96000)check(oldFirst<0,"old high rate never becomes eligible");
#endif
        if(root==0&&mode==0){std::printf("%.0f Hz first decision %.6fs, stored groups %d PASS\n",fs,first*hop/fs,e.observedFrames());}e.reset();check(e.propose(raw,mode,p,maj.data(),min.data()).root==-1,"reset clears evidence");}
}
    skell::KeyTemporalConsensus e;e.prepare(std::numeric_limits<double>::infinity(),0);std::array<float,12> invalid{};invalid[0]=std::numeric_limits<float>::infinity();invalid[1]=std::numeric_limits<float>::quiet_NaN();invalid[2]=std::numeric_limits<float>::max();invalid[3]=std::numeric_limits<float>::max();for(int i=0;i<40000;++i)e.observe(invalid);Phrases p;check(e.propose(0,0,p,maj.data(),min.data()).root==-1,"invalid observations do not override");
}

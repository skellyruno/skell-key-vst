#include <SkellCore/WorldTuning.h>
#include <cassert>
#include <string_view>
#include <cmath>
#include <iostream>
using namespace skellcore::world;
int main() {
    for (int root=0;root<12;++root) for (int mode=0;mode<=1;++mode) {
        for(double hz: {400.0,432.0,440.0,444.125,450.0,480.0}) {
            auto tune = makeMusicalTuning(root,mode,hz);
            assert(tune.valid());
            assert((tune.mask & (1u<<root))!=0);
            MusicalTuning parsed;
            auto wire = formatMusicalTuning(tune);
            assert(parseMusicalTuning(wire.data(),parsed));
            assert(parsed.root==root && parsed.mode==mode && parsed.mask==tune.mask);
            assert(std::abs(parsed.a4Hz()-hz)<0.001);
            MusicalTuning unpacked;
            std::uint32_t sequence=0;
            assert(unpackMusicalTuning(packMusicalTuning(tune,123),unpacked,sequence));
            assert(sequence==123 && unpacked.mask==tune.mask && unpacked.a4MilliHz==tune.a4MilliHz);
        }
    }
    MusicalTuning output = makeMusicalTuning(0,0,440);
    for (const std::string_view invalid : {"", "0,0,2741", "0,0,2741,0", "12,0,2741,440000", "0,2,2741,440000",
          "0,0,99999,440000", "0,0,0,440000", "0,0,2741,390000", "0,0,2741,490000",
          "0,0,2741,440000,99", "0,0,2741,440000 ", "0,-1,2741,440000", "+0,0,2741,440000",
          "0,0,4294967295,440000", "0,0,2741,4294967295", "0,0,2741,440000\n"}) {
        assert(!parseMusicalTuning(invalid,output));
    }
    assert(output.root==0 && output.mode==0);  // failed parse never corrupts previous snapshot
    assert(!makeMusicalTuning(1,0,NAN).valid());
    std::cout << "SkellCore musical tuning: 144 round-trips + invalid-frame validation PASS\n";
}

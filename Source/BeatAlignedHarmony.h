#pragma once
// V38: Tempo/onset-aligned harmonic segmentation. Pure C++17, worker-only.
// Separates repeating phrase-end note compatibility from reliable chord transitions.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numeric>

namespace skell {

struct BeatPhaseObservation {
    float ageSeconds = 0.0f;    // Time since most recent beat on the estimated grid
    float confidence = 0.0f;    // Phase concentration, NOT a key/tempo probability
};

// onsetWrite is the next write position. All timing uses the actual onset sample rate.
// This is called only after the tempo estimator has a stable BPM hypothesis.
// The score rewards on-beat transients, with a light offbeat penalty to avoid
// treating double-time hats as a trustworthy downbeat.
template <std::size_t N>
BeatPhaseObservation estimateBeatPhase(const std::array<float,N>& onset, int onsetWrite,
                                       int onsetCount, float onsetRate, float bpm) noexcept {
    if (!(onsetRate > 20.0f && bpm >= 60.0f && bpm <= 220.0f) || onsetCount < onsetRate * 6.0f)
        return {};
    const float period = 60.0f * onsetRate / bpm;
    if (period < 7.0f || period > 240.0f) return {};
    const int observations = std::min(18, static_cast<int>((onsetCount - 4) / period));
    if (observations < 8) return {};
    const int phaseBins = std::max(4, static_cast<int>(std::ceil(period)));
    float best = -1.0e30f, second = -1.0e30f, scoreSum = 0.0f;
    int bestOffset = 0;
    const auto get = [&](int lag) noexcept {
        if (lag < 0 || lag >= onsetCount) return 0.0f;
        return std::max(0.0f, onset[(static_cast<std::size_t>(onsetWrite + static_cast<int>(N) - 1 - lag)) % N]);
    };
    const auto local = [&](int lag) noexcept {
        return 0.22f*get(lag-1) + 0.56f*get(lag) + 0.22f*get(lag+1);
    };
    for (int offset=0;offset<phaseBins;++offset) {
        float on=0.0f, off=0.0f;
        for (int beat=0;beat<observations;++beat) {
            const float at=static_cast<float>(offset)+static_cast<float>(beat)*period;
            on += local(static_cast<int>(std::lround(at)));
            off += local(static_cast<int>(std::lround(at+period*0.5f)));
        }
        const float score=(on-0.18f*off)/observations;
        scoreSum += score;
        if (score>best) { second=best; best=score; bestOffset=offset; }
        else if (score>second) second=score;
    }
    const float average=scoreSum/static_cast<float>(phaseBins);
    if (!(best>0.0001f) || best < average*1.07f) return {};
    return {bestOffset/onsetRate,
            std::clamp((best-average)/std::max(0.0001f, best),0.0f,1.0f)};
}

// Compact beat-by-beat chord summary. Does not assert a cadence from a phrase boundary.
// An onset-aligned grid is configured separately from spectral/chord observations.
class BeatAlignedHarmony {
public:
    struct Event { int beat=-1, root=-1, kind=-1, bass=-1; float confidence=0.0f; };
    void reset() noexcept { *this=BeatAlignedHarmony{}; }

    void configure(float bpm, float lastBeatTimeSeconds, float phaseConfidence) noexcept {
        if (!(bpm >= 60.0f && bpm <= 220.0f && phaseConfidence >= 0.09f)) return;
        const double period=60.0/static_cast<double>(bpm);
        if (beatPeriod_<=0.0 || std::abs(beatPeriod_-period)/period>0.025) {
            // Do not combine different beat grids after a genuine tempo reinterpretation.
            clearSegmentation();
            beatPeriod_=period;
            anchorSeconds_=lastBeatTimeSeconds;
            phaseEvidence_=phaseConfidence;
            return;
        }
        // Avoid re-phasing the history on ambiguous transient/hihat events.
        const double beats=(lastBeatTimeSeconds-anchorSeconds_)/beatPeriod_;
        const double deviation=(beats-std::round(beats))*beatPeriod_;
        if (std::abs(deviation)<0.19*beatPeriod_) {
            anchorSeconds_ += std::clamp(deviation,-0.025*beatPeriod_,0.025*beatPeriod_);
            phaseEvidence_=0.8f*phaseEvidence_+0.2f*phaseConfidence;
        }
    }

    void ingest(const std::array<float,12>& chroma, int bassPc, float bassConfidence,
                int chordRoot, int chordKind, float chordConfidence,
                double absoluteSeconds) noexcept {
        if (beatPeriod_<=0.0 || !std::isfinite(absoluteSeconds) || absoluteSeconds<anchorSeconds_) return;
        const int beat=static_cast<int>(std::floor((absoluteSeconds-anchorSeconds_)/beatPeriod_));
        if (beat<0) return;
        if (activeBeat_<0) activeBeat_=beat;
        if (beat!=activeBeat_) {
            if (beat==activeBeat_+1) finishBeat();
            else { resetBeat(); lastCandidateRoot_=-1; candidateRun_=0; }
            activeBeat_=beat;
        }
        const float total=std::accumulate(chroma.begin(),chroma.end(),0.0f);
        if (total>1.0e-7f) {
            for (int pc=0;pc<12;++pc)
                accumulation_[static_cast<std::size_t>(pc)]+=chroma[static_cast<std::size_t>(pc)]/total;
            ++tonalFrames_;
        }
        if (bassPc>=0 && bassPc<12 && bassConfidence>0.12f)
            bassVotes_[static_cast<std::size_t>(bassPc)]+=std::min(1.0f,bassConfidence);
        // We deliberately do not infer root solely from spectral profile. Chord
        // transcription must itself have passed the existing three-tone gate.
        if (chordRoot>=0 && chordRoot<12 && chordKind>=0 && chordKind<=1 && chordConfidence>0.20f)
            chordVotes_[static_cast<std::size_t>(chordRoot*2+chordKind)]+=chordConfidence;
    }

    float bonus(int root) const noexcept {
        if (root<0 || root>=12 || completedBeats_<16 || reliableChanges_<2 || phaseEvidence_<0.09f)
            return 0.0f;
        const float total=std::accumulate(resolutionVotes_.begin(),resolutionVotes_.end(),0.0f);
        if (total<0.2f) return 0.0f;
        const float share=resolutionVotes_[static_cast<std::size_t>(root)]/total;
        return std::clamp(0.028f*(share-1.0f/12.0f),-0.006f,0.021f);
    }

    int completedBeats() const noexcept { return completedBeats_; }
    int reliableChanges() const noexcept { return reliableChanges_; }
    int probableResolutions() const noexcept { return resolutions_; }
    float phaseConfidence() const noexcept { return phaseEvidence_; }
    const std::array<float,12>& resolutionVotes() const noexcept {return resolutionVotes_;}
private:
    void clearSegmentation() noexcept {
        resetBeat(); activeBeat_=-1; completedBeats_=reliableChanges_=resolutions_=0;
        lastCandidateRoot_=lastCandidateKind_=lastAcceptedRoot_=lastAcceptedKind_=-1; candidateRun_=0;
        resolutionVotes_.fill(0.0f);
    }
    void resetBeat() noexcept {
        accumulation_.fill(0.0f);bassVotes_.fill(0.0f);chordVotes_.fill(0.0f);
        tonalFrames_=0;
    }
    void finishBeat() noexcept {
        ++completedBeats_;
        if (tonalFrames_<2) {resetBeat();return;}
        int best=-1;float score=0.0f,runner=0.0f;
        for (int i=0;i<24;++i) {
            const float vote=chordVotes_[static_cast<std::size_t>(i)];
            if (vote>score) {runner=score;score=vote;best=i;}
            else runner=std::max(runner,vote);
        }
        if (best<0 || score<0.35f || score < 1.20f*runner) {
            lastCandidateRoot_=-1;candidateRun_=0;resetBeat();return;
        }
        const int root=best/2, kind=best%2;
        const float mass=std::accumulate(accumulation_.begin(),accumulation_.end(),0.0f);
        const int third=(root+(kind==0?4:3))%12, fifth=(root+7)%12;
        const float triad=(accumulation_[static_cast<std::size_t>(root)] +
                           accumulation_[static_cast<std::size_t>(third)] +
                           accumulation_[static_cast<std::size_t>(fifth)])/std::max(mass,1.0e-8f);
        const float weakest=std::min({accumulation_[static_cast<std::size_t>(root)],
                                     accumulation_[static_cast<std::size_t>(third)],
                                     accumulation_[static_cast<std::size_t>(fifth)]})/std::max(mass,1.0e-8f);
        if (triad<0.46f || weakest<0.035f) {
            lastCandidateRoot_=-1;candidateRun_=0;resetBeat();return;
        }
        if (root==lastCandidateRoot_ && kind==lastCandidateKind_) ++candidateRun_;
        else {lastCandidateRoot_=root;lastCandidateKind_=kind;candidateRun_=1;}
        // Require two consecutive confidently transcribed beats before accepting
        // a chord transition; avoids kick/hi-hat induced frame-level changes.
        if (candidateRun_>=2 && (root!=lastAcceptedRoot_ || kind!=lastAcceptedKind_)) {
            if (lastAcceptedRoot_>=0) {
                ++reliableChanges_;
                // Root motion on its own is not a cadence: require new root
                // in bass as well as major-triad evidence at the target beat.
                const float bassMass=std::accumulate(bassVotes_.begin(),bassVotes_.end(),0.0f);
                const float bassShare=bassMass>0.01f?bassVotes_[static_cast<std::size_t>(root)]/bassMass:0.0f;
                const bool dominant=lastAcceptedRoot_==(root+7)%12;
                const bool subdominant=lastAcceptedRoot_==(root+5)%12;
                if (kind==0 && (dominant||subdominant) && bassShare>=0.20f) {
                    resolutionVotes_[static_cast<std::size_t>(root)]+=std::min(0.5f,bassShare);
                    ++resolutions_;
                }
            }
            lastAcceptedRoot_=root;lastAcceptedKind_=kind;
        }
        resetBeat();
    }
    double beatPeriod_=-1.0,anchorSeconds_=0.0;
    float phaseEvidence_=0.0f;
    int activeBeat_=-1,tonalFrames_=0,completedBeats_=0,reliableChanges_=0,resolutions_=0;
    int lastCandidateRoot_=-1,lastCandidateKind_=-1,candidateRun_=0,lastAcceptedRoot_=-1,lastAcceptedKind_=-1;
    std::array<float,12> accumulation_{},bassVotes_{},resolutionVotes_{};
    std::array<float,24> chordVotes_{};
};

} // namespace skell

#pragma once
// Lightweight worker-only pitch/root and phrase-resolution evidence.
// Standard C++17; shared verbatim by the JUCE processor and the standalone C++ bench.
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <numeric>

namespace skell {
struct BassFrame {
    int midi = -1;
    int pc = -1;
    float strength = 0.0f;
    float confidence = 0.0f;
};

class BassFundamentalEstimator {
public:
    // Uses a pre-separated tonal spectrum. No FFT or allocations, ~30 candidates x 4 partials.
    static BassFrame analyze(const float* tonalPower, int bins, double sampleRate) noexcept {
        std::array<float, 27> salience{};
        constexpr int lo = 34, hi = 60; // Bb1 through C4; fewer sub-bass errors than 20 Hz tracking.
        // The spectral map is fixed for each MIDI note; construct it once instead
        // of calling pow() for every FFT frame (~27 notes per 93 ms).
        static const std::array<float,27> frequencies = [] {
            // Keep these bounds inside the lambda: MSVC cannot implicitly capture
            // the function-local lo/hi variables in a captureless lambda.
            constexpr int lo = 34, hi = 60;
            std::array<float,27> f{};
            for (int midi = lo; midi <= hi; ++midi)
                f[static_cast<std::size_t>(midi-lo)] =
                    440.0f * std::pow(2.0f, (midi - 69) / 12.0f);
            return f;
        }();
        static const std::array<float,4> harmonicDenominators = [] {
            std::array<float,4> a{};
            for (int h = 1; h <= 4; ++h)
                a[static_cast<std::size_t>(h-1)] = std::pow(static_cast<float>(h), 0.85f);
            return a;
        }();
        for (int midi = lo; midi <= hi; ++midi) {
            const float fundamental = frequencies[static_cast<std::size_t>(midi-lo)];
            float sum = 0.0f, first = 0.0f, total = 0.0f;
            for (int harmonic = 1; harmonic <= 4; ++harmonic) {
                const float hz = fundamental * harmonic;
                const int k = static_cast<int>(std::lround(hz * (2.0 * bins) / sampleRate));
                if (k < 8 || k + 9 >= bins) break;
                float center = 0.0f;
                for (int b = k - 1; b <= k + 1; ++b)
                    center = std::max(center, std::sqrt(std::max(0.0f, tonalPower[b])));
                // Estimate local spectral leakage/noise without sampling the main lobe.
                float side = 0.0f;
                for (int delta : {-8, -7, -6, 6, 7, 8})
                    side += std::sqrt(std::max(0.0f, tonalPower[k + delta]));
                const float contrast = std::max(0.0f, center - 0.7f * (side / 6.0f));
                const float weighted = contrast / harmonicDenominators[static_cast<std::size_t>(harmonic-1)];
                total += weighted;
                if (harmonic == 1) first = weighted;
                sum += weighted;
            }
            // Explicit fundamental support penalizes octave confusion but allows missing fundamentals.
            salience[static_cast<size_t>(midi - lo)] = sum * (0.55f + 0.45f * first / (total + 1.0e-8f));
        }
        int bestIndex = -1;
        float best = 0.0f;
        for (int i = 0; i < static_cast<int>(salience.size()); ++i)
            if (salience[static_cast<size_t>(i)] > best) { best = salience[static_cast<size_t>(i)]; bestIndex = i; }
        if (bestIndex < 0 || best < 1.0e-4f) return {};
        const int bestMidi = lo + bestIndex;
        float second = 0.0f;
        for (int i = 0; i < static_cast<int>(salience.size()); ++i) {
            const int midi = lo + i;
            if (std::abs(midi - bestMidi) <= 1 || (midi - bestMidi) % 12 == 0) continue;
            second = std::max(second, salience[static_cast<size_t>(i)]);
        }
        const float margin = best / (second + 1.0e-8f);
        // Weak / non-dominant bass guesses abstain instead of contaminating phrase decisions.
        if (margin < 1.12f) return {};
        BassFrame frame;
        frame.midi = bestMidi;
        frame.pc = bestMidi % 12;
        frame.strength = best;
        frame.confidence = std::clamp((margin - 1.0f) / 1.0f, 0.0f, 1.0f);
        return frame;
    }
};

class TonalPhraseTracker {
public:
    void reset() noexcept { *this = TonalPhraseTracker{}; }

    void ingest(BassFrame bass, const std::array<float, 12>& chroma,
                int chordRoot, int chordKind, float chordConfidence,
                float tempoBpm, float frameSeconds) noexcept {
        if (!(frameSeconds > 0.0f && frameSeconds < 1.0f)) return;
        // Beat-derived four-bar windows; if tempo is unavailable use an 8-second window.
        const float phraseLength = tempoBpm >= 60.0f && tempoBpm <= 200.0f
            ? std::clamp(960.0f / tempoBpm, 5.0f, 12.0f) : 8.0f;
        elapsed_ += frameSeconds;
        ++frames_;
        if (bass.pc >= 0 && bass.confidence > 0.12f) {
            const int pc = bass.pc;
            if (pc == pendingBass_) ++pendingFrames_;
            else { pendingBass_ = pc; pendingFrames_ = 1; }
            if (pendingFrames_ >= 2) stableBass_ = pc;
            missedBassFrames_ = 0;
        } else {
            ++missedBassFrames_;
            if (missedBassFrames_ >= 3) { stableBass_ = pendingBass_ = -1; pendingFrames_ = 0; }
        }
        if (stableBass_ >= 0 && bass.confidence > 0.12f) {
            float weight = std::clamp(bass.confidence, 0.0f, 0.8f);
            phraseBass_[static_cast<size_t>(stableBass_)] += weight;
            if (elapsed_ >= phraseLength * 0.75f)
                endingBass_[static_cast<size_t>(stableBass_)] += weight;
            totalBass_[static_cast<size_t>(stableBass_)] += weight;
        }
        float mass = std::accumulate(chroma.begin(), chroma.end(), 0.0f);
        if (mass > 1.0e-7f) {
            for (int i = 0; i < 12; ++i) {
                const float v = chroma[static_cast<size_t>(i)] / mass;
                phraseChroma_[static_cast<size_t>(i)] += v;
                if (elapsed_ >= phraseLength * 0.75f) endingChroma_[static_cast<size_t>(i)] += v;
            }
            ++harmonicFrames_;
        }
        if (chordRoot >= 0 && chordKind >= 0 && chordKind <= 1 && chordConfidence > 0.20f) {
            if (chordRoot != lastChordRoot_ || chordKind != lastChordKind_) {
                previousChordRoot_ = lastChordRoot_;
                lastChordRoot_ = chordRoot;
                lastChordKind_ = chordKind;
                if (elapsed_ >= phraseLength * 0.65f)
                    endingChordRoot_ = chordRoot;
            }
        }
        if (elapsed_ >= phraseLength) {
            finishPhrase();
            elapsed_ -= phraseLength;
            phraseBass_.fill(0.0f); endingBass_.fill(0.0f);
            phraseChroma_.fill(0.0f); endingChroma_.fill(0.0f);
            harmonicFrames_ = 0; previousChordRoot_ = -1; endingChordRoot_ = -1;
        }
    }

    int completedPhrases() const noexcept { return completed_; }
    int crediblePhrases() const noexcept { return credible_; }
    int cadenceEvents() const noexcept { return cadences_; }
    const std::array<float,12>& cadenceVotes() const noexcept { return cadence_; }
    const std::array<float, 12>& bassHistogram() const noexcept { return totalBass_; }
    const std::array<float, 12>& resolutionVotes() const noexcept { return resolution_; }
    // Diagnostics: most recent 32 completed phrase endings, -1 when no credible tonic.
    int phraseRootAt(int index) const noexcept {
        return index >= 0 && index < std::min(completed_, 32)
            ? phraseRoots_[static_cast<size_t>(index)] : -1;
    }
    float bonus(int root) const noexcept {
        if (credible_ < 3 || root < 0 || root >= 12) return 0.0f;
        const float total = std::accumulate(resolution_.begin(), resolution_.end(), 0.0f);
        if (total < 0.6f) return 0.0f;
        const float relative = resolution_[static_cast<size_t>(root)] / total;
        // The phrase evidence can only break close calls; never substitute for harmonic analysis.
        const float phraseEndPrior = std::clamp(0.10f * (relative - 1.0f / 12.0f), -0.025f, 0.055f);
        const float cadenceMass = std::accumulate(cadence_.begin(), cadence_.end(), 0.0f);
        const float cadencePrior = cadences_ >= 2 && cadenceMass > 0.01f
            ? std::clamp(0.03f * (cadence_[static_cast<size_t>(root)] / cadenceMass - 1.0f / 12.0f),
                         -0.01f, 0.025f) : 0.0f;
        return std::clamp(phraseEndPrior + cadencePrior, -0.035f, 0.075f);
    }
private:
    void finishPhrase() noexcept {
        ++completed_;
        if (completed_ <= 32) phraseRoots_[static_cast<size_t>(completed_ - 1)] = -1;
        const float bassMass = std::accumulate(endingBass_.begin(), endingBass_.end(), 0.0f);
        const float harmMass = std::accumulate(endingChroma_.begin(), endingChroma_.end(), 0.0f);
        if (bassMass < 0.2f || harmMass < 0.2f || harmonicFrames_ < 12) return;
        // Evaluate complete triads at phrase ends; bass fifths alone do not establish a tonic.
        float bestFit = 0.0f; int bestRoot = -1;
        for (int root = 0; root < 12; ++root) {
            const float bassShare = endingBass_[static_cast<size_t>(root)] / bassMass;
            const float third = endingChroma_[static_cast<size_t>((root + 4) % 12)] / harmMass;
            const float tonic = endingChroma_[static_cast<size_t>(root)] / harmMass;
            const float fifth = endingChroma_[static_cast<size_t>((root + 7) % 12)] / harmMass;
            const float triad = tonic + third + fifth;
            // Require all three triad notes: an isolated bass plus its fifth is insufficient.
            if (bassShare < 0.22f || std::min({tonic, third, fifth}) < 0.025f || triad < 0.28f) continue;
            // A V/IV -> I chord change is a cadence candidate; tonic-associated
            // bass/harmony at a boundary alone is only phrase-end support.
            const float fit = bassShare * triad;
            if (fit > bestFit) {bestFit = fit; bestRoot = root;}
        }
        if (bestRoot >= 0 && bestFit > 0.12f) {
            ++credible_;
            if (completed_ <= 32) phraseRoots_[static_cast<size_t>(completed_ - 1)] = bestRoot;
            for (float& v : resolution_) v *= 0.995f;
            resolution_[static_cast<size_t>(bestRoot)] += std::min(bestFit, 0.8f);
            // Strict event definition: last credible chord change must land on
            // this tonic from its dominant or subdominant within this phrase.
            if (endingChordRoot_ == bestRoot &&
                (previousChordRoot_ == (bestRoot + 7) % 12 ||
                 previousChordRoot_ == (bestRoot + 5) % 12)) {
                ++cadences_;
                for (float& v : cadence_) v *= 0.995f;
                cadence_[static_cast<size_t>(bestRoot)] += std::min(bestFit, 0.8f);
            }
        }
    }
    std::array<int, 32> phraseRoots_ = [] { std::array<int, 32> a{}; a.fill(-1); return a; }();
    std::array<float, 12> phraseBass_{}, endingBass_{}, totalBass_{};
    std::array<float, 12> phraseChroma_{}, endingChroma_{}, resolution_{}, cadence_{};
    int stableBass_ = -1, pendingBass_ = -1, pendingFrames_ = 0, missedBassFrames_ = 0;
    int lastChordRoot_ = -1, lastChordKind_ = -1, previousChordRoot_ = -1, endingChordRoot_ = -1;
    int frames_ = 0, harmonicFrames_ = 0, completed_ = 0, credible_ = 0, cadences_ = 0;
    float elapsed_ = 0.0f;
};
} // namespace skell

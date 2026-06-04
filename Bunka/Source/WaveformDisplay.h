#pragma once
#include <JuceHeader.h>
#include "BunkaLookAndFeel.h"
#include <vector>
#include <functional>
#include <algorithm>

//==============================================================================
// Static waveform view for M1a: draws a min/max thumbnail of the loaded loop,
// steel slice dividers, and a spark highlight on the most-recently-triggered
// slice. Also acts as the drag-and-drop target for .wav files.
//
// (Draggable slice markers arrive in M2 — this component is built so those
//  hooks slot in without restructuring.)
//==============================================================================
class WaveformDisplay : public juce::Component,
                        public juce::FileDragAndDropTarget
{
public:
    std::function<void (const juce::File&)> onFileDropped;
    std::function<void (const std::vector<int>&)> onSlicesEdited;   // drag/add/delete commit
    std::function<int (int)> snapSample;                            // snap a sample pos on drop

    void setThumbnail (std::vector<float> minMax, std::vector<int> sliceStarts,
                       int numSamples)
    {
        mMinMax     = std::move (minMax);
        mSlices     = std::move (sliceStarts);
        mNumSamples = juce::jmax (1, numSamples);
        repaint();
    }

    void setActiveSlice (int idx) { if (idx != mActiveSlice) { mActiveSlice = idx; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xFF0C0C0C));
        g.fillRoundedRectangle (b, 3.f);
        g.setColour (juce::Colour (0xFF1A1A1A));
        g.drawRoundedRectangle (b.reduced (0.5f), 3.f, 1.f);

        const float w   = b.getWidth();
        const float h   = b.getHeight();
        const float mid = b.getCentreY();

        if (mMinMax.empty())
        {
            g.setColour (bunkaDim);
            g.setFont (juce::Font (juce::FontOptions ("Courier New", 12.f, juce::Font::bold)));
            g.drawText (mDragOver ? "RELEASE TO LOAD" : "DROP A DRUM LOOP  (.wav)",
                        getLocalBounds(), juce::Justification::centred, false);
            return;
        }

        // active slice fill (spark, faint)
        if (mActiveSlice >= 0 && mActiveSlice < (int) mSlices.size())
        {
            const float x0 = sampleToX (mSlices[(size_t) mActiveSlice], w);
            const float x1 = mActiveSlice + 1 < (int) mSlices.size()
                                 ? sampleToX (mSlices[(size_t) mActiveSlice + 1], w) : w;
            g.setColour (bunkaSpark.withAlpha (0.10f));
            g.fillRect (x0, 0.f, x1 - x0, h);
        }

        // waveform (steel)
        g.setColour (bunkaSteel.withAlpha (0.85f));
        const int bins = (int) (mMinMax.size() / 2);
        for (int px = 0; px < (int) w; ++px)
        {
            const int bin = juce::jlimit (0, bins - 1, (int) ((px / w) * bins));
            const float mn = mMinMax[(size_t) bin * 2];
            const float mx = mMinMax[(size_t) bin * 2 + 1];
            const float y0 = mid - mx * (h * 0.5f - 2.f);
            const float y1 = mid - mn * (h * 0.5f - 2.f);
            g.drawVerticalLine (px + (int) b.getX(), juce::jmin (y0, y1), juce::jmax (y0, y1));
        }

        // slice dividers (steel); active-slice edges + hovered/dragged marker in spark
        for (int i = 0; i < (int) mSlices.size(); ++i)
        {
            const float x = sampleToX (mSlices[(size_t) i], w);
            const bool activeEdge = (i == mActiveSlice || i == mActiveSlice + 1);
            const bool handled    = (i == mHoverIndex || i == mDragIndex);
            const bool hot        = activeEdge || handled;
            g.setColour (hot ? bunkaSpark : bunkaSteel.withAlpha (0.70f));
            g.drawLine (x, 0.f, x, h, handled ? 2.f : (hot ? 1.5f : 1.f));
        }
    }

    //== Slice editing (mouse) ================================================
    void mouseMove (const juce::MouseEvent& e) override
    {
        const int idx = hitTestMarker ((float) e.x);
        if (idx != mHoverIndex) { mHoverIndex = idx; repaint(); }
        setMouseCursor (idx >= 1 ? juce::MouseCursor::LeftRightResizeCursor
                                 : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (mHoverIndex != -1) { mHoverIndex = -1; repaint(); }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        mDragIndex = hitTestMarker ((float) e.x);   // -1 or 0 => no drag (slice 0 is fixed)
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (mDragIndex < 1 || mDragIndex >= (int) mSlices.size()) return;
        mSlices[(size_t) mDragIndex] = clampBetweenNeighbours (mDragIndex, xToSample ((float) e.x));
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (mDragIndex >= 1 && mDragIndex < (int) mSlices.size())
        {
            int s = mSlices[(size_t) mDragIndex];
            if (snapSample) s = clampBetweenNeighbours (mDragIndex, snapSample (s));
            mSlices[(size_t) mDragIndex] = s;
            repaint();
            if (onSlicesEdited) onSlicesEdited (mSlices);
        }
        mDragIndex = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (mMinMax.empty()) return;
        mDragIndex = -1;

        const int idx = hitTestMarker ((float) e.x);
        if (idx >= 1)                                   // delete an existing cut (slice 0 stays)
        {
            mSlices.erase (mSlices.begin() + idx);
        }
        else                                            // add a new cut at the click position
        {
            int s = xToSample ((float) e.x);
            if (snapSample) s = snapSample (s);
            auto it = std::lower_bound (mSlices.begin(), mSlices.end(), s);
            if (it == mSlices.end() || *it != s) mSlices.insert (it, s);
        }
        mHoverIndex = -1;
        repaint();
        if (onSlicesEdited) onSlicesEdited (mSlices);
    }

    //== Drag & drop ==========================================================
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files)
            if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif")
                || f.endsWithIgnoreCase (".aiff") || f.endsWithIgnoreCase (".flac"))
                return true;
        return false;
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { mDragOver = true;  repaint(); }
    void fileDragExit  (const juce::StringArray&)           override { mDragOver = false; repaint(); }
    void filesDropped  (const juce::StringArray& files, int, int) override
    {
        mDragOver = false; repaint();
        for (auto& f : files)
        {
            juce::File file (f);
            if (file.existsAsFile() && onFileDropped) { onFileDropped (file); break; }
        }
    }

private:
    float sampleToX (int sample, float w) const
    {
        return (sample / (float) mNumSamples) * w;   // local paint coords
    }

    int xToSample (float x) const
    {
        const float w = juce::jmax (1.f, (float) getWidth());
        return juce::jlimit (0, mNumSamples - 1, (int) ((x / w) * mNumSamples));
    }

    // keep marker `idx` strictly between its neighbours (no overlap / no zero-width slices)
    int clampBetweenNeighbours (int idx, int sample) const
    {
        const int gap = juce::jmax (1, (int) ((4.f / juce::jmax (1.f, (float) getWidth())) * mNumSamples));
        const int lo  = mSlices[(size_t) idx - 1] + gap;
        const int hi  = (idx + 1 < (int) mSlices.size() ? mSlices[(size_t) idx + 1] : mNumSamples) - gap;
        return juce::jlimit (lo, juce::jmax (lo, hi), sample);
    }

    // returns the index of a slice marker within ~5px of x, or -1
    int hitTestMarker (float x) const
    {
        if (mSlices.empty()) return -1;
        const float w = (float) getWidth();
        int   best = -1;
        float bestD = 5.f;
        for (int i = 0; i < (int) mSlices.size(); ++i)
        {
            const float d = std::abs (sampleToX (mSlices[(size_t) i], w) - x);
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    std::vector<float> mMinMax;
    std::vector<int>   mSlices;
    int   mNumSamples  = 1;
    int   mActiveSlice = -1;
    int   mHoverIndex  = -1;
    int   mDragIndex   = -1;
    bool  mDragOver    = false;
};

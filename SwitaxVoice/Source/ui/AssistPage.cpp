#include "AssistPage.h"
#include "../PluginProcessor.h"

using namespace sx::analysis;

namespace sx::ui
{
namespace
{
float wrapped (juce::Graphics* g, const juce::String& text, const juce::Font& f, juce::Colour c, float x, float y, float w)
{
    juce::AttributedString as;
    as.append (text, f, c);
    as.setWordWrap (juce::AttributedString::byWord);
    juce::TextLayout tl;
    tl.createLayout (as, juce::jmax (10.0f, w));
    if (g != nullptr) tl.draw (*g, { x, y, w, tl.getHeight() + 2.0f });
    return tl.getHeight();
}

juce::Colour basisColour (Basis b)
{
    switch (b)
    {
        case Basis::Measured:   return colours::measured;
        case Basis::Heuristic:  return colours::heuristic;
        case Basis::Simulated:  return colours::simulated;
        case Basis::Suggestion: return colours::suggestion;
    }
    return colours::textDim;
}

juce::Colour severityColour (int s)
{
    switch (s)
    {
        case 3:  return colours::meterHigh;
        case 2:  return colours::coral;
        case 1:  return colours::gold;
        default: return colours::cyan;
    }
}

void styleButton (juce::TextButton& b, juce::Colour accent, bool primary = false)
{
    b.getProperties().set ("accent", (juce::int64) accent.getARGB());
    b.getProperties().set ("primary", primary);
}
} // namespace

//==============================================================================
RecCard::RecCard (const Recommendation& r) : rec (r)
{
    const bool act = rec.isActionable();
    for (auto* b : { &auditionButton, &acceptButton, &rejectButton, &detailsButton }) addAndMakeVisible (b);
    styleButton (auditionButton, colours::emerald);
    styleButton (acceptButton, colours::gold, true);
    styleButton (rejectButton, colours::coral);
    styleButton (detailsButton, colours::cyan);
    auditionButton.setClickingTogglesState (false);
    auditionButton.setTooltip ("Hear your current settings with this change (nothing is committed). Click again to stop.");
    acceptButton.setTooltip ("Apply this change to the parameters (undoable, automatable)");
    rejectButton.setTooltip (act ? "Dismiss this proposal" : "Dismiss this note");
    if (! act) { rejectButton.setButtonText ("GOT IT"); auditionButton.setVisible (false); acceptButton.setVisible (false); }

    amountSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    amountSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    amountSlider.setRange (0.0, 150.0, 5.0);
    amountSlider.setValue (100.0, juce::dontSendNotification);
    amountSlider.setDoubleClickReturnValue (true, 100.0);
    amountSlider.getProperties().set ("accent", (juce::int64) colours::gold.getARGB());
    amountSlider.setTooltip ("Modify: scale the proposed change (100 % = as proposed). Double-click resets.");
    amountSlider.onValueChange = [this]
    {
        amount = (float) amountSlider.getValue() / 100.0f;
        repaint();
        if (auditionButton.getToggleState() && onAudition) { onAudition (*this); onAudition (*this); }   // restart with new amount
    };
    addChildComponent (amountSlider);
    amountSlider.setVisible (act);

    auditionButton.onClick = [this] { if (onAudition) onAudition (*this); };
    acceptButton.onClick = [this] { if (onAccept) onAccept (*this); };
    rejectButton.onClick = [this] { if (onReject) onReject (*this); };
    detailsButton.onClick = [this] { expanded = ! expanded; detailsButton.setButtonText (expanded ? "LESS" : "DETAILS"); if (onLayoutChanged) onLayoutChanged (*this); };
}

void RecCard::setStatus (Status s)
{
    status = s;
    const bool pending = s == Status::Pending;
    auditionButton.setEnabled (pending);
    acceptButton.setEnabled (pending);
    rejectButton.setEnabled (pending);
    amountSlider.setEnabled (pending);
    repaint();
}

void RecCard::setAuditioning (bool a)
{
    auditionButton.setToggleState (a, juce::dontSendNotification);
    auditionButton.setButtonText (a ? "STOP" : "AUDITION");
    repaint();
}

float RecCard::layoutText (juce::Graphics* g, int width) const
{
    const float x = 18.0f, w = (float) width - 30.0f;
    float y = 36.0f;
    const auto body = font (12.5f), small = font (11.5f);

    if (rec.isActionable())
    {
        y += wrapped (g, "Proposal: " + juce::String (rec.proposal), font (12.5f, 1), colours::text, x, y, w) + 4.0f;
        if (! rec.predicted.empty())
        {
            if (g != nullptr) drawChip (*g, { x, y, 80, 14 }, "SIMULATED", colours::simulated);
            y += wrapped (g, rec.predicted.front(), small, colours::simulated.withAlpha (0.95f), x + 78.0f, y, w - 78.0f) + 4.0f;
        }
    }
    else
    {
        y += wrapped (g, rec.proposal, body, colours::text, x, y, w) + 4.0f;
    }

    if (expanded)
    {
        y += 4.0f;
        for (const auto& e : rec.evidence)
        {
            if (g != nullptr) drawChip (*g, { x, y, 90, 14 }, basisLabel (e.basis), basisColour (e.basis));
            y += wrapped (g, e.text, small, colours::textDim.brighter (0.3f), x + 84.0f, y, w - 84.0f) + 4.0f;
        }
        if (g != nullptr) drawChip (*g, { x, y, 90, 14 }, "HEURISTIC", colours::heuristic);
        y += wrapped (g, "Likely issue: " + juce::String (rec.issue), small, colours::textDim.brighter (0.3f), x + 84.0f, y, w - 84.0f) + 4.0f;
        for (size_t i = 1; i < rec.predicted.size(); ++i)
        {
            if (g != nullptr) drawChip (*g, { x, y, 90, 14 }, "SIMULATED", colours::simulated);
            y += wrapped (g, rec.predicted[i], small, colours::simulated.withAlpha (0.95f), x + 84.0f, y, w - 84.0f) + 4.0f;
        }
        y += 2.0f;
        y += wrapped (g, "Confidence " + juce::String (juce::roundToInt (rec.confidence * 100)) + " %: " + juce::String (rec.confidenceBasis) + ".",
                      small, colours::textFaint.brighter (0.25f), x, y, w) + 3.0f;
        y += wrapped (g, "Limitations: " + juce::String (rec.limitations), small, colours::textFaint.brighter (0.25f), x, y, w) + 3.0f;
    }
    return y;
}

int RecCard::heightForWidth (int width) const
{
    return (int) std::ceil (layoutText (nullptr, width)) + 42;
}

void RecCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const bool dim = status != Status::Pending;
    g.setColour (auditionButton.getToggleState() ? colours::bg2.brighter (0.06f) : colours::bg2);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (auditionButton.getToggleState() ? colours::emerald : colours::lineSoft.brighter (0.1f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, auditionButton.getToggleState() ? 1.5f : 1.0f);
    g.setColour (severityColour (rec.severity).withAlpha (dim ? 0.35f : 1.0f));
    g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 8.0f, 4.0f, r.getHeight() - 16.0f), 2.0f);

    // header: category chip, title, status, confidence
    float x = 18.0f;
    x += drawChip (g, { x, 10.0f, 100.0f, 16.0f }, juce::String (rec.category).toUpperCase(), severityColour (rec.severity)) + 8.0f;
    g.setFont (font (14.5f, 3));
    g.setColour (dim ? colours::textDim : colours::text);
    g.drawText (rec.title, juce::Rectangle<float> (x, 8.0f, r.getWidth() - x - 200.0f, 20.0f), juce::Justification::centredLeft, true);

    auto conf = juce::Rectangle<float> (r.getWidth() - 186.0f, 10.0f, 172.0f, 16.0f);
    if (status != Status::Pending)
    {
        drawChip (g, conf.withTrimmedLeft (70.0f), status == Status::Accepted ? "ACCEPTED" : (rec.isActionable() ? "REJECTED" : "NOTED"),
                  status == Status::Accepted ? colours::emerald : colours::textFaint, status == Status::Accepted);
    }
    else
    {
        g.setFont (font (10.0f, 3));
        g.setColour (colours::textDim);
        g.drawText ("CONFIDENCE", conf.removeFromLeft (70.0f), juce::Justification::centredLeft);
        auto bar = conf.removeFromLeft (64.0f).reduced (0.0f, 5.0f);
        g.setColour (colours::bg0);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (rec.confidence > 0.66 ? colours::emerald : (rec.confidence > 0.4 ? colours::gold : colours::coral));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) rec.confidence), 3.0f);
        g.setFont (font (11.0f, 1));
        g.setColour (colours::text);
        g.drawText (juce::String (juce::roundToInt (rec.confidence * 100)) + "%", conf, juce::Justification::centredRight);
    }

    layoutText (&g, getWidth());

    if (rec.isActionable() && status == Status::Pending)
    {
        g.setFont (font (10.5f, 1));
        g.setColour (colours::textDim);
        const auto sb = amountSlider.getBounds().toFloat();
        g.drawText ("AMOUNT " + juce::String (juce::roundToInt (amount * 100)) + "%", juce::Rectangle<float> (sb.getX() - 86.0f, sb.getY(), 82.0f, sb.getHeight()),
                    juce::Justification::centredRight);
    }
}

void RecCard::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    auto row = r.removeFromBottom (26);
    detailsButton.setBounds (row.removeFromRight (74));
    row.removeFromRight (8);
    rejectButton.setBounds (row.removeFromRight (rec.isActionable() ? 70 : 74));
    if (rec.isActionable())
    {
        row.removeFromRight (6);
        acceptButton.setBounds (row.removeFromRight (74));
        auditionButton.setBounds (row.removeFromLeft (92));
        row.removeFromLeft (96);
        amountSlider.setBounds (row.removeFromLeft (juce::jmin (150, row.getWidth() - 10)));
    }
}

void RecCard::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && e.y < 34) detailsButton.triggerClick();
}

//==============================================================================
AssistPage::AssistPage (SwitaxVoiceAudioProcessor& p) : proc (p)
{
    for (int c = 0; c < (int) Context::kNumContexts; ++c) contextBox.addItem (contextName ((Context) c), c + 1);
    contextBox.setTooltip ("What this track carries. SwitaxVoice can only analyse its own input (plus the sidechain).");
    lengthBox.addItem ("Listen for 10 s", 1);
    lengthBox.addItem ("Listen for 20 s", 2);
    lengthBox.addItem ("Listen for 30 s", 3);
    lengthBox.addItem ("Listen for 45 s", 4);
    lengthBox.setTooltip ("Seconds of signal (pauses are not counted) captured before analysis");
    targetBox.setTooltip ("Compare with a style target (heuristic house target) or a measured reference file");
    refreshTargetMenu();
    syncSettingsFromProcessor();

    contextBox.onChange = [this] { writeSettingsToProcessor(); refreshTargetMenu(); };
    targetBox.onChange = [this] { writeSettingsToProcessor(); };
    lengthBox.onChange = [this] { writeSettingsToProcessor(); };

    styleButton (learnButton, colours::gold, true);
    learnButton.onClick = [this]
    {
        const auto st = proc.analysis.getState();
        if (st == AnalysisEngine::State::Capturing)
        {
            if (proc.analysis.getCapturedActiveSeconds() >= 3.0) proc.analysis.finishLearningNow();
            else proc.analysis.cancelLearning();
        }
        else if (st != AnalysisEngine::State::Analysing)
        {
            startLearn();
        }
    };
    learnButton.setTooltip ("Capture the vocal while you play it, then analyse it and propose changes");
    styleButton (reanalyseButton, colours::cyan);
    reanalyseButton.setTooltip ("Analyse the last capture again with the current target, context and settings");
    reanalyseButton.onClick = [this]
    {
        writeSettingsToProcessor();
        auto* pp = &proc;
        proc.analysis.reanalyse (proc.assist, [pp] { return pp->getParamValues(); });
    };
    styleButton (loadRefButton, colours::cyan);
    loadRefButton.setTooltip ("Load a reference vocal (or mix) file: WAV, AIFF, FLAC, OGG or MP3");
    loadRefButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a reference", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f.existsAsFile())
            {
                proc.analysis.loadReference (f);
                proc.assist.genre = -1;
            }
        });
    };
    styleButton (clearRefButton, colours::coral);
    clearRefButton.setTooltip ("Forget the reference");
    clearRefButton.onClick = [this] { proc.analysis.clearReference(); if (proc.assist.genre < 0) proc.assist.genre = 0; refreshTargetMenu(); };

    styleButton (auditionAllButton, colours::emerald);
    auditionAllButton.setTooltip ("Hear every pending proposal together (at their amounts). Click again to stop.");
    auditionAllButton.onClick = [this]
    {
        if (auditioningAll) { stopAudition(); return; }
        stopAudition();
        auto v = proc.getParamValues();
        juce::StringArray ids;
        for (auto& c : cards)
            if (c->status == RecCard::Status::Pending && c->rec.isActionable())
            {
                applyScaled (v, c->rec, c->amount);
                for (auto& ch : c->rec.changes) ids.addIfNotAlreadyThere (sx::kParamDefs[(size_t) ch.index].id);
            }
        proc.startAudition (v);
        auditioningAll = true;
        auditionAllButton.setToggleState (true, juce::dontSendNotification);
        auditionAllButton.setButtonText ("STOP");
        if (onHighlight) onHighlight (ids);
    };
    styleButton (acceptAllButton, colours::gold);
    acceptAllButton.onClick = [this] { acceptAll(); };
    styleButton (undoButton, colours::coral);
    undoButton.setTooltip ("Undo the last accept");
    undoButton.onClick = [this] { undo(); };

    for (juce::Component* c : std::initializer_list<juce::Component*> { &contextBox, &targetBox, &lengthBox, &learnButton, &reanalyseButton,
                                                                         &loadRefButton, &clearRefButton, &graph, &tiles, &viewport,
                                                                         &auditionAllButton, &acceptAllButton, &undoButton })
        addAndMakeVisible (c);
    viewport.setViewedComponent (&listContent, false);
    viewport.setScrollBarsShown (true, false);
    rebuildFromReport();
}

AssistPage::~AssistPage()
{
    proc.stopAudition();
}

//==============================================================================
Context AssistPage::currentContext() const { return (Context) juce::jmax (0, contextBox.getSelectedId() - 1); }

void AssistPage::refreshTargetMenu()
{
    const int keep = proc.assist.genre;
    targetBox.clear (juce::dontSendNotification);
    const bool vocal = isVocalContext (currentContext());
    if (vocal)
    {
        targetBox.addSectionHeading ("House targets (heuristic)");
        const auto names = genreNames();
        for (size_t i = 0; i < names.size(); ++i) targetBox.addItem (names[i], (int) i + 1);
    }
    else
    {
        targetBox.addSectionHeading ("Mix targets (heuristic)");
        targetBox.addItem ("Afrobeat / Afro-pop mix", (int) Genre::AfrobeatLead + 1);
        targetBox.addItem ("Amapiano mix", (int) Genre::AmapianoVocal + 1);
    }
    targetBox.addSeparator();
    targetBox.addSectionHeading ("Measured");
    const auto ref = proc.analysis.getReferenceName();
    targetBox.addItem (ref.isNotEmpty() ? "Reference: " + ref : juce::String ("Reference (load a file)"), 100);
    targetBox.setItemEnabled (100, ref.isNotEmpty());
    if (keep < 0 && ref.isNotEmpty()) targetBox.setSelectedId (100, juce::dontSendNotification);
    else if (targetBox.indexOfItemId (juce::jmax (0, keep) + 1) >= 0) targetBox.setSelectedId (juce::jmax (0, keep) + 1, juce::dontSendNotification);
    else targetBox.setSelectedItemIndex (1, juce::dontSendNotification);
}

void AssistPage::syncSettingsFromProcessor()
{
    contextBox.setSelectedId ((int) proc.assist.context + 1, juce::dontSendNotification);
    const double s = proc.assist.captureSeconds;
    lengthBox.setSelectedId (s <= 12 ? 1 : s <= 22 ? 2 : s <= 35 ? 3 : 4, juce::dontSendNotification);
    refreshTargetMenu();
}

void AssistPage::writeSettingsToProcessor()
{
    proc.assist.context = currentContext();
    const int id = targetBox.getSelectedId();
    proc.assist.genre = id == 100 ? -1 : juce::jmax (0, id - 1);
    const double secs[] = { 10.0, 20.0, 30.0, 45.0 };
    proc.assist.captureSeconds = secs[juce::jlimit (0, 3, lengthBox.getSelectedId() - 1)];
}

void AssistPage::startLearn()
{
    writeSettingsToProcessor();
    stopAudition();
    auto* pp = &proc;
    proc.analysis.startLearning (proc.assist, [pp] { return pp->getParamValues(); });
}

//==============================================================================
void AssistPage::applyScaled (sx::ParamValues& v, const Recommendation& r, float amount)
{
    for (const auto& c : r.changes)
    {
        const auto& d = sx::kParamDefs[(size_t) c.index];
        const float cur = v[(size_t) c.index];
        float to = c.to;
        if (d.kind == sx::ParamKind::Float) to = cur + (c.to - cur) * amount;
        else if (amount <= 0.0f) to = cur;
        v[(size_t) c.index] = sx::clampParam (c.index, to);
    }
}

void AssistPage::stopAudition()
{
    proc.stopAudition();
    if (auditioning != nullptr) auditioning->setAuditioning (false);
    auditioning = nullptr;
    auditioningAll = false;
    auditionAllButton.setToggleState (false, juce::dontSendNotification);
    auditionAllButton.setButtonText ("AUDITION ALL");
    if (onHighlight) onHighlight ({});
}

void AssistPage::toggleAudition (RecCard& c)
{
    if (auditioning == &c) { stopAudition(); return; }
    stopAudition();
    auto v = proc.getParamValues();
    applyScaled (v, c.rec, c.amount);
    proc.startAudition (v);
    auditioning = &c;
    c.setAuditioning (true);
    juce::StringArray ids;
    for (auto& ch : c.rec.changes) ids.add (sx::kParamDefs[(size_t) ch.index].id);
    if (onHighlight) onHighlight (ids);
}

void AssistPage::accept (RecCard& c)
{
    if (c.status != RecCard::Status::Pending || ! c.rec.isActionable()) return;
    stopAudition();
    const auto before = proc.getParamValues();
    auto v = before;
    applyScaled (v, c.rec, c.amount);
    proc.applyValues (v);
    undoStack.push_back ({ before, { &c } });
    c.setStatus (RecCard::Status::Accepted);
    repaint();
}

void AssistPage::reject (RecCard& c)
{
    if (auditioning == &c) stopAudition();
    c.setStatus (RecCard::Status::Rejected);
}

void AssistPage::acceptAll()
{
    stopAudition();
    const auto before = proc.getParamValues();
    auto v = before;
    UndoEntry e { before, {} };
    for (auto& c : cards)
        if (c->status == RecCard::Status::Pending && c->rec.isActionable())
        {
            applyScaled (v, c->rec, c->amount);
            c->setStatus (RecCard::Status::Accepted);
            e.cards.push_back (c.get());
        }
    if (e.cards.empty()) return;
    proc.applyValues (v);
    undoStack.push_back (e);
    repaint();
}

void AssistPage::undo()
{
    if (undoStack.empty()) return;
    stopAudition();
    auto e = undoStack.back();
    undoStack.pop_back();
    proc.applyValues (e.values);
    for (auto* c : e.cards)
        for (auto& own : cards)
            if (own.get() == c) c->setStatus (RecCard::Status::Pending);
    repaint();
}

void AssistPage::rebuildFromReport()
{
    stopAudition();
    report = proc.analysis.getReport();
    shownVersion = proc.analysis.getReportVersion();
    cards.clear();
    undoStack.clear();
    if (report != nullptr)
        for (const auto& r : report->recs)
        {
            auto card = std::make_unique<RecCard> (r);
            card->onAudition = [this] (RecCard& c) { toggleAudition (c); };
            card->onAccept = [this] (RecCard& c) { accept (c); };
            card->onReject = [this] (RecCard& c) { reject (c); };
            card->onLayoutChanged = [this] (RecCard&) { layoutCards(); };
            listContent.addAndMakeVisible (*card);
            cards.push_back (std::move (card));
        }
    layoutCards();
    repaint();
}

void AssistPage::layoutCards()
{
    const int w = juce::jmax (100, viewport.getWidth() - 12);
    int y = 0;
    for (auto& c : cards)
    {
        const int h = c->heightForWidth (w);
        c->setBounds (0, y, w, h);
        y += h + 8;
    }
    listContent.setSize (w, juce::jmax (y, 10));
}

//==============================================================================
void AssistPage::tick()
{
    const auto st = proc.analysis.getState();
    if (proc.analysis.getReportVersion() != shownVersion) rebuildFromReport();

    juce::String label;
    switch (st)
    {
        case AnalysisEngine::State::Capturing:
            label = juce::String::formatted ("LISTENING %.0f / %.0f s", proc.analysis.getCapturedActiveSeconds(), proc.assist.captureSeconds);
            break;
        case AnalysisEngine::State::Analysing:
            label = "ANALYSING " + juce::String (juce::roundToInt (proc.analysis.getAnalysisProgress() * 100)) + "%";
            break;
        default:
            label = report != nullptr && report->valid ? "LEARN AGAIN" : "LEARN";
    }
    if (learnButton.getButtonText() != label) learnButton.setButtonText (label);
    learnButton.setEnabled (st != AnalysisEngine::State::Analysing);
    reanalyseButton.setEnabled (st != AnalysisEngine::State::Analysing && st != AnalysisEngine::State::Capturing && proc.analysis.hasCapture());
    undoButton.setEnabled (! undoStack.empty());
    bool anyPending = false;
    for (auto& c : cards) anyPending |= c->status == RecCard::Status::Pending && c->rec.isActionable();
    acceptAllButton.setEnabled (anyPending);
    auditionAllButton.setEnabled (anyPending || auditioningAll);

    const auto rs = proc.analysis.getReferenceStatus();
    if (rs != refStatus)
    {
        refStatus = rs;
        refreshTargetMenu();
    }
    if (st != lastState || st == AnalysisEngine::State::Capturing || st == AnalysisEngine::State::Analysing) repaint();
    lastState = st;
}

void AssistPage::paint (juce::Graphics& g)
{
    // setup column card
    auto col = juce::Rectangle<float> (0.0f, 0.0f, 236.0f, (float) getHeight());
    g.setColour (colours::bg1);
    g.fillRoundedRectangle (col, 10.0f);
    g.setColour (colours::lineSoft);
    g.drawRoundedRectangle (col.reduced (0.5f), 10.0f, 1.0f);

    auto label = [&g] (const char* t, float y)
    {
        g.setFont (font (10.0f, 3));
        g.setColour (colours::textDim);
        g.drawText (t, juce::Rectangle<float> (16.0f, y, 210.0f, 14.0f), juce::Justification::centredLeft);
    };
    g.setFont (font (17.0f, 3));
    g.setColour (colours::text);
    g.drawText ("ASSIST", juce::Rectangle<float> (16.0f, 12.0f, 200.0f, 22.0f), juce::Justification::centredLeft);
    g.setFont (font (11.0f));
    g.setColour (colours::textDim);
    g.drawFittedText ("Measures your vocal, explains what it finds and proposes changes you can audition, scale, accept or reject.",
                      juce::Rectangle<int> (16, 34, 206, 44), juce::Justification::topLeft, 3);
    label ("WHAT IS ON THIS TRACK?", 82.0f);
    label ("COMPARE WITH", 134.0f);
    label ("REFERENCE FILE", 186.0f);
    g.setFont (font (10.5f));
    g.setColour (proc.analysis.hasReference() ? colours::emerald : colours::textFaint);
    g.drawFittedText (refStatus.isNotEmpty() ? refStatus : juce::String ("None loaded (house targets are heuristic)"),
                      juce::Rectangle<int> (16, 232, 206, 28), juce::Justification::topLeft, 2);

    // learn progress bar under the button
    const auto lb = learnButton.getBounds().toFloat();
    auto prog = juce::Rectangle<float> (lb.getX(), lb.getBottom() + 5.0f, lb.getWidth(), 4.0f);
    g.setColour (colours::bg0);
    g.fillRoundedRectangle (prog, 2.0f);
    const auto st = proc.analysis.getState();
    const float pv = st == AnalysisEngine::State::Capturing ? proc.analysis.getCaptureProgress()
                   : st == AnalysisEngine::State::Analysing ? proc.analysis.getAnalysisProgress() : (report != nullptr && report->valid ? 1.0f : 0.0f);
    g.setColour (st == AnalysisEngine::State::Analysing ? colours::cyan : colours::gold);
    g.fillRoundedRectangle (prog.withWidth (prog.getWidth() * pv), 2.0f);

    // notes about what was (and was not) analysed
    float y = (float) reanalyseButton.getBottom() + 14.0f;
    label ("SIGNALS", y);
    y += 16.0f;
    juce::StringArray notes;
    if (report != nullptr && ! report->signalNotes.empty())
        for (auto& n : report->signalNotes) notes.add (n);
    else
        notes.add ("SwitaxVoice analyses only this plugin's input (and its sidechain if you route a beat to it). It never hears other tracks.");
    for (auto& n : notes)
    {
        const float h = wrapped (nullptr, n, font (10.5f), colours::textDim, 16.0f, y, 206.0f);
        if (y + h > (float) getHeight() - 110.0f) break;
        wrapped (&g, n, font (10.5f), colours::textDim, 16.0f, y, 206.0f);
        y += h + 5.0f;
    }

    // legend
    float ly = (float) getHeight() - 92.0f;
    label ("HOW TO READ THE CARDS", ly);
    ly += 17.0f;
    const std::pair<const char*, juce::Colour> chips[] = { { "MEASURED", colours::measured }, { "HEURISTIC", colours::heuristic },
                                                          { "SIMULATED", colours::simulated }, { "SUGGESTION", colours::suggestion } };
    const char* desc[] = { "a number taken from your audio", "an interpretation (rule of thumb)", "predicted on your audio with the real DSP", "a taste call: audition it" };
    for (int i = 0; i < 4; ++i)
    {
        drawChip (g, { 16.0f, ly, 90.0f, 14.0f }, chips[i].first, chips[i].second);
        g.setFont (font (10.0f));
        g.setColour (colours::textFaint.brighter (0.3f));
        g.drawText (desc[i], juce::Rectangle<float> (96.0f, ly, 136.0f, 14.0f), juce::Justification::centredLeft, true);
        ly += 17.0f;
    }

    // recommendations header
    const auto vb = viewport.getBounds();
    g.setFont (font (12.0f, 3));
    g.setColour (colours::text);
    juce::String hdr = "RECOMMENDATIONS";
    if (report != nullptr && report->valid) hdr << "  (" << (int) report->recs.size() << ")";
    g.drawText (hdr, juce::Rectangle<int> (vb.getX(), vb.getY() - 30, 260, 24), juce::Justification::centredLeft);
    if (report != nullptr && cards.empty())
    {
        g.setFont (font (13.0f));
        g.setColour (report->valid ? colours::emerald : colours::textDim);
        g.drawFittedText (report->status, vb.reduced (10, 10), juce::Justification::centredTop, 3);
    }
    else if (report == nullptr)
    {
        g.setFont (font (13.0f));
        g.setColour (colours::textDim);
        g.drawFittedText ("No analysis yet. Choose what is on this track and what to compare with, press LEARN and play 10-45 s of the part. "
                          "SwitaxVoice then measures it, simulates fixes on your audio, and lists them here.", vb.reduced (10, 10), juce::Justification::centredTop, 4);
    }
}

void AssistPage::resized()
{
    contextBox.setBounds (16, 98, 206, 30);
    targetBox.setBounds (16, 150, 206, 30);
    loadRefButton.setBounds (16, 202, 166, 26);
    clearRefButton.setBounds (188, 202, 34, 26);
    lengthBox.setBounds (16, 266, 206, 30);
    learnButton.setBounds (16, 306, 206, 46);
    reanalyseButton.setBounds (16, 366, 206, 28);

    auto r = getLocalBounds().withTrimmedLeft (248);
    graph.setBounds (r.removeFromTop (220));
    r.removeFromTop (8);
    tiles.setBounds (r.removeFromTop (60));
    r.removeFromTop (8);
    auto hdr = r.removeFromTop (30);
    undoButton.setBounds (hdr.removeFromRight (70).reduced (0, 2));
    hdr.removeFromRight (6);
    acceptAllButton.setBounds (hdr.removeFromRight (100).reduced (0, 2));
    hdr.removeFromRight (6);
    auditionAllButton.setBounds (hdr.removeFromRight (112).reduced (0, 2));
    viewport.setBounds (r);
    layoutCards();
}

//==============================================================================
void AssistPage::Graph::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::bg1);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (colours::lineSoft);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);
    auto a = bounds.reduced (12.0f, 10.0f).withTrimmedTop (18.0f).withTrimmedLeft (24.0f).withTrimmedBottom (12.0f);
    auto xFor = [&a] (double f) { return a.getX() + a.getWidth() * (float) (std::log (f / 20.0) / std::log (1000.0)); };
    auto yFor = [&a] (double db) { return juce::jmap ((float) juce::jlimit (-30.0, 15.0, db), -30.0f, 15.0f, a.getBottom(), a.getY()); };

    g.setFont (font (10.5f, 3));
    g.setColour (colours::gold);
    g.drawText ("TONAL BALANCE VS TARGET", bounds.reduced (14.0f, 8.0f).removeFromTop (14.0f), juce::Justification::topLeft);

    g.setFont (font (9.5f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        g.setColour (colours::lineSoft);
        g.drawVerticalLine ((int) xFor (f), a.getY(), a.getBottom());
        g.setColour (colours::textFaint);
        g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), juce::Rectangle<float> (xFor (f) - 15, a.getBottom() + 1, 30, 11), juce::Justification::centred);
    }
    for (double db : { -24.0, -12.0, 0.0, 12.0 })
    {
        g.setColour (db == 0 ? colours::line : colours::lineSoft);
        g.drawHorizontalLine ((int) yFor (db), a.getX(), a.getRight());
        g.setColour (colours::textFaint);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (a.getX() - 26, yFor (db) - 6, 22, 12), juce::Justification::centredRight);
    }

    const auto& rep = owner.report;
    if (rep == nullptr || ! rep->valid)
    {
        g.setFont (font (13.0f));
        g.setColour (colours::textDim);
        g.drawText (rep == nullptr ? "Press LEARN and play your part to see its tonal balance here" : juce::String (rep->status), a, juce::Justification::centred);
        return;
    }

    const auto& t = rep->target;
    const auto& valid = rep->raw.bandValid;
    auto offsetFor = [&] (const Measurements& m)
    {
        std::vector<double> d;
        for (int b = 0; b < kNumBands; ++b)
            if (m.bandValid[(size_t) b] && bandCentreHz (b) >= 100 && bandCentreHz (b) <= 12000) d.push_back (m.relDb[(size_t) b] - t.relDb[(size_t) b]);
        return percentileOf (d, 0.5);
    };
    auto curve = [&] (const BandArray& v, double off)
    {
        juce::Path p;
        bool started = false;
        for (int b = 0; b < kNumBands; ++b)
        {
            if (! valid[(size_t) b] || bandCentreHz (b) < 25.0) continue;
            const float x = xFor (bandCentreHz (b)), y = yFor (v[(size_t) b] - off);
            started ? p.lineTo (x, y) : p.startNewSubPath (x, y);
            started = true;
        }
        return p;
    };

    // tolerance band around the target
    juce::Path band;
    bool first = true;
    for (int b = 0; b < kNumBands; ++b)
        if (valid[(size_t) b] && bandCentreHz (b) >= 25.0)
        {
            const float x = xFor (bandCentreHz (b)), y = yFor (t.relDb[(size_t) b] + t.toleranceDb);
            first ? band.startNewSubPath (x, y) : band.lineTo (x, y);
            first = false;
        }
    for (int b = kNumBands - 1; b >= 0; --b)
        if (valid[(size_t) b] && bandCentreHz (b) >= 25.0) band.lineTo (xFor (bandCentreHz (b)), yFor (t.relDb[(size_t) b] - t.toleranceDb));
    band.closeSubPath();
    g.setColour (colours::gold.withAlpha (0.09f));
    g.fillPath (band);

    juce::Path dashed;
    const float dashes[] = { 5.0f, 4.0f };
    juce::PathStrokeType (1.6f).createDashedStroke (dashed, curve (t.relDb, 0.0), dashes, 2);
    g.setColour (colours::gold);
    g.fillPath (dashed);

    g.setColour (colours::cyan.withAlpha (0.65f));
    g.strokePath (curve (rep->raw.relDb, offsetFor (rep->raw)), juce::PathStrokeType (1.2f));
    g.setColour (colours::text.withAlpha (0.85f));
    g.strokePath (curve (rep->current.relDb, offsetFor (rep->current)), juce::PathStrokeType (1.8f));
    g.setColour (colours::emerald);
    g.strokePath (curve (rep->predicted.relDb, offsetFor (rep->predicted)), juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // legend
    auto lg = bounds.reduced (14.0f, 8.0f).removeFromTop (14.0f);
    const std::pair<juce::String, juce::Colour> items[] = {
        { "with all proposals (simulated)", colours::emerald }, { "now (your settings)", colours::text },
        { "input", colours::cyan }, { juce::String (t.name) + (t.measured ? "" : " (heuristic)"), colours::gold } };
    g.setFont (font (10.0f, 1));
    for (auto& [txt, c] : items)
    {
        const float w = juce::GlyphArrangement::getStringWidth (font (10.0f, 1), txt);
        auto it = lg.removeFromRight (w + 22.0f);
        g.setColour (c);
        g.fillRect (it.getX(), it.getCentreY() - 1.0f, 12.0f, 2.5f);
        g.setColour (colours::textDim);
        g.drawText (txt, it.withTrimmedLeft (16.0f), juce::Justification::centredLeft);
        lg.removeFromRight (6.0f);
    }
}

void AssistPage::Tiles::paint (juce::Graphics& g)
{
    const auto& rep = owner.report;
    struct Tile { juce::String title, value, sub; juce::Colour c; };
    std::vector<Tile> tl;
    auto f1 = [] (double v) { return v < -150 ? juce::String ("--") : juce::String (v, 1); };
    if (rep != nullptr && rep->valid)
    {
        const auto& r = rep->raw;
        const auto& p = rep->predicted;
        tl.push_back ({ "LOUDNESS", f1 (r.integratedLufs) + " LUFS", "after plan " + f1 (p.integratedLufs), colours::gold });
        tl.push_back ({ "LEVEL RANGE", f1 (rep->current.levelRange) + " dB", "plan " + f1 (p.levelRange) + " / target " + juce::String (rep->target.levelRangeMax, 1), colours::coral });
        tl.push_back ({ "TRUE PEAK", f1 (r.truePeakDb) + " dBTP", "plan " + f1 (p.truePeakDb) + " dBTP", r.truePeakDb > -1 ? colours::meterHigh : colours::cyan });
        tl.push_back ({ "CREST / PLR", f1 (r.crestDb) + " dB", "PLR " + f1 (r.plrDb) + " dB", colours::violet });
        tl.push_back ({ "SIBILANCE", r.sibilance.events > 0 ? juce::String (rep->current.sibilance.peakRelBodyDb, 1) + " dB" : juce::String ("none"),
                        juce::String (juce::roundToInt (r.sibilance.eventsPerMinute)) + "/min, plan " + (p.sibilance.events > 0 ? juce::String (p.sibilance.peakRelBodyDb, 1) : juce::String ("--")), colours::coral });
        tl.push_back ({ "PITCH (F0)", r.pitch.voicedFrames > 20 ? juce::String (juce::roundToInt (r.pitch.p5)) + "-" + juce::String (juce::roundToInt (r.pitch.p95)) + " Hz" : juce::String ("--"),
                        juce::String (juce::roundToInt (r.pitch.voicedFraction * 100)) + "% voiced", colours::emerald });
        tl.push_back ({ "STEREO", r.stereo ? juce::String (r.correlation, 2) : juce::String ("mono"), r.stereo ? "correlation" : "1 channel", colours::cyan });
    }
    else
    {
        for (auto* t : { "LOUDNESS", "LEVEL RANGE", "TRUE PEAK", "CREST / PLR", "SIBILANCE", "PITCH (F0)", "STEREO" })
            tl.push_back ({ t, "--", "", colours::textFaint });
    }
    const float gap = 8.0f;
    const float w = ((float) getWidth() - gap * (float) (tl.size() - 1)) / (float) tl.size();
    float x = 0.0f;
    for (auto& t : tl)
    {
        auto r = juce::Rectangle<float> (x, 0.0f, w, (float) getHeight());
        g.setColour (colours::bg1);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (t.c.withAlpha (0.8f));
        g.fillRoundedRectangle (r.removeFromTop (3.0f).reduced (10.0f, 0.0f), 1.5f);
        r = r.reduced (10.0f, 4.0f);
        g.setFont (font (9.5f, 3));
        g.setColour (colours::textDim);
        g.drawText (t.title, r.removeFromTop (13.0f), juce::Justification::centredLeft);
        g.setFont (font (15.5f, 3));
        g.setColour (colours::text);
        g.drawFittedText (t.value, r.removeFromTop (21.0f).toNearestInt(), juce::Justification::centredLeft, 1);
        g.setFont (font (9.5f));
        g.setColour (colours::textFaint.brighter (0.3f));
        g.drawFittedText (t.sub, r.toNearestInt(), juce::Justification::centredLeft, 1);
        x += w + gap;
    }
}

} // namespace sx::ui

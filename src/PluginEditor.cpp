#include "PluginEditor.h"

using namespace pg::ui;

namespace
{
constexpr int kBaseW = 1200, kBaseH = 820; // design size (100 %)
constexpr int kMinW = 980, kMinH = 700;
constexpr int kMaxW = 2600, kMaxH = 1800;
} // namespace

PostGrapeEditor::PostGrapeEditor (PostGrapeProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      eqPanel (p.apvts, [&p] { return p.currentSampleRate(); }),
      compPanel (p.apvts, p.engine.comp.grDb),
      widerPanel (p.apvts, p.engine.wide.correlation),
      monoPanel (p.apvts),
      panPanel (p.apvts),
      lofiPanel (p.apvts),
      limPanel (p.apvts, p.engine.lim)
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    for (juce::Component* c : { (juce::Component*) &prevBtn, (juce::Component*) &nextBtn, (juce::Component*) &saveBtn,
                                (juce::Component*) &renameBtn, (juce::Component*) &deleteBtn, (juce::Component*) &folderBtn,
                                (juce::Component*) &presetBox, (juce::Component*) &scaleSel, (juce::Component*) &eqPanel,
                                (juce::Component*) &compPanel, (juce::Component*) &widerPanel, (juce::Component*) &monoPanel,
                                (juce::Component*) &panPanel, (juce::Component*) &lofiPanel, (juce::Component*) &limPanel })
        content.addAndMakeVisible (c);

    prevBtn.onClick = [this] { proc.presets.loadPrevious(); };
    nextBtn.onClick = [this] { proc.presets.loadNext(); };
    saveBtn.onClick = [this] { savePreset(); };
    renameBtn.onClick = [this] { renamePreset(); };
    deleteBtn.onClick = [this] { deletePreset(); };
    folderBtn.onClick = [] {
        auto dir = pg::PresetManager::userFolder();
        dir.createDirectory();
        dir.startAsProcess();
    };
    presetBox.setTooltip ("Click: preset list");
    proc.presets.onChange = [this] { presetBox.repaint(); };

    scale = proc.uiScale;
    scaleSel.setSelected (scale > 1.1f ? 1 : 0, false);
    scaleSel.onChange = [this] (int i) { setUiScale (i == 1 ? 1.25f : 1.0f); };

    // Read the saved size first: setResizeLimits() calls resized(), which writes the size.
    const int w = juce::jlimit (kMinW, kMaxW, proc.uiWidth), h = juce::jlimit (kMinH, kMaxH, proc.uiHeight);
    settingScale = true;
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (kMinW * scale), juce::roundToInt (kMinH * scale),
                     juce::roundToInt (kMaxW * scale), juce::roundToInt (kMaxH * scale));
    settingScale = false;
    setSize (juce::roundToInt ((float) w * scale), juce::roundToInt ((float) h * scale));

    startTimerHz (30);
}

PostGrapeEditor::~PostGrapeEditor()
{
    proc.presets.onChange = nullptr;
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void PostGrapeEditor::setUiScale (float s)
{
    if (std::abs (s - scale) < 0.001f)
        return;
    const float logicalW = (float) getWidth() / scale, logicalH = (float) getHeight() / scale;
    scale = s;
    proc.uiScale = s;
    settingScale = true;
    setResizeLimits (juce::roundToInt (kMinW * scale), juce::roundToInt (kMinH * scale),
                     juce::roundToInt (kMaxW * scale), juce::roundToInt (kMaxH * scale));
    setSize (juce::roundToInt (logicalW * scale), juce::roundToInt (logicalH * scale));
    settingScale = false;
    resized();
}

void PostGrapeEditor::resized()
{
    const int lw = juce::roundToInt ((float) getWidth() / scale);
    const int lh = juce::roundToInt ((float) getHeight() / scale);
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, lw, lh);
    if (! settingScale)
    {
        proc.uiWidth = lw;
        proc.uiHeight = lh;
    }
}

// ---------------------------------------------------------------- layout
void PostGrapeEditor::Content::resized() { owner.layoutContent(); }

void PostGrapeEditor::layoutContent()
{
    const float W = (float) content.getWidth(), H = (float) content.getHeight();
    // Elements grow with the window. 1.0 at the design size.
    const float z = juce::jlimit (0.82f, 1.8f, juce::jmin (W / (float) kBaseW, H / (float) kBaseH));
    content.z = z;
    auto R = [] (float v) { return juce::roundToInt (v); };

    const int pad = R (14.0f * z), gap = R (12.0f * z), headerH = R (62.0f * z);
    auto area = content.getLocalBounds().withTrimmedLeft (pad).withTrimmedRight (pad).withTrimmedBottom (pad);
    auto header = area.removeFromTop (headerH);

    // Header: preset bar in the centre.
    const int bs = R (34.0f * z), bg = R (8.0f * z), boxW = R (300.0f * z);
    const int barW = bs * 6 + boxW + bg * 7;
    int x = header.getCentreX() - barW / 2;
    const int by = header.getCentreY() - bs / 2;
    prevBtn.setBounds (x, by, bs, bs); x += bs + bg;
    presetBox.z = z;
    presetBox.setBounds (x, by, boxW, bs); x += boxW + bg;
    for (auto* b : { &nextBtn, &saveBtn, &renameBtn, &deleteBtn, &folderBtn })
    {
        b->setBounds (x, by, bs, bs);
        x += bs + bg;
    }
    scaleSel.setZ (z);
    const int sw = scaleSel.preferredWidth(), sh = R (26.0f * z);
    scaleSel.setBounds (header.getRight() - sw, header.getCentreY() - sh / 2, sw, sh);
    const float chipW = textWidth (font (11.0f * z, Weight::ExtraBold), "LATENCY 000 smp") + 20.0f * z;
    content.latencyChip = { (float) scaleSel.getX() - 10.0f * z - chipW, (float) header.getCentreY() - 13.0f * z, chipW, 26.0f * z };

    // Panels: rows 1.55 : 1 : 1.08
    const float avail = (float) (area.getHeight() - gap * 2);
    const int h1 = R (avail * 1.55f / 3.63f), h2 = R (avail * 1.0f / 3.63f);
    auto row1 = area.removeFromTop (h1);
    area.removeFromTop (gap);
    auto row2 = area.removeFromTop (h2);
    area.removeFromTop (gap);
    auto row3 = area;

    auto split = [gap] (juce::Rectangle<int> row, std::initializer_list<float> parts) {
        std::vector<juce::Rectangle<int>> out;
        float total = 0.0f;
        for (float p : parts) total += p;
        const float w = (float) (row.getWidth() - gap * ((int) parts.size() - 1));
        for (float p : parts)
        {
            out.push_back (row.removeFromLeft (juce::roundToInt (w * p / total)));
            row.removeFromLeft (gap);
        }
        out.back().setRight (out.back().getRight() + row.getWidth() + gap); // remainder
        return out;
    };

    const auto r1 = split (row1, { 1.8f, 1.0f });
    const auto r2 = split (row2, { 1.0f, 1.0f, 1.0f, 1.3f });
    ModulePanel* p1[] = { &eqPanel, &compPanel };
    ModulePanel* p2[] = { &widerPanel, &monoPanel, &panPanel, &lofiPanel };
    for (int i = 0; i < 2; ++i) { p1[i]->setZ (z); p1[i]->setBounds (r1[(size_t) i]); }
    for (int i = 0; i < 4; ++i) { p2[i]->setZ (z); p2[i]->setBounds (r2[(size_t) i]); }
    limPanel.setZ (z);
    limPanel.setBounds (row3);
    content.repaint();
}

// ---------------------------------------------------------------- painting
void PostGrapeEditor::Content::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    juce::ColourGradient bg (col::winTop, r.getCentreX(), 0.0f, col::winBottom, r.getCentreX(), r.getBottom() * 1.1f, true);
    bg.addColour (0.45, col::win);
    g.setGradientFill (bg);
    g.fillAll();

    // Logo
    const float pad = 14.0f * z;
    const auto& im = Images::get();
    drawImageW (g, im.grapes, pad + 2.0f * z, 8.0f * z, 40.0f * z);
    g.setColour (col::deep);
    g.setFont (font (26.0f * z, Weight::Black).withExtraKerningFactor (0.06f));
    g.drawText ("POST-GRAPE", juce::Rectangle<float> (pad + 48.0f * z, 9.0f * z, 260.0f * z, 30.0f * z), juce::Justification::centredLeft, false);
    g.setColour (col::lav2);
    g.setFont (font (11.0f * z, Weight::Bold).withExtraKerningFactor (0.14f));
    g.drawText (juce::String::fromUTF8 ("BY DESNOTTO \xc2\xb7 GUITAR BUS FX"), juce::Rectangle<float> (pad + 49.0f * z, 38.0f * z, 260.0f * z, 14.0f * z),
                juce::Justification::centredLeft, false);

    // Latency chip
    const int lat = owner.lastLatency < 0 ? 0 : owner.lastLatency;
    auto chip = latencyChip;
    g.setColour (juce::Colour (0xfff3e3f0));
    g.fillRoundedRectangle (chip, 9.0f * z);
    g.setColour (col::line);
    g.drawRoundedRectangle (chip.reduced (0.5f), 9.0f * z, 1.0f);
    const auto f = font (11.0f * z, Weight::ExtraBold).withExtraKerningFactor (0.06f);
    const juce::String a = "LATENCY ", b = juce::String (lat) + " smp";
    const float wa = textWidth (f, a), wb = textWidth (f, b);
    float tx = chip.getCentreX() - (wa + wb) * 0.5f;
    g.setFont (f);
    g.setColour (col::muted);
    g.drawText (a, juce::Rectangle<float> (tx, chip.getY(), wa + 2.0f, chip.getHeight()), juce::Justification::centredLeft, false);
    g.setColour (lat == 0 ? juce::Colour (0xff4f9a45) : col::hot);
    g.drawText (b, juce::Rectangle<float> (tx + wa, chip.getY(), wb + 2.0f, chip.getHeight()), juce::Justification::centredLeft, false);
}

void PostGrapeEditor::PresetBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffdf6fb), 0, r.getY(), juce::Colour (0xfff3e3f0), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    g.setColour (col::line2);
    g.drawRoundedRectangle (r, rad, 1.0f);

    const bool factory = presets.isCurrentFactory();
    const juce::String cat = factory ? "FACTORY" : "USER";
    const auto fc = font (11.0f * z, Weight::ExtraBold).withExtraKerningFactor (0.08f);
    const float cw = textWidth (fc, cat) * 1.08f;
    float x = r.getX() + 14.0f * z;
    g.setFont (fc);
    g.setColour (col::lav2);
    g.drawText (cat, juce::Rectangle<float> (x, r.getY(), cw + 2.0f, r.getHeight()), juce::Justification::centredLeft, false);
    x += cw + 8.0f * z;
    g.setFont (font (14.0f * z, Weight::Bold));
    g.setColour (col::deep);
    const auto name = presets.currentName() + (presets.isDirty() ? " *" : "");
    g.drawText (name, juce::Rectangle<float> (x, r.getY(), r.getRight() - x - 30.0f * z, r.getHeight()), juce::Justification::centredLeft, true);

    juce::Path arrow;
    const float ax = r.getRight() - 20.0f * z, ay = r.getCentreY();
    arrow.startNewSubPath (ax - 4.0f * z, ay - 2.0f * z);
    arrow.lineTo (ax, ay + 2.0f * z);
    arrow.lineTo (ax + 4.0f * z, ay - 2.0f * z);
    g.setColour (col::muted);
    g.strokePath (arrow, juce::PathStrokeType (2.0f * z, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void PostGrapeEditor::PresetBox::mouseDown (const juce::MouseEvent&)
{
    presets.refresh();
    juce::PopupMenu menu;
    const auto& list = presets.entries();
    const int current = presets.currentIndex();
    menu.addSectionHeader ("FACTORY");
    bool userHeader = false;
    for (int i = 0; i < (int) list.size(); ++i)
    {
        if (! list[(size_t) i].factory && ! userHeader)
        {
            menu.addSectionHeader ("USER");
            userHeader = true;
        }
        menu.addItem (i + 1, list[(size_t) i].name, true, i == current);
    }
    if (! userHeader)
    {
        menu.addSectionHeader ("USER");
        menu.addItem (-1, "(no user presets: click the save button)", false, false);
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                        [this] (int result) {
                            if (result > 0)
                                presets.load (result - 1);
                        });
}

// ---------------------------------------------------------------- timer
void PostGrapeEditor::timerCallback()
{
    for (ModulePanel* p : { (ModulePanel*) &eqPanel, (ModulePanel*) &compPanel, (ModulePanel*) &widerPanel, (ModulePanel*) &monoPanel,
                            (ModulePanel*) &panPanel, (ModulePanel*) &lofiPanel, (ModulePanel*) &limPanel })
        p->refresh();

    const auto text = proc.presets.currentName() + (proc.presets.isDirty() ? "*" : "");
    if (text != lastPresetText)
    {
        lastPresetText = text;
        presetBox.repaint();
    }
    const int lat = proc.getLatencySamples();
    if (lat != lastLatency)
    {
        lastLatency = lat;
        content.repaint (content.latencyChip.toNearestInt().expanded (2));
    }
}

// ---------------------------------------------------------------- dialogs
void PostGrapeEditor::showTextDialog (const juce::String& title, const juce::String& message, const juce::String& initial,
                                      const juce::String& okText, std::function<void (juce::String)> onOk)
{
    auto* w = new juce::AlertWindow (title, message, juce::MessageBoxIconType::NoIcon, &content);
    w->setLookAndFeel (&lnf);
    w->addTextEditor ("name", initial, "Name:");
    if (auto* te = w->getTextEditor ("name"))
    {
        te->setFont (font (15.0f, Weight::Bold));
        te->setInputRestrictions (64);
        te->selectAll();
    }
    w->addButton (okText, 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    content.addAndMakeVisible (w);
    w->setCentrePosition (content.getLocalBounds().getCentre());
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, onOk] (int result) {
                            if (result == 1 && onOk)
                                onOk (w->getTextEditorContents ("name").trim());
                        }),
                        true);
}

void PostGrapeEditor::showConfirm (const juce::String& title, const juce::String& message, const juce::String& okText,
                                   std::function<void()> onOk)
{
    auto* w = new juce::AlertWindow (title, message, juce::MessageBoxIconType::NoIcon, &content);
    w->setLookAndFeel (&lnf);
    w->addButton (okText, 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    content.addAndMakeVisible (w);
    w->setCentrePosition (content.getLocalBounds().getCentre());
    w->enterModalState (true, juce::ModalCallbackFunction::create ([onOk] (int result) {
                            if (result == 1 && onOk)
                                onOk();
                        }),
                        true);
}

void PostGrapeEditor::savePreset()
{
    const auto current = proc.presets.isCurrentFactory() ? proc.presets.currentName() + " copy" : proc.presets.currentName();
    showTextDialog ("Save preset", "Type a name for the preset.", current, "Save", [this] (juce::String name) {
        if (name.isEmpty())
            return;
        auto doSave = [this, name] {
            if (! proc.presets.save (name))
                showConfirm ("Save preset", "Cannot save the preset file.", "OK", nullptr);
        };
        if (proc.presets.userPresetExists (name) && name != proc.presets.currentName())
            showConfirm ("Replace preset", "The preset \"" + name + "\" exists. Replace it?", "Replace", doSave);
        else
            doSave();
    });
}

void PostGrapeEditor::renamePreset()
{
    if (proc.presets.isCurrentFactory() || proc.presets.currentIndex() < 0)
    {
        showConfirm ("Rename preset", "You cannot rename a factory preset. Save it as a user preset first.", "OK", nullptr);
        return;
    }
    const int index = proc.presets.currentIndex();
    showTextDialog ("Rename preset", "Type a new name.", proc.presets.currentName(), "Rename", [this, index] (juce::String name) {
        if (name.isNotEmpty() && ! proc.presets.rename (index, name))
            showConfirm ("Rename preset", "Cannot rename the preset. A preset with this name can exist.", "OK", nullptr);
    });
}

void PostGrapeEditor::deletePreset()
{
    if (proc.presets.isCurrentFactory() || proc.presets.currentIndex() < 0)
    {
        showConfirm ("Delete preset", "You cannot delete a factory preset.", "OK", nullptr);
        return;
    }
    const int index = proc.presets.currentIndex();
    showConfirm ("Delete preset", "Delete the preset \"" + proc.presets.currentName() + "\"?\nThe file goes to the Recycle Bin.", "Delete",
                 [this, index] { proc.presets.remove (index); });
}

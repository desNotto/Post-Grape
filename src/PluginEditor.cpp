#include "PluginEditor.h"

using namespace pg::ui;

namespace
{
constexpr int kBaseW = 1200, kBaseH = 856; // design size (100 %)
constexpr int kMinW = 980, kMinH = 730;
constexpr int kMaxW = 2600, kMaxH = 1800;
constexpr juce::int64 kDayMs = 24 * 60 * 60 * 1000;
} // namespace

PostGrapeEditor::PostGrapeEditor (PostGrapeProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      eqPanel (p.apvts, [&p] { return p.currentSampleRate(); }),
      compPanel (p.apvts, p.engine.comp.grDb),
      widerPanel (p.apvts, p.engine.wide.correlation),
      monoPanel (p.apvts),
      panPanel (p.apvts),
      lofiPanel (p.apvts),
      limPanel (p.apvts, p.engine.lim),
      agPanel (p.apvts, p.engine.ag)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    for (juce::Component* c : { (juce::Component*) &prevBtn, (juce::Component*) &nextBtn, (juce::Component*) &saveBtn,
                                (juce::Component*) &renameBtn, (juce::Component*) &deleteBtn, (juce::Component*) &folderBtn,
                                (juce::Component*) &presetBox, (juce::Component*) &langSel, (juce::Component*) &updateBtn,
                                (juce::Component*) &scaleSel, (juce::Component*) &hintBar, (juce::Component*) &eqPanel,
                                (juce::Component*) &compPanel, (juce::Component*) &widerPanel, (juce::Component*) &monoPanel,
                                (juce::Component*) &panPanel, (juce::Component*) &lofiPanel, (juce::Component*) &limPanel,
                                (juce::Component*) &agPanel })
        content.addAndMakeVisible (c);

    // Hints for the header controls.
    setHint (prevBtn, "prev", "PREVIOUS");
    setHint (nextBtn, "next", "NEXT");
    setHint (saveBtn, "save", "SAVE");
    setHint (renameBtn, "rename", "RENAME");
    setHint (deleteBtn, "del", "DELETE");
    setHint (folderBtn, "folder", "FOLDER");
    setHint (updateBtn, "upd", "UPDATE");
    setHint (presetBox, "pname", "PRESET");
    scaleSel.setHints ({ "z100", "z125" }, "SCALE");

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
    updateBtn.onClick = [this] { onUpdateButton(); };
    proc.presets.onChange = [this] { presetBox.repaint(); };

    langSel.setRussian (settings->hintsRussian());
    langSel.onChange = [this] (bool ru) {
        settings->setHintsRussian (ru);
        updateHint (nullptr);
    };

    autoCheckToggle.setToggleState (settings->autoCheckUpdates(), juce::dontSendNotification);
    autoCheckToggle.setColour (juce::ToggleButton::textColourId, col::muted);
    autoCheckToggle.setColour (juce::ToggleButton::tickColourId, col::hot);
    autoCheckToggle.setColour (juce::ToggleButton::tickDisabledColourId, col::line2);
    autoCheckToggle.onClick = [this] { settings->setAutoCheckUpdates (autoCheckToggle.getToggleState()); };
    autoCheckToggle.setSize (420, 26);

    // Hint bar: listen to the mouse over all controls.
    content.addMouseListener (this, true);
    updateHint (nullptr);

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

    // Automatic update check: one time each day.
    const auto now = juce::Time::currentTimeMillis();
    if (settings->autoCheckUpdates() && now - settings->lastUpdateCheck() > kDayMs
        && updates->status() == pg::UpdateChecker::Status::Idle)
    {
        settings->setLastUpdateCheck (now);
        updates->startCheck();
    }

    startTimerHz (30);
}

PostGrapeEditor::~PostGrapeEditor()
{
    content.removeMouseListener (this);
    proc.presets.onChange = nullptr;
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

    // Hint bar at the bottom.
    hintBar.z = z;
    hintBar.setBounds (area.removeFromBottom (R (28.0f * z)));
    area.removeFromBottom (R (10.0f * z));

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

    // Header right: flags, update, scale (each sized to its content).
    scaleSel.setZ (z);
    langSel.setZ (z);
    const int sh = R (26.0f * z);
    int rx = header.getRight();
    const int sw = scaleSel.preferredWidth();
    rx -= sw;
    scaleSel.setBounds (rx, header.getCentreY() - sh / 2, sw, sh);
    rx -= bg + bs;
    updateBtn.setBounds (rx, by, bs, bs);
    const int lw = langSel.preferredWidth();
    rx -= bg + lw;
    langSel.setBounds (rx, header.getCentreY() - sh / 2, lw, sh);

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
    const auto r3 = split (row3, { 3.3f, 1.0f });
    ModulePanel* p1[] = { &eqPanel, &compPanel };
    ModulePanel* p2[] = { &widerPanel, &monoPanel, &panPanel, &lofiPanel };
    ModulePanel* p3[] = { &limPanel, &agPanel };
    for (int i = 0; i < 2; ++i) { p1[i]->setZ (z); p1[i]->setBounds (r1[(size_t) i]); }
    for (int i = 0; i < 4; ++i) { p2[i]->setZ (z); p2[i]->setBounds (r2[(size_t) i]); }
    for (int i = 0; i < 2; ++i) { p3[i]->setZ (z); p3[i]->setBounds (r3[(size_t) i]); }
    content.repaint();
}

// ---------------------------------------------------------------- hints
void PostGrapeEditor::mouseMove (const juce::MouseEvent& e) { updateHint (&e); }
void PostGrapeEditor::mouseEnter (const juce::MouseEvent& e) { updateHint (&e); }
void PostGrapeEditor::mouseDrag (const juce::MouseEvent&) {}

void PostGrapeEditor::mouseExit (const juce::MouseEvent& e)
{
    const auto p = e.getEventRelativeTo (&content).position.toInt();
    if (! content.getLocalBounds().contains (p))
    {
        mouseInside = false;
        updateHint (nullptr);
    }
}

void PostGrapeEditor::updateHint (const juce::MouseEvent* e)
{
    if (e != nullptr)
    {
        lastMouse = e->getEventRelativeTo (&content).position.toInt();
        mouseInside = true;
    }

    juce::String key, label;
    bool found = false;
    if (mouseInside && content.getLocalBounds().contains (lastMouse))
    {
        for (auto* c = content.getComponentAt (lastMouse); c != nullptr && c != &content && ! found; c = c->getParentComponent())
        {
            if (auto* hs = dynamic_cast<HintSource*> (c))
                found = hs->getHint (c->getLocalPoint (&content, lastMouse), key, label);
            if (! found)
                found = getFixedHint (*c, key, label);
        }
    }

    const auto text = found ? hintText (key, lang()) : juce::String();
    if (found && text.isNotEmpty())
        hintBar.setHint (label, text);
    else
        hintBar.setHint ({}, idleHint (lang()));
}

void PostGrapeEditor::HintBar::setHint (const juce::String& l, const juce::String& t)
{
    if (l != label || t != text)
    {
        label = l;
        text = t;
        repaint();
    }
}

juce::Rectangle<float> PostGrapeEditor::HintBar::latencyArea() const
{
    const auto f = font (11.0f * z, Weight::ExtraBold).withExtraKerningFactor (0.06f);
    const float vw = textWidth (font (10.5f * z, Weight::ExtraBold), "v" + pg::UpdateChecker::currentVersion()) + 12.0f * z;
    const float w = textWidth (f, "LATENCY 000 smp") + 20.0f * z;
    const auto r = getLocalBounds().toFloat();
    return { r.getRight() - vw - w - 6.0f * z, r.getY() + 3.0f * z, w, r.getHeight() - 6.0f * z };
}

void PostGrapeEditor::HintBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffdf6fb), 0, r.getY(), juce::Colour (0xfff3e3f0), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    g.setColour (col::line2);
    g.drawRoundedRectangle (r, rad, 1.0f);

    drawImageW (g, Images::get().grapes, r.getX() + 8.0f * z, r.getCentreY() - 9.0f * z, 18.0f * z);

    // Latency chip and version on the right.
    const auto chip = latencyArea();
    g.setColour (juce::Colour (0xfff3e3f0));
    g.fillRoundedRectangle (chip, 9.0f * z);
    g.setColour (col::line);
    g.drawRoundedRectangle (chip.reduced (0.5f), 9.0f * z, 1.0f);
    const auto f = font (11.0f * z, Weight::ExtraBold).withExtraKerningFactor (0.06f);
    const juce::String a = "LATENCY ", b = juce::String (latency) + " smp";
    const float wa = textWidth (f, a), wb = textWidth (f, b);
    const float tx = chip.getCentreX() - (wa + wb) * 0.5f;
    g.setFont (f);
    g.setColour (col::muted);
    g.drawText (a, juce::Rectangle<float> (tx, chip.getY(), wa + 2.0f, chip.getHeight()), juce::Justification::centredLeft, false);
    g.setColour (latency == 0 ? juce::Colour (0xff4f9a45) : col::hot);
    g.drawText (b, juce::Rectangle<float> (tx + wa, chip.getY(), wb + 2.0f, chip.getHeight()), juce::Justification::centredLeft, false);
    g.setColour (col::muted);
    g.setFont (font (10.5f * z, Weight::ExtraBold).withExtraKerningFactor (0.06f));
    g.drawText ("v" + pg::UpdateChecker::currentVersion(), juce::Rectangle<float> (chip.getRight(), r.getY(), r.getRight() - chip.getRight(), r.getHeight()),
                juce::Justification::centred, false);

    // Hint text: label (bold, pink) — text.
    float x = r.getX() + 32.0f * z;
    const float right = chip.getX() - 8.0f * z;
    if (label.isNotEmpty())
    {
        const auto lf = font (12.0f * z, Weight::Black).withExtraKerningFactor (0.06f);
        const auto l = label + juce::String::fromUTF8 (" \xe2\x80\x94 ");
        const float lw = textWidth (lf, l);
        g.setFont (lf);
        g.setColour (col::hot);
        g.drawText (label, juce::Rectangle<float> (x, r.getY(), lw, r.getHeight()), juce::Justification::centredLeft, false);
        g.setColour (col::deep);
        g.drawText (juce::String::fromUTF8 ("\xe2\x80\x94"), juce::Rectangle<float> (x + textWidth (lf, label + " "), r.getY(), 20.0f * z, r.getHeight()),
                    juce::Justification::centredLeft, false);
        x += lw + 2.0f * z;
    }
    g.setColour (col::deep);
    g.setFont (font (12.0f * z, Weight::Bold));
    g.drawText (text, juce::Rectangle<float> (x, r.getY(), juce::jmax (0.0f, right - x), r.getHeight()), juce::Justification::centredLeft, true);
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
                            (ModulePanel*) &panPanel, (ModulePanel*) &lofiPanel, (ModulePanel*) &limPanel, (ModulePanel*) &agPanel })
        p->refresh();

    const auto text = proc.presets.currentName() + (proc.presets.isDirty() ? "*" : "");
    if (text != lastPresetText)
    {
        lastPresetText = text;
        presetBox.repaint();
    }
    hintBar.setLatency (proc.getLatencySamples());

    // Update check
    const auto st = updates->status();
    updateBtn.setBadge (st == pg::UpdateChecker::Status::Available);
    if (st != lastUpdateStatus)
    {
        lastUpdateStatus = st;
        const bool busy = st == pg::UpdateChecker::Status::Checking || st == pg::UpdateChecker::Status::Downloading;
        if (st == pg::UpdateChecker::Status::Downloaded)
            updates->downloadedFile().revealToUser();
        if (updateDialogPending && ! busy)
        {
            updateDialogPending = false;
            showUpdateResult();
        }
    }
}

// ---------------------------------------------------------------- updates
void PostGrapeEditor::onUpdateButton()
{
    using S = pg::UpdateChecker::Status;
    const auto st = updates->status();
    if (st == S::Available || st == S::Downloaded)
    {
        showUpdateResult();
        return;
    }
    updateDialogPending = true;
    if (st != S::Checking && st != S::Downloading)
    {
        settings->setLastUpdateCheck (juce::Time::currentTimeMillis());
        updates->startCheck();
    }
}

void PostGrapeEditor::showUpdateResult()
{
    using S = pg::UpdateChecker::Status;
    const auto cur = pg::UpdateChecker::currentVersion();
    const auto latest = updates->latestVersion();
    auto openReleasePage = [this] {
        const auto url = updates->releaseUrl();
        juce::URL (url.isNotEmpty() ? url : "https://github.com/" + juce::String (pg::UpdateChecker::kRepo) + "/releases").launchInDefaultBrowser();
    };

   #if JUCE_MAC
    const juce::String install = "Close the DAW. Open the zip file. Copy Post-Grape.vst3 to ~/Library/Audio/Plug-Ins/VST3 "
                                 "and Post-Grape.component to ~/Library/Audio/Plug-Ins/Components. Replace the old files.";
   #else
    const juce::String install = "Close the DAW, then extract the Post-Grape folder to C:\\Program Files\\Common Files\\VST3 "
                                 "and replace the old file.";
   #endif

    switch (updates->status())
    {
        case S::Available:
            if (updates->assetName().isEmpty())
            {
                showMessage ("Update available",
                             "Post-Grape v" + latest + " is available. You have v" + cur + ".\n\n"
                             "This release has no file for your system yet. Open the release page.",
                             { { "Release notes", openReleasePage }, { "Later", nullptr } }, true);
                break;
            }
            showMessage ("Update available",
                         "Post-Grape v" + latest + " is available. You have v" + cur + ".\n\n"
                         "Click Download. The plugin saves the zip file to the Downloads folder and opens the folder. " + install,
                         { { "Download", [this] { updateDialogPending = true; updates->startDownload(); } },
                           { "Release notes", openReleasePage },
                           { "Later", nullptr } },
                         true);
            break;
        case S::UpToDate:
            showMessage ("No update", "You have the latest version (v" + cur + ").", { { "OK", nullptr } }, true);
            break;
        case S::Downloaded:
            showMessage ("Download complete",
                         "Saved: " + updates->downloadedFile().getFullPathName() + "\n\n" + install,
                         { { "Open folder", [this] { updates->downloadedFile().revealToUser(); } }, { "OK", nullptr } });
            break;
        case S::DownloadError:
            showMessage ("Update", updates->errorText() + "\nOpen the release page and download the file manually.",
                         { { "Release notes", openReleasePage }, { "OK", nullptr } });
            break;
        case S::Error:
            showMessage ("Update", updates->errorText() + "\nCheck the internet connection.",
                         { { "Retry", [this] { onUpdateButton(); } }, { "OK", nullptr } }, true);
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------- dialogs
void PostGrapeEditor::showMessage (const juce::String& title, const juce::String& message,
                                   std::vector<std::pair<juce::String, std::function<void()>>> buttons, bool withAutoCheck)
{
    auto* w = new juce::AlertWindow (title, message, juce::MessageBoxIconType::NoIcon, &content);
    w->setLookAndFeel (&lnf);
    if (withAutoCheck)
    {
        autoCheckToggle.setToggleState (settings->autoCheckUpdates(), juce::dontSendNotification);
        w->addCustomComponent (&autoCheckToggle);
    }
    for (int i = 0; i < (int) buttons.size(); ++i)
    {
        w->addButton (buttons[(size_t) i].first, i + 1,
                      i == 0 ? juce::KeyPress (juce::KeyPress::returnKey) : juce::KeyPress(),
                      i == (int) buttons.size() - 1 ? juce::KeyPress (juce::KeyPress::escapeKey) : juce::KeyPress());
        if (i == 0 && buttons.size() > 1)
            if (auto* b = w->getButton (i))
                b->getProperties().set ("primary", true);
    }
    content.addAndMakeVisible (w);
    w->setCentrePosition (content.getLocalBounds().getCentre());
    w->enterModalState (true, juce::ModalCallbackFunction::create ([buttons] (int result) {
                            if (result >= 1 && result <= (int) buttons.size() && buttons[(size_t) result - 1].second)
                                buttons[(size_t) result - 1].second();
                        }),
                        true);
}

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
    if (onOk == nullptr)
    {
        showMessage (title, message, { { okText, nullptr } });
        return;
    }
    showMessage (title, message, { { okText, std::move (onOk) }, { "Cancel", nullptr } });
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

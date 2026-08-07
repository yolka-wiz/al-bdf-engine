// MIT License
//
// Copyright (c) 2018-2025 Jakub Melka and Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// ALBDF FORK DIVERGENCE (2026-08-07): Text-to-speech is compiled out.
//
// The upstream file used Qt6::TextToSpeech, which is not provisioned in the
// albdf toolchain (no qt6-texttospeech-dev package on the build box, and the
// GUI is an optional build target). Rather than link an unavailable module,
// this TU provides a no-op implementation of the same class API:
//   - isValid() returns false, so the sidebar Speech page is hidden
//   - initializeUI() populates the controls with a "disabled" state
//   - all playback/settings methods are inert
// Keep the class shape identical to upstream so the rest of the GUI compiles
// unchanged; if Qt6::TextToSpeech is later provisioned, restore this file from
// upstream tag v1.6.0.0 (src/Pdf4QtLibGui/pdftexttospeech.cpp).

#include "pdftexttospeech.h"

#include "pdfviewersettings.h"
#include "pdfdrawspacecontroller.h"
#include "pdfcompiler.h"
#include "pdfdrawwidget.h"

#include <QLabel>
#include <QAction>
#include <QSlider>
#include <QComboBox>
#include <QToolButton>
#include <QTextBrowser>

#include "pdfdbgheap.h"

namespace pdfviewer
{

PDFTextToSpeech::PDFTextToSpeech(QObject* parent)
    : QObject(parent)
    , m_textToSpeech(nullptr)
    , m_document(nullptr)
    , m_proxy(nullptr)
    , m_state(Invalid)
    , m_initialized(false)
    , m_speechLocaleComboBox(nullptr)
    , m_speechVoiceComboBox(nullptr)
    , m_speechRateEdit(nullptr)
    , m_speechVolumeEdit(nullptr)
    , m_speechPitchEdit(nullptr)
    , m_speechPlayButton(nullptr)
    , m_speechPauseButton(nullptr)
    , m_speechStopButton(nullptr)
    , m_speechSynchronizeButton(nullptr)
    , m_speechRateValueLabel(nullptr)
    , m_speechPitchValueLabel(nullptr)
    , m_speechVolumeValueLabel(nullptr)
    , m_speechActualTextBrowser(nullptr)
{
    // No-op: text to speech is not compiled into albdf (fork divergence).
}

bool PDFTextToSpeech::isValid() const
{
    return false;
}

void PDFTextToSpeech::setDocument(const pdf::PDFModifiedDocument& document)
{
    Q_UNUSED(document)
    // No-op.
}

void PDFTextToSpeech::setSettings(const PDFViewerSettings* viewerSettings)
{
    Q_UNUSED(viewerSettings)
    // No-op.
}

void PDFTextToSpeech::setProxy(pdf::PDFDrawWidgetProxy* proxy)
{
    Q_UNUSED(proxy)
    // No-op.
}

void PDFTextToSpeech::initializeUI(QComboBox* speechLocaleComboBox,
                                   QComboBox* speechVoiceComboBox,
                                   QSlider* speechRateEdit,
                                   QSlider* speechPitchEdit,
                                   QSlider* speechVolumeEdit,
                                   QToolButton* speechPlayButton,
                                   QToolButton* speechPauseButton,
                                   QToolButton* speechStopButton,
                                   QToolButton* speechSynchronizeButton,
                                   QLabel* speechRateValueLabel,
                                   QLabel* speechPitchValueLabel,
                                   QLabel* speechVolumeValueLabel,
                                   QTextBrowser* speechActualTextBrowser)
{
    m_speechLocaleComboBox = speechLocaleComboBox;
    m_speechVoiceComboBox = speechVoiceComboBox;
    m_speechRateEdit = speechRateEdit;
    m_speechVolumeEdit = speechVolumeEdit;
    m_speechPitchEdit = speechPitchEdit;
    m_speechPlayButton = speechPlayButton;
    m_speechPauseButton = speechPauseButton;
    m_speechStopButton = speechStopButton;
    m_speechSynchronizeButton = speechSynchronizeButton;
    m_speechRateValueLabel = speechRateValueLabel;
    m_speechPitchValueLabel = speechPitchValueLabel;
    m_speechVolumeValueLabel = speechVolumeValueLabel;
    m_speechActualTextBrowser = speechActualTextBrowser;

    // Populate controls with a disabled "no engine" state.
    if (m_speechLocaleComboBox)
    {
        m_speechLocaleComboBox->addItem(tr("Unavailable"), QString());
        m_speechLocaleComboBox->setEnabled(false);
    }
    if (m_speechVoiceComboBox)
    {
        m_speechVoiceComboBox->addItem(tr("Unavailable"), QString());
        m_speechVoiceComboBox->setEnabled(false);
    }
    if (m_speechRateEdit)
    {
        m_speechRateEdit->setEnabled(false);
    }
    if (m_speechPitchEdit)
    {
        m_speechPitchEdit->setEnabled(false);
    }
    if (m_speechVolumeEdit)
    {
        m_speechVolumeEdit->setEnabled(false);
    }
    if (m_speechPlayButton)
    {
        m_speechPlayButton->setEnabled(false);
    }
    if (m_speechPauseButton)
    {
        m_speechPauseButton->setEnabled(false);
    }
    if (m_speechStopButton)
    {
        m_speechStopButton->setEnabled(false);
    }
    if (m_speechSynchronizeButton)
    {
        m_speechSynchronizeButton->setEnabled(false);
    }
    if (m_speechActualTextBrowser)
    {
        m_speechActualTextBrowser->setPlainText(tr("Text to speech is not available in this build."));
        m_speechActualTextBrowser->setEnabled(false);
    }

    m_initialized = true;
}

void PDFTextToSpeech::updateUI()
{
    // No-op.
}

void PDFTextToSpeech::stop()
{
    // No-op.
}

void PDFTextToSpeech::setLocale(const QString& locale)
{
    Q_UNUSED(locale)
    // No-op.
}

void PDFTextToSpeech::setVoice(const QString& voice)
{
    Q_UNUSED(voice)
    // No-op.
}

void PDFTextToSpeech::setRate(const double rate)
{
    Q_UNUSED(rate)
    // No-op.
}

void PDFTextToSpeech::setPitch(const double pitch)
{
    Q_UNUSED(pitch)
    // No-op.
}

void PDFTextToSpeech::setVolume(const double volume)
{
    Q_UNUSED(volume)
    // No-op.
}

void PDFTextToSpeech::onLocaleChanged()
{
    // No-op.
}

void PDFTextToSpeech::onVoiceChanged()
{
    // No-op.
}

void PDFTextToSpeech::onRateChanged(int rate)
{
    Q_UNUSED(rate)
    // No-op.
}

void PDFTextToSpeech::onPitchChanged(int pitch)
{
    Q_UNUSED(pitch)
    // No-op.
}

void PDFTextToSpeech::onVolumeChanged(int volume)
{
    Q_UNUSED(volume)
    // No-op.
}

void PDFTextToSpeech::onPlayClicked()
{
    // No-op.
}

void PDFTextToSpeech::onPauseClicked()
{
    // No-op.
}

void PDFTextToSpeech::onStopClicked()
{
    // No-op.
}

void PDFTextToSpeech::updatePlay()
{
    // No-op.
}

void PDFTextToSpeech::updateVoices()
{
    // No-op.
}

void PDFTextToSpeech::updateToNextPage(pdf::PDFInteger pageIndex)
{
    Q_UNUSED(pageIndex)
    // No-op.
}

}   // namespace pdfviewer

#include "../include/info_cluster.h"

#include "../include/engine_sim_application.h"
#include "../include/ui_button.h"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <string>

InfoCluster::InfoCluster() {
    m_engine = nullptr;
    m_logMessage = "Started";

    m_loadEngineButton = nullptr;
    m_reloadButton = nullptr;
    m_loadThemeButton = nullptr;
    m_exitButton = nullptr;
}

InfoCluster::~InfoCluster() {
    /* void */
}

void InfoCluster::initialize(EngineSimApplication *app) {
    UiElement::initialize(app);

    m_loadEngineButton = addElement<UiButton>(this);
    m_loadEngineButton->m_text = "LOAD ENGINE";

    m_reloadButton = addElement<UiButton>(this);
    m_reloadButton->m_text = "RELOAD";

    m_loadThemeButton = addElement<UiButton>(this);
    m_loadThemeButton->m_text = "LOAD THEME";

    m_exitButton = addElement<UiButton>(this);
    m_exitButton->m_text = "EXIT";
}

void InfoCluster::destroy() {
    UiElement::destroy();
}

void InfoCluster::update(float dt) {
    // Button row along the bottom of the title panel
    const Bounds titleBounds = getTitleBounds();
    const Bounds buttonRow = titleBounds.inset(10.0f);
    const float rowHeight = titleBounds.height() * 0.24f;

    Grid buttonGrid;
    buttonGrid.h_cells = 4;
    buttonGrid.v_cells = 1;

    UiButton *buttons[] = { m_loadEngineButton, m_reloadButton, m_loadThemeButton, m_exitButton };
    for (int i = 0; i < 4; ++i) {
        const Bounds cell = buttonGrid.get(
            Bounds(buttonRow.width(), rowHeight, buttonRow.getPosition(Bounds::bl), Bounds::bl), i, 0);
        buttons[i]->m_bounds = cell.inset(4.0f);
        buttons[i]->m_fontSize = rowHeight * 0.55f;
    }

    UiElement::update(dt);
}

void InfoCluster::signal(UiElement *element, Event event) {
    if (event != Event::Clicked) return;

    if (element == m_loadEngineButton) {
        m_app->requestAction(EngineSimApplication::UserAction::LoadEngine);
    }
    else if (element == m_reloadButton) {
        m_app->requestAction(EngineSimApplication::UserAction::Reload);
    }
    else if (element == m_loadThemeButton) {
        m_app->requestAction(EngineSimApplication::UserAction::LoadTheme);
    }
    else if (element == m_exitButton) {
        m_app->requestAction(EngineSimApplication::UserAction::Exit);
    }
}

Bounds InfoCluster::getTitleBounds() const {
    Grid grid;
    grid.h_cells = 6;
    grid.v_cells = 4;

    return grid.get(m_bounds, 1, 0, 5, 2);
}

void InfoCluster::render() {
    Grid grid;
    grid.h_cells = 6;
    grid.v_cells = 4;

    const Bounds logoBounds = grid.get(m_bounds, 0, 0, 1, 2);
    drawFrame(logoBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    drawModel(
        m_app->getAssetManager()->GetModelAsset("Logo"),
        m_app->getForegroundColor(),
        logoBounds.getPosition(Bounds::center),
        Point(logoBounds.height(), logoBounds.height()) * 0.75f);

    const Bounds titleBounds = getTitleBounds();
    drawFrame(titleBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    // Text above the button row
    const Bounds titleTextBounds(
        titleBounds.width(),
        titleBounds.height() * 0.74f,
        titleBounds.getPosition(Bounds::tl),
        Bounds::tl);

    // The three lines are sized for a full-size window. In a smaller one
    // they shrink together, so they stay inside the panel and clear of the
    // buttons instead of running over both.
    const Bounds textArea = titleTextBounds.inset(6.0f);
    const std::string lines[] = {
        "ENGINE SIMULATOR",
        "YOUTUBE/ANGETHEGREAT",
        "BUILD: v" + EngineSimApplication::getBuildVersion() + " // " __DATE__
    };
    const float sizes[] = { 42.0f, 24.0f, 16.0f };
    const float gap = 6.0f;

    // drawAlignedText puts the top of the text 0.75 of its size above the
    // baseline, and the lines are capitals, so that is the height each takes
    const float lineHeight = 0.75f;

    float stackHeight = 2 * gap;
    float widest = 0.0f;
    for (int i = 0; i < 3; ++i) {
        stackHeight += sizes[i] * lineHeight;
        widest = (std::max)(widest, m_app->getTextRenderer()->CalculateWidth(lines[i], sizes[i]));
    }

    float scale = 1.0f;
    if (stackHeight > 0.0f) scale = (std::min)(scale, textArea.height() / stackHeight);
    if (widest > 0.0f) scale = (std::min)(scale, textArea.width() / widest);
    scale = (std::max)(scale, 0.0f);

    // Centred vertically in the space above the buttons
    float offset = (textArea.height() - stackHeight * scale) / 2.0f;
    for (int i = 0; i < 3; ++i) {
        Bounds lineArea = textArea;
        lineArea.move({ 0.0f, -offset });
        drawAlignedText(
            lines[i],
            lineArea,
            sizes[i] * scale,
            Bounds::tl,
            Bounds::tl);
        offset += (sizes[i] * lineHeight + gap) * scale;
    }

    const Bounds engineInfoBounds = grid.get(m_bounds, 0, 2, 6, 1);
    drawFrame(engineInfoBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    drawAlignedText(
        (m_engine != nullptr) ? m_engine->getName() : "<NO ENGINE>",
        engineInfoBounds.inset(10.0f),
        24.0f,
        Bounds::lm,
        Bounds::lm);

    std::stringstream ss;
    if (m_engine != nullptr) {
        ss << std::fixed;

        if (m_engine->getDisplacement() < units::volume(1.0, units::L)) {
            ss << std::setprecision(0) << units::convert(m_engine->getDisplacement(), units::cc) << " cc -- ";
        }
        else {
            ss << std::setprecision(1) << units::convert(m_engine->getDisplacement(), units::L) << " L -- ";
        }

        ss << std::setprecision(0) << units::convert(m_engine->getDisplacement(), units::cubic_inches) << " CI";
    }
    else {
        ss << "N/A";
    }

    drawAlignedText(
        ss.str(),
        engineInfoBounds.inset(10.0f),
        24.0f,
        Bounds::rm,
        Bounds::rm);

    const Bounds infoMessagesBounds = grid.get(m_bounds, 0, 3, 6, 1);
    drawFrame(infoMessagesBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    drawAlignedText(
        m_logMessage,
        infoMessagesBounds.inset(10.0f),
        24.0f,
        Bounds::lm,
        Bounds::lm);

    UiElement::render();
}

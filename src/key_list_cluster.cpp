#include "../include/key_list_cluster.h"

#include "../include/engine_sim_application.h"
#include "../include/key_list.h"

#include <algorithm>

KeyListCluster::KeyListCluster() {
    /* void */
}

KeyListCluster::~KeyListCluster() {
    /* void */
}

void KeyListCluster::render() {
    const ysVector foreground = m_app->getForegroundColor();
    const ysVector background = m_app->getBackgroundColor();

    drawFrame(m_bounds, 1.0f, foreground, background);
    const Bounds inner = m_bounds.inset(10.0f);

    const float headerHeight = (std::min)(48.0f, inner.height() * 0.1f);
    const Bounds header(inner.width(), headerHeight, inner.getPosition(Bounds::tl), Bounds::tl);
    drawAlignedText("KEYS", header.inset(6.0f), 32.0f, Bounds::lm, Bounds::lm);
    drawAlignedText("F1 OR THE KEYS BUTTON CLOSES THIS", header.inset(6.0f), 16.0f, Bounds::rm, Bounds::rm);

    const Bounds body(
        inner.width(), inner.height() - headerHeight, inner.getPosition(Bounds::bl), Bounds::bl);

    const std::vector<key_list::Group> &groups = key_list::groups();

    // Every group gets as many rows as the longest one, so the rows line up
    // across the columns
    size_t rows = 0;
    for (const key_list::Group &group : groups) rows = (std::max)(rows, group.keys.size());
    const float rowCount = static_cast<float>(rows) + 1.5f;

    Grid grid;
    grid.h_cells = 2;
    grid.v_cells = static_cast<int>((groups.size() + 1) / 2);

    auto cellArea = [&](size_t i) {
        return grid.get(body, static_cast<int>(i % 2), static_cast<int>(i / 2)).inset(4.0f);
    };

    // One text size for every entry: the largest at which all of them fit
    const Bounds firstArea = cellArea(0).inset(8.0f);
    const float rowHeight = firstArea.height() / rowCount;
    const float keyWidth = firstArea.width() * 0.32f;
    const float actionWidth = firstArea.width() - keyWidth;
    float textSize = (std::min)(20.0f, rowHeight * 0.6f);
    for (const key_list::Group &group : groups) {
        for (const key_list::Key &key : group.keys) {
            textSize = (std::min)(textSize, fitTextHeight(key.keys, keyWidth - 8.0f, textSize));
            textSize = (std::min)(textSize, fitTextHeight(key.action, actionWidth - 4.0f, textSize));
        }
    }

    for (size_t i = 0; i < groups.size(); ++i) {
        const Bounds cell = cellArea(i);
        drawFrame(cell, 1.0f, foreground, background);

        const Bounds area = cell.inset(8.0f);
        auto row = [&](float index) {
            return Bounds(
                area.width(), rowHeight,
                area.getPosition(Bounds::tl) + Point(0.0f, -index * rowHeight), Bounds::tl);
        };

        drawAlignedText(groups[i].title, row(0.0f), textSize * 1.2f, Bounds::lm, Bounds::lm);

        for (size_t k = 0; k < groups[i].keys.size(); ++k) {
            const Bounds line = row(static_cast<float>(k) + 1.5f);
            const Bounds keyBounds(keyWidth, rowHeight, line.getPosition(Bounds::tl), Bounds::tl);
            const Bounds actionBounds(
                line.width() - keyWidth, rowHeight, line.getPosition(Bounds::tr), Bounds::tr);

            drawAlignedText(groups[i].keys[k].keys, keyBounds.inset(2.0f), textSize, Bounds::lm, Bounds::lm);
            drawAlignedText(groups[i].keys[k].action, actionBounds.inset(2.0f), textSize, Bounds::lm, Bounds::lm);
        }
    }
}

#pragma once

namespace UiPanels {
void beginCenteredCard(const char *name, float windowWidth, float windowHeight,
                       float cardWidth, float cardHeight, bool *open = nullptr);
void endCenteredCard();
void controlsList();
} // namespace UiPanels

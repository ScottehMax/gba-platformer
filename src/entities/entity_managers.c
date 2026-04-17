#include "entity_managers.h"

void initEntityManagers(EntityManagers* em) {
    initSpringManager(&em->springs);
    initRedBubbleManager(&em->redBubbles);
    initGreenBubbleManager(&em->greenBubbles);
}

void loadEntitiesFromLevel(EntityManagers* em, const Level* level) {
    loadSpringsFromLevel(&em->springs, level);
    loadRedBubblesFromLevel(&em->redBubbles, level);
    loadGreenBubblesFromLevel(&em->greenBubbles, level);
}

void updateEntities(EntityManagers* em, Player* player) {
    updateSprings(&em->springs, player);
    updateRedBubbles(&em->redBubbles, player);
    updateGreenBubbles(&em->greenBubbles, player);
}

void renderEntities(const EntityManagers* em, int cameraX, int cameraY) {
    renderSprings(&em->springs, cameraX, cameraY);
    renderRedBubbles(&em->redBubbles, cameraX, cameraY);
    renderGreenBubbles(&em->greenBubbles, cameraX, cameraY);
}

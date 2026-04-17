#ifndef ENTITY_MANAGERS_H
#define ENTITY_MANAGERS_H

#include "spring.h"
#include "redbubble.h"
#include "greenbubble.h"

typedef struct {
    SpringManager      springs;
    RedBubbleManager   redBubbles;
    GreenBubbleManager greenBubbles;
} EntityManagers;

void initEntityManagers(EntityManagers* em);
void loadEntitiesFromLevel(EntityManagers* em, const Level* level);
void updateEntities(EntityManagers* em, Player* player);
void renderEntities(const EntityManagers* em, int cameraX, int cameraY);

#endif

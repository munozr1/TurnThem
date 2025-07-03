#include "mouse_handler.h"
#include "common.h"

void HandleMouseInput(MouseState& mouse_state, Deck* deck) {
    switch (mouse_state) {
        case MouseState::NORMAL:
            if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 mouse_pos = GetMousePosition();
                selected_card = deck->getSelectedCard(mouse_pos);
                if (selected_card != nullptr){ 
                    mouse_state = MouseState::DRAGGING;
                    selected_card->set_dragging(true);
                }
            }
            break;
            
        case MouseState::DRAGGING:{
                if (selected_card == nullptr) {
                    mouse_state = MouseState::NORMAL;
                    break;
                }

                if(selected_block != nullptr) selected_block->tint = BlockTint::none;
                
                auto mouse_pos = GetMousePosition();
                selected_card->setxyDrag(mouse_pos);
                auto normalized_mouse_pos = NormalizeCoordinate(mouse_pos);

                auto it = grid_map.find(normalized_mouse_pos);
                selected_block = it->second;
                if (selected_block != nullptr) {
                    // check if position is valid and set appropriate tint
                    if (IsValidWeaponPosition(mouse_pos)) {
                        selected_block->tint = BlockTint::green;
                    } else {
                        selected_block->tint = BlockTint::red;
                    }
                }
                
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
                    mouse_state = MouseState::DROP;
                }
                break;
            }
            
        case MouseState::DROP:{
                if (selected_card == nullptr) {
                    mouse_state = MouseState::NORMAL;
                    break;
                }

                if (selected_block != nullptr) selected_block->tint = BlockTint::none;
                selected_card->set_dragging(false);
                Vector2 mouse_pos = GetMousePosition();

                if(deck->isPointInside(mouse_pos)){
                    deck->resetCardPosition(selected_card->slotId());
                } else if (selected_block != nullptr && IsValidWeaponPosition(mouse_pos)) {
                    // only place weapon if position is valid
                    auto pos = selected_block->getPos();
                    PlaceWeapon(selected_card->cardData(), pos);
                    deck->removeCard(selected_card->slotId());
                } else {
                    // invalid position, return card to deck
                    deck->resetCardPosition(selected_card->slotId());
                }
                selected_card  = nullptr;
                selected_block = nullptr;
                mouse_state = MouseState::NORMAL;
                break;
            }
            
        default:
            break;
    }
}

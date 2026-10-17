// MIT License © 2026 Binary Dice Games
/// @file uml.hpp
/// @brief The UML diagram elements (spec/03_uml_diagrams/DESIGN.md): class,
///        sequence, state, activity and use case diagrams, drawn by ImUml.
#pragma once

#include "schema/registry.hpp"

namespace bdg::nymph {

/// @brief Slots of the diagrams' children.
inline constexpr const char* kUmlClassItemSlot = "uml.class.item";
inline constexpr const char* kUmlSequenceItemSlot = "uml.sequence.item";
inline constexpr const char* kUmlStateItemSlot = "uml.state.item";
inline constexpr const char* kUmlActivityItemSlot = "uml.activity.item";
inline constexpr const char* kUmlUseCaseItemSlot = "uml.usecase.item";
/// @brief `Note`, accepted by every diagram (and `Fragment`, `State`).
inline constexpr const char* kUmlNoteSlot = "uml.note";

/// @brief Registers `ClassDiagram`, `Class`, `ClassRelation`,
///        `SequenceDiagram`, `Participant`, `Message`, `Fragment`, `Else`,
///        `StateDiagram`, `State`, `Transition`, `ActivityDiagram`, `Action`,
///        `ActivityControl`, `Flow`, `UseCaseDiagram`, `Actor`, `UseCase`,
///        `UseCaseRelation` and `Note`.
void register_uml(registry& reg);

} // namespace bdg::nymph

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
inline constexpr const char* kUmlFlowItemSlot = "uml.flow.item";
inline constexpr const char* kUmlErItemSlot = "uml.er.item";
inline constexpr const char* kUmlDataFlowItemSlot = "uml.dataflow.item";
inline constexpr const char* kUmlNetworkItemSlot = "uml.network.item";
inline constexpr const char* kUmlComponentItemSlot = "uml.component.item";
inline constexpr const char* kUmlDeploymentItemSlot = "uml.deployment.item";
inline constexpr const char* kUmlPackageItemSlot = "uml.package.item";
inline constexpr const char* kUmlObjectItemSlot = "uml.object.item";
inline constexpr const char* kUmlTreeItemSlot = "uml.tree.item";
inline constexpr const char* kUmlMindItemSlot = "uml.mind.item";
inline constexpr const char* kUmlMindLinkSlot = "uml.mind.link";
inline constexpr const char* kUmlOrgItemSlot = "uml.org.item";
inline constexpr const char* kUmlOrgLinkSlot = "uml.org.link";
/// @brief `Note`, accepted by every diagram (and `Fragment`, `State`).
inline constexpr const char* kUmlNoteSlot = "uml.note";

/// @brief Registers `ClassDiagram`, `Class`, `ClassRelation`,
///        `SequenceDiagram`, `Participant`, `Message`, `Fragment`, `Else`,
///        `StateDiagram`, `State`, `Transition`, `ActivityDiagram`, `Action`,
///        `ActivityControl`, `Flow`, `UseCaseDiagram`, `Actor`, `UseCase`,
///        `UseCaseRelation`, `Flowchart`, `FlowNode`, `FlowGroup`, `FlowLink`,
///        `ERDiagram`, `Entity`, `Relationship`, `DataFlowDiagram`,
///        `ExternalEntity`, `Process`, `DataStore`, `DataFlow`,
///        `TrustBoundary`, `NetworkDiagram`, `Device`, `Link`, `Zone` and
///        `Note`.
void register_uml(registry& reg);

} // namespace bdg::nymph

#include "AppearanceSnapshot.h"

namespace DAVSyncTogether
{
    bool AppearanceSnapshot::Empty() const noexcept
    {
        return faceMorphs.empty() &&
               bodyMorphs.empty() &&
               faceOverlays.empty() &&
               bodyOverlays.empty() &&
               headParts.empty() &&
               skinIdentity.empty() &&
               bodyTextureSet.empty();
    }

    std::string AppearanceSnapshot::Summary() const
    {
        return fmt::format(
            "player={} race={} faceMorphs={} bodyMorphs={} faceOverlays={} bodyOverlays={} headParts={} skin={} textures={}",
            playerName.empty() ? "<unknown>" : playerName,
            raceEditorID.empty() ? "<unknown>" : raceEditorID,
            faceMorphs.size(),
            bodyMorphs.size(),
            faceOverlays.size(),
            bodyOverlays.size(),
            headParts.size(),
            skinIdentity.empty() ? "<none>" : skinIdentity,
            bodyTextureSet.empty() ? "<none>" : bodyTextureSet);
    }
}

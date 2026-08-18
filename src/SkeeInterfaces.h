#pragma once

#include "PCH.h"

namespace DAVSyncTogether::SKEE
{
    using u64 = std::uint64_t;
    using u32 = std::uint32_t;

    class IPluginInterface
    {
    public:
        virtual ~IPluginInterface() = default;
        virtual u32 GetVersion() = 0;
        virtual void Revert() = 0;
    };

    class IInterfaceMap
    {
    public:
        virtual IPluginInterface* QueryInterface(const char* name) = 0;
        virtual bool AddInterface(const char* name, IPluginInterface* pluginInterface) = 0;
        virtual IPluginInterface* RemoveInterface(const char* name) = 0;
    };

    struct InterfaceExchangeMessage
    {
        static constexpr std::uint32_t kMessageExchangeInterface = 0x9E3779B9;
        IInterfaceMap* interfaceMap{ nullptr };
    };

    class IBodyMorphInterface : public IPluginInterface
    {
    public:
        class MorphKeyVisitor
        {
        public:
            virtual void Visit(const char*, float) = 0;
        };

        class StringVisitor
        {
        public:
            virtual void Visit(const char*) = 0;
        };

        class ActorVisitor
        {
        public:
            virtual void Visit(RE::TESObjectREFR*) = 0;
        };

        class MorphValueVisitor
        {
        public:
            virtual void Visit(RE::TESObjectREFR*, const char*, const char*, float) = 0;
        };

        class MorphVisitor
        {
        public:
            virtual void Visit(RE::TESObjectREFR*, const char*) = 0;
        };

        virtual void SetMorph(RE::TESObjectREFR*, const char*, const char*, float) = 0;
        virtual float GetMorph(RE::TESObjectREFR*, const char*, const char*) = 0;
        virtual void ClearMorph(RE::TESObjectREFR*, const char*, const char*) = 0;
        virtual float GetBodyMorphs(RE::TESObjectREFR*, const char*) = 0;
        virtual void ClearBodyMorphNames(RE::TESObjectREFR*, const char*) = 0;
        virtual void VisitMorphs(RE::TESObjectREFR*, MorphVisitor&) = 0;
        virtual void VisitKeys(RE::TESObjectREFR*, const char*, MorphKeyVisitor&) = 0;
        virtual void VisitMorphValues(RE::TESObjectREFR*, MorphValueVisitor&) = 0;
        virtual void ClearMorphs(RE::TESObjectREFR*) = 0;
        virtual void ApplyVertexDiff(RE::TESObjectREFR*, RE::NiAVObject*, bool = false) = 0;
        virtual void ApplyBodyMorphs(RE::TESObjectREFR*, bool = true) = 0;
        virtual void UpdateModelWeight(RE::TESObjectREFR*, bool = false) = 0;
        virtual void SetCacheLimit(u64) = 0;
        virtual bool HasMorphs(RE::TESObjectREFR*) = 0;
        virtual u32 EvaluateBodyMorphs(RE::TESObjectREFR*) = 0;
        virtual bool HasBodyMorph(RE::TESObjectREFR*, const char*, const char*) = 0;
        virtual bool HasBodyMorphName(RE::TESObjectREFR*, const char*) = 0;
        virtual bool HasBodyMorphKey(RE::TESObjectREFR*, const char*) = 0;
        virtual void ClearBodyMorphKeys(RE::TESObjectREFR*, const char*) = 0;
        virtual void VisitStrings(StringVisitor&) = 0;
        virtual void VisitActors(ActorVisitor&) = 0;
        virtual u64 ClearMorphCache() = 0;
    };

    class IOverlayInterface : public IPluginInterface
    {
    public:
        enum class OverlayType
        {
            Normal,
            Spell
        };

        enum class OverlayLocation
        {
            Body,
            Hand,
            Feet,
            Face
        };

        using OverlayInstallCallback = void (*)(RE::TESObjectREFR*, RE::NiAVObject*);

        virtual bool HasOverlays(RE::TESObjectREFR*) = 0;
        virtual void AddOverlays(RE::TESObjectREFR*, bool defer = true) = 0;
        virtual void RemoveOverlays(RE::TESObjectREFR*, bool defer = true) = 0;
        virtual void RevertOverlays(RE::TESObjectREFR*, bool resetDiffuse, bool defer = true) = 0;
        virtual void RevertOverlay(RE::TESObjectREFR*, const char*, u32, u32, bool, bool defer = true) = 0;
        virtual void EraseOverlays(RE::TESObjectREFR*, bool defer = true) = 0;
        virtual void RevertHeadOverlays(RE::TESObjectREFR*, bool resetDiffuse, bool defer = true) = 0;
        virtual void RevertHeadOverlay(RE::TESObjectREFR*, const char*, u32, u32, bool, bool defer = true) = 0;
        virtual u32 GetOverlayCount(OverlayType, OverlayLocation) = 0;
        virtual const char* GetOverlayFormat(OverlayType, OverlayLocation) = 0;
        virtual bool RegisterInstallCallback(const char*, OverlayInstallCallback) = 0;
        virtual bool UnregisterInstallCallback(const char*) = 0;
    };
}

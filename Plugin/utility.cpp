#include "utility.h"

#include <windows.h>

#include "JCAPI.h"

namespace Utility {
    void GetName(RE::TESObjectREFR* ref, char* buf, size_t bufSize) {
        if (!ref || !ref->GetBaseObject()) {
            std::snprintf(buf, bufSize, "%s", "unknown");
            return;
        }

        RE::BSFixedString s;

        const char* desc = ref->GetDisplayFullName();
        if (!desc || desc[0] == '\0' || strstr(desc, "not be visible")) {
            desc = ref->GetBaseObject()->GetName();
            if (!desc || desc[0] == '\0' || strstr(desc, "not be visible")) {
                JC::ObjectId markerDb = JC::GetMarkerDb();
                if (markerDb > 0) {
                    s = JC::jFormMapGetStr(JC::Domain, markerDb, ref, "");
                    desc = s.c_str();
                }
                if (!desc || desc[0] == '\0') {
                    desc = ref->GetFormEditorID();
                    if (!desc || desc[0] == '\0') {
                        desc = Utility::GetFormEditorID(ref->GetBaseObject()->GetFormID());
                    }
                }
            }
        }
        if (!desc || desc[0] == '\0') {
            desc = "unknown";
        }
        std::snprintf(buf, bufSize, "%s", desc);
    }

    const char* GetFormEditorID(std::uint32_t formId) {
        using GetFormEditorIDFn = const char* (*)(std::uint32_t);

        static auto getFormEditorID = reinterpret_cast<GetFormEditorIDFn>(
            GetProcAddress(GetModuleHandleA("po3_Tweaks.dll"), "GetFormEditorID")
        );

        return getFormEditorID ? getFormEditorID(formId) : nullptr;
    }

    void CreateTaskDescription(RE::Actor* actor, std::string& buf) {
        RE::TESDataHandler* dh = RE::TESDataHandler::GetSingleton();
        if (!dh) {
            return;
        }

        static auto* BRSS_PackageKeyword1 = dh->LookupForm<RE::BGSKeyword>(0xAA0F, "SkyrimSlavery.esp");
        static auto* BRSS_PackageKeyword2 = dh->LookupForm<RE::BGSKeyword>(0xAA10, "SkyrimSlavery.esp");
        if (!BRSS_PackageKeyword1 || !BRSS_PackageKeyword2) {
            return;
        }

        int procedureCode = -1;
        RE::ActorValueOwner* valueOwner = actor->AsActorValueOwner();
        if (valueOwner) {
            procedureCode = (int)valueOwner->GetBaseActorValue(RE::ActorValue::kVariable08);
        }
        char nameP1[128];
        char nameP2[128];
        Utility::GetName(actor->GetLinkedRef(BRSS_PackageKeyword1), nameP1, sizeof(nameP1));
        Utility::GetName(actor->GetLinkedRef(BRSS_PackageKeyword2), nameP2, sizeof(nameP2));
        switch (procedureCode) {
            case 0:
                buf.append("Idle");
                break;

            case 1:
                buf.append("Traveling to ");
                buf.append(nameP1);
                break;

            case 2:
                buf.append("Following ");
                buf.append(nameP1);
                break;

            case 3:
                buf.append("Using Idle Marker ");
                buf.append(nameP1);
                break;

            case 4:
                buf.append("Using weapon on ");
                buf.append(nameP2);
                break;

            case 6:
                buf.append("Patrolling between ");
                buf.append(nameP1);
                buf.append(" and ");
                buf.append(nameP2);
                break;

            case 7:
                buf.append("Aiming at ");
                buf.append(nameP2);
                break;

            case 8:
                buf.append("Sitting at ");
                buf.append(nameP1);
                break;

            case 9:
                buf.append("Using weapon on ");
                buf.append(nameP2);
                break;

            case 10:
                buf.append("Using Idle Marker ");
                buf.append(nameP1);
                break;

            default:
                buf.append("No action assigned");
                break;
        }
    }

    void CreateMarkerDescription(RE::TESObjectREFR* marker, std::string& buf, float distance) {
        char tmp[32];

        JC::ObjectId markerDb = JC::GetMarkerDb();
        if (markerDb == 0) {
            return;
        }

        buf.clear();

        buf.append(JC::jFormMapGetStr(JC::Domain, markerDb, marker, "unknown").c_str());
        buf.append("[");

        auto [ptr1, ec1] = std::to_chars(tmp, tmp + sizeof(tmp), marker->GetFormID(), 16);
        if (ec1 == std::errc{}) {
            buf.append(tmp, ptr1);
        } else {
            buf.append("unknown");
        }
        buf.append(", ");

        const char* desc = marker->GetDisplayFullName();
        if (!desc || desc[0] == '\0' || strstr(desc, "not be visible")) {
            desc = marker->GetBaseObject()->GetName();
            if (!desc || desc[0] == '\0' || strstr(desc, "not be visible")) {
                desc = marker->GetFormEditorID();
                if (!desc || desc[0] == '\0') {
                    desc = Utility::GetFormEditorID(marker->GetBaseObject()->GetFormID());
                }
            }
        }
        buf.append(desc ? desc : "unknown");
        buf.append(", ");

        const char* locStr = "unknown";
        RE::BGSLocation* loc = marker->GetCurrentLocation();
        if (loc) {
            const char* locName = loc->GetName();
            if (locName && locName[0] != '\0') {
                locStr = locName;
            }
        }
        buf.append(locStr);

        buf.append("(");
        auto [ptr2, ec2] = std::to_chars(tmp, tmp + sizeof(tmp), distance, std::chars_format::fixed, 2);
        if (ec2 == std::errc{}) {
            buf.append(tmp, ptr2);
        } else {
            buf.append("unknown");
        }
        buf.append(")");

        buf.append("]");
    }
}

#include "utility.h"

#include <windows.h>

#include "JCAPI.h"
#include "UI/common/builders.h"

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

    const char* GetWeaponName(RE::Actor* actor) {
        RE::TESDataHandler* dh = RE::TESDataHandler::GetSingleton();
        if (!dh) {
            return "unknown";
        }

        static auto* battleaxe = dh->LookupForm<RE::TESObjectWEAP>(0x13980, "Skyrim.esm");
        static auto* greatsword = dh->LookupForm<RE::TESObjectWEAP>(0x1359D, "Skyrim.esm");
        static auto* mace = dh->LookupForm<RE::TESObjectWEAP>(0x13982, "Skyrim.esm");
        static auto* sword = dh->LookupForm<RE::TESObjectWEAP>(0x12EB7, "Skyrim.esm");
        static auto* waraxe = dh->LookupForm<RE::TESObjectWEAP>(0x13790, "Skyrim.esm");
        static auto* warhammer = dh->LookupForm<RE::TESObjectWEAP>(0x13981, "Skyrim.esm");
        static auto* bow = dh->LookupForm<RE::TESObjectWEAP>(0x13985, "Skyrim.esm");
        static auto* crossbow = dh->LookupForm<RE::TESObjectWEAP>(0x801, "Dawnguard.esm");
        static auto* whip = dh->LookupForm<RE::TESObjectWEAP>(0x6004, "ZaZAnimationPack.esm");
        if (!battleaxe || !greatsword || !mace || !sword || !waraxe || !warhammer || !bow || !crossbow || !whip) {
            return "unknown";
        }

        for (const auto& [item, _] : actor->GetInventory()) {
            if (item == battleaxe) return "BattleAxe";
            if (item == greatsword) return "Greatsword";
            if (item == mace) return "Mace";
            if (item == sword) return "Sword";
            if (item == waraxe) return "WarAxe";
            if (item == warhammer) return "Warhammer";
            if (item == bow) return "Bow";
            if (item == crossbow) return "Crossbow";
            if (item == whip) return "Whip";
        }

        return "Unarmed";
    }

    static void ForEachReferenceInRange(RE::TES* tes, RE::TESObjectREFR* a_origin, float a_radius, std::function<RE::BSContainer::ForEachResult(RE::TESObjectREFR& a_ref)> a_callback) {
		if (a_origin && a_radius > 0.0f) {
			const auto originPos = a_origin->GetPosition();

			if (tes->interiorCell) {
				tes->interiorCell->ForEachReferenceInRange(originPos, a_radius, [&](RE::TESObjectREFR& a_ref) {
					return a_callback(a_ref);
				});
			} else {
				if (const auto gridLength = tes->gridCells ? tes->gridCells->length : 0; gridLength > 0) {
					const float yPlus = originPos.y + a_radius;
					const float yMinus = originPos.y - a_radius;
					const float xPlus = originPos.x + a_radius;
					const float xMinus = originPos.x - a_radius;

					std::uint32_t x = 0;
					do {
						std::uint32_t y = 0;
						do {
							if (const auto cell = tes->gridCells->GetCell(x, y); cell && cell->IsAttached()) {
								if (const auto cellCoords = cell->GetCoordinates(); cellCoords) {
									const RE::NiPoint2 worldPos{ cellCoords->worldX, cellCoords->worldY };
									if (worldPos.x < xPlus && (worldPos.x + 4096.0f) > xMinus && worldPos.y < yPlus && (worldPos.y + 4096.0f) > yMinus) {
										cell->ForEachReferenceInRange(originPos, a_radius, [&](RE::TESObjectREFR& a_ref) {
											return a_callback(a_ref);
										});
									}
								}
							}
							++y;
						} while (y < gridLength);
						++x;
					} while (x < gridLength);
				}
			}

            auto originCell = a_origin->GetParentCell();
            auto originWorldSpace = originCell ? originCell->GetRuntimeData().worldSpace : nullptr;
			if (const auto skyCell = originWorldSpace ? originWorldSpace->GetSkyCell() : nullptr; skyCell) {
				skyCell->ForEachReferenceInRange(originPos, a_radius, [&](RE::TESObjectREFR& a_ref) {
					return a_callback(a_ref);
				});
			}
		} else {
			tes->ForEachReference([&](RE::TESObjectREFR& a_ref) {
				return a_callback(a_ref);
			});
		}
	}

    // Copyright (c) 2021-2025 powerofthree
    // See LICENSES/MIT-powerofthree.txt
    std::vector<RE::TESObjectREFR*> FindAllReferencesOfFormTypes(
        RE::TESObjectREFR* a_origin,
        std::initializer_list<std::uint32_t> a_formTypes,
        float a_radius)
    {
        std::vector<RE::TESObjectREFR*> result;

        const auto TES = RE::TES::GetSingleton();
        if (!TES || !a_origin) {
            return result;
        }

        ForEachReferenceInRange(TES, a_origin, a_radius, [&](RE::TESObjectREFR& a_ref) {
            if (!a_ref.Is3DLoaded()) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            const auto base = a_ref.GetBaseObject();

            for (auto rawType : a_formTypes) {
                const auto formType = static_cast<RE::FormType>(rawType);

                if (formType == RE::FormType::None || a_ref.Is(formType) || (base && base->Is(formType))) {
                    result.push_back(&a_ref);
                    break;
                }
            }

            return RE::BSContainer::ForEachResult::kContinue;
        });

        return result;
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
        MarkerTableRow row = {};
        PopulateMarkerTableRow(marker, distance, row);
        FormatMarkerDescription(row, buf);
    }
}

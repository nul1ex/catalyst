#include <stdafx.hpp>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>

namespace features::skinchanger {

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    void skin_db::initialize() {
        CURL* curl = curl_easy_init();
        if (!curl) return;

        std::string readBuffer;
        curl_easy_setopt(curl, CURLOPT_URL, "https://raw.githubusercontent.com/ByMykel/CSGO-API/main/public/api/en/skins.json");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            curl_easy_cleanup(curl);
            return;
        }
        curl_easy_cleanup(curl);

        if (readBuffer.empty())
            return;

        try {
            auto jsonData = nlohmann::json::parse(readBuffer);
            
            if (!jsonData.is_array())
                return;
                
            for (auto& skin : jsonData) {
                if (!skin.is_object())
                    continue;
                    
                skin_info info;
                
                if (skin.contains("paint_index")) {
                    try {
                        if (skin["paint_index"].is_string()) {
                            info.paint_kit = std::stoi(skin["paint_index"].get<std::string>());
                        } else if (skin["paint_index"].is_number_integer()) {
                            info.paint_kit = skin["paint_index"].get<int>();
                        } else {
                            info.paint_kit = 0;
                        }
                    } catch (...) {
                        info.paint_kit = 0;
                    }
                } else {
                    info.paint_kit = 0;
                }
                
                if (skin.contains("name")) {
                    try {
                        if (skin["name"].is_string()) {
                            info.name = skin["name"].get<std::string>();
                        } else if (skin["name"].is_object() && skin["name"].contains("en")) {
                            info.name = skin["name"]["en"].get<std::string>();
                        } else {
                            info.name = "";
                        }
                    } catch (...) {
                        info.name = "";
                    }
                } else {
                    info.name = "";
                }
                
                if (skin.contains("legacy_model") && skin["legacy_model"].is_boolean()) {
                    info.uses_old_model = skin["legacy_model"].get<bool>();
                    g::console.print("Skin: {}, Legacy Model: {}", info.name.c_str(), info.uses_old_model);
                }
                auto get_weapon_enum = [](const std::string& name) -> weapons_enum {
                    if (name.find("AK-47") != std::string::npos) return weapons_enum::ak47;
                    if (name.find("M4A4") != std::string::npos) return weapons_enum::m4a4;
                    if (name.find("M4A1-S") != std::string::npos) return weapons_enum::m4a1s;
                    if (name.find("AWP") != std::string::npos) return weapons_enum::awp;
                    if (name.find("Desert Eagle") != std::string::npos) return weapons_enum::deagle;
                    if (name.find("USP-S") != std::string::npos) return weapons_enum::usps;
                    if (name.find("Glock-18") != std::string::npos) return weapons_enum::glock;
                    if (name.find("Dual Berettas") != std::string::npos) return weapons_enum::elite;
                    if (name.find("Five-SeveN") != std::string::npos) return weapons_enum::fiveseven;
                    if (name.find("P2000") != std::string::npos) return weapons_enum::p2000;
                    if (name.find("P250") != std::string::npos) return weapons_enum::p250;
                    if (name.find("Tec-9") != std::string::npos) return weapons_enum::tec9;
                    if (name.find("CZ75-Auto") != std::string::npos) return weapons_enum::cz75;
                    if (name.find("R8 Revolver") != std::string::npos) return weapons_enum::revolver;
                    if (name.find("MAC-10") != std::string::npos) return weapons_enum::mac10;
                    if (name.find("MP5-SD") != std::string::npos) return weapons_enum::mp5sd;
                    if (name.find("MP7") != std::string::npos) return weapons_enum::mp7;
                    if (name.find("MP9") != std::string::npos) return weapons_enum::mp9;
                    if (name.find("P90") != std::string::npos) return weapons_enum::p90;
                    if (name.find("UMP-45") != std::string::npos) return weapons_enum::ump45;
                    if (name.find("PP-Bizon") != std::string::npos) return weapons_enum::bizon;
                    if (name.find("Galil AR") != std::string::npos) return weapons_enum::galil;
                    if (name.find("FAMAS") != std::string::npos) return weapons_enum::famas;
                    if (name.find("AUG") != std::string::npos) return weapons_enum::aug;
                    if (name.find("SG 553") != std::string::npos) return weapons_enum::sg556;
                    if (name.find("G3SG1") != std::string::npos) return weapons_enum::g3sg1;
                    if (name.find("SCAR-20") != std::string::npos) return weapons_enum::scar20;
                    if (name.find("SSG 08") != std::string::npos) return weapons_enum::ssg08;
                    if (name.find("Nova") != std::string::npos) return weapons_enum::nova;
                    if (name.find("XM1014") != std::string::npos) return weapons_enum::xm1014;
                    if (name.find("Sawed-Off") != std::string::npos) return weapons_enum::sawedoff;
                    if (name.find("MAG-7") != std::string::npos) return weapons_enum::mag7;
                    if (name.find("M249") != std::string::npos) return weapons_enum::m249;
                    if (name.find("Negev") != std::string::npos) return weapons_enum::negev;
                    return weapons_enum::none;
                };

                info.weapon_type = get_weapon_enum(info.name);
                if (info.weapon_type != weapons_enum::none)
                    m_skins.push_back(info);
            }
        } catch (...) {}
    }

    std::vector<skin_info> skin_db::get_weapon_skins(weapons_enum type) {
        std::vector<skin_info> results;
        results.push_back({ 0, false, "Vanilla", type });
        for (const auto& skin : m_skins) {
            if (skin.weapon_type == type)
                results.push_back(skin);
        }
        return results;
    }
 
    std::vector<music_kit_info> skin_db::get_music_kits() {
        return {
            { 0, "Default" },
            { 1, "Counter-Strike 2" },
            { 2, "Valve CSGO 2 (hidden)" },
            { 3, "Crimson Assault" },
            { 4, "Sharpened" },
            { 5, "Insurgency" },
            { 6, "A_D_8" },
            { 7, "High Noon" },
            { 8, "Death's Head Demolition" },
            { 9, "Desert Fire" },
            { 10, "LNOE" },
            { 11, "Metal" },
            { 12, "All I Want For Christmas" },
            { 13, "Iso Rhythm" },
            { 14, "For No Mankind" },
            { 15, "Hotline Miami" },
            { 16, "The 8-Bit Kit" },
            { 17, "The Talos Principle" },
            { 18, "Battlepack" },
            { 19, "Molotov" },
            { 20, "Uber Blasto Phone" },
            { 21, "Hazardous Environments" },
            { 22, "Headshot" },
            { 23, "Total Domination" },
            { 24, "I Am" },
            { 25, "Diamonds" },
            { 26, "Invasion!" },
            { 27, "Lion's Mouth" },
            { 28, "Sponge Fingerz" },
            { 29, "Aggressive" },
            { 30, "Java Havana Funkaloo" },
            { 31, "Moments CSGO" },
            { 32, "Disgusting" },
            { 33, "The Good Youth" },
            { 34, "FREE" },
            { 35, "Life's Not Out To Get You" },
            { 36, "Backbone" },
            { 37, "GLA" },
            { 38, "Arena" },
            { 39, "EZ4ENCE" },
            { 40, "Halo" },
            { 41, "King, Scar" },
            { 42, "Anti-Citizen" },
            { 43, "Bachram" },
            { 44, "Taco Truck" },
            { 45, "Eye of the Dragon" },
            { 46, "M.U.D.D. FORCE" },
            { 47, "Neo Noir" },
            { 48, "Bodacious" },
            { 49, "Drifter" },
            { 50, "All For Dust" },
            { 51, "Hades" },
            { 52, "The Lowfile Pack" },
            { 53, "CHAINSAW Lxadxut" },
            { 54, "Mocha Petal" },
            { 55, "Yellow Magic" },
            { 56, "VICI" },
            { 57, "Atro Bellum" },
            { 58, "Work Hard, Play Hard" },
            { 59, "Kolibri" },
            { 60, "U Mad!" },
            { 61, "Flashbang Dance" },
            { 62, "Heading For The Source" },
            { 63, "Void" },
            { 64, "Shooters" },
            { 65, "Dashstar*" },
            { 66, "Gothic Luxury" },
            { 67, "Lock Me Up" },
            { 68, "Hua Lian" },
            { 69, "Ultimate" },
            { 76, "Under Bright Lights" },
            { 84, "CS:GO" }
        };
    }

    struct CEconItemAttribute {
        std::uintptr_t vtable; //0x0000
        std::uintptr_t owner; //0x0008
        char pad_0010[32]; //0x0010
        std::uint16_t defIndex; //0x0030
        char pad_0032[2]; //0x0032
        float value; //0x0034
        float initValue; //0x0038
        std::int32_t refundableCurrency; //0x003C
        bool setBonus; //0x0040
        char pad_0041[7]; //0x0041
    };

    struct CPtrGameVector {
        std::uint64_t size;
        std::uintptr_t ptr;
    };

    std::uintptr_t skinchanger::get_hud_arms(std::uintptr_t local_player) {
        const auto handle = g::memory.read<std::uint32_t>(local_player + SCHEMA("C_CSPlayerPawn", "m_hHudModelArms"_hash));
        return systems::g_entities.lookup(handle);
    }

    std::uintptr_t skinchanger::get_hud_weapon(std::uintptr_t local_player, std::uintptr_t weapon) {
        const auto arms = this->get_hud_arms(local_player);

        const auto node = g::memory.read<std::uintptr_t>(arms + SCHEMA("C_BaseEntity", "m_pGameSceneNode"_hash));

        const auto child_offset = SCHEMA("CGameSceneNode", "m_pChild"_hash);
        const auto sibling_offset = SCHEMA("CGameSceneNode", "m_pNextSibling"_hash);
        const auto owner_offset = SCHEMA("CGameSceneNode", "m_pOwner"_hash);
        const auto owner_handle_offset = SCHEMA("C_BaseEntity", "m_hOwnerEntity"_hash);

        for (auto view_model = g::memory.read<std::uintptr_t>(node + child_offset); view_model; view_model = g::memory.read<std::uintptr_t>(view_model + sibling_offset)) {
            
            const auto entity_instance = g::memory.read<std::uintptr_t>(view_model + owner_offset); // CGameSceneNode -> m_pOwner
            if (!entity_instance) continue;

            const auto owner_handle = g::memory.read<std::uint32_t>(entity_instance + owner_handle_offset);
            if (systems::g_entities.lookup(owner_handle) == weapon)
                return entity_instance;
            continue;
        }

        return get_hud_weapon(local_player, weapon);
    }

    void skinchanger::tick() {
        if (!settings::g_skinchanger.enabled) return;

        static bool initialized = false;
        if (!initialized) {
            regen_skins = g::memory.find_pattern(g::memory.get_module("client.dll"), "48 83 EC ? E8 ? ? ? ? 48 85 C0 0F 84 ? ? ? ? 48 8B 10");
            g::memory.write<uint16_t>(regen_skins + 0x52, SCHEMA("C_EconEntity", "m_AttributeManager"_hash) + SCHEMA("C_AttributeContainer", "m_Item"_hash) + SCHEMA("C_EconItemView", "m_AttributeList"_hash) + SCHEMA("CAttributeList", "m_Attributes"_hash));
            initialized = true;
        }

        const auto local_player = systems::g_local.pawn();
        if (!local_player) return;
        
        const auto life_state = g::memory.read<std::uint8_t>(local_player + SCHEMA("C_BaseEntity", "m_lifeState"_hash));
        if (life_state != 0) return; // 0 = alive, anything else = dead/dying
        
        const auto local_controller = g::memory.read<std::uintptr_t>(g::offsets.local_player_controller);
        if (local_controller) {
            const auto inventory_services = g::memory.read<std::uintptr_t>(local_controller + SCHEMA("CBasePlayerController", "m_pInventoryServices"_hash));
            if (inventory_services) {
                g::memory.write<std::uint16_t>(inventory_services + SCHEMA("CPlayer_InventoryServices", "m_unMusicID"_hash), settings::g_skinchanger.music_kit);
            }
        }
        
        bool should_update = false;
        std::shared_lock lock(settings::g_skinchanger.mutex);

        const auto weapons = this->get_weapons(local_player);

        for (const auto weapon : weapons) {
            if (!weapon) continue;

            const auto item = weapon + SCHEMA("C_EconEntity", "m_AttributeManager"_hash) + SCHEMA("C_AttributeContainer", "m_Item"_hash);

            if (this->m_force_update)
                g::memory.write<std::uint32_t>(item + SCHEMA("C_EconItemView", "m_iItemIDHigh"_hash), NULL);

            if (g::memory.read<std::uint32_t>(item + SCHEMA("C_EconItemView", "m_iItemIDHigh"_hash)) == -1)
                continue;

            g::memory.write<std::uint32_t>(item + SCHEMA("C_EconItemView", "m_iItemIDHigh"_hash), -1);

            const auto def_idx = g::memory.read<std::uint16_t>(item + SCHEMA("C_EconItemView", "m_iItemDefinitionIndex"_hash));
            const auto it = settings::g_skinchanger.weapon_skins.find(def_idx);
            if (it == settings::g_skinchanger.weapon_skins.end()) continue;

            const auto& cfg = it->second;
            if (cfg.paint_kit == 0) continue;

            g::memory.write<std::uint32_t>(weapon + SCHEMA("C_EconEntity", "m_nFallbackPaintKit"_hash), cfg.paint_kit);

            const uint64_t mask = cfg.uses_old_model + 1; // identical to bUsesOldModel + 1
            g::console.print("Uses old model? {}", cfg.uses_old_model ? "Yes" : "No");
            const auto hud_weapon = this->get_hud_weapon(local_player, weapon);

            auto set_mesh_mask = [&](std::uintptr_t ent, std::uint64_t target_mask) {
                if (!ent) return;
                const auto node = g::memory.read<std::uintptr_t>(ent + SCHEMA("C_BaseEntity", "m_pGameSceneNode"_hash));
                if (!node) return;
                const auto model_state = node + SCHEMA("CSkeletonInstance", "m_modelState"_hash);

                // Fixed: m_MeshGroupMask is NOT a pointer. 0xD8 is the pointer to dirty model data.
                const auto dirty_attributes = g::memory.read<std::uintptr_t>(model_state + 0xD8);
                if (dirty_attributes) {
                    g::memory.write<std::uint64_t>(dirty_attributes + 0x10, target_mask);
                }

                for (int retries = 0; retries < 20; retries++) {
                    for (int i = 0; i < 700; i++) {
                        g::memory.write<std::uint64_t>(model_state + SCHEMA("CModelState", "m_MeshGroupMask"_hash), target_mask);
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    if (g::memory.read<std::uint64_t>(model_state + SCHEMA("CModelState", "m_MeshGroupMask"_hash)) == target_mask)
                        break;
                }

            };


            set_mesh_mask(weapon, mask);
            set_mesh_mask(hud_weapon, mask);

            std::vector<CEconItemAttribute> attributes;
            if (cfg.paint_kit > 0) {
                CEconItemAttribute attr{};
                
                attr.defIndex = 6;
                attr.value = static_cast<float>(cfg.paint_kit);
                attr.initValue = attr.value;
                attributes.push_back(attr);

                attr.defIndex = 7;
                attr.value = cfg.seed;
                attr.initValue = attr.value;
                attributes.push_back(attr);

                attr.defIndex = 8;
                attr.value = cfg.wear;
                attr.initValue = attr.value;
                attributes.push_back(attr);
            }

            const auto attr_list = item + SCHEMA("C_EconItemView", "m_AttributeList"_hash) + SCHEMA("CAttributeList", "m_Attributes"_hash); // m_Attributes
            const auto preAttributes = g::memory.read<CPtrGameVector>(attr_list);

            if (!attributes.empty() && preAttributes.size == 0 && preAttributes.ptr == 0) {
                const auto alloc_size = attributes.size() * sizeof(CEconItemAttribute);
                const auto mem_block = g::memory.allocate(0, alloc_size, PAGE_READWRITE);

                if (mem_block) {
                    for (size_t i = 0; i < attributes.size(); i++) {
                        g::memory.write<CEconItemAttribute>(mem_block + (i * sizeof(CEconItemAttribute)), attributes[i]);
                    }

                    CPtrGameVector newAttributes{};
                    newAttributes.size = attributes.size();
                    newAttributes.ptr = mem_block;

                    g::memory.write<CPtrGameVector>(attr_list, newAttributes);
                }
            }

            should_update = true;
        }

        if (should_update || this->m_force_update) {
            g::memory.call_remote(regen_skins);

            for (const auto weapon : weapons) {
                if (g::memory.read<uint32_t>(weapon + SCHEMA("C_EconEntity", "m_nFallbackPaintKit"_hash)) == -1)
                    continue;

                g::memory.write<uint32_t>(weapon + SCHEMA("C_EconEntity", "m_nFallbackPaintKit"_hash), -1);

                const auto item = weapon + SCHEMA("C_EconEntity", "m_AttributeManager"_hash) + SCHEMA("C_AttributeContainer", "m_Item"_hash);
                const auto attr_list = item + SCHEMA("C_EconItemView", "m_AttributeList"_hash) + SCHEMA("CAttributeList", "m_Attributes"_hash);
                const auto preAttributes = g::memory.read<CPtrGameVector>(attr_list);

                if (preAttributes.size) {
                    g::memory.write<CPtrGameVector>(attr_list, CPtrGameVector{0, 0});
                    g::memory.free(preAttributes.ptr);
                }
            }
        }

        this->m_force_update = false;
    }

    std::vector<std::uintptr_t> skinchanger::get_weapons(std::uintptr_t local_player) {
        std::vector<std::uintptr_t> list;
        if (!local_player) return list;
        
        const auto weapon_services = g::memory.read<std::uintptr_t>(local_player + SCHEMA("C_BasePlayerPawn", "m_pWeaponServices"_hash));
        if (!weapon_services) return list;

        const auto size = g::memory.read<std::uint64_t>(weapon_services + SCHEMA("CPlayer_WeaponServices", "m_hMyWeapons"_hash));
        const auto data = g::memory.read<std::uintptr_t>(weapon_services + SCHEMA("CPlayer_WeaponServices", "m_hMyWeapons"_hash) + 8);

        if (size > 256 || size == 0 || !data) return list;

        for (std::uint32_t i = 0; i < size; ++i) {
            const auto handle = g::memory.read<std::uint32_t>(data + i * 4);
            if (!handle || handle == 0xFFFFFFFF) continue;

            const auto weapon = systems::g_entities.lookup(handle);
            if (weapon) list.push_back(weapon);
        }
        return list;
    }

}

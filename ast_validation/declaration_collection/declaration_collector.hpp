#include "general_validator_structs.hpp"

bool fill_declaration_info(
    std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Func>>& func_name_map,
    std::unordered_map<std::string, std::shared_ptr<ActorFrontend>>& actor_frontend_map,
    Program *root);
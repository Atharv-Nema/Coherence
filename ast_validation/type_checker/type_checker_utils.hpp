#include "types.hpp"
#include "general_validator_structs.hpp"

bool ref_cap_equal(Cap c1, Cap c2);
std::optional<Cap> viewpoint_adaptation_op(std::optional<Cap> outer_view, std::optional<Cap> inner_view);
bool capabilities_assignable(Cap c1, Cap c2);
bool capability_shareable(Cap cap);
bool capability_mutable(Cap c);
std::shared_ptr<const Type> get_dereferenced_type(std::shared_ptr<const Type> type);
std::shared_ptr<const Type> apply_viewpoint_to_type(std::optional<Cap> viewpoint, std::shared_ptr<const Type> type);
std::shared_ptr<const Type> get_type_of_nameable(std::shared_ptr<NameableType> nameable_type);
std::shared_ptr<const Type> get_standardized_type_from_name(TypeContext& type_context, std::optional<Cap> viewpoint, const std::string& type_name);
std::shared_ptr<const Type> get_standardized_type(TypeContext& type_context, std::shared_ptr<const Type> type);
std::optional<NameableType::Struct> get_struct_type(TypeContext& type_context, const std::string& struct_type_name);
bool type_is_printable(TypeContext &type_context, std::shared_ptr<const Type> type);
bool type_is_int(TypeContext& type_context, std::shared_ptr<const Type> type);
bool type_is_bool(TypeContext& type_context, std::shared_ptr<const Type> type);
bool type_rel_comparable(TypeContext& type_context, std::shared_ptr<const Type> type_1, std::shared_ptr<const Type> type_2);
bool type_equality_comparable(TypeContext& type_context, std::shared_ptr<const Type> type_1, std::shared_ptr<const Type> type_2);
bool type_assignable(TypeContext& type_context, std::shared_ptr<const Type> lhs, std::shared_ptr<const Type> rhs);
bool type_shareable(TypeContext& type_context, std::shared_ptr<const Type> type);
std::shared_ptr<const Type> unaliased_type(std::shared_ptr<const Type> type);
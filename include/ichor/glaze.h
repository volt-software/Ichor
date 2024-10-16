#pragma once

// Glaze uses different conventions than Ichor, ignore them to prevent being spammed by warnings
#if defined( __GNUC__ )
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wsign-conversion"
#    pragma GCC diagnostic ignored "-Wshadow"
#    pragma GCC diagnostic ignored "-Wconversion"
#    pragma GCC diagnostic ignored "-Wmissing-braces"
#endif
#include <glaze/glaze.hpp>
#if defined( __GNUC__ )
#    pragma GCC diagnostic pop
#endif



template<>
struct fmt::formatter<glz::error_code> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const glz::error_code& input, FormatContext& ctx) const -> decltype(ctx.out()) {
        switch(input) {
            case glz::error_code::none:
                return fmt::format_to(ctx.out(), "glz::error_code::none");
            case glz::error_code::no_read_input:
                return fmt::format_to(ctx.out(), "glz::error_code::no_read_input");
            case glz::error_code::data_must_be_null_terminated:
                return fmt::format_to(ctx.out(), "glz::error_code::data_must_be_null_terminated");
            case glz::error_code::parse_number_failure:
                return fmt::format_to(ctx.out(), "glz::error_code::parse_number_failure");
            case glz::error_code::expected_brace:
                return fmt::format_to(ctx.out(), "glz::error_code::expected_brace");
            case glz::error_code::expected_bracket:
                return fmt::format_to(ctx.out(), "glz::error_code::expected_bracket");
            case glz::error_code::expected_quote:
                return fmt::format_to(ctx.out(), "glz::error_code::expected_quote");
            case glz::error_code::expected_comma:
                return fmt::format_to(ctx.out(), "glz::error_code::expected_comma");
            case glz::error_code::expected_colon:
                return fmt::format_to(ctx.out(), "glz::error_code::expected_colon");
            case glz::error_code::exceeded_static_array_size:
                return fmt::format_to(ctx.out(), "glz::error_code::exceeded_static_array_size");
            case glz::error_code::unexpected_end:
                return fmt::format_to(ctx.out(), "glz::error_code::unexpected_end");
            case glz::error_code::expected_end_comment:
                return fmt::format_to(ctx.out(), "glz::error_code::expected_end_comment");
            case glz::error_code::syntax_error:
                return fmt::format_to(ctx.out(), "glz::error_code::syntax_error");
            case glz::error_code::key_not_found:
                return fmt::format_to(ctx.out(), "glz::error_code::key_not_found");
            case glz::error_code::unexpected_enum:
                return fmt::format_to(ctx.out(), "glz::error_code::unexpected_enum");
            case glz::error_code::attempt_const_read:
                return fmt::format_to(ctx.out(), "glz::error_code::attempt_const_read");
            case glz::error_code::attempt_member_func_read:
                return fmt::format_to(ctx.out(), "glz::error_code::attempt_member_func_read");
            case glz::error_code::attempt_read_hidden:
                return fmt::format_to(ctx.out(), "glz::error_code::attempt_read_hidden");
            case glz::error_code::invalid_nullable_read:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_nullable_read");
            case glz::error_code::invalid_variant_object:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_variant_object");
            case glz::error_code::invalid_variant_array:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_variant_array");
            case glz::error_code::invalid_variant_string:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_variant_string");
            case glz::error_code::no_matching_variant_type:
                return fmt::format_to(ctx.out(), "glz::error_code::no_matching_variant_type");
            case glz::error_code::expected_true_or_false:
                return fmt::format_to(ctx.out(), "glz::error_code::expected_true_or_false");
            case glz::error_code::unknown_key:
                return fmt::format_to(ctx.out(), "glz::error_code::unknown_key");
            case glz::error_code::invalid_flag_input:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_flag_input");
            case glz::error_code::invalid_escape:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_escape");
            case glz::error_code::u_requires_hex_digits:
                return fmt::format_to(ctx.out(), "glz::error_code::u_requires_hex_digits");
            case glz::error_code::file_extension_not_supported:
                return fmt::format_to(ctx.out(), "glz::error_code::file_extension_not_supported");
            case glz::error_code::could_not_determine_extension:
                return fmt::format_to(ctx.out(), "glz::error_code::could_not_determine_extension");
            case glz::error_code::seek_failure:
                return fmt::format_to(ctx.out(), "glz::error_code::seek_failure");
            case glz::error_code::unicode_escape_conversion_failure:
                return fmt::format_to(ctx.out(), "glz::error_code::unicode_escape_conversion_failure");
            case glz::error_code::file_open_failure:
                return fmt::format_to(ctx.out(), "glz::error_code::file_open_failure");
            case glz::error_code::file_close_failure:
                return fmt::format_to(ctx.out(), "glz::error_code::file_close_failure");
            case glz::error_code::file_include_error:
                return fmt::format_to(ctx.out(), "glz::error_code::file_include_error");
            case glz::error_code::dump_int_error:
                return fmt::format_to(ctx.out(), "glz::error_code::dump_int_error");
            case glz::error_code::get_nonexistent_json_ptr:
                return fmt::format_to(ctx.out(), "glz::error_code::get_nonexistent_json_ptr");
            case glz::error_code::get_wrong_type:
                return fmt::format_to(ctx.out(), "glz::error_code::get_wrong_type");
            case glz::error_code::cannot_be_referenced:
                return fmt::format_to(ctx.out(), "glz::error_code::cannot_be_referenced");
            case glz::error_code::invalid_get:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_get");
            case glz::error_code::invalid_get_fn:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_get_fn");
            case glz::error_code::invalid_call:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_call");
            case glz::error_code::invalid_partial_key:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_partial_key");
            case glz::error_code::name_mismatch:
                return fmt::format_to(ctx.out(), "glz::error_code::name_mismatch");
            case glz::error_code::array_element_not_found:
                return fmt::format_to(ctx.out(), "glz::error_code::array_element_not_found");
            case glz::error_code::elements_not_convertible_to_design:
                return fmt::format_to(ctx.out(), "glz::error_code::elements_not_convertible_to_design");
            case glz::error_code::unknown_distribution:
                return fmt::format_to(ctx.out(), "glz::error_code::unknown_distribution");
            case glz::error_code::invalid_distribution_elements:
                return fmt::format_to(ctx.out(), "glz::error_code::invalid_distribution_elements");
            case glz::error_code::missing_key:
                return fmt::format_to(ctx.out(), "glz::error_code::missing_key");
            case glz::error_code::hostname_failure:
                return fmt::format_to(ctx.out(), "glz::error_code::hostname_failure");
            case glz::error_code::includer_error:
                return fmt::format_to(ctx.out(), "glz::error_code::includer_error");
        }
        return fmt::format_to(ctx.out(), "glz::error_code:: ??? report bug!");
    }
};

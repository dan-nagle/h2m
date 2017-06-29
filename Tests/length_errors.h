// This file should cause a whole host of length errors.
// It should warn about them, however, as h2m processes
// it.

#define this_macro_is_too_long_to_be_a_fortran_macro_because_there_are_line_limits_in_fortran
#define this_macr_name_is_exactly_63_characters_long_thus_should_be_ok();


int this_function_will_produce_lines_too_long_woe_is_us(int arg1, double arg2, void* arg3,
    char arg4, char* arg5, int arg6);

void this_function_will_also_produce_lines_too_long_woe_is_us(int arg1, double arg2, void* arg3,
    char arg4, char* arg5, int arg6);

int this_function_is_exactly_one_character_too_long_for_fortran_woe_();
void this_subroutn_is_exactly_one_character_too_long_for_fortran_woe_();

int this_is_not_a_fancy_function_but_you_notice_that_it_is_also_too_long_for_fortran;
double but_this_one_should_be_just_barely_short_enough_to_be_accepted_

double this_var_name_is_exactly_63_characters_long_thus_should_be_ok__;

double this_func_name_is_exactly_63_characters_long_thus_should_be_ok_();

void this_subr_name_is_exactly_63_characters_long_thus_should_be_ok();


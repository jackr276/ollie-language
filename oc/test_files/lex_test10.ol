/*
declare structure {
	u_int32 i := 3,
	u_int32 j,
	u_int32 prog
} my_struct;

define structure my_struct as custom;
define u_int8* as int_ptr;
*/

func my_func(u_int32* error_code) -> u_int32{
	let u_int32 i := 0;
	let u_int32 my_func := 1;
	let u_int32 k := 0;
	declare float64 i_do_not_exist;

	main(i, j, i_do_not_exist); 
}

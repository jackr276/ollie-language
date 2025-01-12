declare structure {
	u_int32 i := 3,
	u_int32 j,
	u_int32 prog
} my_struct;

define structure my_struct as custom;

func my_func(u_int32* error_code) -> u_int32{
	let u_int32 i := 0;
	let u_int32 i := 1;
	let u_int32 k := 0;
	let structure my_struct* a := 0;

	main(i, j, i_do_not_exist); 
}

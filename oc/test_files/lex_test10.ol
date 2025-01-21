define construct my_struct{
	u_int32 i,
	u_int32 j,
	u_int32 prog
};

alias construct my_struct as custom;
alias u_int8& as int_ptr;


func my_func(u_int32 error_code) -> u_int32{
	let u_int32 i := 0;
	let u_int32 my_func := 1;
	let u_int32 k := 0;
	declare construct my_struct[500] struct_arr;

	let u_int32 i := struct_arr[3]=>i;
	

	declare float64 i_do_not_exist;

	main(i, j, i_do_not_exist); 
}

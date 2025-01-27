define construct my_struct{
	u_int32 i;
	u_int32 j;
	u_int32 prog;
};

alias u_int8* as int_ptr;

func unused() -> u_int16{
	let char* str_literal := "I do nothing";

	ret 2;
}


func my_func(u_int32 error_code) -> u_int32{
	let u_int32 l := 0;
	let u_int32 a := 1;
	let u_int32 k := 0;
	declare construct my_struct*[500] struct_arr;

	let u_int32 struct_access := struct_arr[3]=>i;
	
	declare float64 i_do_not_exist;

	@main(i, j, i_do_not_exist); 
}

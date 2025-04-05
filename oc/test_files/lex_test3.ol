/**
* Another sample program here
* This program is total nonsense, it just tests the lexing
*/


alias char** as str_arr;
alias char* as str;

fn:static main(arg_count:u32, arg_vector:str_arr) -> i32{
	declare my_str:str;
	declare c:u32;

	if(arg_count != 0) then {
		arg_count := -1;
		//arg_vector := "hello";

	} else if(arg_count >= -1) then {
		let a:f32 := .23;
		let b:f32 := 2.322;
		let c:f32 := a + b;


	} else {
		arg_count := -2;
	}

	ret c >> 3;
}

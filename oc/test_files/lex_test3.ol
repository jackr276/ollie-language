/**
* Another sample program here
* This program is total nonsense, it just tests the lexing
*/
alias char** as str_arr;
alias char* as str;

func:static main(u32 arg_count, str_arr arg_vector) -> i32{
	declare str my_str;
	declare u32 c;

	if(arg_count != 0) then {
		asn arg_count := -1;
		//asn arg_vector := "hello";

	} else if(arg_count >= -1) then {
		let f32 a := .23;
		let f32 b := 2.322;
		let f32 c := a + b;


	} else {
		asn arg_count := -2;
	}

	ret c >> 3;
}

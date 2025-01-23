/**
* Another sample program here
* This program is total nonsense, it just tests the lexing
*/
alias char** as str_arr;
alias char* as str;

func:static main(u_int32 arg_count, str_arr arg_vector) -> u_int32{
	declare str my_str;
	declare u_int32 c;

	if(arg_count == 0) then {
		arg_count := -1;
		arg_vector := "hello";

	} else if(arg_count <= -1) then {
		let float32 a := .23;
		let float32 b := 2.322;
		let float32 c := a + b;

	} else {
		asn arg_count := -2;
	}

	ret c >> 3;
}

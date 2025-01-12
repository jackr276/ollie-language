/**
* Another sample program here
* This program is total nonsense, it just tests the lexing
*/
func:static main(u_int32 arg_count, str arg_vector) -> u_int32{
	declare str my_str;

	if(arg_count == 0) then {
		asn arg_count := -1;
		asn arg_vector := "hello";

	} else if(arg_count <= -1) then {
		let float32 a := .23;
		let float32 b := 2.322;
		let float32 c := a + b;

	} else {
		asn arg_count := -2;
	}

	ret c >> 3;
}
